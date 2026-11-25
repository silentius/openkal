# Honor

Press **V** for total Honor Points, current-week Honor Points, rank, and Reward
Points. In single player, `/honor +=12090` and `/honor -=100` adjust the
selected character's saved Honor. Local ranks update immediately from the Honor
thresholds. Reward Points are independent.

Open **Settings → Misc**, enable **Show own name**, and save. In single player
and multiplayer, the overhead label shows your character's name with their
Honor rank once earned.

Multiplayer ranks are published on Monday at 00:00 UTC, based on total Honor
earned before that cutoff. Characters are ordered by Honor descending and
character ID ascending. Each receives their highest eligible rank with a free
place. Awards recorded after a rank snapshot take effect in the next ranking.
Top-ten weekly bonuses are follow-up work.

## Honor Merchant and coupons

The Honor Merchant is near the starting area on map 0. The purchase screen
shows Reward Points, the current rank and the selected offer's required rank.
MainSvr checks the currently assigned rank and debits the Reward Points in the
same transaction that gives the items. Honor Points and the weekly score are
independent of purchases.

Use an Honor Coupon (item 23032) from the inventory. The starting denomination
is 1,000 Reward Points per coupon. Redemption consumes one coupon and credits
the Reward Points together. Coupon amounts come from a usable item definition's
`specialty.honor_reward_points`. Coupons can be distributed as items by events.
In single player, `/getitemid 23032 1` gives a coupon for local testing.

The starter catalog in `config/server/shops/60004_honor_merchant.json` sets a
price and a `minRank` for each offer. Rank IDs are 0 for unrestricted and
1–10 for Recruit through General. MainSvr also supports the merchant and coupon
transactions in single player, where the rank is derived directly from saved
Honor Points. The catalog and coupon value are OpenKal balancing choices.

## Event integration

In dedicated multiplayer, submit an `HonorGrant` batch to
`IDataStore::grant_honor`. Set a stable source, event instance, reason, character
ID, and event timestamp for each award. Event code determines eligibility and
sets the two point amounts independently. Replaying the same key and values
succeeds without increasing the balance. A conflicting replay rejects the batch.
The connected players' Honor snapshots refresh after the commit.

Castle War books its Honor awards in the same transaction as the completed
round. Its existing balances are imported once as total Honor, with zero
weekly Honor and zero Reward Points. New Castle War awards currently add Honor
Points. Their Reward Point amounts are a separate balancing decision.

## Historical E-Mok rule

An optional historical E-Mok ruleset can add a time bonus based on the
published rank. Its base allowance by level is 30 minutes for 1–59, 45 minutes
for 60–69, and 60 minutes from 70. The additional time is 30 minutes up to
Specialist, 40 through Lieutenant, 50 through Major, and 60 through General.
This is a future E-Mok area rule.

Behavioral references from INIXSOFT, consulted November 21, 2026, include the
[Honor rank thresholds and caps](https://www.kalonline.co.kr/Honor), the
[English Honor guide](https://gameagit.com/kalonline/Honor), and the
[2023 E-Mok update](https://gameagit.com/kalonline/updateView/1953/1).
The historical time brackets come from a screenshot supplied for reference.
