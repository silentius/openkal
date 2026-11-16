/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/PlayerRenderSelection.h"

#include "test_util.h"

#include <algorithm>
int main() {
    using openkal::client::select_player_ids;
    using openkal::test::expect;
    std::vector<openkal::client::PlayerRenderCandidate> players;
    for (std::uint32_t id = 1; id <= 51; ++id) {
        players.push_back({id, static_cast<float>(id * id), id == 51});
    }
    const auto nearest = select_player_ids(players, 10);
    expect(nearest.size() == 10 && nearest.count(51) && nearest.count(9) && !nearest.count(10),
           "selected distant target takes one slot before the nine nearest players");
    expect(select_player_ids(players, 0).size() == 51, "Max includes every remote player");
    expect(select_player_ids(players, -1).empty(), "Off hides all remote labels");
    expect(select_player_ids({}, 10).empty(), "empty scenes need no special budget handling");
    for (auto& candidate : players) {
        candidate.distance_squared = 0;
        candidate.selected = false;
    }
    const auto first = select_player_ids(players, 25);
    std::reverse(players.begin(), players.end());
    expect(first == select_player_ids(players, 25) && first.count(25) && !first.count(26),
           "equal-distance crowds retain the same ids regardless of enumeration order");
    expect(select_player_ids(players, 100).size() == 51, "larger budgets retain every candidate");
    for (auto& player : players) {
        player.distance_squared = static_cast<float>(player.id * player.id);
        player.party_member = player.id >= 47;
        player.selected = player.id == 51;
    }
    const auto with_party = select_player_ids(players, 10);
    expect(with_party.size() == 15 && with_party.count(10) && !with_party.count(11),
           "five distant party members are added to the full ten-player non-party budget");
    for (std::uint32_t id = 47; id <= 51; ++id) {
        expect(with_party.count(id) != 0, "every own-party member bypasses the body limit");
    }
    const auto party_names_only = select_player_ids(players, -1);
    expect(party_names_only.size() == 5 && party_names_only.count(47) && party_names_only.count(51),
           "Off still displays every own-party nameplate without adding non-party names");
    const auto one_name = select_player_ids(players, 1);
    expect(one_name.size() == 6 && one_name.count(1),
           "party nameplates do not consume even a one-player name budget");
    expect(select_player_ids(players, 0).size() == 51,
           "Max includes party and non-party members without duplicates");
    for (auto& player : players) {
        player.party_member = false;
    }
    expect(select_player_ids(players, 10).size() == 10 && select_player_ids(players, -1).empty(),
           "leaving the party restores ordinary body and name limits immediately");
    return openkal::test::g_failures ? 1 : 0;
}
