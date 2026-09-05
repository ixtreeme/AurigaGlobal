# Server ECS regression tests

## Native inventory placement and detachment

The existing InventorySystem.cpp now owns ordinary placement/removal and the
main/extra/dragon-soul empty-slot queries. The previous ItemSystem.cpp bodies
were removed, not retained as a second implementation. PlaceItemEcs no longer
prewrites ownership, calls a legacy boundary, restores possibly destroyed
components, or resynchronizes from CItem. The extra-inventory entity query no
longer resolves either CHARACTER or CItem. Ordinary AddToCharacter uses the same
native insertion core; its acquisition-only rune/accessory rules remain separate
from pure transfer placement.

Placement validates entity generations, raw ownership, ground state, window,
footprint, page/unlock limits, DS boxes, extra categories and cross-window aliases.
Both item anchors and occupancy grids must be empty. Slot/grid and ownership/
location change together before saving or sending packets. Pending destroy-event
leases are moved out of components before cancellation and the destination is
rechecked afterward. Publication reads current slot state, so a nested move or
replacement is not overwritten by a rollback. Extra unlock multiplication uses
64-bit arithmetic before clamping.

RemoveItemEcs now enters the actual removal engine BEFORE clearing metadata.
Previously the owner was cleared first and the engine returned without removing
inventory references. Removal clears matching anchors/footprints together with
ownership; it rechecks location after component-construction callbacks. Unequip
no longer calls SetItemCell with the old owner after detaching (that setter also
reattached ownership). It commits the wear slot and detached metadata together,
preserves the EQUIPMENT/absolute-cell wire format, rejects recursive unequipping,
and stops after callback destruction or reownership.

QuickslotTests was extended in place: it links the real InventorySystem.cpp,
including placement, detachment, unequip and query bodies. Fixtures have only
entities/components, with no attached CHARACTER/CItem. Tests cover main, extra,
DS, belt and switchbot storage; invalid cells, grids and aliases; stale/same-PID
owners; timer cancellation, save/packet callbacks, nested relocation and entity
recycling; equipment detachment and acquisition-vs-transfer accessory behavior.
External item accessors, combat/quest/timer services, persistence, switchbot
registration and packet transport are controlled doubles. Container removal is
covered separately by the existing safebox/item-manager suites, not by a live
cross-subsystem server session.

The construction-destruction test initially exposed an ASAN heap-buffer-overflow
in the new helper's use of EnTT get_or_emplace: its signal mixin calls get after
on_construct. The helper now uses single-entity insert, which publishes the same
construction signal without returning a reference to a possibly deleted object.

Verified on 2026-09-06: GameServer Release and all 13 Release/ASAN tests pass.
Run the expanded inventory test through the existing QuickslotTests target:

```powershell
cmake --build build --config Release --target GameServer QuickslotTests
ctest --test-dir build -C Release --output-on-failure
cmake --build build-asan --config RelWithDebInfo --target QuickslotTests
ctest --test-dir build-asan -C RelWithDebInfo --output-on-failure
```

This is not the complete equipment/shop migration. EquipTo, higher-level
equip/unequip wrappers and other legacy callers still need migration and broader
effect-level callback tests. Actual client inventory/equipment UI, relog, mount
account storage, safebox transfers and DB persistence require integration tests
before deployment. Saves are not a durable multi-record transaction.

## Entity-owned exchange sessions

The existing exchange.cpp/exchange.h now contain the complete exchange state
and implementation. CExchange, its owner/company pointers, CHARACTER's exchange
pointer and duplicated exchange timestamp are removed. ExchangeRef stores a
generation-checked session entity; both offers belong to one ExchangeSession.
input_main decodes the request and passes native entities to ExchangeSystem.
Only START resolves a network VID. Social/window guards query live ECS state.

Completion validates item identities, original slots/owners, counts, attributes,
sockets, locks, duplicate inventory references, gold, player/window state and
available space. Main, extra and dragon-soul grids are planned as value snapshots.
Both outgoing offers are removed from those snapshots first, so full-inventory
swaps work. Live inventories, ownership/location, balances and old quickslot
bindings are updated before the first publication callback. Transfer does not
rerun the item-creation rune-autoequip or accessory-randomization rules.
Old CGrid heap allocations and sequential unchecked detach/add calls are gone.

Cancellation and participant/session destruction release matching reservations
and item flags. END publication retains both reservations until completion;
revision checks stop old publications after reentrant changes. Item-slot packets
read current state, and quickslot packets stop on revision changes.
VCardUse moved from db.cpp into the existing exchange implementation. It copies
identities before callbacks and requires successful item retirement before
submitting credit. It does not create an acknowledgement-based DB transaction.

