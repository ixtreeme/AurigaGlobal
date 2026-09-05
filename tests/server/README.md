# Item attribute regression tests

The `ItemAttributeTests` target compiles the production `ItemAttributeSystem.cpp`
and the complete `new_switchbot.cpp` and `DragonSoul.cpp` with an EnTT registry and headless inventory,
payment, persistence, network, and RNG doubles. Items
in the fixtures have no `CItem`/`LegacyItemPtr`, and no database is started.
Switchbot implementation and declarations stay in `new_switchbot.cpp/.h`;
the tests compile that complete production translation unit. Timer/UI/manager
service doubles fail immediately if called, so these transaction tests cannot
silently exercise unimplemented server services.

From the repository root:

```powershell
cmake --build build --config Release --target GameServer ItemAttributeTests
ctest --test-dir build -C Release -R '^item_attributes$' --output-on-failure
```

For an existing `build-asan` configured with `ENABLE_ASAN=ON`:

```powershell
cmake -S . -B build-asan
cmake --build build-asan --config RelWithDebInfo --target ItemAttributeTests
ctest --test-dir build-asan -C RelWithDebInfo -R '^item_attributes$' --output-on-failure
```

Checks cover invalid/stale entities, invalid attribute sets, empty and
zero-weight tables, weighted probability boundaries (including partially
assigned level distributions), locked normal bonuses, rare-slot gaps,
atomic failure on insufficient candidates, addon removal, duplicate bonuses,
costume values, and force-attribute bounds/client updates.

Paid operations also cover failed preparation, rejected payment, insufficient
stacks/Yang, foreign or detached materials, locked/exchanging/equipped items,
last-unit material destruction, stale handles, wrong switchbot owners/slots,
limited and Zodiac changers, stack selection, and atomic costume reset. The
payment doubles assert that original attributes remain unpublished until debit.

The costume change/add/remove entry point runs with entity-only characters and
items. Checks include strict dialog selection parsing, invalidation of a prior
selection after malformed input, negative/overflowed indices, empty rare slots,
rare-slot compaction, duplicate bonuses, signed socket values and narrowing
bounds, invalid owners/materials, and exactly one publish after payment.
These tests call the production selection handler and operation, not the chat
packet dispatcher or UI. In-game, verify both remover choices, addition of the
sixth/seventh bonuses, and that rejecting an equipped costume leaves its buffs
unchanged. The legacy CHARACTER selection field/accessors have been removed;
the state now belongs to the character's CostumeAttributeSelection component.

Stole enchant tests cover all four variants at grades 1-4, the existing cap
for higher positive grades, rejected zero/negative grades, six bonuses in one
commit, and preservation of the seventh slot. Attribute-lock tests enumerate
every add/change candidate, skip the current/empty slots without an RNG retry
loop, reject changes with no alternative, and allow the remover to repair a
malformed stored lock. All four operations cover owner/material validation,
equipped/exchanging/locked items, rejected payment and last-unit consumption.
The payment doubles also assert that the lock index has not changed before
debit. The native lock getter/setter are compiled from ItemAttributeSystem.cpp;
only their network and persistence services are doubled.

In-game, check stole enchant at each grade and all three lock consumables,
including rejection on equipped items, lock preservation during subsequent
bonus rerolls, and persistence after relog. The item-use dispatcher and its
chat/buff side effects are not executed by these headless tests.

Dragon-soul checks cover entity-only initialization, bonus refresh and paid
enchant, missing/failed table loads, reload ownership, invalid counts/weights,
NaN/infinity/narrowing bounds, weighted sampling without replacement (including
zero weights and both RNG endpoints), stale handles and rejected payments.
The original bonuses remain unchanged until preparation and payment succeed;
one complete update clears obsolete additional slots. Active stones are
rejected, while inactive equipped stones retain the legacy allowance. Tests
cover the absolute main-inventory cell used for equipped dragon souls.

The DS table loader/accessors are doubles, not the real text parser or deployed
balance tables. Refinement/extraction/activation services fail immediately if
entered; the tests call the real attribute refresh, not the complete strength
refinement flow. Before deployment, verify the live DS table, new stone creation,
enchant in the DS bag and inactive equipment, active-deck rejection, strength
refinement, and relog. Failed new-stone initialization now returns no item after
cleanup; the actual item-manager creation/destruction path needs in-game testing.

The doubles verify save/update calls, not the DB packet encoding or a live
client session. Test in-game inventory, relog, bonus changers, rune items and
mount attributes separately before deploying.

The transactions are synchronous game-thread operations, not cross-service DB
transactions or recovery from process termination. An item payment uses one
validated stack (the current switchbot cost is one item). The tests do not run
the switchbot timer/UI, shop listings, rank/Battle Pass side effects, the actual
inventory destruction path, or the legacy `ChangeKKAK` special-case path.

## Costume attribute transfer

