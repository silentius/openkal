/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "TradingPostStore.h"

#include "openkal/platform/Json.h"

#include <algorithm>
#include <charconv>
#include <limits>

namespace openkal::server::trading_post {
    namespace {
        using protocol::TradingPostDelivery;
        using protocol::TradingPostEntry;
        using protocol::TradingPostView;
        std::int64_t integer(const std::string& text) {
            std::int64_t n = 0;
            const auto r = std::from_chars(text.data(), text.data() + text.size(), n);
            return r.ec == std::errc{} && r.ptr == text.data() + text.size() ? n : -1;
        }
        const std::string columns =
            "prefix,cur_end,max_end,x_attack,x_magic,x_hit,x_defense,x_dodge,"
            "upgrade_level,upgrade_rate,mix_type,bof,qigong_perforated,qigong_effect,dragon_effect,"
            "random_stat_type,random_stat_value,transform_level,transform_exp,enchant_protected,"
            "pet_expire_at,ride_expire_at";
        std::vector<std::string> attributes(const ItemRecord& i) {
            std::vector<std::string> out;
            for (const auto value : {static_cast<std::int64_t>(i.prefix),
                                     static_cast<std::int64_t>(i.cur_end),
                                     static_cast<std::int64_t>(i.max_end),
                                     static_cast<std::int64_t>(i.x_attack),
                                     static_cast<std::int64_t>(i.x_magic),
                                     static_cast<std::int64_t>(i.x_hit),
                                     static_cast<std::int64_t>(i.x_defense),
                                     static_cast<std::int64_t>(i.x_dodge),
                                     static_cast<std::int64_t>(i.upgrade_level),
                                     static_cast<std::int64_t>(i.upgrade_rate),
                                     static_cast<std::int64_t>(i.mix_type),
                                     static_cast<std::int64_t>(i.bof),
                                     static_cast<std::int64_t>(i.qigong_perforated),
                                     static_cast<std::int64_t>(i.qigong_effect),
                                     static_cast<std::int64_t>(i.dragon_effect),
                                     static_cast<std::int64_t>(i.random_stat_type),
                                     static_cast<std::int64_t>(i.random_stat_value),
                                     static_cast<std::int64_t>(i.transform_level),
                                     static_cast<std::int64_t>(i.transform_exp),
                                     static_cast<std::int64_t>(i.enchant_protected ? 1 : 0),
                                     i.pet_expire_at,
                                     i.ride_expire_at}) {
                out.push_back(std::to_string(value));
            }
            return out;
        }
        bool decode(const std::string& text, protocol::TradeItem& item) {
            platform::Json a;
            if (!platform::Json::parse(text, a) || !a.is_array() || a.array().size() != 22) {
                return false;
            }
            for (std::size_t i = 0; i < 20; ++i) {
                const auto n = integer(a.array()[i].as_string());
                if (n < std::numeric_limits<int>::min() || n > std::numeric_limits<int>::max()) {
                    return false;
                }
                item.attributes[i] = static_cast<int>(n);
            }
            const auto pet = integer(a.array()[20].as_string()),
                       ride = integer(a.array()[21].as_string());
            if (pet < 0 || ride < 0) {
                return false;
            }
            item.pet_expire_at = static_cast<std::uint64_t>(pet);
            item.ride_expire_at = static_cast<std::uint64_t>(ride);
            return true;
        }
        TradingPostResult fail(std::string text) {
            return {false, 0, 0, std::move(text)};
        }
        std::string item_columns(std::string_view alias = {}) {
            std::string result;
            std::size_t begin = 0;
            do {
                const auto end = columns.find(',', begin);
                if (!result.empty()) {
                    result += ',';
                }
                result += alias;
                result += columns.substr(begin, end - begin);
                if (end == std::string::npos) {
                    break;
                }
                begin = end + 1;
            } while (true);
            return result;
        }
        bool read_item(const std::vector<std::string>& row,
                       std::size_t offset,
                       protocol::TradeItem& item) {
            if (row.size() < offset + 22) {
                return false;
            }
            for (std::size_t n = 0; n < 20; ++n) {
                const auto value = integer(row[offset + n]);
                if (value < INT32_MIN || value > INT32_MAX) {
                    return false;
                }
                item.attributes[n] = static_cast<int>(value);
            }
            const auto pet = integer(row[offset + 20]), ride = integer(row[offset + 21]);
            if (pet < 0 || ride < 0) {
                return false;
            }
            item.pet_expire_at = static_cast<std::uint64_t>(pet);
            item.ride_expire_at = static_cast<std::uint64_t>(ride);
            return true;
        }
        bool changed(const std::optional<Rows>& rows) {
            return rows && rows->size() == 1;
        }
        bool receipt(const Query& q,
                     int owner,
                     std::int64_t iid,
                     int index,
                     int count,
                     std::int64_t price,
                     TradingPostDelivery kind) {
            return q("INSERT INTO trading_post_deliveries(character_id,item_iid,item_index,count,"
                     "unit_price,kind) VALUES(?,NULLIF(CAST(? AS BIGINT),0),?,?,?,?)",
                     {std::to_string(owner),
                      std::to_string(iid),
                      std::to_string(index),
                      std::to_string(count),
                      std::to_string(price),
                      std::to_string(static_cast<int>(kind))})
                .has_value();
        }
        // Only a real stack split creates a new item row; whole instances keep their IID.
        bool split(const Query& q, int source, int iid, int count, int account) {
            if (iid <= 0 || iid == source) {
                return false;
            }
            const auto mounts = q("SELECT mount_iid FROM battle_mounts WHERE mount_iid=?",
                                  {std::to_string(source)});
            if (!mounts || !mounts->empty()) {
                return false;
            }
            return changed(q(
                "INSERT INTO items(iid,character_id,account_id,location,item_index,"
                "count,slot," +
                    columns +
                    ",account_bound,character_bound) "
                    "SELECT ?,NULL,?,2,item_index,?,0," +
                    columns + ",account_bound,character_bound FROM items WHERE iid=? RETURNING iid",
                {std::to_string(iid),
                 std::to_string(account),
                 std::to_string(count),
                 std::to_string(source)}));
        }
    }  // namespace