ExchangeTests links production exchange.cpp with entity-only inventory fixtures.
It covers lifecycle cancellation/destruction, stale handles, source/offer guards,
display bounds and overlap, changed item metadata, duplicate slot aliases,
full-inventory swaps, page and capacity boundaries, all six extra categories,
locked extra slots, dragon-soul boxes, invalid/insufficient/overflowing gold,
64-bit gold transfers, missing DB/quest/inventory state, cooldown changes,
callback disconnects/cancellation/slot replacement and VCard retirement failure.
Inventory/owner mutations and packet construction are production code. Actor,
item-accessor, quest, persistence, card-destruction and packet-transport services
are test doubles; no CHARACTER or CItem is attached to a fixture.

The tests also exposed a pre-existing MSVC enum truncation: GOLD_MAX became
-1727379968 instead of 999000000000. The shared limit is now an explicit int64_t
constant, outside EMisc, with a compile-time width check.

Verified on 2026-09-06: GameServer and Database Release builds succeeded;
all 13 tests passed in Release and MSVC AddressSanitizer RelWithDebInfo.
ExchangeTests reported 11,984 checks in each configuration.

```powershell
cmake --build build --config Release --target GameServer ExchangeTests
ctest --test-dir build -C Release --output-on-failure
cmake --build build-asan --config RelWithDebInfo --target ExchangeTests
ctest --test-dir build-asan -C RelWithDebInfo --output-on-failure
```

Before deployment verify actual client exchange UI, all inventory types, quickslot
updates, VCards, disconnect/relog and item/gold persistence. The headless tests do
not execute input_main, the production item manager, real sockets or DB workers.
In-process all-or-nothing ownership changes are not a durable multi-record DB
transaction: process/DB failure between saves still requires persistence-layer
work. Existing gold-log numeric fields are also still 32-bit. Other subsystems
and services retain legacy internals; this is not the end of the ECS migration.

## Skill runtime, colors and input recipients

SkillDamageBonus::useInfo is now the only skill-use runtime store. The
CHARACTER map, CheckSkillHitCount and GetUsedSkillMasterType methods were
removed, along with the splash functor's retained TSkillUseInfo pointer.
Cast registration, hit consumption, per-target limits, main-target identity
and used mastery operate on generation-checked entities. Rejected cooldown
requests no longer clear target hit counts or change the accepted cast's mastery.
The shoot path resolves the target before setting it; a network VID is not an
EnTT entity and must not be converted by static_cast.

SkillColor is the sole color store: client changes, DB loading and buff-color
copying use the existing SkillSystem. CHARACTER's array/getter/setter and its
initialization were removed. Paid color changes check consumption success,
reject recursive purchases, reacquire state after item callbacks, and persist
the committed snapshot before publishing the character update. Zero-color
resets remain free. Payment and DB persistence are separate operations, not a
durable transaction; process/connection failure can still leave a partial save.

The previous zero-result native GetSkillPower stub now uses the real skill
power tables, language-ring and guild rules. Existing legacy callers delegate
to this same implementation; the migrated skill calculations and chat
recipients invoke it directly with their existing entities. Chat recipient/
party delivery and melee victim checks no longer resolve a character just to
recover its entity. The unreachable FYmirChatPacket implementation was removed
from input_main rather than kept alongside the active implementation.

This pass reduces input_main's textual LPCHARACTER count from 48 to 44,
LegacyCharOf from 32 to 31 and GetEntityHandle from 31 to 18. Across the changed
server files, GetEntityHandle occurrences decrease by 103. Counts include
comments/debug text and do not establish that the whole combat engine is native.

GmCommandTests additionally compiles the real skill_power.cpp. Entity-only
fixtures execute all byte-sized skill hit-limit branches, forbidden attack
skills, independent targets, cooldown rejection, shared splash hit budgets,
accepted-cast resets, stale/recycled owners/targets, mastery and power-table
selection/clamping. Color tests cover load/save, slot bounds, missing/failed
payment, free resets, all five buff mappings, self buffs, callbacks that
destroy/recycle entities or change colors, recursive purchases, and missing DB.
Inventory consumption, guild lookup, character/network publication and DB
transport are service doubles. CombatStateTests only adds a fail-fast link
double for the unexecuted shoot-to-skill call.

These headless tests do not execute input_main/input_db, live damage or splash
traversal, real item payment, sockets or DB persistence. GameServer compilation
checks those integrations. Before deployment exercise melee/multi-hit skills,
charge/ranged skills, GM cooldown mode, self/party buffs, paid coloring and free
reset with logout/relog, cross-empire chat/language rings and party delivery.
Legacy damage execution, sector traversal and SkillLevels array ownership still
need further migration; this is not a complete ECS conversion of combat.

## Main input: storage, messenger and inventory guards

The preceding input_main pass reduced LPCHARACTER occurrences from 68 to 48 and
LegacyCharOf from 42 to 32 (textual counts, including old comments/debug strings).
Main/dead dispatch uses DESC::GetEntity and verifies the current player/session
binding. Safebox/mall and account mount-inventory handlers, messenger requests,
quest confirmations, party removal, guild creation and chat helpers pass entities.
The admin-whisper Manager entry point also accepts an entity directly.
No parallel production input implementation was created.

