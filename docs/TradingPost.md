# Trading Post

The menu bar's brass balance emblem opens the anonymous fixed-price
market. The interface uses the universal `openkal.ui/v1` documents
`trading_post`, `trading_post_quantity`, and `trading_post_option` and FreeType
text. The balance artwork is an generated OpenKal bronze relief; no
third-party game icon or asset was imported. Its source and generation prompt
are kept under `RuntimeData/templates/dragon_menu/source/trading_post.*`.
Regenerate its two runtime BMPs with
`python3 tools/export_trading_post_icon.py`.

## Browse and purchase

Buy filters by class, item type, prefix, and grade. Class indices retain the
catalog's Knight/Magician/Archer/Thief mapping while the selector presents
Any/Knight/Archer/Magician/Thief. Prefix selection is enabled for weapons and
the five armor categories. Grade choices come from the matching item catalog.
Quest/task and bound items remain subject to the existing trade restrictions.
A category may therefore have no tradeable offers. The catalog's `upperarmor`
and `lowerarmor` equipment types map to Chest and Shorts, including their grade
choices; the existing `upper` and `pants` names remain supported.

Price ordering uses Geons **per item**, allowing different stack sizes to be
compared. Enhancement ordering uses the instance's Explosive Blow upgrade
level. The four orders use listing id as a stable final tie-breaker. Each page
contains at most 60 offers, arranged in ten columns. Every row's heading shows
the actual minimum and maximum unit prices of its up to ten offers, including
when sorting by enhancement. Repeated prices can span multiple rows. Arrow
buttons and the mouse wheel navigate server-filtered pages. The counter between
the arrows shows the current page and total matching pages, for example `1/2`.
Empty results show `1/1`. If offers disappear, refreshing a page beyond the end
returns the last available page. Sell uses the filtered sale inventory count.
Each endpoint, unit price, stack total, editable sale price and pending proceeds
amount follows the shared Geon color thresholds documented in
`docs/ui/TEMPLATE.md`.

Hover uses the inventory tooltip, including instance prefix, endurance, charm
bonuses, Explosive Blow, mixing, Bead of Fire, perforation, Qigong, Dragon
Fusing, random stats, protection and animal progression. Market tooltips add
unit price and complete stack price. Responses contain no seller name,
character id, or source inventory instance id.

Select an offer and choose Buy selected. The quantity dialog reuses the shop's
minus/plus, numeric entry, 10/100/1000, Max, Cancel and Confirm controls. A
purchase requests 1 through the displayed available count. If competing buyers
have reduced that offer, the transaction buys the available remainder and
reports the actual quantity. Insufficient funds reject the transaction; they do
not silently reduce the requested quantity further. Sold or withdrawn offers
are removed by the authoritative refresh and produce an explanatory message.
A player cannot buy their own offer.

## Sell and My Offers

Sell shows eligible unequipped, unbound General inventory instances. Select an
item, enter a positive Geon unit price in the input beside the selection, and
choose the quantity to list. There is no default selling price: a new item
selection clears and focuses the field. The quantity confirmation repeats the
unit price and the total for the chosen quantity. The
selected quantity leaves inventory only after its escrow transaction commits.
The remaining stack stays in inventory. My Offers shows the character's active
listings and allows withdrawing their remaining quantity into Collection.
Offers currently have no fees or expiry. Unit prices range from 1 through
2,000,000,000 Geons; this first step preserves the current gameplay wallet
limit instead of defining the separate high-value-currency feature request.

Every persisted inventory and storage item retains its owning `account_id`.
The character reference identifies inventory placement; `location`
distinguishes account storage from trading post custody. Saving inventory
derives the account from the persisted character, including after direct
player trades.

The reserved system account `-1` (`!openkal-trading-post`) owns listed items
and is excluded from account login lookup. Items always remain in `items`;
offers and collection receipts reference them through `item_iid`. Item
`location` distinguishes inventory (0), account storage (1), and trading post
custody (2), independently of account ownership. Whole-item listing, purchase,
withdrawal and collection preserve the IID. Only splitting a stack creates a
new item IID for the separated quantity. On purchase, the referenced item
records the buyer's account immediately; unsold quantities remain assigned
to the system account.
Withdrawal assigns the referenced item to the seller's account. Collection
writes the recipient's account into the restored inventory item. Sale proceeds
belong to the seller's account and do not represent another item instance.
Receipts remain character-specific even when characters share an account.
SQLite schema 28 and PostgreSQL schema 4 backfill existing inventory ownership
and convert existing market snapshots into referenced item rows once,
preserving storage and Battle Riding state. Legacy snapshots lack an IID;
only those pre-migration snapshots require newly allocated IDs.

All instance values and absolute pet/riding expirations remain in their
existing item row. Battle Riding grade, satiety, revision and ability state
remain attached to the same IID in `battle_mounts`.
Mount equipment must be removed before listing, so independent equipment IIDs
cannot become dangling attachments to a sold mount.

