# Feature requests

Product requests and unresolved scope. These entries describe desired behavior;
current interfaces are documented in their feature references.

## 1. Marketplace inspection

Implemented: class/type/prefix/grade filtering and inventory tooltips with instance
stats, options and upgrades. See [Trading Post](TradingPost.md). Further category
or pristine/enhanced/enchanted presentation changes need a concrete remaining UX
requirement; the existing inspection must not be reimplemented from this wishlist.

## 2. Support trading high-value items

- Investigate how the player currency holding limit restricts high-value sales.
- Provide a way to complete those transactions at their intended prices.
- The desired currency limit and transaction mechanism remain unspecified.

## 3. Further inventory expansion

The current General inventory supports 216 slots, six category filters and
mouse-wheel/scrollbar navigation; quest/task items have separate sections.
See [inventory bindings](ui/VARIABLES.md). Any additional request must define a
larger target capacity or a different tab arrangement before changing persistence
or protocol. Existing filtering and scrolling are implemented.

## 4. Multiple enhancement stones

The existing enhancement sequence is constrained by game mechanics. Any batch
operation must preserve resource consumption, success chances and destruction
behavior. A safe 100% process for +15/+16 is not specified; this remains an open
mechanics question rather than an implementation task.

## 5. In-game reference database (Powerbook)

Implemented: monster levels, available drops and spawn-map display, with explicit
handling of missing optional catalogs/artwork. See the
[Powerbook interface](ui/VARIABLES.md#powerbook). The project does not acquire data
packs or change drop lists as part of this feature. Additional region-first
navigation requires a separate concrete UX proposal.

## Excluded scope

Enhancement probabilities, aid availability/pricing, rare stones and
luck-dependent consumable supply concern externally maintained drop lists,
balancing or monetization and are outside this feature list's scope. The Powerbook
request concerns displaying available information, not changing drop lists.