CanHandleItems, inventory capacity/grid checks, belt occupancy and refine mode,
scroll cell and NPC identity live in the existing InventorySystem/components.
The obsolete CHARACTER refine fields and warp-event member were removed; old
callers delegate into the same state. ClearRefineMode deliberately retains the
scroll cell consumed by DoRefineWithScroll after closing the mode. NPC handles
are generation-checked, not re-resolved through a potentially reused VID.

Storage handlers retain safebox lifetime, revalidate ownership/location after
callbacks, restrict source windows and only clear quickslots after successful
check-in. Rollback never overwrites occupied inventory or claims a moved item.
Messenger/guild/admin-whisper text fields are bounded; quest confirmation no
longer writes into the input packet. The unsafe unbounded test-server chat log
was removed from dispatch.

QuickslotTests compiles the real InventorySystem.cpp and now also exercises
entity-only item-handling guards, refine state and stale NPCs, pending warp,
inventory-size clamping, the entire 16-bit main-slot range, page crossing,
extra-inventory locks/large indices, exception cells, Dragon Soul secondary
occupancy, belt contents, and invalid/stale owners. Quest flags, item validity,
event reference counting and unrelated engine services are test doubles.
The existing safebox and item-manager suites exercise their real production
container/lifecycle code. These tests do NOT execute input_main, the account
mount container, messenger/admin-whisper dispatch, real network or DB effects.
GameServer builds check those integration signatures, not runtime correctness.

Before deployment verify all three storage windows, failed/full-slot moves,
quickslots, mount bonuses and logout/relog persistence with the actual client/DB;
also check messenger, guild creation, quest confirmation and admin messages.
Transfers are not durable DB transactions. Item use/drop/move, combat/movement,
cube, battle pass and other remaining character-based services need more ECS
work; native packet boundaries do not imply a completely legacy-free engine.

## GM commands and skill state

The cmd_gm migration removes 28 of its 46 LegacyCharOf calls, plus pointer
target lookups and repeated GetEntityHandle conversions. Stat, skill, affect,
quest-flag, user-list, notice, item-purge and item/socket command paths pass
entities directly. Skill prerequisite checks and group-change packets now live
in the existing SkillSystem implementation; player saves read the ECS skill
group. SkillLevels still references the existing skill array: this is not a
complete conversion of skill storage ownership.

Inventory purge snapshots selected slots, checks item generation/ownership and
slot identity before deletion, and only clears quickslots after successful
deletion if the slot is still empty. Missing pet systems are accepted; summoned
growth pets block purge as before. Selectors now require exact documented names
or aliases instead of arbitrary matching prefixes. Item grant failures are not
logged as successful grants and surviving unowned items are cleaned up.

GmCommandTests compiles the complete production cmd_gm.cpp, SkillSystem.cpp and
skill_power.cpp, the real PID registry, argument parsing and temporary buffers.
Entity-only fixtures execute inventory purge, user listing, socket commands,
setskill and set_skill_group, notice/P2P serialization and native skill setters
and resets. Coverage includes all supported purge windows/aliases, duplicate
entries, invalid/stale/non-PC owners, active/missing pet systems, failed deletion,
callback-time owner/item destruction, recycled indices, ownership/slot changes,
new/replacement items, nested user commands, disconnected/new notice recipients,
map/empire/GM filtering, literal percent signs, 10,000-character notices, numeric
range/junk rejection, skill caps/master thresholds, helper prerequisites,
anti-skill level/book rules, zero resets, group reset and packet/point callbacks
destroying the owner.

Inventory storage/destruction, quickslot delivery, pet state, character lookup,
point calculation, descriptors/chat/network transport, skill-prototype lookup
and DB services are doubles. Other linked GM/legacy services fail fast if called.
These tests do not execute the real inventory allocator, quest flag-list
implementation, character save/relog, remote warp, command authorization/dispatch
or client UI. The main GameServer build checks those integration signatures.
Before deployment verify /set, /setskill, /setskillother, /set_skill_group and
skill-group persistence across relog; quest flag/state commands, item grants,
purge/quickslots and local/cross-core notices in-game.

18 LegacyCharOf boundaries remain in cmd_gm: remote PID warp, sector purge/
weaken, reset/save, safebox sizing, monster-control commands, observer/build,
horse commands, cannot-dead/all-skills horse state, and the legacy Gaya balance.
Sector traversal and some spawn paths also still use legacy entity pointers.
Movement, item and other services may retain legacy internals; no additional
wrapper was introduced to hide those boundaries or duplicate a production file.

```powershell
cmake --build build --config Release --target GameServer GmCommandTests
ctest --test-dir build -C Release -R gm_commands --output-on-failure
cmake --build build-asan --config RelWithDebInfo --target GmCommandTests
ctest --test-dir build-asan -C RelWithDebInfo -R gm_commands --output-on-failure
```

## General commands and entity-owned window state

