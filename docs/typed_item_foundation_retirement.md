# Typed item foundation retirement

The GameServer no longer compiles the raw ItemSystem, InventorySystem,
ItemAttributeSystem, ITEM_MANAGER, item registry, raw item components or the
LegacyBridge. This removes the second item lifetime/storage model, its disabled
factory, 32-bit identity index, socket and attribute mutations, in-memory stack
transactions, raw equip/trade/shop/warehouse paths, and the old switchbot job.

The mandatory ItemStore and TypedItems snapshot remain authoritative. Existing
typed operations use durable commits, owner/session fences and confirmed
publication. No catalog, quest, inventory, wallet or migration rows are rewritten
by this code release.

## Retained responsibilities

PlayerItemActions contains only character admission, the completed-login barrier,
and skill/command quickslots. Old inventory-cell bindings are rejected on input
and discarded on load. Item hotbar slots remain native item-instance bindings.
The separate native move, drop, pickup, forge, storage, shop and trade protocols
keep their packet layouts.

Combat and skills read typed equipment/weapon projections; NPC prototype damage
and cloned weapon facts remain available. Arrow eligibility uses the typed
ammunition runtime. Raw item entities no longer participate in spatial insertion,
destruction, visibility or packet publication. Typed ground loot retains its own
spatial kind, add packet, ownership label and remove packet.

Native accessory checks cover leadership books and emotion masks using approved
category/local keys, including the existing catalog variants. The existing XP
accessory bonuses stay in their native path. Removed unique-group/vnum fallbacks
cannot award an extra parallel bonus.

TypedDropRate owns the existing encounter, level, premium and event multiplier
calculation. The shared summon/map eligibility rules and campfire character expiry
were moved out of the deleted item bridge without changing their rules. The
ordinary character, premium, horse and world-event lifecycles remain independent.

Item issuance now has a separate TypedItemGrants translation unit. The actual
completion preserves its committed result after logout and fences a later login.
It no longer links unrelated utility/quest-use code into the grant fixture.

The old benchmark loadout injector used the disabled raw factory. It is removed;
benchmark telemetry records current typed equipment and its 64-bit weapon instance,
without replacing the player's items.

## Compatibility boundaries

Old database bootstrap prototype/shop/ID-range frame fields are still consumed
to preserve the Database wire layout. They do not instantiate a catalog, item
manager, raw storage or item entity in GameServer. Numeric packet IDs remain
reserved. The obsolete over-nine Lua refine registration, raw attribute-dialog
commands and unused raw map-teleporter handler are retired; native forge, costume
transfer and travel entry points remain in their respective services.

The old raw bonus-catalog publisher is removed; native equipment/details packets
continue to provide item and combat data. No Unity packet layout changes are
required by this server-only foundation removal. The subsequent fourth work
unit retires Unity legacy receivers and retains typed hover tooltips; see
[the Unity retirement record](typed_unity_item_receivers_retirement.md).

The removed raw-engine tests retire with those engines. Mixed character,
combat, points and spatial suites now use native projection/world fixtures;
QuickslotTests exercises the remaining real player-action implementation.
ItemFoundationBoundaryTests.py prevents the deleted engines or raw item
identity/storage symbols from being reintroduced into GameServer.

The 637 deferred entries and unrelated Mission Studio/mission-engine edits are
outside this work unit. Historical migration sources remain offline.


## Verification (2026-11-26)

The isolated checkout contains the committed base plus this work unit only;
unrelated Mission Studio/mission-engine edits are not part of the built image.

- Full MSVC Release GameServer build succeeded.
- All 45 selected CTest registrations passed, including the foundation boundary,
  grant completion, spatial/visibility, combat, equipment, trade, shops, storage,
  attributes, effects/clocks, party, buildings, dungeons and quest runtime.
- The obsolete raw daily-reward fixture now runs the actual native command and
  grant planner against controlled store/SQL boundaries. It checks atomic
  status/award publication, typed owner/key/count, conflicts, lost COMMIT
  acknowledgements without replay, unavailable offers/store and logout fencing.
- The spatial boundary test exposed an out-of-range double-to-integer movement
  conversion. The duration factor now clamps before conversion; the original
  0..350 speed limits and extreme-duration saturation checks pass.
- Unity 6000.3.27f1 compiled the isolated client snapshot and passed five suites:
  equipment 708, ammunition 1,340, trade 4,558, shops 17,983 and storage 4,667
  checks (29,256 total). No client packet layout change is needed for this unit.
- A fresh isolated local server and the actual Unity client codecs completed the
  TLS game flow: 64-bit instance move, partial-stack ground drop and publication,
  pickup, exact counts after reconnect, two-account trade with both approvals
  and persisted owners, automatic-bottle supply, wall/online/worn clocks and
  expiry after reconnect. The final GameServer image hash matches that fixture.

Unity checks and game flow ran in batch mode without graphics. Rendered visual
inspection was not performed. This is targeted verification, not the entire
repository test suite. The subsequent Unity receiver/tooltip work has its own
verification record linked above.