The same `ItemAttributeTests` target also compiles the complete existing
`attr_transfer.cpp`. The window, NPC binding and all three selections are ECS
state; the command handler and operation run without `CHARACTER` or `CItem`.
Selections retain the original inventory cell as well as the versioned entity.
The tests cover strict command parsing (including the client's `del` alias),
negative/overflow/trailing-junk indices, invalid/stale owners and NPCs, distance
and map changes, conflicting windows, duplicate/replaced/moved/foreign items,
locked/exchanging/equipped items, invalid costume types and stacks, malformed
bonus types, rare-slot clearing, rejected batches, reentrant operations and
exactly one target-attribute publication after the complete batch commits.

The batch payment, retirement queue, attribute preparation/commit and full
transfer command/window implementation are real production code. Tests assert
that every count and the new attributes are already visible at the first
save/packet/destruction callback, without invoking sequential ConsumeItemEcs.
They cover 1-64 costs, rejected empty/oversized/duplicate/invalid batches,
last-unit retirement, failed cleanup and retry without a second debit,
recursive processing, queue growth from callbacks, stale/recycled handles,
post-commit target destruction and nested changes not overwritten by an older
publication snapshot. Pending zero stacks cannot be spent again.

Only synchronous, main-game-thread state changes are atomic. After that commit,
deletion failure means pending cleanup, not a failed payment or a refund. The
item-manager update/shutdown retries cleanup; failed entries remain retired and
are logged once. No DB transaction, journal or cross-process crash recovery is
implemented: separate save/delete messages can still persist partly if the
process or connection fails. This is not a durable all-or-nothing guarantee.

Position, quest lookup, chat, logging, inventory lookup, network/save and actual
item destruction are doubles. The native count setters' removal of the legacy
mirror, count clamping, retirement write guards, and the real item-manager
tick/shutdown/persistence integration are built with GameServer but are not
executed by these headless tests. Verify stack caps, splitting/merging, zero and
last-unit consumption, quickslots and DB reconnect/logout on a test server.

Before deployment verify NPC interaction, each supported costume subtype,
selection/preview/clear/close, competing trade windows, disconnect/death,
successful consumption and relog persistence with the actual client and DB.

## Mount and pet lifecycle regression tests

`MountLifecycleTests` compiles the complete, existing `MountSystem.cpp` and
`PetSystem.cpp`, plus `New_PetSystem.cpp`; there is no parallel production
implementation.
It exercises owner, follower and summon-item handles with entity-only fixtures,
without creating a `CHARACTER` or `CItem`. Checks cover summon/unsummon, repeated
destruction, stale owners/items/followers with recycled entity indices, ownership
changes, reused item VIDs, failed spawn/show, follow and map transitions, expired
items, malformed/missing prototypes, war restrictions, skin changes, mounting,
unmounting, cancelled timers and replacement subsystems/actors.

Pet checks additionally cover item locks/socket state, owner-death survival,
follower death, update cadence, walk/run thresholds, follow options, multiple
pets, complete UnsummonAll, reentrant ComputePoints -> RefreshBuff during actor
deletion, nested Destroy, and independent updates after another actor's AI fails.
They verify stable pet identity and dungeon-bonus restrictions under skins,
failed skin respawns, duplicate item binding rejection, stale-owner callbacks,
prototype-bonus removal after item destruction/dungeon exit, skill-bonus
removal encoding, malformed apply types and unnegatable bonus values.

```powershell
cmake --build build --config Release --target GameServer ItemAttributeTests MountLifecycleTests
ctest --test-dir build -C Release -R '^(item_attributes|mount_lifecycle)$' --output-on-failure
cmake -S . -B build-asan
cmake --build build-asan --config RelWithDebInfo --target ItemAttributeTests MountLifecycleTests
ctest --test-dir build-asan -C RelWithDebInfo -R '^(item_attributes|mount_lifecycle)$' --output-on-failure
```

Factory, spatial movement, horse, affect, timer, item-point and packet services
are doubles. The point-calculation double resets the fixture bonus total and
calls the real PetSystem::RefreshBuff, including during deletion; it is not
the complete CHARACTER::ComputePoints/ItemSystem::ModifyPoints implementation.
The tests do not execute the real `SpawnMobEntity` allocation, pending character
destruction, engine movement/network code, login/logout or persistence. They do
not execute the new native walking packet service or command/packet dispatch.

Before deployment verify NPC/monster/metin spawning (including event-spawned
metins), mount follow across sectors/maps, name/skin display, riding/unmounting,
item expiry and logout/relog, and pet summon/skin/bonus behaviour in-game.
`SpawnMobEntity` now owns the shared spawn implementation; the old pointer-return
entry point remains only for unmigrated callers. Allocation, movement and horse
services still contain legacy internals: this is not a fully legacy-free server.
Growth-pet checks use a fake SQL client and database (no connection or worker
threads), and exercise the actual SELECT parsing, save queries and actor code.
They cover missing/invalid rows and fields, null columns, range checks, stale
owner/item/follower generations, independent update/expiry timers, one-minute
expiry without unsigned underflow, no duration loss from summon/skin refresh,
initial creature-level publication, non-levelling DB hydration, evolution,
item-ID-based escaped renaming, rejected slot indices, selected-item replacement,
failed consumption, duplicate/foreign/locked skill books, multi-pet teardown
and retained callbacks against a replacement system.

The SQL and skill/EXP tables are fixtures, not the live schema, deployed balance
data, or actual MySQL escaping implementation. The active ENABLE_NEW_PET_EDITS
configuration is covered; alternative legacy skill rules are not runtime-tested.
The test packet/affect doubles do not simulate the complete client UI, nested
engine affect recalculations or real network insert/reencode serialization.
Before deployment, verify pet seal DB rows (level 1-120, evolution 0-3, valid
skill slots/levels, positive remaining duration), egg-to-first-summon, logout/relog,
age/expiry, evolution/EXP gates, skins, rename and feeding/skill windows in-game.
Runtime factories, movement, point/affect calculation and persistence still
have legacy internals; removing pointer round trips here is not a full engine
or DB-layer migration.