The current cmd_general migration removes 21 of its 54 LegacyCharOf calls.
Stat commands, duel settings/target lookup, party leave, walking preference,
growth-pet commands, stone crafting, shop-window guards, safebox-open position,
costume visibility, event-manager checks and itemshop protection use entity APIs.
The timed logout countdown resolves a CHARACTER only at its logging/disconnect
boundary. Horse/riding, inventory sorting, fishing/restart, skill learning,
storage persistence, party requests, logging/block mode, observer cleanup,
legacy cube and disabled guild-renewal branches still need further migration.
No parallel production command implementation was added.

Cube NPC and dragon-soul refine opener state now store versioned entities;
every existing reader was updated. Safebox-open and preferred walk/run mode
are component-owned, with CHARACTER compatibility accessors delegating inward.
Quest current-PC/NPC entity getters no longer resolve CHARACTER pointers.
Duel option access goes directly through entity quest flags, preserving the
boolean GetDuel contract (including BetMoney).

RefineWindowTests compiles the complete production DragonSoulSystem.cpp. It
covers entity-only open/self-open/reopen/close, missing descriptors, null/stale
owners/openers, recycled indices, failed-open permissions, packet-time owner or
NPC destruction, nested close and component-pool growth. Packet/descriptor,
item and affect services are doubles; deck operations fail fast if invoked.
PointCalculationTests executes the real stamina point-change branch, verifying
that exhaustion preserves the player's preference and recovery restores it
instead of the current movement tick's isWalking flag. Walking packet delivery
is a double in that test.

These targets do not execute cmd_general dispatch, native session helpers,
quest context getters, duel persistence or live client/DB traffic. The complete
GameServer build checks their integration. Before deployment smoke-test both
duel participants, stat add/reset, walk/run after exhaustion, pet evolution,
stone crafting, competing windows, itemshop malformed/repeated requests and
disconnect/relog. Craft reward/payment ordering and multi-material pet costs
are not converted into an atomic inventory/DB transaction by this change.

```powershell
cmake --build build --config Release --target GameServer RefineWindowTests PointCalculationTests
ctest --test-dir build -C Release --output-on-failure
cmake --build build-asan --config RelWithDebInfo --target RefineWindowTests PointCalculationTests
ctest --test-dir build-asan -C RelWithDebInfo --output-on-failure
```

## Character manager indices, queues and event drops

`CharacterManagerTests` compiles the complete production `char_manager.cpp` and
the VID/PID registries. Name lookup, selection, PC/FSM snapshots and pending
destruction use versioned entities, including fixtures without a CHARACTER shell.
Checks cover case-insensitive names, stale/recycled handles, replacement index
entries, incomplete names, reservoir sampling and job-mask shift bounds, deferred
and recursive destruction, stale-index shutdown, removed snapshot members and
preservation of an outer pending-destruction scope.

Save tests attach inert CHARACTER shells to exercise the actual manager's
SaveReal boundary: callback requeue, destruction of another queued save, exactly
one final save and ECS state remaining alive until shell teardown. The shell
destructor and EntityFactory are doubles; this is not an end-to-end inventory,
dungeon, session or mount teardown test.

Drop tests force successful event rolls and vector growth, verifying unique
clone handles, counts, invalid/failed items, stacking, boss variants, mission
books, dungeon tickets and the soul-only filter. Item allocation is a double.
Disconnected itemshop entry points must return without contacting live services.
Actual SQL/packet exchange and the full legacy spawn/destructor paths are not
executed by this target. The itemshop coin query now reads the entity's session;
protection times are ECS-owned. Neither change replaces DB-side purchase
validation or makes the length-less itemshop DB packet parser bounds-checked.

`CombatStateTests` additionally exercises the production ECS chat/mount counters:
default reads, increments, independent mount reset, combined periodic reset,
byte rollover and null/stale/recycled owners.

```powershell
cmake --build build --config Release --target GameServer CharacterManagerTests CombatStateTests
ctest --test-dir build -C Release -R '^(character_manager|combat_state)$' --output-on-failure
cmake --build build-asan --config RelWithDebInfo --target CharacterManagerTests CombatStateTests
ctest --test-dir build-asan -C RelWithDebInfo -R '^(character_manager|combat_state)$' --output-on-failure
```

Remaining `LegacyCharOf` calls in `char_manager.cpp` are explicit boundaries for
Disconnect, CHARACTER/dungeon teardown, pointer-returning FindPC/SpawnMob and
SaveReal. Group/range spawning and `for_each_pc` still have legacy callers; this
change does not claim the entire character manager or AISystem is legacy-free.

## Affect ownership and point application

`AffectLifecycleTests` compiles the complete existing `AffectSystem.cpp` and
`affect.cpp` and `horsename_manager.cpp`, using entity-only fixtures with no
CHARACTER or CItem allocation.
`AffectList` owns the live records, saved skill buffs, flags and loaded state.
There is no CHARACTER list/flag mirror or periodic copying. Storage insertion
initializes the entire record before publication, and removal detaches it before
point callbacks. Short-lived shared leases keep removed records alive only while
an operation/snapshot still uses them; the registry owns normal live membership.

