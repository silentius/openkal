/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/server/AccountPassword.h"

#include <algorithm>
#include <array>
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

namespace openkal::server {
    namespace {
        bool alnum(char c) {
            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
        }
        std::string lower(std::string_view value) {
            std::string out(value);
            for (char& c : out) {
                if (c >= 'A' && c <= 'Z') {
                    c = static_cast<char>(c - 'A' + 'a');
                }
            }
            return out;
        }
        std::string hex(const unsigned char* data, std::size_t size) {
            const char* digits = "0123456789abcdef";
            std::string out;
            for (std::size_t i = 0; i < size; ++i) {
                out += digits[data[i] >> 4];
                out += digits[data[i] & 15];
            }
            return out;
        }
        bool unhex(std::string_view text, unsigned char* out, std::size_t size) {
            if (text.size() != size * 2) {
                return false;
            }
            for (std::size_t i = 0; i < size; ++i) {
                unsigned v = 0;
                for (unsigned j = 0; j < 2; ++j) {
                    char c = text[i * 2 + j];
                    if (c >= '0' && c <= '9') {
                        v = v * 16 + static_cast<unsigned>(c - '0');
                    } else if (c >= 'a' && c <= 'f') {
                        v = v * 16 + static_cast<unsigned>(c - 'a' + 10);
                    } else {
                        return false;
                    }
                }
                out[i] = static_cast<unsigned char>(v);
            }
            return true;
        }
        bool derive(std::string_view password,
                    const std::array<unsigned char, 16>& salt,
                    std::array<unsigned char, 32>& out) {
            return EVP_PBE_scrypt(password.data(),
                                  password.size(),
                                  salt.data(),
                                  salt.size(),
                                  32768,
                                  8,
                                  3,
                                  64 * 1024 * 1024,
                                  out.data(),
                                  out.size()) == 1;
        }
    }  // namespace
    std::string normalize_account_name(std::string_view name) {
        if (name.size() < 3 || name.size() > 32 ||
            !std::all_of(name.begin(), name.end(), [](char c) { return alnum(c) || c == '_'; })) {
            return {};
        }
        return lower(name);
    }
    std::string normalize_account_email(std::string_view email) {
        if (email.size() > 254 || email.size() < 5) {
            return {};
        }
        const auto at = email.find('@');
        if (at == std::string_view::npos || at == 0 || at > 64 || at != email.rfind('@')) {
            return {};
        }
        auto local = email.substr(0, at);
        auto domain = email.substr(at + 1);
        if (local.front() == '.' || local.back() == '.' ||
            local.find("..") != std::string_view::npos ||
            !std::all_of(local.begin(), local.end(), [](char c) {
                return alnum(c) || c == '.' || c == '_' || c == '+' || c == '-';
            })) {
            return {};
        }
        if (domain.find('.') == std::string_view::npos || domain.empty() || domain.front() == '.' ||
            domain.back() == '.') {
            return {};
        }
        std::size_t start = 0;
        while (start < domain.size()) {
            auto end = domain.find('.', start);
            if (end == std::string_view::npos) {
                end = domain.size();
            }
            auto label = domain.substr(start, end - start);
            if (label.empty() || label.size() > 63 || !alnum(label.front()) ||
                !alnum(label.back()) || !std::all_of(label.begin(), label.end(), [](char c) {
                    return alnum(c) || c == '-';
                })) {
                return {};
            }
            start = end + 1;
        }
        return lower(email);
    }
    bool valid_account_password(std::string_view password) {
        return password.size() >= 12 && password.size() <= 128 &&
               std::all_of(password.begin(), password.end(), [](unsigned char c) {
                   return c >= 32 && c != 127;
               });
    }
    std::string hash_account_password(std::string_view password) {
        if (!valid_account_password(password)) {
            return {};
        }
        std::array<unsigned char, 16> salt{};
        std::array<unsigned char, 32> hash{};
        if (RAND_bytes(salt.data(), salt.size()) != 1 || !derive(password, salt, hash)) {
            return {};
        }
        return "scrypt-v1$" + hex(salt.data(), salt.size()) + "$" + hex(hash.data(), hash.size());
    }
    bool verify_account_password(std::string_view password, std::string_view encoded) {
        if (!valid_account_password(password) || encoded.size() != 107 ||
            encoded.substr(0, 10) != "scrypt-v1$" || encoded[42] != '$') {
            return false;
        }
        std::array<unsigned char, 16> salt{};
        std::array<unsigned char, 32> expected{}, actual{};
        return unhex(encoded.substr(10, 32), salt.data(), salt.size()) &&
               unhex(encoded.substr(43), expected.data(), expected.size()) &&
               derive(password, salt, actual) &&
               CRYPTO_memcmp(expected.data(), actual.data(), actual.size()) == 0;
    }
}  // namespace openkal::server
