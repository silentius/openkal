/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "DevelopmentCertificate.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <fcntl.h>
#include <filesystem>
#include <memory>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <openssl/x509v3.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

namespace openkal::server {
    namespace {
        using File = std::unique_ptr<FILE, decltype(&std::fclose)>;

        File open_file(const std::string& path, int flags) {
            const int fd = ::open(path.c_str(), flags | O_CLOEXEC | O_NOFOLLOW, 0600);
            if (fd < 0) {
                return {nullptr, std::fclose};
            }
            FILE* file = ::fdopen(fd, "w");
            if (!file) {
                ::close(fd);
            }
            return {file, std::fclose};
        }

        bool add_extension(X509* certificate, int nid, const char* value) {
            const std::unique_ptr<X509_EXTENSION, decltype(&X509_EXTENSION_free)> extension(
                X509V3_EXT_conf_nid(nullptr, nullptr, nid, value), X509_EXTENSION_free);
            return extension && X509_add_ext(certificate, extension.get(), -1) == 1;
        }
    }  // namespace

    bool ensure_development_certificate(const std::string& directory, std::string& error) {
        error = "Could not prepare localhost development TLS credentials.";
        const std::filesystem::path root(directory);
        std::error_code filesystem_error;
        const bool created_directory = std::filesystem::create_directories(root, filesystem_error);
        if (filesystem_error || (created_directory && ::chmod(directory.c_str(), 0700) != 0)) {
            return false;
        }
        const auto lock = open_file((root / ".generation.lock").string(), O_CREAT | O_RDWR);
        if (!lock || ::flock(::fileno(lock.get()), LOCK_EX) != 0) {
            return false;
        }
        const auto cert_path = root / "server.pem", key_path = root / "server-key.pem";
        const bool has_certificate = std::filesystem::exists(cert_path, filesystem_error);
        if (filesystem_error) {
            return false;
        }
        const bool has_key = std::filesystem::exists(key_path, filesystem_error);
        if (filesystem_error) {
            return false;
        }
        if (has_certificate || has_key) {
            if (has_certificate && has_key) {
                error.clear();
                return true;
            }
            error = "Incomplete development TLS pair; restore the missing certificate or key. "
                    "Existing credentials were not replaced.";
            return false;
        }
        const std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> key(
            EVP_PKEY_Q_keygen(nullptr, nullptr, "RSA", std::size_t{3072}), EVP_PKEY_free);
        const std::unique_ptr<X509, decltype(&X509_free)> certificate(X509_new(), X509_free);
        std::uint64_t serial = 0;
        if (!key || !certificate ||
            RAND_bytes(reinterpret_cast<unsigned char*>(&serial), sizeof(serial)) != 1 ||
            X509_set_version(certificate.get(), 2) != 1 ||
            ASN1_INTEGER_set_uint64(X509_get_serialNumber(certificate.get()),
                                    serial ? serial : 1) != 1 ||
            !X509_gmtime_adj(X509_getm_notBefore(certificate.get()), -60) ||
            !X509_gmtime_adj(X509_getm_notAfter(certificate.get()), 30L * 24 * 60 * 60) ||
            X509_set_pubkey(certificate.get(), key.get()) != 1) {
            return false;
        }
        auto* subject = X509_get_subject_name(certificate.get());
        if (!subject ||
            X509_NAME_add_entry_by_txt(subject,
                                       "CN",
                                       MBSTRING_ASC,
                                       reinterpret_cast<const unsigned char*>("localhost"),
                                       -1,
                                       -1,
                                       0) != 1 ||
            X509_set_issuer_name(certificate.get(), subject) != 1 ||
            !add_extension(certificate.get(), NID_basic_constraints, "critical,CA:TRUE") ||
            !add_extension(certificate.get(),
                           NID_key_usage,
                           "critical,digitalSignature,keyEncipherment,keyCertSign") ||
            !add_extension(certificate.get(), NID_ext_key_usage, "serverAuth") ||
            !add_extension(certificate.get(), NID_subject_alt_name, "DNS:localhost,IP:127.0.0.1") ||
            X509_sign(certificate.get(), key.get(), EVP_sha256()) <= 0) {
            return false;
        }
        auto key_file = open_file(key_path.string(), O_CREAT | O_EXCL | O_WRONLY);
        if (!key_file) {
            return false;
        }
        bool ok = PEM_write_PrivateKey(
                      key_file.get(), key.get(), nullptr, nullptr, 0, nullptr, nullptr) == 1 &&
                  std::fflush(key_file.get()) == 0;
        key_file.reset();
        if (ok) {
            auto cert_file = open_file(cert_path.string(), O_CREAT | O_EXCL | O_WRONLY);
            if (cert_file) {
                ok = PEM_write_X509(cert_file.get(), certificate.get()) == 1 &&
                     std::fflush(cert_file.get()) == 0;
                cert_file.reset();
                if (!ok) {
                    std::filesystem::remove(cert_path, filesystem_error);
                }
            } else {
                ok = false;
            }
        }
        if (!ok) {
            std::filesystem::remove(key_path, filesystem_error);
            return false;
        }
        error.clear();
        return true;
    }
}  // namespace openkal::server