Addition/overwrite, point application/refresh, lookup, flags, individual/type
removal and good/bad affect removal are native entity operations. Checks cover foreign/repeated
removal, destruction and recycled entity handles, shared allocation lifetime,
both flag words and item IDs in the flag field, loaded-state transitions,
INT32_MIN bonus reversal, invalid apply values, the missing-guild gate,
deduplicated snapshots, nested refresh, removal/replacement/addition during a
refresh, replacement of the component on the same entity, exceptions, HP/SP
clamping, revive/mount exceptions, finite type-removal batches and 1,000 repeated
allocation/refresh/removal cycles. The obsolete global ECS expiry pass is a
no-op: the affect event drives the single native `ProcessAffect(entity)` pass
through its still-legacy outer recovery tick.

Addition also tests cube apply-slot matching, non-overwrite stacking, zero
duration, invalid apply rejection, scheduling failure before any grant, polymorph
conflicts, movement abort on stun, same-type reentrant changes during overwrite,
owner destruction at scheduler/point/update/DB boundaries, and packet ordering.
Real client/DB packet structures are captured by inert descriptor transport
doubles; tests check fields, player ID, no-save types, DB remove/add order,
independent wire keys and latest-value publication after nested replacements.
The (type, apply) wire key cannot represent multiple same-key records: runtime
stacking is retained, but only the newest matching record is published.

Expiry checks cover one-second duration/SP-cost application, ordinary expiry
without full point recomputation, insufficient SP, zero/negative and extreme
durations, soul item IDs in the SP-cost field, the missing-guild gate, HP/SP
clamping, duplicate leases, recursive ticks, exceptions, callback replacement
of current/future records, owner destruction/recycling, replacement of affect
storage, and reentrant client/DB publication. Records added/replaced during the
ordinary duration batch wait for the next pass; absolute-deadline preprocessing
still runs before that batch, preserving the existing order.

Battle-pass deadlines are ECS-owned with no CHARACTER mirror; account premium,
battle-pass and quest-driven hair/horse deadlines are covered at expiry and
integer boundaries. `PREMIUM_MAX_NUM` is treated as an array count, not an extra
premium slot. Horse-name validation uses versioned entities and owned affect
leases; tests cover expiry, recursive validation, callback replacements,
destruction during quest/summon/removal callbacks and the actual name DB packet.
The horse summon transport is a double: engine spawn/movement still has legacy
internals. The redundant entity -> CHARACTER -> entity summon wrapper is gone,
but this does not make the full horse actor lifecycle legacy-free.

`AffectTickState` owns the scheduled event. Its move-only component cancels the
timer on destruction; CHARACTER no longer has an affect-event member. Start/stop,
shutdown cancellation before event-queue teardown, pending/reentrant starts,
component relocation/replacement, stale callbacks and recycled owners are
checked with an in-memory scheduler double. The callback
validates both entity generation and current event identity before the legacy
outer recovery leaf; an entity without that leaf stops safely. Native expiry
can now run independently through `ProcessAffect(entity)`, but the scheduled
recovery/item/recall tick is not native yet. Muyeong/Gyeonggong are separate
skill timers and have not been migrated in this step.

```powershell
cmake --build build --config Release --target GameServer AffectLifecycleTests
ctest --test-dir build -C Release -R '^affect_lifecycle$' --output-on-failure
cmake --build build-asan --config RelWithDebInfo --target AffectLifecycleTests
ctest --test-dir build-asan -C RelWithDebInfo -R '^affect_lifecycle$' --output-on-failure
```

Point changes/recomputation, guild/premium/quest lookup, clock, movement/posture,
horse spawning, scheduler and packet transport are service doubles; unrelated
legacy services fail if called. This
does not execute the complete point/affect feedback cycle, live guild-war rules,
socket/SQL I/O, the real event queue, login hydration, clear-on-death policy,
outer recovery or skill timer callbacks. Load/clear/recovery and
Muyeong/Gyeonggong timer internals still include legacy CHARACTER work, although
they now use the same owning ECS storage. The disabled vote-for-bonus branch
is retained but not runtime-tested. The player factory seeds the deadline
component; legacy DB hydration/serialization call sites use the new deadline
API, but complete login/save is not executed by these headless tests. Raw CAffect*
lookup remains a borrowed compatibility API: do not retain it across callbacks,
and do not release it directly. A lease prevents deallocation, not logical
membership changes or arbitrary mutations of a retained record. Expiry guards
prevent duplicate work within a recursive pass; an exception does not roll back
already-applied point changes.

Refresh suppresses recursive refresh and skips removed/changed snapshot entries;
it does not transactionally roll back point changes or queue/rebuild all effects
after arbitrary reentrant mutations. Newly added records are not replayed in the
current batch. Before deployment verify login/logout and character selection,
death/revive saved buffs, poison/fire/bleeding expiry, recovery item locks, guild
war buffs, repeated equipment/point recomputation and both skill timers against
the real client and DB. This is a bounded ECS migration, not a claim that every
affect lifecycle path or the original core crashes have been resolved.