    bool migrate_escrow(const Query& q) {
        const auto highest = q("SELECT COALESCE(MAX(iid),0) FROM items", {});
        if (!highest || highest->size() != 1) {
            return false;
        }
        auto next = integer(highest->front()[0]);
        for (const bool offers : {true, false}) {
            const auto rows = q(offers ? "SELECT id,seller,item_index,count,attributes,mount FROM "
                                         "trading_post_offers WHERE count>0"
                                       : "SELECT id,character_id,item_index,count,attributes,mount "
                                         "FROM trading_post_deliveries WHERE kind<>1",
                                {});
            if (!rows) {
                return false;
            }
            for (const auto& row : *rows) {
                if (++next <= 0 || next > INT32_MAX) {
                    return false;
                }
                protocol::TradeItem item;
                platform::Json attrs;
                if (!decode(row[4], item) || !platform::Json::parse(row[4], attrs)) {
                    return false;
                }
                std::vector<std::string> args{std::to_string(next), row[1], row[2], row[3]};
                if (offers) {
                    args[1] = std::to_string(kTradingPostAccountId);
                }
                for (const auto& value : attrs.array()) {
                    args.push_back(value.as_string());
                }
                std::string placeholders;
                for (std::size_t n = 0; n < 22; ++n) {
                    placeholders += ",?";
                }
                const auto account =
                    offers ? "?" : "(SELECT account_id FROM characters WHERE id=?)";
                if (!q("INSERT INTO items(iid,account_id,item_index,count,location," + columns +
                           ") VALUES(?," + account + ",?,?,2" + placeholders + ")",
                       args)) {
                    return false;
                }
                if (!row[5].empty()) {
                    platform::Json mount;
                    if (!platform::Json::parse(row[5], mount) || !mount.is_array() ||
                        mount.array().size() != 15) {
                        return false;
                    }
                    std::vector<std::string> mount_args{std::to_string(next)};
                    for (const auto& value : mount.array()) {
                        if (integer(value.as_string()) < 0) {
                            return false;
                        }
                        mount_args.push_back(value.as_string());
                    }
                    if (!q("INSERT INTO battle_mounts(mount_iid,grade,satiety,max_satiety,revision,"
                           "ability0,ability1,ability2,ability3,ability4,ability5,equipment0,"
                           "equipment1,equipment2,equipment3,equipment4) "
                           "VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)",
                           mount_args)) {
                        return false;
                    }
                }
                if (!q(std::string("UPDATE ") +
                           (offers ? "trading_post_offers" : "trading_post_deliveries") +
                           " SET item_iid=? WHERE id=?",
                       {std::to_string(next), row[0]})) {
                    return false;
                }
            }
        }
        return true;
    }

