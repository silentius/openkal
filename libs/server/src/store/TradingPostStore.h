/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#pragma once
#include "openkal/server/store/DataStore.h"

#include <functional>

namespace openkal::server::trading_post {
    using Rows = std::vector<std::vector<std::string>>;
    /** Prepared SQL boundary shared by SQLite and PostgreSQL implementations. */
    using Query =
        std::function<std::optional<Rows>(const std::string&, const std::vector<std::string>&)>;
    /** @param query Prepared-query executor. @param character Private-view owner.
     * @param request Search and paging. @return Page, or no value on failure. */
    std::optional<protocol::TradingPostState>
    list(const Query& query, int character, const protocol::TradingPostRequest& request);

    /** Restores legacy escrow snapshots to item rows during schema migration.
     * @param query Prepared SQL boundary inside the migration transaction.
     * @return Whether every legacy item and reference was restored. */
    [[nodiscard]] bool migrate_escrow(const Query& query);

    /** Claims one receipt inside the caller's market transaction.
     * @param query Prepared SQL boundary. @param character Expected owner and balance.
     * @param receipt Receipt ID; referenced item IIDs are preserved unless merged.
     * @param merge_target Expected compatible inventory stack, or null for a new stack.
     * @return Pickup outcome to commit only on success. */
    TradingPostCollectionResult collect(const Query& query,
                                        const CharacterRecord& character,
                                        std::uint64_t receipt,
                                        const ItemRecord* merge_target);

    /** @param query Prepared-query executor in a transaction. @param item Expected inventory row.
     * @param quantity Offered quantity. @param price Unit price. @param category Trusted metadata.
     * @param split_iid Unused IID for stack splitting.
     * @return Mutation outcome, committed by the caller only on success. */
    TradingPostResult sell(const Query& query,
                           const ItemRecord& item,
                           int quantity,
                           std::int64_t price,
                           const TradingPostCategory& category,
                           std::int32_t split_iid);

    /** @param query Prepared-query executor in a transaction. @param buyer Current buyer.
     * @param listing Offer id. @param quantity Requested count. @param price Expected unit price.
     * @param split_iid Unused IID for stack splitting.
     * @return Committed fill and charge. */
    TradingPostResult buy(const Query& query,
                          const CharacterRecord& buyer,
                          std::uint64_t listing,
                          int quantity,
                          std::int64_t price,
                          std::int32_t split_iid);

    /** @param query Prepared-query executor in a transaction. @param character Owner.
     * @param listing Offer id. @return Cancellation outcome. */
    TradingPostResult cancel(const Query& query, int character, std::uint64_t listing);
}  // namespace openkal::server::trading_post