## Real event-queue lifecycle

`EventLifecycleTests` compiles the existing `event.cpp` and `event_queue.cpp`,
using the production allocator, scheduler and queue with only a fixed clock and
fatal-error/logging hooks. Checks cover self-cancellation inside a running
callback, ECS affect-timer destruction during a callback, EnTT component
relocation, cancellation before execution, repeat timing, reset with a cancelled
older queue entry, shutdown and externally retained event handles.

Deleting a queue entry now clears its event's backlink only when that entry is
still current. This prevents cancellation from dereferencing freed queue memory
without erasing a newer entry installed by a timer reset. These checks complement
the affect scheduler double above; they do not run full affect expiry or network
and database integration.
Before the fix, the self-cancellation case reproduced an AddressSanitizer
heap-use-after-free in `event_cancel`, with the entry freed by `CEventQueue::Delete`
inside `event_process` before the callback. This proves the queue defect, not the
cause of a particular historical live-server crash.

```powershell
cmake --build build --config Release --target EventLifecycleTests
ctest --test-dir build -C Release -R '^event_lifecycle$' --output-on-failure
cmake --build build-asan --config RelWithDebInfo --target EventLifecycleTests
ctest --test-dir build-asan -C RelWithDebInfo -R '^event_lifecycle$' --output-on-failure
```

## Combat state

`CombatStateTests` compiles the complete existing `CombatSystem.cpp` and
`battle.cpp`, including `ENABLE_ANTICHEAT` and `ENABLE_CHECK_BATTLE` in this test
target only. Alignment,
its tier calculation, killer/PK mode and attack/damage multipliers operate on ECS
components with no CHARACTER allocation or entity-to-pointer lookup. The legacy
alignment, timer and multiplier fields are removed; compatibility methods only
forward to the native services, and saving reads the component. Both point
recomputation and alignment updates use the same tier function.

Checks cover every tier boundary, signed reductions and saturation (including
INT64_MIN/MAX), unchanged values, sub-display changes, a newer nested alignment
update including ABA, destruction/recycled entity indices during compute/packet
callbacks, killer-mode expiry across signed/unsigned tick wrap, guild-mode
normalization, absent components, non-character/stale/null handles and finite,
zero, negative and non-finite multiplier inputs. The alignment revision suppresses
an obsolete outer publication; state commits before external callbacks. It is not
a transaction or rollback mechanism for point calculation or networking.

Battle checks exercise entity-only target ownership and clearing, attack/skill
state, melee and arrow calculations, weapon/refine/defense formulas, live mob
prototype and berserk data, stale items and recycled character handles. Monster
damage uses the legacy non-PC meaning rather than the narrower `TagNPC` meaning.
Normal-hit poison/stun callbacks destroy their victim to check that processing
does not continue on a dead generation. Anti-cheat checks cover riding, a zero
speed denominator, both attack logs, recycled target identities and clock wrap.
Combat pulses and wall-clock attack milliseconds have separate fields.

The two remaining `battle.cpp` character resolutions are explicit boundaries to
soul consumption and the complete legacy `Damage` pipeline, not converted
operations. Those pipelines, marriage services, networking, and affect execution
are not integration-tested by this headless target; the dependent services are
doubles and unexpected legacy character operations fail immediately.

```powershell
cmake --build build --config Release --target GameServer CombatStateTests
ctest --test-dir build -C Release -R '^combat_state$' --output-on-failure
cmake --build build-asan --config RelWithDebInfo --target CombatStateTests
ctest --test-dir build-asan -C RelWithDebInfo -R '^combat_state$' --output-on-failure
```

Point recomputation, time, guild lookup and packet publication are test doubles;
other combat/loot/DB/quest/legacy services fail if called. This does not exercise
damage resolution, actual network serialization, client rendering, the complete
point/affect reentrancy cycle or a live database. The GM target-alignment fix,
bounded Lua input conversion, factory hydration and persistence call sites are
compiled with GameServer, not executed by these headless tests. Before deployment
verify relogged alignment, GM/quest changes, rank bonuses, killer-mode expiry,
guild changes and dungeon/quest multipliers with the real client and DB. Only
non-finite and negative multipliers are rejected here; downstream damage overflow
for extremely large finite multipliers is outside this state migration.

## Point calculation

`PointCalculationTests` compiles the complete existing `PointSystem.cpp` and
`StatSystem.cpp` with entity-only fixtures, without creating a `CHARACTER` or
`CItem`. The recomputation pipeline, point arrays, flat/base/final HP and SP
separation, alignment bonuses and battle formulas run as production code.
The old CHARACTER entry points forward to ECS; the alignment cache and legacy
skill-damage map are removed. Login and both spawn paths initialize the full
archetype before prototype loading invokes point calculation, without a late
legacy-to-ECS point copy.