    std::optional<protocol::TradingPostState>
    list(const Query& q, int owner, const protocol::TradingPostRequest& r) {
        protocol::TradingPostState s;
        s.view = r.view;
        s.page = r.filter.page;
        s.acknowledged = r.operation;
        if (r.view == TradingPostView::Sell) {
            s.page = 0;
            return s;
        }
        const auto& f = r.filter;
        std::vector<std::string> args;
        std::string select, from, order;
        if (r.view == TradingPostView::Collection) {
            select = "SELECT d.id,COALESCE(i.item_index,d.item_index),COALESCE(i.count,d.count),"
                     "d.unit_price,d.kind," +
                     item_columns("i.");
            from = " FROM trading_post_deliveries d LEFT JOIN items i ON i.iid=d.item_iid "
                   "WHERE d.character_id=?";
            order = " ORDER BY d.id DESC";
            args.push_back(std::to_string(owner));
        } else {
            select = "SELECT o.id,i.item_index,i.count,o.unit_price,0," + item_columns("i.");
            from = " FROM trading_post_offers o JOIN items i ON i.iid=o.item_iid WHERE i.count>0";
            if (r.view == TradingPostView::Own) {
                from += " AND o.seller=?";
                args.push_back(std::to_string(owner));
            }
            if (r.view == TradingPostView::Buy) {
                if (f.clazz >= 0) {
                    from += " AND (o.clazz=-1 OR o.clazz=?)";
                    args.push_back(std::to_string(f.clazz));
                }
                if (f.type != protocol::TradingPostType::Any) {
                    from += " AND o.type=?";
                    args.push_back(std::to_string(static_cast<int>(f.type)));
                }
                if (f.prefix >= 0 && protocol::trading_post_prefixes(f.type)) {
                    from += " AND i.prefix=?";
                    args.push_back(std::to_string(f.prefix));
                }
                if (f.grade >= 0) {
                    from += " AND o.grade=?";
                    args.push_back(std::to_string(f.grade));
                }
            }
            static constexpr const char* orders[] = {
                "o.unit_price DESC,o.id ASC",
                "o.unit_price ASC,o.id ASC",
                "i.upgrade_level DESC,o.unit_price ASC,o.id ASC",
                "i.upgrade_level ASC,o.unit_price ASC,o.id ASC"};
            const auto order_index = static_cast<unsigned>(f.order);
            if (order_index > 3) {
                return std::nullopt;
            }
            order = " ORDER BY " + std::string(orders[order_index]);
        }
        const auto count = q("SELECT COUNT(*)" + from, args);
        if (!count || count->size() != 1 || count->front().size() != 1) {
            return std::nullopt;
        }
        const auto total = integer(count->front()[0]);
        if (total < 0) {
            return std::nullopt;
        }
        const auto pages = std::max<std::int64_t>(
            1,
            total / protocol::kTradingPostPageSize + (total % protocol::kTradingPostPageSize != 0));
        if (pages > UINT32_MAX) {
            return std::nullopt;
        }
        s.total_pages = static_cast<std::uint32_t>(pages);
        s.page = std::min(s.page, s.total_pages - 1);
        const auto sql = select + from + order + " LIMIT " +
                         std::to_string(protocol::kTradingPostPageSize + 1) + " OFFSET ?";
        args.push_back(
            std::to_string(static_cast<std::uint64_t>(s.page) * protocol::kTradingPostPageSize));
        const auto rows = q(sql, args);
        if (!rows) {
            return std::nullopt;
        }
        for (const auto& row : *rows) {
            if (s.entries.size() == protocol::kTradingPostPageSize) {
                s.more = true;
                break;
            }
            TradingPostEntry e;
            e.id = static_cast<std::uint64_t>(integer(row[0]));
            e.item.index = static_cast<int>(integer(row[1]));
            e.item.count = static_cast<int>(integer(row[2]));
            e.unit_price = integer(row[3]);
            e.delivery = static_cast<TradingPostDelivery>(integer(row[4]));
            if (!e.id || e.item.index <= 0 || e.item.count <= 0 || e.unit_price <= 0 ||
                (e.delivery != TradingPostDelivery::Proceeds && !read_item(row, 5, e.item))) {
                return std::nullopt;
            }
            s.entries.push_back(e);
        }
        return s;
    }

