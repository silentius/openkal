/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/ArrowHitFeedback.h"

#include "openkal/protocol/Attack.h"
#include "test_util.h"

int main() {
    using openkal::client::ArrowHitFeedback;
    using openkal::protocol::kAtkHit;
    using openkal::test::expect;
    ArrowHitFeedback::Impact impact;
    {
        ArrowHitFeedback feedback;
        feedback.confirm(1, 900, kAtkHit);
        expect(!feedback.take(impact), "packet alone cannot start feedback");
        const auto shot = feedback.launch(1, 900, {10, 20, 30}, 1.25f);
        feedback.update(0.05f);
        expect(!feedback.take(impact), "animation release alone cannot start feedback");
        feedback.arrive(shot, {50, 25, 30});
        expect(feedback.take(impact) && impact.attacker == 1 && impact.target == 900 &&
                   impact.flags == kAtkHit && impact.origin.x == 10 && impact.position.x == 50 &&
                   impact.position.y == 25 && impact.scale == 1.25f,
               "actual projectile arrival supplies the hit position and shooter");
        expect(!feedback.take(impact), "one arrow produces feedback exactly once");
    }
    {
        ArrowHitFeedback feedback;
        const auto shot = feedback.launch(2, 901, {}, 1);
        feedback.arrive(shot, {100, 0, 0});
        expect(!feedback.take(impact), "unconfirmed arrival cannot invent a hit");
        feedback.confirm(2, 901, kAtkHit);
        expect(feedback.take(impact) && impact.position.x == 100,
               "late confirmation uses the completed arrow without waiting for another shot");
    }
    {
        ArrowHitFeedback feedback;
        const auto miss = feedback.launch(1, 900, {}, 1);
        const auto hit = feedback.launch(1, 900, {}, 1);
        feedback.confirm(1, 900, 0);
        feedback.confirm(1, 900, kAtkHit);
        feedback.arrive(miss, {1, 0, 0});
        expect(feedback.take(impact) && impact.flags == 0,
               "miss consumes its own arrow rather than stealing a later hit");
        expect(!feedback.take(impact), "later confirmed hit still waits for its own arrival");
        feedback.arrive(hit, {2, 0, 0});
        expect(feedback.take(impact) && impact.flags == kAtkHit && impact.position.x == 2,
               "hit following a miss remains synchronized");
    }
    {
        ArrowHitFeedback feedback;
        const auto first = feedback.launch(1, 900, {}, 1);
        const auto second = feedback.launch(2, 900, {}, 1);
        const auto other_target = feedback.launch(1, 901, {}, 1);
        feedback.confirm(2, 900, kAtkHit);
        feedback.confirm(1, 901, kAtkHit);
        feedback.arrive(first, {});
        expect(!feedback.take(impact), "another shooter cannot release a confirmed hit");
        feedback.arrive(other_target, {});
        expect(feedback.take(impact) && impact.target == 901,
               "target switch cannot release an unrelated arrow");
        feedback.arrive(second, {});
        expect(feedback.take(impact) && impact.attacker == 2,
               "two archers hitting one monster remain independent");
    }
    {
        ArrowHitFeedback feedback;
        feedback.confirm(1, 900, 0);
        feedback.confirm(1, 900, kAtkHit);
        const auto miss = feedback.launch(1, 900, {}, 1);
        const auto hit = feedback.launch(1, 900, {}, 1);
        feedback.arrive(hit, {});
        expect(feedback.take(impact) && impact.flags == kAtkHit,
               "queued outcomes pair in launch order even when arrival order differs");
        feedback.arrive(miss, {});
        expect(feedback.take(impact) && impact.flags == 0,
               "early miss confirmation remains bound to the first launch");
    }
    {
        ArrowHitFeedback feedback;
        const auto shot = feedback.launch(1, 900, {}, 1);
        feedback.confirm(1, 900, kAtkHit);
        feedback.update(6);
        feedback.arrive(shot, {});
        expect(!feedback.take(impact), "expired or canceled arrows cannot produce delayed flashes");
        feedback.confirm(2, 901, kAtkHit);
        feedback.update(6);
        const auto later = feedback.launch(2, 901, {}, 1);
        feedback.arrive(later, {});
        expect(!feedback.take(impact), "stale confirmation cannot mark a new arrow as a hit");
    }
    return openkal::test::g_failures ? 1 : 0;
}
