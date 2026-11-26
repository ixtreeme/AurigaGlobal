# Typed item affects and clocks

The item-backed affect families for automatic recovery, ampoules, souls,
old growth pets and recall intent no longer enter character affect storage.
Their numeric IDs remain reserved. Loading old database rows skips them;
Attach/Add reject them before applying points, flags, locks or timers.
Ordinary skill, premium and character affects keep their entity-owned timer.
The old unpersisted HP/SP potion reserve counters no longer heal or keep that
timer alive. Their point IDs stay reserved and reject further point changes;
continuous character regeneration remains independent.

The raw unique-item, worn-time, real-time, accessory-degradation and soul
charging callbacks, their start/stop APIs and five ItemEvents slots are
removed. Raw equipment publication no longer starts these clocks or writes
first-use deadlines into sockets. The leftover ground-object ownership and
despawn events are independent of item use lifetimes and remain available.

The old rune socket/affect runtime and its benchmark injection are retired.
Typed regalia persists rune state. Typed accessory, recovery, companion,
costume and soul services persist their own clocks and transaction receipts.
The old soul damage adapter and marriage-item affect/vnum bonus queries are
removed; typed strike hooks and item-effect bonus projection remain the
authoritative paths. Recovery facts already re-evaluate party leadership
and marriage eligibility, including after reconnect.

Unity filters the same retired affect IDs from status state and icon
presentation. Item effect clocks continue through the typed recovery,
inventory and soul packets; GC126/127 still represent character affects.
No packet layouts or live catalog/quest definitions change.

The obsolete raw timer/rune test cases are removed with their APIs. Native
typed-clock, recovery, effect, costume, companion, soul, relationship and
affect lifecycle suites cover their replacements. The relationship fixture
now explicitly links the catalog and declares its inactive private-shop
boundary. The recovery UI validator uses current Nexoria labels and the
HUD's active-recovery entry point.

No player inventory rows or legacy affect rows are deleted by this change.

## Verification

All 22 selected native suites pass, including the old reserve-counter
regression and retired affect admission. The pinned Unity Editor compiles
the client and passes status state/icon checks and 3,212 recovery integration
checks. The isolated TLS gameplay run verifies old database affects being
ignored, an ordinary character affect loading, automatic recovery supply
consumption and restoration, unchanged wall-clock deadlines, persisted
online time and costume expiry surviving a second reconnect. The local
gameplay fixture owns its database/account; no live character was used.