    TradingPostCollectionResult collect(const Query& q,
                                        const CharacterRecord& owner,
                                        std::uint64_t id,
                                        const ItemRecord* merge_target) {
        TradingPostCollectionResult result;
        if (owner.id <= 0 || !id || id > static_cast<std::uint64_t>(INT64_MAX) || owner.money < 0 ||
            owner.money > protocol::kTradingPostMaxPrice) {
            return result;
        }
        const auto rows =
            q("SELECT COALESCE(i.item_index,d.item_index),COALESCE(i.count,d.count),"
              "d.unit_price,d.kind,COALESCE(d.item_iid,0)," +
                  item_columns("i.") + ",i.account_bound,i.character_bound" +
                  " FROM trading_post_deliveries d LEFT JOIN items i ON i.iid=d.item_iid "
                  "WHERE d.id=? AND d.character_id=?",
              {std::to_string(id), std::to_string(owner.id)});
        if (!rows) {
            return result;
        }
        if (rows->empty()) {
            result.status = "This delivery has already been collected or is not yours.";
            return result;
        }
        const auto& row = rows->front();
        const auto count = integer(row[1]), price = integer(row[2]), kind = integer(row[3]),
                   iid = integer(row[4]);
        if (count <= 0 || count > INT32_MAX || price <= 0 ||
            price > protocol::kTradingPostMaxPrice || kind < 0 || kind > 2) {
            return result;
        }
        int claimed = static_cast<int>(count);
        if (kind == static_cast<int>(TradingPostDelivery::Proceeds)) {
            claimed = static_cast<int>(
                std::min(count, (protocol::kTradingPostMaxPrice - owner.money) / price));
            if (!claimed) {
                result.status = "Your wallet has no room for these sale proceeds.";
                return result;
            }
            result.geons = price * claimed;
            if (!changed(q("UPDATE characters SET money=money+? WHERE id=? AND money=? AND "
                           "money<=? RETURNING id",
                           {std::to_string(result.geons),
                            std::to_string(owner.id),
                            std::to_string(owner.money),
                            std::to_string(protocol::kTradingPostMaxPrice - result.geons)}))) {
                return {};
            }
        } else {
            protocol::TradeItem item;
            if (iid <= 0 || iid > INT32_MAX || !read_item(row, 5, item)) {
                return result;
            }
            item.iid = static_cast<std::uint32_t>(iid);
            item.index = static_cast<int>(integer(row[0]));
            item.count = claimed;
            if (!changed(
                    q("UPDATE items SET character_id=?,account_id=(SELECT account_id FROM "
                      "characters WHERE id=?),location=0,slot=0 "
                      "WHERE iid=? AND location=2 AND character_id IS NULL AND account_id=(SELECT "
                      "account_id FROM characters WHERE id=?) AND count=? RETURNING iid",
                      {std::to_string(owner.id),
                       std::to_string(owner.id),
                       std::to_string(iid),
                       std::to_string(owner.id),
                       row[1]}))) {
                return {};
            }
            result.received_count = claimed;
            if (merge_target) {
                const auto& target = *merge_target;
                const auto expected_attributes = attributes(target);
                if (target.iid <= 0 || target.iid == iid || target.character_id != owner.id ||
                    target.slot != 0 || target.index != item.index || target.count <= 0 ||
                    target.count > INT32_MAX - claimed || row.size() != 29 ||
                    !std::equal(
                        expected_attributes.begin(), expected_attributes.end(), row.begin() + 5) ||
                    target.account_bound != (integer(row[27]) != 0) ||
                    target.character_bound != (integer(row[28]) != 0)) {
                    return {};
                }
                std::string predicate;
                std::size_t begin = 0;
                do {
                    const auto end = columns.find(',', begin);
                    predicate += " AND " + columns.substr(begin, end - begin) + "=?";
                    if (end == std::string::npos) {
                        break;
                    }
                    begin = end + 1;
                } while (true);
                std::vector<std::string> args{std::to_string(claimed),
                                              std::to_string(target.iid),
                                              std::to_string(owner.id),
                                              std::to_string(owner.id),
                                              std::to_string(item.index),
                                              std::to_string(target.count)};
                args.insert(args.end(), expected_attributes.begin(), expected_attributes.end());
                args.push_back(target.account_bound ? "1" : "0");
                args.push_back(target.character_bound ? "1" : "0");
                if (!changed(q("UPDATE items SET count=count+? WHERE iid=? AND character_id=? "
                               "AND account_id=(SELECT account_id FROM characters WHERE id=?) "
                               "AND location=0 AND slot=0 AND item_index=? AND count=?" +
                                   predicate +
                                   " AND account_bound=? AND character_bound=? RETURNING iid",
                               args))) {
                    return {};
                }
                // Remove the receipt reference before deleting its consumed source instance.
                if (!changed(q("DELETE FROM trading_post_deliveries WHERE id=? AND character_id=? "
                               "RETURNING id",
                               {std::to_string(id), std::to_string(owner.id)})) ||
                    !changed(q("DELETE FROM items WHERE iid=? AND character_id=? AND count=? "
                               "AND location=0 AND slot=0 RETURNING iid",
                               {std::to_string(iid),
                                std::to_string(owner.id),
                                std::to_string(claimed)}))) {
                    return {};
                }
                item.iid = static_cast<std::uint32_t>(target.iid);
                item.count += target.count;
                result.item = item;
                result.committed = true;
                result.status = "Items collected.";
                return result;
            }
            result.item = item;
        }
        const auto updated = claimed == count
                                 ? q("DELETE FROM trading_post_deliveries WHERE id=? AND "
                                     "character_id=? RETURNING id",
                                     {std::to_string(id), std::to_string(owner.id)})
                                 : q("UPDATE trading_post_deliveries SET count=count-? WHERE id=? "
                                     "AND character_id=? AND count=? RETURNING id",
                                     {std::to_string(claimed),
                                      std::to_string(id),
                                      std::to_string(owner.id),
                                      row[1]});
        if (!changed(updated)) {
            return {};
        }
        result.committed = true;
        result.status = claimed < count ? "Collected available Geons. Remaining proceeds stay in "
                                          "Collection until your wallet has room."
                        : result.item   ? "Items collected."
                                        : "Sale proceeds collected.";
        return result;
    }