## NPC collection

Collection lists purchased quantities, sale proceeds, and withdrawn quantities
as separate durable receipts. Proceeds are the receipt's count times unit
price. Receipts belong to the individual character, including when buyer and
seller share an account. Proceeds use a coin icon and a currency tooltip with
the sold item's name and quantity; purchased and withdrawn items retain their
item icons and inventory tooltips. Items do not enter the buyer's inventory and
proceeds do not enter the seller's wallet from the ordinary menu. Visit the
Trading Post Agent on map 0 at server position `(232525, 291711, 45410)`, facing
176.6 degrees. The agent opens Collection. Its main action, Collect All, needs
no selected receipt. Collect selected on the left supports individual pickup.
Collect All processes all pages, including purchases, returns and proceeds. Receipts that cannot fit
in the unlocked inventory or wallet remain available; other receipts still
collect, and a result message reports the outcome. Each receipt commits
independently and repeated commands cannot duplicate it. The same view opened
from the menu remains read-only for pickup.
The server verifies the runtime NPC, collector role, map and talk distance on
each pickup. A purchase or return needs a free unlocked inventory slot. Removing the receipt and returning the existing item row to inventory commit
together, with the existing IID. Replays cannot create another item.
Proceeds enter the wallet atomically. If only part fits the wallet limit,
whole sold-unit proceeds are paid and the remaining quantity stays in
Collection; a full wallet leaves the receipt unchanged. No pickup extends a
pet or riding expiration.

Committed pickups also use the trade's battle-chat receipt presentation: green
`You got ...` lines identify each collected item stack and its actual quantity;
white `You got ... geons` reports the actual proceeds added to the wallet.
Collect All totals its proceeds into one currency line. Blocked receipts,
failed pickups and replayed commands never produce incoming-reward messages.
Purchasing or listing alone produces no pickup receipt.

The configuration is `config/npcs/60003_trading_post_agent.json` and
its dialog is `config/dialogs/990320_trading_post_agent.json`. Its optional
`overhead_icon` references the existing balance bitmap. See `docs/Npcs.md` for
the reusable overhead-symbol field and the coordinate/facing convention.

The menu balance displays an outlined `!` whenever the character has any
collection receipts. The server sends the flag on world entry and immediately
after committed purchases or withdrawals; an online seller is also notified of
new proceeds. Pickup refreshes the flag and clears it after the final receipt.
The notification is separate from offer pages, so it works while
the market is closed and does not replace an open search. Opening Collection
does not dismiss it. Reconnecting reloads the flag from durable receipts.

## Transactions and transport

SQLite and PostgreSQL use the same prepared-query transaction rules. Mutations
acquire the market lock within their database transaction; SQLite also uses
`BEGIN IMMEDIATE`. Offer availability, the buyer's expected wallet balance,
the debit, the remaining quantity, and both collection receipts commit or roll
back together. The server changes live inventory/balance only after commit.
Listing revalidates every persisted instance attribute. Cancellation checks
ownership inside the transaction. Expired character cleanup cascades that
character's listings/receipts without removing another character's receipts.

Commands use a monotonically increasing operation id per connection. The server
returns its cached response to a duplicate operation and ignores older ones.
The client permits one pending command, refreshes after a response, and uses a
read-only refresh after a timeout. New OpenKal opcodes are appended, preserving
existing opcode ordinals. Each binary response has a maximum of 60 full item
snapshots and fits the existing 8,000-byte frame limit.

The initial implementation serializes market mutations through one database
lock row. Search and receipts are normalized tables, with indexed category and
owner lookups and bounded SQL pages. This prioritizes a simple transaction
boundary; higher write throughput can later use per-offer locks while retaining
the same persistence contract.

## Verification

`trading_post` checks protocol bounds, complete item snapshots, escrow splits,
partial fills, authorization, sold-out offers, rollback after receipt failure,
restart persistence and concurrent purchases over separate SQLite connections.
The shared persistence contract also runs in the optional PostgreSQL multiplayer
test when `OPENKAL_TEST_POSTGRES_DSN` is configured. `trading_post_ui` checks
ten-column row ranges, quantity controls, pending requests and inventory sale
selection; its `--capture PREFIX` option renders the window, quantity dialog,
dropdown and small-viewport layout with the OpenGL backend.
`trading_post_server` drives a real local server through browse, buy, duplicate
commands, inventory listing, withdrawal, reduced fills and restart inspection.
It checks the collection indicator for an empty login, a committed purchase,
withdrawal and the returning seller after restart. `menu_bar_hud --capture`
verifies the indicator's hidden/visible rendering and the shared message-counter
outline. The optional PostgreSQL multiplayer integration checks delivery to
both connected participants and restoration after reconnect.