Checks cover repeated calculations and zero-delta updates, SP percentage caps,
current-HP/SP preservation, removed equipment and skill bonuses, immunity,
entity-only PC versus NPC formulas, inactive/duplicate/foreign rune equipment,
active and invalid Dragon Soul decks, belt whitelist/duplicate/malformed applies,
mount bonus input, support-skill bounds, alignment upgrades/downgrades, incomplete
archetypes, stale handles, recursive callbacks, exception/guard recovery and owner
destruction with entity-index reuse.

```powershell
cmake --build build --config Release --target GameServer PointCalculationTests
ctest --test-dir build -C Release -R '^point_calculation$' --output-on-failure
cmake --build build-asan --config RelWithDebInfo --target PointCalculationTests
ctest --test-dir build-asan -C RelWithDebInfo -R '^point_calculation$' --output-on-failure
```

Item point application, mount-inventory aggregation, skill/prototype lookup,
affects, pets, events and network services are doubles in this target. Balance
tables are test fixtures, not deployed data. The guard suppresses nested computes;
it does not provide transactional rollback or queue equipment changes made by a
callback. A retry rebuilds the point arrays, not arbitrary external side effects.
Legacy leaves still exist for moving-character motion duration, affects/pets and
some level-up, save, mount and Gaya operations. This is not a fully legacy-free
server. Factory/bootstrap changes are compiled, not executed by these headless
tests. Before deployment test login, every NPC/monster/stone/horse spawn type,
equipment swaps, buffs, mounting, alignment transitions, death and logout/relog
with the real client, prototypes and database.

## Quickslots

`QuickslotTests` compiles the complete production `ecs/systems/InventorySystem.cpp`
with entity-only fixtures. Quickslot implementation stays in that existing file;
`CHARACTER::m_quickslot`, its pointer-returning getter, add/delete/swap methods
and unused chain helper are removed. The remaining CHARACTER sync entry point
only forwards unmigrated callers; it stores no mirror. Its byte-sized arguments
are still a legacy boundary: the new native sync API accepts wider cells and
rejects values that cannot be represented instead of wrapping them.

The native setters validate before removing duplicates, commit the complete
state before packet publication and suppress an obsolete publication after a
nested modification or owner destruction. Relocation and database hydration
retain the last valid duplicate, matching the old ordered assignment behaviour.
Factory hydration filters malformed entries before publishing the component.
Login sends that completed state (including empty-slot clears); save reads ECS
values, and auto-give no longer sends a second add packet itself.

```powershell
cmake --build build --config Release --target GameServer QuickslotTests
ctest --test-dir build -C Release -R '^quickslots$' --output-on-failure
cmake -S . -B build-asan
cmake --build build-asan --config RelWithDebInfo --target QuickslotTests
ctest --test-dir build-asan -C RelWithDebInfo -R '^quickslots$' --output-on-failure
```

Checks enumerate all byte-sized type/position combinations and invalid target
indices; exercise add/delete/swap, duplicate removal, item relocation/removal,
wide-cell rejection, invalid/stale/recycled owners, nested publication, malformed
and duplicate persisted slots, full login publication and value-copy roundtrips.
Client-facing validation covers missing/foreign/stale items, allowed item types,
extra-inventory alias 12, and potion rejection for both alias and canonical
extra-inventory types without modifying the caller's request value.

Item lookup and network services are doubles. The target verifies requested
packet kind, recipient, ordering and values, not actual socket serialization or
client rendering. It executes the production client-validation service, not the
input dispatcher/framing code. The thin packet handlers, network packet builders,
factory and character-save call sites are compiled with GameServer; the complete
login/DB-save flow is not executed headlessly. Existing packet framing still owns
the minimum-length checks. Other inventory/equipment/spatial services fail if
the tests accidentally call them. Test shortcut drag/drop/delete/swap, potion
auto-assignment, item movement/consumption, empty-slot clearing on login and relog
persistence with a real client/DB before deployment. Bulk item-manager shutdown
still needs migration.

## Item-manager destruction

`ItemManagerLifecycleTests` compiles the complete production `item_manager.cpp`,
`safebox.cpp` and `Base/grid.cpp`. Both high-level `RemoveItem` and low-level
`DestroyItem` read identity, location and ownership from ECS; an optional legacy
allocation is resolved only at the final release boundary.
The existing implementation is edited in place, not duplicated into another system.

```powershell
cmake --build build --config Release --target GameServer ItemManagerLifecycleTests
ctest --test-dir build -C Release -R '^item_manager_lifecycle$' --output-on-failure
cmake -S . -B build-asan
cmake --build build-asan --config RelWithDebInfo --target ItemManagerLifecycleTests
ctest --test-dir build-asan -C RelWithDebInfo -R '^item_manager_lifecycle$' --output-on-failure
```