    TradingPostResult sell(const Query& q,
                           const ItemRecord& i,
                           int count,
                           std::int64_t price,
                           const TradingPostCategory& category,
                           std::int32_t split_iid) {
        if (i.iid <= 0 || i.character_id <= 0 || i.index <= 0 || i.slot || i.account_bound ||
            i.character_bound || count <= 0 || count > i.count || price <= 0 ||
            price > protocol::kTradingPostMaxPrice) {
            return fail("This item cannot be offered.");
        }
        const auto row =
            q("SELECT " + columns +
                  " FROM items WHERE iid=? AND character_id=? AND item_index=? AND count=? "
                  "AND location=0 AND slot=0 AND account_bound=0 AND character_bound=0",
              {std::to_string(i.iid),
               std::to_string(i.character_id),
               std::to_string(i.index),
               std::to_string(i.count)});
        if (!row || row->size() != 1 || row->front() != attributes(i)) {
            return fail("The item changed. Refresh your inventory.");
        }
        const auto mounts = q("SELECT equipment0,equipment1,equipment2,equipment3,equipment4 FROM "
                              "battle_mounts WHERE mount_iid=?",
                              {std::to_string(i.iid)});
        if (!mounts) {
            return {};
        }
        if (!mounts->empty()) {
            for (const auto& gear : mounts->front()) {
                if (integer(gear) != 0) {
                    return fail("Remove the mount's equipment before listing it.");
                }
            }
        }
        int iid = i.iid;
        if (count < i.count) {
            iid = split_iid;
            if (!split(q, i.iid, iid, count, kTradingPostAccountId) ||
                !changed(q("UPDATE items SET count=count-? WHERE iid=? RETURNING iid",
                           {std::to_string(count), std::to_string(i.iid)}))) {
                return {};
            }
        } else if (!changed(q("UPDATE items SET character_id=NULL,account_id=?,location=2,slot=0 "
                              "WHERE iid=? RETURNING iid",
                              {std::to_string(kTradingPostAccountId), std::to_string(iid)}))) {
            return {};
        }
        if (!q("INSERT INTO trading_post_offers(seller,item_iid,unit_price,clazz,type,grade) "
               "VALUES(?,?,?,?,?,?)",
               {std::to_string(i.character_id),
                std::to_string(iid),
                std::to_string(price),
                std::to_string(category.clazz),
                std::to_string(static_cast<int>(category.type)),
                std::to_string(category.grade)})) {
            return {};
        }
        return {true, count, 0, "Offer listed. Items are held by the trading post."};
    }

    TradingPostResult buy(const Query& q,
                          const CharacterRecord& buyer,
                          std::uint64_t id,
                          int desired,
                          std::int64_t price,
                          std::int32_t split_iid) {
        if (buyer.id <= 0 || desired <= 0 || !id || id > static_cast<std::uint64_t>(INT64_MAX) ||
            price <= 0 || price > protocol::kTradingPostMaxPrice) {
            return fail("Invalid purchase.");
        }
        const auto rows =
            q("SELECT o.seller,i.item_index,i.count,o.unit_price,i.iid FROM trading_post_offers o "
              "JOIN items i ON i.iid=o.item_iid WHERE o.id=? AND i.count>0 AND i.location=2 AND "
              "i.account_id=-1",
              {std::to_string(id)});
        if (!rows) {
            return {};
        }
        if (rows->empty()) {
            return fail("This offer has already been sold or withdrawn.");
        }
        const auto& row = rows->front();
        const auto seller = integer(row[0]), remaining = integer(row[2]);
        if (seller == buyer.id) {
            return fail("You cannot buy your own offer.");
        }
        if (price != integer(row[3])) {
            return fail("The price changed. Review the refreshed offer.");
        }
        const int count = static_cast<int>(std::min<std::int64_t>(desired, remaining));
        const auto charge = price * count;
        if (count <= 0 || buyer.money < charge) {
            return fail("You do not have enough Geons for this purchase.");
        }
        if (!changed(q("UPDATE characters SET money=money-? WHERE id=? AND money=? AND money>=? "
                       "RETURNING id",
                       {std::to_string(charge),
                        std::to_string(buyer.id),
                        std::to_string(buyer.money),
                        std::to_string(charge)}))) {
            return fail("Your balance changed. Please try again.");
        }
        const auto accounts =
            q("SELECT account_id FROM characters WHERE id=?", {std::to_string(buyer.id)});
        if (!accounts || accounts->size() != 1) {
            return {};
        }
        int iid = static_cast<int>(integer(row[4]));
        if (count < remaining) {
            const int source = iid;
            iid = split_iid;
            if (!split(q, source, iid, count, static_cast<int>(integer(accounts->front()[0]))) ||
                !changed(q("UPDATE items SET count=count-? WHERE iid=? RETURNING iid",
                           {std::to_string(count), std::to_string(source)}))) {
                return {};
            }
        } else if (!changed(q("UPDATE items SET account_id=? WHERE iid=? RETURNING iid",
                              {accounts->front()[0], std::to_string(iid)})) ||
                   !changed(q("DELETE FROM trading_post_offers WHERE id=? RETURNING id",
                              {std::to_string(id)}))) {
            return {};
        }
        if (!receipt(q, buyer.id, iid, 0, 0, price, TradingPostDelivery::Purchase) ||
            !receipt(q,
                     static_cast<int>(seller),
                     0,
                     static_cast<int>(integer(row[1])),
                     count,
                     price,
                     TradingPostDelivery::Proceeds)) {
            return {};
        }
        return {true,
                count,
                charge,
                count < desired
                    ? "Only " + std::to_string(count) + " of " + std::to_string(desired) +
                          " items remained; that quantity was purchased. Collect at a trading post "
                          "NPC later."
                    : "Purchase complete. Items are ready for collection at a Trading Post Agent.",
                static_cast<int>(seller)};
    }

    TradingPostResult cancel(const Query& q, int owner, std::uint64_t id) {
        if (owner <= 0 || !id || id > static_cast<std::uint64_t>(INT64_MAX)) {
            return fail("Invalid offer.");
        }
        const auto rows = q("SELECT i.iid,o.unit_price,i.count FROM trading_post_offers o JOIN "
                            "items i ON i.iid=o.item_iid "
                            "WHERE o.id=? AND o.seller=? AND i.location=2 AND i.account_id=-1",
                            {std::to_string(id), std::to_string(owner)});
        if (!rows) {
            return {};
        }
        if (rows->empty()) {
            return fail("This offer has already been sold or withdrawn.");
        }
        const auto& row = rows->front();
        if (!changed(q("UPDATE items SET account_id=(SELECT account_id FROM characters WHERE id=?) "
                       "WHERE iid=? RETURNING iid",
                       {std::to_string(owner), row[0]})) ||
            !receipt(
                q, owner, integer(row[0]), 0, 0, integer(row[1]), TradingPostDelivery::Returned) ||
            !changed(q("DELETE FROM trading_post_offers WHERE id=? RETURNING id",
                       {std::to_string(id)}))) {
            return {};
        }
        return {
            true,
            static_cast<int>(integer(row[2])),
            0,
            "Offer withdrawn. Remaining items are ready for collection at a Trading Post Agent."};
    }
}  // namespace openkal::server::trading_post