Fixtures cover entity-only and legacy-backed items with entity-only owners,
including zero/mismatched last-owner PIDs, absolute equipment cells, missing or
foreign slots and invalid owners. They verify recursive manager removal during
ground/detach/factory callbacks, repeated destruction, recycled entity indices
and ID/VID reuse, ownership transfer during ground/detach callbacks (also to
stale owners), guard release after exceptions, rejected factory cleanup and
retry, mismatched legacy bindings, and persistent-item deferral without a DB
descriptor. Factory callbacks see unpublished indices; unsuccessful factory
cleanup restores still-live identities without overwriting newer mappings.
High-level tests cover normal/extra inventory, equipment, dragon-soul, switchbot
and mount windows, wide quickslot cells, logging/quickslot/mount callbacks,
reentrant removal, ownership transfer, recycled IDs and rejected detachment.
The real safebox/mall integration is exercised for removal, close during logging
or detachment, shared-lease lifetime, failed removal and transfer to another owner.

Item access, inventory, factory, logging and DB services are doubles. Legacy
CItem/CEntity construction and destruction are doubles too: the tests verify
allocation release order, not engine timers, component destruction callbacks,
spatial removal, packet delivery or durable persistence. Unrelated manager
creation/drop dependencies fail immediately if called. Bulk shutdown `Destroy`
still needs migration. The ItemSystem dispatch now uses the same high-level
manager path for entity-only and legacy-backed items; this adapter is compiled
with GameServer, not executed by this target. Native mount lookup/packet code
and session adapters are also compile-checked only. Point computation still
uses the existing legacy-backed PointSystem service; its internals are not
migrated by this change. General client-facing inventory lookup is unchanged:
storage-window lookups are limited to the manager's internal removal path.

The missing-descriptor guard is not a reconnect queue or a DB acknowledgement.
It does not undo side effects already performed by a higher-level caller, nor
does factory failure roll back a previously buffered DB delete. Direct factory
callers and engine callbacks that independently delete legacy allocations are
outside this manager reentry guard. Before deployment test expiry, ground-item
removal, inventory/equipment/mount removal, last-unit consumption, disconnect,
shutdown, DB interruption/reconnect and relog with the actual client and DB.

## Safebox and mall lifecycle

`SafeboxLifecycleTests` compiles the existing production `safebox.cpp` and
`Base/grid.cpp`. The owner and every item are versioned EnTT entities with no
`CHARACTER`, `CItem`, or legacy-pointer component. The implementation remains
in `safebox.cpp/.h`; there is no parallel ECS copy of the storage system.

```powershell
cmake -S . -B build
cmake --build build --config Release --target GameServer SafeboxLifecycleTests
ctest --test-dir build -C Release -R '^safebox_lifecycle$' --output-on-failure
```

For ASan, configure `build-asan` with `ENABLE_ASAN=ON`, build the same test target
in `RelWithDebInfo`, and run the test with `ctest --test-dir build-asan -C
RelWithDebInfo -R '^safebox_lifecycle$' --output-on-failure`.

Coverage includes both storage windows, detached-item attachment, duplicate and
overlapping placements, multi-cell footprints, invalid heights/positions,
growth up to the fixed item-array limit, and preservation of occupied cells.
It also covers invalid/NPC/recycled owners, recycled items, items moved to
another owner/window/cell, failed publication/detachment, and teardown after
owner destruction. Reentrant flush/detach/destroy callbacks see an already
unpublished container and cannot add, remove, move, save or destroy it again.
Post-callback item identity/ownership is revalidated before further work.

Grid ownership is RAII; copy/move construction is disabled. SafeboxRef owns
both containers through shared leases; CHARACTER no longer has duplicate raw
owning pointers. SafeboxSystem lives in the existing safebox.cpp/.h. Close
unpublishes the container before callbacks, blocks reopening that window during
teardown, and retires contents immediately even if another caller holds a lease.
Component tests cover independent windows, stale owners, callback reentry,
lease release and post-close reopen. Session/character teardown adapters are
compiled by GameServer, not executed in the headless tests; legacy GetSafebox/
GetMall borrowed-pointer accessors remain for unmigrated callers.

Persistence, descriptor lookup, item detachment/destruction and stack-count
services are doubles. No socket, DB, CHARACTER, CItem, or item-manager instance
is created. Tests cover safe no-DB saving, not successful account-save packet
delivery or actual engine item cleanup. Stack tests cover locks/exchange flags,
caps, oversized counts, normal partial/full merges and rejected consumption.
The existing debit-then-credit merge is still sequential: a credit failure
after a successful debit is logged but cannot be rolled back reliably. No DB
transaction, durable recovery, or general atomic stack-transfer API is added.

Before deployment verify safebox/mall load, deposit/withdraw/merge, resize,
close/reopen, relog/disconnect and DB interruption on a test server. Confirm
that closing unloads runtime items without deleting their stored DB rows.

## Item attributes

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
Runtime factories, movement, affect calculation and persistence still
have legacy internals; removing pointer round trips here is not a full engine
or DB-layer migration.
