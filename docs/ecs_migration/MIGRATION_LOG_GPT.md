# MIGRATION_LOG_GPT

Date: 2026-04-26
Workspace: `E:\AurigaGlobal\LiveWork\AurigaGlobal`

This log records the GPT-session changes and runtime fixes from the current ECS migration work window.

## 1. ECS Summary Log Created

File added:
- `docs/ecs_migration/MIGRATION_LOG_ECS_SUMMARY.md`

Purpose:
- Created a separate summary-style ECS migration log instead of extending the event-heavy root `MIGRATION_LOG.md`.
- Summarizes completed ECS work through:
  - baseline / early Phase 15 accessor work
  - VID-A completion
  - Phase 15B ECS system cleanup
  - Phase 15C storage migration
  - Phase 15D LPCHARACTER reduction
  - Phase 15E ChatSystem / quality audit / CharacterAccessors cleanup
- Explicitly marks parked work:
  - `CHARACTER_POINT_INSTANT.points[]`
  - remaining `CHARACTER_POINT` unique fields
  - remaining honest `LPCHARACTER` holders
  - questlua reverse bridge work

Status:
- Documentation only.
- Not committed in this session.

## 2. Phase 15E ItemSystem Public API Cleanup

Problem:
- Phase 15E quality audit found one remaining public ECS API backdoor:
  - `ItemSystem.hpp`
  - `void SyncExtraInventoryAll(LPCHARACTER ch);`
- This violated the current ECS migration rule that public ECS APIs must be entity-first.

Files changed:
- `SRC/Server/GameServer/ecs/systems/ItemSystem.hpp`
- `SRC/Server/GameServer/ecs/systems/ItemSystem.cpp`

Public API before:
```cpp
void SyncExtraInventoryAll(LPCHARACTER ch);
```

Public API after:
```cpp
void SyncExtraInventoryAll(entt::entity e);
```

Implementation:
- Added `SyncExtraInventoryAll(entt::entity e)` in `ItemSystem.cpp`.
- The function resolves the legacy character internally with the existing local `LegacyCharOf(e)` bridge.
- The bridge remains implementation-only and is not exposed through `ItemSystem.hpp`.
- The function syncs all extra inventory slots to the client using the existing packet structures:
  - `TPacketGCItemSet`
  - `TPacketGCItemDelDeprecated`
- Extra inventory storage layout was not changed.
- Main inventory, DragonSoul, stats, `points[]`, and packet formats were not changed.

Call sites:
- Tree scan found no existing call sites of `SyncExtraInventoryAll`.
- Therefore no call-site migration was required.

Validation:
- Build gate passed:
```powershell
cmake --build build --config RelWithDebInfo --target GameServer --parallel 8
```
- `ItemSystem.hpp` scan:
  - `LPCHARACTER|CHARACTER\s*\*`: no matches
- `SyncExtraInventoryAll` scan:
  - declaration: `void SyncExtraInventoryAll(entt::entity e);`
  - definition: `void SyncExtraInventoryAll(entt::entity e)`
  - no LPCHARACTER call sites

Remaining public ECS header matches from broader scan:
- These were reported but intentionally not changed because they are outside this task:
  - `ecs/components/identity_components.hpp`: `LegacyCharPtr`
  - `ecs/components/inventory_components.hpp`: transient NPC pointer component fields
  - `ecs/components/social_components.hpp`: partner pointer field
  - `ecs/AIHelpers.hpp`: existing `EcsOf(...)` bridge declarations
  - `ecs/EntityFactory.hpp`: legacy character bootstrap declaration

Commit:
- `7bc2076` `Phase 15E: Make ItemSystem extra inventory sync entity-first`

Commit hygiene note:
- The first commit attempt accidentally included previously staged unrelated launcher/bgfx files.
- Corrected immediately with:
  - soft reset of the accidental commit
  - clearing the index
  - staging only `ItemSystem.hpp` and `ItemSystem.cpp`
  - recommitting the scoped ItemSystem change
- Final commit contains only the two intended ItemSystem files.

## 3. Slow Damage / Metin Collapse Lag Returned

User-reported symptom:
- The old delayed combat feedback returned:
  - metin stone damage appeared late
  - spawned mobs appeared late
  - stone collapse / break feedback appeared late

Initial checks:
- Compared hashes for:
  - local build `GameServer.exe`
  - WinTest `share/bin/GameServer.exe`
  - WinTest `auth/GameServer.exe`
- Found binary drift again.

Hashes before correction:
- local build:
  - `5DC3E1133E171E6C3069D6ED126D25BCB2B2FDD621C884A3AD5EBFABB1B38650`
- WinTest `share/bin`:
  - `3D73D1E725FB45B7A15CBB35E94992AF34BB818B7C17CC9D31B43FBF5800EA35`
- WinTest `auth`:
  - `BC19F4FB117BF0E733BBE1A9D2B3A9953A2FE50F4FB3EA66F451CA0217E9D589`

Log evidence:
- `core99` showed the same movement correction family that previously caused delayed combat feedback:
  - `MOVE: sdfse trying to move too far`
  - immediate `SHOW`
- `core99` also had a fresh boot-period stall:
  - `heart_idle: losing 43 seconds. (lag occured)`
- The strongest actionable cause was inconsistent deployed binaries, because the same issue had already been resolved once by forcing all server roles onto one build.

Corrective action:
- Force-stopped:
  - `GameServer.exe`
  - `Database.exe`
- Re-copied the fresh local build to:
  - `C:\AurigaGlobal-WinTest\srv1\share\bin\GameServer.exe`
  - `C:\AurigaGlobal-WinTest\srv1\auth\GameServer.exe`
- Restarted:
  - `Database`
  - `auth/GameServer`
  - `ch1/core1`
  - `ch1/core2`
  - `ch99/core99`

Hashes after correction:
- local build:
  - `5DC3E1133E171E6C3069D6ED126D25BCB2B2FDD621C884A3AD5EBFABB1B38650`
- WinTest `share/bin`:
  - `5DC3E1133E171E6C3069D6ED126D25BCB2B2FDD621C884A3AD5EBFABB1B38650`
- WinTest `auth`:
  - `5DC3E1133E171E6C3069D6ED126D25BCB2B2FDD621C884A3AD5EBFABB1B38650`

Post-restart log check:
- Fresh `core99` tail showed normal boot/P2P setup.
- No immediate new `heart_idle`.
- No immediate new `trying to move too far` wave.

Runtime result:
- User confirmed:
  - `jó lett`
- Interpretation:
  - delayed metin damage / spawn / collapse behavior was again resolved by enforcing one consistent deployed binary across WinTest.

## 4. Current Notes

Current committed GPT-session code change:
- `7bc2076` `Phase 15E: Make ItemSystem extra inventory sync entity-first`

Current uncommitted documentation from GPT session:
- `docs/ecs_migration/MIGRATION_LOG_ECS_SUMMARY.md`
- `docs/ecs_migration/MIGRATION_LOG_GPT.md`

Known unrelated dirty worktree areas still present:
- Launcher/bgfx work
- root `MIGRATION_LOG.md`
- previous server movement-fix files if still uncommitted in the local tree

## 5. Phase 15E-2 Public ECS Header Leak Audit

Mode:
- Audit only.
- No code changes.
- No commit.

Output file:
- `docs/ecs_migration/phase15e_2_public_ecs_header_leak_audit.txt`

Scope:
- Scanned public ECS headers under:
  - `SRC/Server/GameServer/ecs`
- Target symbols:
  - `LPCHARACTER`
  - `CHARACTER*`
  - `LPITEM`
  - `LPENTITY`
  - `LPSECTREE`

Result:
- Total matches: `58`
- Main public API leak areas:
  - `ecs/systems/ItemSystem.hpp`
  - `ecs/systems/DragonSoulSystem.hpp`
  - `ecs/SpatialHelpers.hpp`
- Transitional component storage remains in:
  - `ecs/components/inventory_components.hpp`
  - `ecs/components/quest_components.hpp`
  - `ecs/components/session_components.hpp`
  - `ecs/components/social_components.hpp`
- Explicit bridge/bootstrap exceptions remain in:
  - `ecs/components/identity_components.hpp`
  - `ecs/AIHelpers.hpp`
  - `ecs/EntityFactory.hpp`

Priority next tasks from audit:
- ItemSystem LPITEM public API cleanup.
- DragonSoul opener API cleanup from `LPENTITY` to `entt::entity`.
- Spatial helper cleanup for `LPSECTREE` / `LPSECTREE_MAP`.
- Later component storage cleanup for LPITEM / LPCHARACTER / LPENTITY fields.

## 6. Phase 15E-3a ItemSystem Read-only API Cleanup

Mode:
- Code change.
- Safe subset only.
- No commit yet in this session for this substep.

Goal:
- Convert a minimal set of read-only `ItemSystem` APIs from returning `LPITEM` to returning item `entt::entity`.
- Preserve runtime behavior and avoid storage/layout/packet changes.

Files changed:
- `SRC/Server/GameServer/ecs/systems/ItemSystem.hpp`
- `SRC/Server/GameServer/ecs/systems/ItemSystem.cpp`

APIs migrated:
```cpp
LPITEM GetItem(entt::entity e, TItemPos cell);
LPITEM GetInventoryItem(entt::entity e, uint16_t cell);
LPITEM GetExtraInventoryItem(entt::entity e, uint16_t cell);
LPITEM GetWearItem(entt::entity e, uint8_t wearPos);
```

New signatures:
```cpp
entt::entity GetItem(entt::entity e, TItemPos cell);
entt::entity GetInventoryItem(entt::entity e, uint16_t cell);
entt::entity GetExtraInventoryItem(entt::entity e, uint16_t cell);
entt::entity GetWearItem(entt::entity e, uint8_t wearPos);
```

Mapping used:
- Existing legacy `LPITEM` is resolved internally in `ItemSystem.cpp`.
- The item pointer is converted to an ECS item entity with:
```cpp
EntityFactory::CreateItemEntity(g_registry, item)
```
- This uses existing item infrastructure:
  - `CItemRegistry`
  - `ecs::ItemIdentity`
  - item location/count/owner/equipped/flags/socket/attribute components

Call sites:
- Tree scan found no external `ItemSystem::GetItem(...)`, `GetInventoryItem(...)`, `GetExtraInventoryItem(...)`, or `GetWearItem(...)` call sites.
- Therefore no caller migration was needed.

Validation:
- Build gate passed:
```powershell
cmake --build build --config RelWithDebInfo --target GameServer --parallel 8
```
- Targeted header check confirmed the four migrated declarations no longer expose `LPITEM`.
- Targeted implementation check confirmed all four functions return `entt::entity` and use `EntityFactory::CreateItemEntity(g_registry, item)`.

Explicitly not touched:
- Inventory storage layout.
- Extra inventory layout.
- Main inventory layout.
- DragonSoul.
- Packets.
- Equip / unequip.
- Refine.
- Give / receive.
- `points[]` / stat migration.

Remaining known `LPITEM` public API leaks in `ItemSystem.hpp`:
- `FindSpecifyItem`
- `FindItemByID`
- `SetWearItem`
- `UnequipItem`
- `EquipItem`
- `CanEquipNow`
- `CanUnequipNow`
- `UseItemEx`
- `AutoGiveItem`
- `AutoGiveDS`
- `CanReceiveItem`
- `ReceiveItem`
- `GiveItemFromSpecialItemGroup` output vector
- `DoRefine`
- `DoRefineWithScroll`
- `DoRefineItemSoul`
- `RefineItem`

Recommended next split:
- `15E-3b`: item lookup APIs (`FindSpecifyItem`, `FindItemByID`, `AutoGiveItem` return path).
- `15E-3c`: equip/unequip APIs.
- `15E-3d`: give/receive APIs.
- `15E-3e`: refine APIs.

## 2026-04-26 - Phase 15E-3b: ItemSystem lookup API cleanup

Scope:
- Continue the narrow public `ItemSystem.hpp` API cleanup after 15E-3a.
- Convert only lookup-only APIs away from public `LPITEM` return values.
- Preserve storage, packet, equip/unequip, refine, give/receive, DragonSoul and `points[]` behavior.

Files changed:
- `SRC/Server/GameServer/ecs/systems/ItemSystem.hpp`
- `SRC/Server/GameServer/ecs/systems/ItemSystem.cpp`

APIs migrated:
```cpp
LPITEM FindSpecifyItem(entt::entity e, uint32_t vnum);
LPITEM FindItemByID(entt::entity e, uint32_t id);
```

New signatures:
```cpp
entt::entity FindSpecifyItem(entt::entity e, uint32_t vnum);
entt::entity FindItemByID(entt::entity e, uint32_t id);
```

Mapping used:
- Legacy lookup still happens internally in `ItemSystem.cpp` through the existing `CHARACTER` methods:
```cpp
ch->FindSpecifyItem(...)
ch->FindItemByID(...)
```
- The resolved `LPITEM` is converted to an ECS item entity with the existing item mapping pattern:
```cpp
EntityFactory::CreateItemEntity(g_registry, item)
```
- Missing character/item cases return `entt::null`.

Call sites:
- Tree scan found no external `ItemSystem::FindSpecifyItem(...)` or `ItemSystem::FindItemByID(...)` call sites.
- No caller migration was needed.
- Existing legacy `CHARACTER::FindSpecifyItem` / `CHARACTER::FindItemByID` usages remain untouched and are outside this narrow API cleanup.

Validation:
- Build gate passed:
```powershell
cmake --build build --config RelWithDebInfo --target GameServer --parallel 8
```
- Header check confirmed the two migrated declarations no longer expose `LPITEM`.
- Implementation check confirmed both functions now return `entt::entity` and use `EntityFactory::CreateItemEntity(g_registry, item)`.

Explicitly not touched:
- Inventory storage layout.
- Extra inventory layout.
- Main inventory layout.
- DragonSoul.
- Packets.
- Equip / unequip.
- Refine.
- Give / receive.
- `points[]` / stat migration.

Remaining known `LPITEM` public API leaks in `ItemSystem.hpp` after 15E-3b:
- `SetWearItem`
- `UnequipItem`
- `EquipItem`
- `CanEquipNow`
- `CanUnequipNow`
- `UseItemEx`
- `AutoGiveItem`
- `AutoGiveDS`
- `CanReceiveItem`
- `ReceiveItem`
- `GiveItemFromSpecialItemGroup` output vector
- `DoRefine`
- `DoRefineWithScroll`
- `DoRefineItemSoul`
- `RefineItem`

Recommended next split:
- `15E-3c`: equip/unequip APIs.
- `15E-3d`: give/receive APIs.
- `15E-3e`: refine APIs.

Process note:
- From this point forward, every completed migration/work round should update `docs/ecs_migration/MIGRATION_LOG_GPT.md` automatically so the user does not need to request it each time.

## 2026-04-26 - Phase 15E-3c-pre: ItemSystem equip/unequip API audit

Mode:
- Audit only.
- No code changes were made to `ItemSystem`.
- No commit was made.

Target public APIs audited:
```cpp
void SetWearItem(entt::entity e, uint8_t wearPos, LPITEM item);
bool UnequipItem(entt::entity e, LPITEM item);
bool EquipItem(entt::entity e, LPITEM item, int candidateCell = -1);
bool CanEquipNow(entt::entity e, const LPITEM item, const TItemPos& srcCell, const TItemPos& destCell);
bool CanUnequipNow(entt::entity e, const LPITEM item, const TItemPos& srcCell, const TItemPos& destCell);
```

Definitions:
- `SRC/Server/GameServer/ecs/systems/ItemSystem.hpp:32-39`
- `SRC/Server/GameServer/ecs/systems/ItemSystem.cpp:755-801`

Current implementation shape:
- Public `ItemSystem` APIs take owner as `entt::entity`.
- Item argument is still public `LPITEM`.
- Implementation immediately resolves owner with `LegacyCharOf(e)` and delegates to legacy `CHARACTER` methods:
```cpp
ch->SetWear(...)
ch->UnequipItem(...)
ch->EquipItem(...)
ch->CanEquipNow(...)
ch->CanUnequipNow(...)
```

Direct `ItemSystem::...` call sites:
- No external direct call sites were found for:
  - `ItemSystem::SetWearItem`
  - `ItemSystem::UnequipItem`
  - `ItemSystem::EquipItem`
  - `ItemSystem::CanEquipNow`
  - `ItemSystem::CanUnequipNow`
- This means changing the public `ItemSystem` signatures should not require broad external caller edits today.

Legacy wrapper / internal call sites found:
- `SRC/Server/GameServer/ecs/systems/ItemSystem.cpp:3017` - `CHARACTER::UnequipItem(LPITEM item)`
- `SRC/Server/GameServer/ecs/systems/ItemSystem.cpp:3088` - `CHARACTER::EquipItem(LPITEM item, int iCandidateCell)`
- `SRC/Server/GameServer/ecs/systems/ItemSystem.cpp:3442` - `CHARACTER::CanEquipNow(const LPITEM item, ...)`
- `SRC/Server/GameServer/ecs/systems/ItemSystem.cpp:3602` - `CHARACTER::CanUnequipNow(const LPITEM item, ...)`
- `SRC/Server/GameServer/ecs/systems/ItemSystem.cpp:4106-4149` - item move path uses `CanUnequipNow`, `UnequipItem`, `EquipItem`
- `SRC/Server/GameServer/ecs/systems/ItemSystem.cpp:6201-6241` - item use path toggles `EquipItem` / `UnequipItem`
- `SRC/Server/GameServer/ecs/systems/ItemSystem.cpp:11191` - totem use path calls `EquipItem`
- `SRC/Server/GameServer/ecs/systems/ItemSystem.cpp:15485` - swap/equipment validation uses both `CanUnequipNow` and `CanEquipNow`
- `SRC/Server/GameServer/ecs/systems/CombatSystem.cpp:5907` - arrow replacement path calls legacy `EquipItem(pkNewArrow)`
- `SRC/Server/GameServer/cmd_general.cpp:259` - mount command unequips costume mount through legacy `ch->UnequipItem(pkItem)`
- `SRC/Server/GameServer/cmd_gm.cpp:4310,4314` - GM job/equipment flow unequips worn items through legacy `ch->UnequipItem(item)`
- `SRC/Server/GameServer/questlua_pc.cpp:4571` - quest Lua equip slot path calls legacy `ch->EquipItem(item)`
- `SRC/Server/GameServer/questlua_pc.cpp:4580` - quest Lua unequip slot path calls legacy `ch->UnequipItem(item)`

Call-site classification:
- Already has item `entt::entity` available: none found for the target public `ItemSystem` APIs.
- Only has `LPITEM` available: all current practical equip/unequip paths.
- Called from legacy `CHARACTER` wrapper: yes, most important paths live inside `CHARACTER::EquipItem`, `CHARACTER::UnequipItem`, `CHARACTER::CanEquipNow`, `CHARACTER::CanUnequipNow`.
- Called from packet/input path: yes, the item move/use logic inside `ItemSystem.cpp` eventually calls legacy equip/unequip.
- Called from inventory/equipment sync path: yes, equip/unequip mutates item location, wear slots, inventory slots, and client item packets indirectly through `RemoveFromCharacter`, `AddToCharacter`, `SetItem`, `EquipTo`, and related legacy item methods.

Item entity mapping status:
- Safe `LPITEM -> entt::entity` mapping exists and is already used by 15E-3a/15E-3b:
```cpp
EntityFactory::CreateItemEntity(g_registry, item)
```
- Existing item identity component:
```cpp
ecs::ItemIdentity { id, vnum, vid, maskVnum }
```
- Existing reverse lookup exists as implementation-local code in `InventorySystem.cpp`:
```cpp
LPITEM LegacyItemOf(entt::entity e)
{
    auto* id = g_registry.try_get<ecs::ItemIdentity>(e);
    if (!id)
        return nullptr;
    return ITEM_MANAGER::instance().Find(id->id);
}
```
- This reverse helper is not currently exposed as a stable public `ItemSystem` API.

Behavior risks:
- Equipment slot mutation: HIGH. `EquipItem` / `UnequipItem` changes wear slots and inventory cells, including costume weapon/body/mount interactions.
- Packet sync: HIGH. Calls flow through `SetItem`, `AddToCharacter`, `RemoveFromCharacter`, item set/delete packets, and equipment window updates.
- Ownership transfer: MEDIUM. Normal equip/unequip should preserve owner, but stale item entity resolution must not allow cross-owner item mutation.
- Affect/stat recalculation: HIGH. Equip/unequip triggers stat recalculation, affect handling, unique item behavior, polymorph/riding checks, weapon costume constraints and max point checks.
- Persistence/save side effects: MEDIUM. Location/equipped state must continue to save correctly after move/equip/relog.
- DragonSoul/special item interaction: MEDIUM-HIGH. `CanEquipNow`, `CanUnequipNow`, and move logic include DragonSoul, belt, switchbot and special DS cases.

Audit conclusion:
- Safe to proceed with a very small implementation batch, but not safe to blindly convert all equip/unequip behavior in one pass.
- The absence of external `ItemSystem::EquipItem` call sites makes the public signature cleanup mechanically small.
- The runtime risk is still high because these APIs delegate to legacy mutation-heavy methods.

Recommended smallest safe 15E-3c implementation batch:
1. Add a `.cpp`-local resolver in `ItemSystem.cpp` only:
```cpp
static LPITEM LegacyItemOf(entt::entity itemEntity)
```
using `ecs::ItemIdentity.id` and `ITEM_MANAGER::instance().Find(id)`.
2. Change only the public `ItemSystem` signatures to item-entity arguments:
```cpp
void SetWearItem(entt::entity owner, uint8_t wearPos, entt::entity item);
bool UnequipItem(entt::entity owner, entt::entity item);
bool EquipItem(entt::entity owner, entt::entity item, int candidateCell = -1);
bool CanEquipNow(entt::entity owner, entt::entity item, const TItemPos& srcCell, const TItemPos& destCell);
bool CanUnequipNow(entt::entity owner, entt::entity item, const TItemPos& srcCell, const TItemPos& destCell);
```
3. Keep legacy mutation behavior unchanged internally:
```cpp
auto* ch = LegacyCharOf(owner);
LPITEM legacyItem = LegacyItemOf(item);
return ch && legacyItem ? ch->EquipItem(legacyItem, candidateCell) : false;
```
4. Do not route legacy `CHARACTER::EquipItem` internals through the public ECS API yet.
5. Do not modify packet paths, storage layout, `SetItem`, `AddToCharacter`, `RemoveFromCharacter`, stats, affects, DragonSoul or `points[]`.
6. Build and then run focused WinTest equip/unequip regression.

Recommended manual test for 15E-3c implementation:
- Login with weapon, armor, costume body, costume weapon and mount costume equipped.
- Open inventory/equipment window and verify equipped items appear in equipment cells.
- Unequip/equip weapon repeatedly.
- Unequip/equip armor repeatedly.
- Unequip/equip costume body and costume weapon.
- Use item double-click equip/unequip path.
- Move item from inventory to equipment slot.
- Relog and verify equipment persists.
- Verify no duplicated item, missing item, wrong inventory cell, wrong equipment cell, stat mismatch, or delayed damage regression.

Proceed recommendation:
- Safe to proceed only with the narrow public signature cleanup described above.
- Not safe to migrate legacy `CHARACTER::EquipItem` / `UnequipItem` implementation semantics yet.
- Not safe to alter storage, packet sync, stat/affect recalculation, or item move behavior in this phase.

## 2026-04-26 - Phase 15E-3c: ItemSystem equip/unequip public signature cleanup

Scope:
- Code change, narrow scope.
- Remove `LPITEM` exposure from the public equip/unequip subset in `ItemSystem.hpp`.
- Preserve legacy equip/unequip behavior exactly.
- Do not migrate `CHARACTER::EquipItem` / `CHARACTER::UnequipItem` semantics.

Files changed:
- `SRC/Server/GameServer/ecs/systems/ItemSystem.hpp`
- `SRC/Server/GameServer/ecs/systems/ItemSystem.cpp`

Old public signatures:
```cpp
void SetWearItem(entt::entity e, uint8_t wearPos, LPITEM item);
bool UnequipItem(entt::entity e, LPITEM item);
bool EquipItem(entt::entity e, LPITEM item, int candidateCell = -1);
bool CanEquipNow(entt::entity e, const LPITEM item, const TItemPos& srcCell, const TItemPos& destCell);
bool CanUnequipNow(entt::entity e, const LPITEM item, const TItemPos& srcCell, const TItemPos& destCell);
```

New public signatures:
```cpp
void SetWearItem(entt::entity e, uint8_t wearPos, entt::entity item);
bool UnequipItem(entt::entity e, entt::entity item);
bool EquipItem(entt::entity e, entt::entity item, int candidateCell = -1);
bool CanEquipNow(entt::entity e, entt::entity item, const TItemPos& srcCell, const TItemPos& destCell);
bool CanUnequipNow(entt::entity e, entt::entity item, const TItemPos& srcCell, const TItemPos& destCell);
```

Implementation detail:
- Added a `.cpp`-local resolver in `ItemSystem.cpp`:
```cpp
static LPITEM LegacyItemOf(entt::entity itemEntity)
{
    if (itemEntity == entt::null || !g_registry.valid(itemEntity))
        return nullptr;

    const auto* identity = g_registry.try_get<ecs::ItemIdentity>(itemEntity);
    if (!identity || identity->id == 0)
        return nullptr;

    return ITEM_MANAGER::instance().Find(identity->id);
}
```
- Each public API now resolves:
  - owner: `LegacyCharOf(e)`
  - item: `LegacyItemOf(item)`
- After resolving, each function delegates to the same legacy `CHARACTER` method as before:
```cpp
ch->SetWear(...)
ch->UnequipItem(...)
ch->EquipItem(...)
ch->CanEquipNow(...)
ch->CanUnequipNow(...)
```

Call sites:
- Search found no direct external call sites for:
  - `ItemSystem::SetWearItem`
  - `ItemSystem::UnequipItem`
  - `ItemSystem::EquipItem`
  - `ItemSystem::CanEquipNow`
  - `ItemSystem::CanUnequipNow`
- Therefore no call-site migration was required.

Validation:
- Build gate passed:
```powershell
cmake --build build --config RelWithDebInfo --target GameServer --parallel 8
```
- Build produced existing warning-class output only; `GameServer.exe` linked successfully.
- Header check confirmed the five target APIs no longer expose `LPITEM`.
- Direct `ItemSystem::...` call-site scan returned no matches.

Remaining known `LPITEM` public API leaks in `ItemSystem.hpp` after 15E-3c:
- `UseItemEx`
- `AutoGiveItem(entt::entity e, LPITEM item, ...)`
- `AutoGiveDS`
- `AutoGiveItem(...)` return value
- `CanReceiveItem`
- `ReceiveItem`
- `GiveItemFromSpecialItemGroup` output vector
- `DoRefine`
- `DoRefineWithScroll`
- `DoRefineItemSoul`
- `RefineItem`

Explicitly not touched:
- `CHARACTER::EquipItem` / `CHARACTER::UnequipItem` internals.
- Legacy item move/equip packet paths.
- `SetItem`, `AddToCharacter`, `RemoveFromCharacter`.
- Inventory storage layout.
- Equipment slot semantics.
- Stat / affect recalculation.
- DragonSoul.
- `points[]`.

Manual WinTest checklist:
- Login with equipped weapon, armor, costume body, costume weapon and mount costume.
- Verify equipment window shows equipped items in equipment cells.
- Unequip/equip weapon repeatedly.
- Unequip/equip armor repeatedly.
- Unequip/equip costume body and costume weapon.
- Double-click item use equip/unequip path.
- Move item from inventory to equipment slot.
- Relog and verify equipped items persist.
- Verify no duplicated item, missing item, wrong inventory cell, wrong equipment cell, stat mismatch or delayed damage regression.

Commit status:
- Not committed yet. User requested review before commit.

## Phase 15E-34 - ECS item mutation engine (count + basic socket extraction)

Date: 2026-04-27

Mode:
- First real engine migration phase.
- Code change, narrow safe subset.
- Not committed yet.

Goal:
- Start replacing simple `LPITEM` count mutation paths with ECS-controlled item mutation APIs.
- Keep legacy as mirror/sync layer only for the migrated simple consume paths.
- Avoid DragonSoul, pet, refine, timed socket, packet, inventory layout, and `points[]` changes.

Files changed:
- `SRC/Server/GameServer/ecs/systems/ItemSystem.hpp`
- `SRC/Server/GameServer/ecs/systems/ItemSystem.cpp`
- `SRC/Server/GameServer/ecs/systems/ItemSystem_LegacyBridge.cpp`
- `docs/ecs_migration/MIGRATION_LOG_GPT.md`

New ECS mutation APIs:
```cpp
bool SetItemCountEcs(entt::entity item, uint32_t count);
bool AddItemCountEcs(entt::entity item, int delta);
bool ConsumeItemEcs(entt::entity item, uint32_t amount = 1);
bool DestroyItemEntityEcs(entt::entity item, const char* reason = nullptr);
bool SetItemSocketEcs(entt::entity item, int index, uint32_t value);
```

Implementation notes:
- `SetItemCountEcs(...)` validates the item entity and delegates to the existing ECS count writer.
- `AddItemCountEcs(...)` reads ECS count, applies the delta, and destroys the item through the centralized entity+legacy destruction path if the result is zero or below.
- `ConsumeItemEcs(...)` is the explicit ECS-facing consume API for simple count decrement flows.
- `DestroyItemEntityEcs(...)` wraps the existing centralized entity+legacy destruction helper.
- `SetItemSocketEcs(...)` is available as an explicit ECS socket mutation API, but no socket call sites were migrated in this phase.

Safe count paths migrated:
- Simple itemshop/reward token consume path.
- Auto metin farm buff item consume path.
- `ITEM_NOG_POCKET` consume path.
- `ITEM_RAMADAN_CANDY` consume path.

Skipped / reverted paths:
- New pet duration item path (`vnum == 55001`) was intentionally kept on legacy `item->SetCount(item->GetCount() - 1)` because it touches pet DB/state semantics.
- DragonSoul, refine, pet socket, timed socket, and special mutation-heavy branches were not migrated.
- Socket writes were not migrated because the found call sites are not part of the safe subset.

Counts:
- `ConsumeItemEcs` call sites in `ItemSystem_LegacyBridge.cpp`: 4.
- Remaining direct `item->SetCount(item->GetCount() - 1)` style call sites in `ItemSystem_LegacyBridge.cpp`: 96.

Validation:
- Build gate passed:
```powershell
cmake --build build --config RelWithDebInfo --target GameServer --parallel 8
```
- `GameServer.exe` linked successfully.
- Existing warning-class output remains present.
- Build output still prints the known post-step environment message:
```text
'pwsh.exe' is not recognized as an internal or external command
```
  but the build command returned exit code `0`.

Header scan:
```powershell
Select-String -Path 'SRC/Server/GameServer/ecs/systems/ItemSystem.hpp' -Pattern 'LPITEM|LPCHARACTER|CHARACTER\s*\*'
```
Result: no matches.

Runtime-sensitive manual WinTest checklist:
- Use itemshop/reward token path if reachable.
- Use auto metin farm buff item.
- Use `ITEM_NOG_POCKET`.
- Use `ITEM_RAMADAN_CANDY`.
- Verify count decrements correctly.
- Verify item disappears at zero count.
- Relog and verify no duplicate/missing item.
- Verify no wrong owner/window/cell.
- Verify no delayed damage/metin collapse regression.

Commit status:
- Not committed yet. User requested review before commit.

## Phase 15E-33 - Legacy item engine extraction blocker plan

Date: 2026-04-27

Mode:
- Audit and design only.
- No gameplay code changes in this slice.

Scope:
- Audited the remaining LPITEM-based mutation engine blockers.
- Classified mutation groups in `ItemSystem_LegacyBridge.cpp`, `DragonSoul.cpp`, and `ItemSystem.cpp`.
- Wrote the extraction design and migration order to:
  - `docs/ecs_migration/phase15e_33_legacy_item_engine_extraction.md`

Mutation audit summary:
```text
ItemSystem_LegacyBridge.cpp:
  SetCount:             133
  SetSocket:             83
  RemoveItem:            37
  AddToCharacter:        35
  SetForceAttribute:     24
  RemoveFromCharacter:   15
  CreateItem:            14
  Refine paths:          11
  SetAttribute:           6
  EquipTo:                3

DragonSoul.cpp:
  Refine paths:          12
  SetCount:              11
  RemoveFromCharacter:    8
  CreateItem:             7
  SetForceAttribute:      4
  SetSocket:              4
  AddToCharacter:         2

ItemSystem.cpp:
  Refine wrappers:       10
  SetSocket:              3
  RemoveItem:             2
  RemoveFromCharacter:    1
  SetCount:               1
  CreateItem:             1
  AddToCharacter:         1
```

Existing ECS mutation APIs confirmed:
- `SetItemCount`
- `ConsumeItem`
- `SetItemSocket`
- `SetItemAttribute`
- `ClearItemAttribute`
- `PlaceItemEcs`
- `RemoveItemEcs`
- `EquipItemEcs`
- `UnequipItemEcs`
- `UseItemEcs`
- `ReceiveItemEcs`
- `RefineItemEcs`

Design output:
- Count/consume should migrate first, using ECS `ItemCount` as primary authority.
- Socket mutation should migrate only after excluding timed unique, DragonSoul, pet, and refine sockets.
- Attribute mutation needs a force-attribute ECS API before pet/refine/costume paths can move.
- Placement/location extraction must wait until owner/location/grid updates are ECS-primary.
- Equip extraction is blocked by affect/stat recalculation side effects.
- Refine and DragonSoul internals must remain legacy until material consumption, result creation, socket/attribute transfer, and logs are reproduced exactly.

Recommended next implementation order:
1. Freeze new direct `LPITEM` mutation outside core/bridge.
2. Move safe non-DS/non-refine count decrements to `ConsumeItemEcs`.
3. Move isolated socket writes to `SetItemSocketEcs`.
4. Add `SetItemForceAttributeEcs`, then migrate quest current item force-attribute paths.
5. Extract normal AutoGive/inventory placement.
6. Extract equip only after stat/affect recalculation API exists.
7. Convert all refine/DS callers to wrappers, then rewrite internals one algorithm at a time.
8. Attempt global `LPITEM` typedef deletion only after the above.

Global LPITEM deletion status:
- Not safe.
- Remaining blockers:
  - `ItemSystem_LegacyBridge.cpp`
  - `DragonSoul.cpp`
  - refine internals
  - exchange / safebox / offlineshop / shopEx transaction engines
  - switchbot
  - quest current item legacy bridge
  - item manager and CItem core
  - packet-sensitive inventory mutation paths

Validation:
- Build gate passed:
```powershell
cmake --build build --config RelWithDebInfo --target GameServer --parallel 8
```
- `GameServer.exe` linked successfully.

Commit status:
- Not committed yet. User requested review before commit.

## Phase 15E-32 - Final non-core LPITEM cleanup before delete attempt

Date: 2026-04-27

Scope:
- Removed the remaining safe `input_db.cpp` direct item lookup from the character item load duplicate-purge path.
- Added ECS-facing helpers for load-time duplicate cleanup without exposing `LPITEM` in public ECS headers.
- Audited switchbot, questmanager, exchange, safebox, offlineshop, and shop-related LPITEM usage.
- Did not rewrite DragonSoul/refine internals, exchange/storage transaction engines, switchbot mutation logic, quest legacy current item bridge, packet formats, inventory layout, or `points[]`.

Files changed:
- `SRC/Server/GameServer/ecs/systems/ItemSystem.hpp`
- `SRC/Server/GameServer/ecs/systems/ItemSystem.cpp`
- `SRC/Server/GameServer/input_db.cpp`
- `docs/ecs_migration/MIGRATION_LOG_GPT.md`

New ItemSystem APIs:
```cpp
uint32_t ItemSystem::GetItemLastOwnerPID(entt::entity item);
bool ItemSystem::DestroyLoadedDuplicateItem(entt::entity item);
```

Implementation details:
- `GetItemLastOwnerPID(...)` reads `ecs::ItemOwner::lastOwnerPID`.
- `DestroyLoadedDuplicateItem(...)` preserves the legacy load-time duplicate purge semantics:
  - resolves legacy item inside `ItemSystem.cpp` only.
  - calls `SetSkipSave(true)`.
  - unregisters/destroys the ECS item entity via `EntityFactory::DestroyItemEntity(...)`.
  - calls `M2_DESTROY_ITEM(...)`.
- `input_db.cpp` item load duplicate purge now uses:
```cpp
const entt::entity staleItem = ItemSystem::FindItemByID(p->id);
ItemSystem::GetItemOwner(staleItem);
ItemSystem::GetItemLastOwnerPID(staleItem);
ItemSystem::DestroyLoadedDuplicateItem(staleItem);
```

Target audit results:
```text
input_db.cpp:                 LPITEM 4 -> 3, Find 1 -> 0, FindByVID 0 -> 0
new_switchbot.cpp:            LPITEM 9 -> 9, Find 1 -> 1, FindByVID 0 -> 0
exchange.cpp:                 LPITEM 8 -> 8, Find 0 -> 0, FindByVID 0 -> 0
safebox.cpp:                  LPITEM 7 -> 7, Find 0 -> 0, FindByVID 0 -> 0
new_offlineshop.cpp:          LPITEM 6 -> 6, Find 0 -> 0, FindByVID 0 -> 0
new_offlineshop_manager.cpp:  LPITEM 6 -> 6, Find 0 -> 0, FindByVID 0 -> 0
shopEx.cpp:                   LPITEM 1 -> 1, Find 0 -> 0, FindByVID 0 -> 0
shop.cpp:                     LPITEM 6 -> 6, Find 0 -> 0, FindByVID 0 -> 0
questmanager.cpp:             LPITEM 5 -> 5, Find 1 -> 1, FindByVID 0 -> 0
```

Whole-tree counts:
```text
LPITEM total:                 989 -> 989
Direct ITEM_MANAGER Find:      16 -> 15
Direct ITEM_MANAGER FindByVID:  6 -> 6
```
- `LPITEM` total did not decrease because this pass added a controlled bridge helper in `ItemSystem.cpp` while removing `input_db.cpp` direct usage.
- This is still a dependency improvement: `input_db.cpp` no longer reaches into `ITEM_MANAGER::Find` for duplicate cleanup.

Remaining blockers:
- `new_switchbot.cpp`:
  - Requires item mutation/attribute reroll audit; lookup immediately enters switchbot item engine.
  - Needs dedicated `SwitchbotItemEcs` wrapper.
- `questmanager.cpp`:
  - `SetCurrentItem(entt::entity)` still bridges to legacy `LPITEM` quest item pointer for remaining old quest flows.
  - Needs full quest current item pointer deletion before safe removal.
- `exchange.cpp`, `safebox.cpp`, `new_offlineshop*.cpp`, `shopEx.cpp`:
  - No direct `Find`/`FindByVID` blockers, but LPITEM is still transaction/storage engine state.
  - Needs dedicated storage/transaction ECS model, not grep-level replacement.
- `ItemSystem.cpp`, `ItemSystem_LegacyBridge.cpp`, `InventorySystem.cpp`:
  - Remaining direct lookups are ECS/legacy bridge internals.
- `DragonSoul.cpp`, `fishing.cpp`, `PetSystem.cpp`, `New_PetSystem.cpp`, `MountSystem.cpp`:
  - Remaining direct `Find(id)` calls are private entity-to-legacy bridge helpers or legacy side-effect engines.

Global LPITEM deletion status:
- Not safe yet.
- Major blockers remain:
  - `ItemSystem_LegacyBridge.cpp`
  - `DragonSoul.cpp`
  - `questlua_pc.cpp`
  - `PlayerRuntimeSystem.cpp`
  - `CombatSystem.cpp`
  - `char.h`
  - transaction/storage systems
  - switchbot/refine/fishing/mining legacy engines

Validation:
- Build gate passed:
```powershell
cmake --build build --config RelWithDebInfo --target GameServer --parallel 8
```
- `GameServer.exe` linked successfully.
- Public ECS header leak scan:
```powershell
Select-String -Path 'SRC/Server/GameServer/ecs/systems/ItemSystem.hpp' -Pattern 'LPITEM|LPCHARACTER|CHARACTER\s*\*'
```
  Result: no matches.
- Existing warning-class output remains present.
- Build output still prints the known post-step environment message:
```text
'pwsh.exe' is not recognized as an internal or external command
```
  but the build command returned exit code `0`.

Manual WinTest checklist:
- login with character containing many saved items
- duplicate/stale item load smoke if reproducible
- inventory open/relog
- extra inventory open/relog
- switchbot smoke test
- exchange/trade smoke test
- safebox/mall put/get
- offline shop open/close if available
- item use/equip/unequip
- no duplicate/missing item
- no wrong owner/window/cell
- no delayed damage/metin collapse regression

Commit status:
- Not committed yet. User requested review before commit.

## Phase 15E-31 - Final LPITEM bridge collapse (logging/shop safe slice)

Date: 2026-04-27

Scope:
- Collapsed safe non-core LPITEM bridge usage in logging and PC-shop validation paths.
- Added an entity-based item logging overload while keeping the legacy overload for compatibility.
- Did not rewrite exchange/safebox/offlineshop/refine/DragonSoul/core item engines.
- Did not change packet formats, inventory layout, item creation/removal semantics, or `points[]`.

Files changed:
- `SRC/Server/GameServer/log.h`
- `SRC/Server/GameServer/log.cpp`
- `SRC/Server/GameServer/questlua_global.cpp`
- `SRC/Server/GameServer/shop.cpp`
- `docs/ecs_migration/MIGRATION_LOG_GPT.md`

New logging API:
```cpp
void LogManager::ItemLogEntity(
    LPCHARACTER ch,
    entt::entity item,
    const char* text,
    const char* hint);
```
- Uses:
  - `ItemSystem::IsValidItem`
  - `ItemSystem::GetItemID`
  - `ItemSystem::GetItemOriginalVnum`
- Legacy `ItemLog(LPCHARACTER, LPITEM, ...)` remains for old item/refine/DragonSoul/exchange engines.

Call-site migration:
- `questlua_global.cpp` `_item_log`:
  - Before: `ITEM_MANAGER::instance().Find(dwItemID)` + `ItemLog(ch, LPITEM, ...)`
  - After: `ItemSystem::FindItemByID(dwItemID)` + `ItemLogEntity(ch, itemEntity, ...)`
- `shop.cpp` PC-shop validation:
  - Before: `ITEM_MANAGER::instance().Find(r_item.itemid)` + `pkSelectedItem->GetOwner()`
  - After: `ItemSystem::FindItemByID(r_item.itemid)` + `ItemSystem::GetItemOwner(...)`

Validation counts:
- Direct `ITEM_MANAGER::instance().Find(`:
```text
Before 15E-31: 18
After 15E-31:  16
```
- Direct `ITEM_MANAGER::instance().FindByVID(`:
```text
Before 15E-31: 6
After 15E-31:  6
```
- `questlua_global.cpp` and `shop.cpp` no longer contain direct `ITEM_MANAGER::instance().Find(` lookups.

Remaining blockers:
- `input_db.cpp` duplicate purge:
  - Still uses legacy `SetSkipSave(true)` + `M2_DESTROY_ITEM(staleItem)`.
  - Needs a load-time duplicate cleanup wrapper preserving no-save destruction semantics.
- `new_switchbot.cpp`:
  - Uses `ITEM_MANAGER::Instance().Find(item_id)` and likely requires switchbot-specific item mutation audit.
- `questmanager.cpp`:
  - Current item entity setter still bridges to legacy item pointer for remaining quest engine paths.
- `InventorySystem.cpp`, `ItemSystem.cpp`, `ItemSystem_LegacyBridge.cpp`:
  - Remaining lookups are bridge/core internals.
- `DragonSoul.cpp`, `fishing.cpp`, `PetSystem.cpp`, `New_PetSystem.cpp`, `MountSystem.cpp`:
  - Remaining direct `Find(id)` calls are private entity-to-legacy bridge helpers or legacy side-effect engines.
- `exchange`, `safebox`, `offlineshop`, `shopEx`:
  - Still LPITEM-heavy by design and require dedicated transaction/storage migration phases.

Validation:
- Build gate passed:
```powershell
cmake --build build --config RelWithDebInfo --target GameServer --parallel 8
```
- `GameServer.exe` linked successfully.
- Existing warning-class output remains present.
- Build output still prints the known post-step environment message:
```text
'pwsh.exe' is not recognized as an internal or external command
```
  but the build command returned exit code `0`.

Manual WinTest checklist:
- quest `item_log` script path if available
- PC shop buy from player shop
- NPC shop buy/sell smoke
- inventory open/relog
- item use/equip/unequip
- no duplicate/missing item
- no wrong owner/window/cell
- no delayed damage/metin collapse regression

Commit status:
- Not committed yet. User requested review before commit.

## Phase 15E-30 - Pet and Mount ECS migration

Date: 2026-04-27

Scope:
- Migrated pet/mount summon-item lookup and ownership validation away from direct `ITEM_MANAGER::FindByVID`.
- Added pointer-free ECS pet/mount item state components.
- Kept legacy `LPITEM` engines where required by `Summon(...)`, `ModifyPoints(...)`, DB persistence, and legacy affect/stat side effects.
- Did not change packet formats, inventory layout, DragonSoul/refine logic, pet DB semantics, or `points[]`.

Files changed:
- `SRC/Server/GameServer/MountSystem.cpp`
- `SRC/Server/GameServer/PetSystem.cpp`
- `SRC/Server/GameServer/New_PetSystem.cpp`
- `SRC/Server/GameServer/ecs/components/pet_mount_components.hpp`
- `docs/ecs_migration/MIGRATION_LOG_GPT.md`

New ECS components:
```cpp
ecs::PetComponent
ecs::MountComponent
```
- Store:
  - owner entity
  - summon item ID
  - summon item VID
  - summon item vnum
  - socket snapshot
  - level/state fields
- These components are synchronized when summon item state is set.
- They are intentionally passive state mirrors in this pass; gameplay authority remains legacy until the pet/mount engines are extracted.

Lookup changes:
- `MountSystem.cpp`, `PetSystem.cpp`, and `New_PetSystem.cpp` now use:
```cpp
ItemSystem::FindItemByVID(vid)
ItemSystem::GetItemOwner(item)
ItemSystem::GetItemID(item)
```
- Ownership validation changed from:
```cpp
ITEM_MANAGER::instance().FindByVID(vid)->GetOwner()
```
  to entity ownership validation through `ItemSystem::GetItemOwner(...)`.
- Direct `FindByVID` use in the three target files was removed.

Mutation changes:
- Safe summon-item socket writes changed from direct `item->SetSocket(...)` to:
```cpp
ItemSystem::SetItemSocket(EntityFactory::CreateItemEntity(g_registry, item), socket, value);
```
- Legacy `Lock(...)`, `ModifyPoints(...)`, `SetForceAttribute(...)`, DB persistence, and summon methods remain legacy-owned.

Private bridge policy:
- Each target file has a private `LegacyItemFromEntity(...)` bridge for code paths that still require `LPITEM`.
- This bridge resolves legacy by item ID only after the ECS lookup succeeds.
- No public ECS header exposes `LPITEM`.

Audit results:
- Target files direct `ITEM_MANAGER::instance().FindByVID(` count:
```text
Before 15E-30:
  MountSystem.cpp:    3
  PetSystem.cpp:      5
  New_PetSystem.cpp: 13
After 15E-30:
  MountSystem.cpp:    0
  PetSystem.cpp:      0
  New_PetSystem.cpp:  0
```
- Whole GameServer direct `FindByVID` count:
```text
Before 15E-30: 27
After 15E-30:   6
```
- Remaining direct `FindByVID` calls:
  - `ItemSystem_LegacyBridge.cpp`: 5
  - `ItemSystem.cpp`: 1
  - These are legacy bridge internals, not pet/mount subsystem callers.
- Whole GameServer direct `ITEM_MANAGER::instance().Find(` count after this pass: `18`.
  - The increase is from private entity-to-legacy bridge helpers needed by existing `LPITEM` engines.

Skipped / still legacy-owned:
- `PetSystem.cpp`:
  - `ModifyPoints(true)` still requires `LPITEM`.
  - `Lock(...)` remains direct legacy side effect.
- `New_PetSystem.cpp`:
  - `SetForceAttribute(...)` remains legacy-owned.
  - DB save/update paths still use legacy item ID semantics.
  - item feeding/removal semantics remain legacy-owned.
- `MountSystem.cpp`:
  - `Summon(...)` and mount skin refresh still require `LPITEM`.
  - `Lock(...)` and mount engine internals remain legacy-owned.

Validation:
- Build gate passed after lookup/mutation migration:
```powershell
cmake --build build --config RelWithDebInfo --target GameServer --parallel 8
```
- Build gate passed again after adding `PetComponent` / `MountComponent`.
- `GameServer.exe` linked successfully.
- Public ECS header leak scan:
```powershell
Select-String -Path 'SRC/Server/GameServer/ecs/systems/ItemSystem.hpp','SRC/Server/GameServer/ecs/components/pet_mount_components.hpp' -Pattern 'LPITEM|LPCHARACTER|CHARACTER\s*\*'
```
  Result: no matches.
- Existing warning-class output remains present.
- Build output still prints the known post-step environment message:
```text
'pwsh.exe' is not recognized as an internal or external command
```
  but the build command returned exit code `0`.

Manual WinTest checklist:
- normal pet summon
- normal pet unsummon
- pet buff apply/remove
- pet skin/name refresh if available
- new pet summon/unsummon
- new pet duration/level/evolution persistence smoke
- mount summon
- mount unsummon
- mount costume skin refresh
- owner validation: summon item removed/moved should unsummon correctly
- inventory open/relog
- no duplicate/missing summon item
- no wrong owner/window/cell
- no delayed damage/metin collapse regression

Commit status:
- Not committed yet. User requested review before commit.

## Phase 15E-29 - ECS item lookup authority

Date: 2026-04-27

Scope:
- Introduced ECS-facing item lookup authority for item ID and item VID.
- Kept `ITEM_MANAGER::Find` / `FindByVID` as transition fallback only where legacy `CItem*` is still required.
- Did not remove `ITEM_MANAGER` maps, did not change item lifecycle semantics, did not touch packets, inventory layout, DragonSoul/refine internals, pet systems, or `points[]`.

Files changed:
- `SRC/Server/GameServer/ecs/ItemRegistry.hpp`
- `SRC/Server/GameServer/ecs/ItemRegistry.cpp`
- `SRC/Server/GameServer/ecs/EntityFactory.cpp`
- `SRC/Server/GameServer/ecs/systems/ItemSystem.hpp`
- `SRC/Server/GameServer/ecs/systems/ItemSystem.cpp`
- `docs/ecs_migration/MIGRATION_LOG_GPT.md`

New public ItemSystem lookup APIs:
```cpp
entt::entity ItemSystem::FindItemByID(uint32_t id);
entt::entity ItemSystem::FindItemByVID(uint32_t vid);
uint32_t ItemSystem::GetItemVID(entt::entity item);
```

CItemRegistry changes:
- Added ECS-side item VID index:
```cpp
entt::entity CItemRegistry::FindByVID(uint32_t itemVID) const;
```
- `CItemRegistry::Register(...)` now supports both item ID and item VID.
- `CItemRegistry::Unregister(itemID)` removes both ID and VID entries.
- `EntityFactory::CreateItemEntity(...)` now refreshes the registry entry for existing item entities as well as new item entities, so VID changes are re-indexed during sync.

Lookup behavior:
- `ItemSystem::FindItemByID(id)`:
  - returns existing ECS entity from `CItemRegistry` if present and valid.
  - falls back to `ITEM_MANAGER::Find(id)` only during transition.
  - wraps fallback legacy item with `EntityFactory::CreateItemEntity(...)`.
- `ItemSystem::FindItemByVID(vid)`:
  - returns existing ECS entity from `CItemRegistry::FindByVID`.
  - falls back to `ITEM_MANAGER::FindByVID(vid)` only during transition.
  - wraps fallback legacy item with `EntityFactory::CreateItemEntity(...)`.

Batch A audit summary:
- Initial direct `ITEM_MANAGER::instance().Find(` count: `14`.
- Initial direct `ITEM_MANAGER::instance().FindByVID(` count: `29`.
- Main categories found:
  - ECS bridge helpers: `ItemSystem.cpp`, `ItemSystem_LegacyBridge.cpp`, `InventorySystem.cpp`.
  - Private subsystem bridges: `DragonSoul.cpp`, `fishing.cpp`.
  - Legacy load/shop/log paths: `input_db.cpp`, `shop.cpp`, `questlua_global.cpp`, `questmanager.cpp`.
  - Unsafe pet/mount subsystem: `PetSystem.cpp`, `New_PetSystem.cpp`, `MountSystem.cpp`.

Replacement policy applied:
- Added ECS lookup APIs first.
- Did not blindly migrate legacy pet/mount/shop/refine/DragonSoul/internal bridge callers because those still require `LPITEM` side effects or ownership checks.
- Direct call-site migration is intentionally deferred to subsystem-specific wrappers once each caller can consume `entt::entity` without immediately converting back to `LPITEM`.

Final lookup scan:
- Final direct `ITEM_MANAGER::instance().Find(` count: `15`.
  - Increase by 1 is intentional: the new `ItemSystem::FindItemByID(id)` transition fallback contains one legacy lookup internally.
  - Remaining direct calls are bridge/unsafe legacy subsystem calls.
- Final direct `ITEM_MANAGER::instance().FindByVID(` count: `27`.
  - Remaining calls are mostly pet/mount systems and legacy bridge internals.
- Final `ItemSystem.hpp` pointer leak scan:
```powershell
Select-String -Path 'SRC/Server/GameServer/ecs/systems/ItemSystem.hpp' -Pattern 'LPITEM|LPCHARACTER|CHARACTER\s*\*'
```
  Result: no matches.

Remaining blockers by subsystem:
- `PetSystem.cpp` / `New_PetSystem.cpp`:
  - `FindByVID` is used for summon item owner validation, socket mutation, DB save IDs, and buff point modification.
  - Needs dedicated pet item ECS bridge before safe migration.
- `MountSystem.cpp`:
  - `FindByVID` is used for summon item owner validation and summon skin refresh.
  - Needs dedicated mount item ECS bridge.
- `ItemSystem.cpp` / `ItemSystem_LegacyBridge.cpp`:
  - Remaining direct lookups are bridge helpers or post-legacy mutation existence checks.
  - Needs future removal only after legacy side-effect engines are gone.
- `DragonSoul.cpp`:
  - Private DragonSoul bridge still resolves legacy item by ID.
  - Needs DS internal algorithm migration before removal.
- `fishing.cpp`:
  - `UseFishEcs` / `GrillFishEcs` wrappers still resolve legacy item internally.
  - Needs full fish-use rewrite to eliminate.
- `shop.cpp`:
  - PC shop validation still checks legacy owner pointer.
  - Needs shop/exchange/offlineshop item ownership ECS model.
- `input_db.cpp`:
  - stale duplicate purge still removes legacy item directly.
  - Needs DB item-load duplicate cleanup wrapper.
- `questlua_global.cpp`:
  - item log still passes `LPITEM` to `LogManager::ItemLog`.
  - Needs logging API overload that accepts item entity.
- `questmanager.cpp`:
  - entity current item setter still bridges to legacy quest item pointer for remaining legacy quest flows.
  - Needs full quest current item LPITEM removal.

Validation:
- Build gate passed:
```powershell
cmake --build build --config RelWithDebInfo --target GameServer --parallel 8
```
- `GameServer.exe` linked successfully.
- Existing warning-class output remains present.
- Build output still prints the known post-step environment message:
```text
'pwsh.exe' is not recognized as an internal or external command
```
  but the build command returned exit code `0`.

Manual WinTest checklist:
- item lookup by ID path
- item lookup by VID path if reachable
- quest reward and current item
- item use
- equip/unequip
- fishing use/grill
- refine smoke test
- DragonSoul smoke test
- GM item commands that inspect item IDs
- inventory open/relog
- no duplicate item
- no missing item
- no wrong owner/window/cell
- no delayed damage/metin collapse regression

Commit status:
- Not committed yet. User requested review before commit.

## Phase 15E-28 - Multi-subsystem ECS mutation bridge expansion

Date: 2026-04-27

Scope:
- Expanded ECS mutation bridge coverage across quest item, quest reward/current item, and fishing item-use paths.
- This phase intentionally focused on moving mutation entry points behind ECS APIs/wrappers, not on blind global `LPITEM` deletion.
- Legacy engines remain active behind bridge boundaries where behavior is complex.
- No packet format, inventory layout, DragonSoul internals, refine internals, polymorph/book internals, pet DB semantics, or `points[]` logic was changed.

Files changed:
- `SRC/Server/GameServer/questlua_item.cpp`
- `SRC/Server/GameServer/questlua_pc.cpp`
- `SRC/Server/GameServer/questmanager.cpp`
- `SRC/Server/GameServer/fishing.h`
- `SRC/Server/GameServer/fishing.cpp`
- `SRC/Server/GameServer/ecs/systems/ItemSystem_LegacyBridge.cpp`
- `docs/ecs_migration/MIGRATION_LOG_GPT.md`

Batch A - scan summary:
- Target files and LPITEM counts before this pass:
  - `questlua_item.cpp`: 16
  - `questlua_pc.cpp`: 51
  - `fishing.cpp`: 11
  - `cmd_gm.cpp`: 18
  - `cmd_general.cpp`: 12
  - `input_main.cpp`: 11
  - `DragonSoul.cpp`: 61
- Target files and LPITEM counts after this pass:
  - `questlua_item.cpp`: 11
  - `questlua_pc.cpp`: 51
  - `fishing.cpp`: 13
  - `cmd_gm.cpp`: 18
  - `cmd_general.cpp`: 12
  - `input_main.cpp`: 11
  - `DragonSoul.cpp`: 63
- `fishing.cpp` count increased because wrapper bridge functions intentionally resolve legacy `LPITEM` internally. This is an entry-point isolation tradeoff, not a public API regression.
- Final validation scan on 2026-04-27 confirmed the same after-pass target-file counts above. `DragonSoul.cpp` remains legacy-heavy internally; the increase is from bridge/wrapper-side implementation already isolated behind ECS entry points.

Batch B - quest item mutation wrapper expansion:
- `item_remove` now uses current item entity:
  - `GetCurrentItemEntity()`
  - `ItemSystem::GetItemOwner(...)`
  - `ItemSystem::ConsumeItem(item, ItemSystem::GetItemCount(item))`
  - `ClearCurrentItem()` remains after removal.
- `item_set_value` now uses:
  - `GetCurrentItemEntity()`
  - `ItemSystem::SetItemAttribute(...)`
- `item_get_attr0` now reads attribute tables through:
  - `ItemSystem::GetItemAttributeType(...)`
  - `ItemSystem::GetItemAttributeValue(...)`
- `item_clear_attr0` now clears attributes through:
  - `ItemSystem::ClearItemAttribute(...)`
- `item_set_attr0` now writes attribute tables through:
  - `ItemSystem::SetItemAttribute(...)`

Batch C - quest reward/current item continuation:
- `pc_give_or_drop_item_and_select` now grants through:
```cpp
ItemSystem::AutoGiveItemEcs(chEntity, dwVnum, icount);
```
- The selected current item is stored through:
```cpp
CQuestManager::Instance().SetCurrentItem(item);
```
  where `item` is `entt::entity`.
- Gold bar logging now uses `ItemSystem::GetItemID(item)`.
- Quest manager item entry points now route current item setup through entity overload:
```cpp
SetCurrentItem(EntityFactory::CreateItemEntity(g_registry, item));
```
  in:
  - `CQuestManager::TakeItem`
  - `CQuestManager::UseItem`
  - `CQuestManager::SIGUse`

Batch D - fishing ECS wrappers:
- Added ECS-facing fishing wrappers:
```cpp
bool fishing::UseFishEcs(entt::entity owner, entt::entity fishItem);
bool fishing::GrillFishEcs(entt::entity owner, entt::entity fishItem);
```
- Wrappers:
  - resolve owner through `ecs::LegacyCharOf`
  - resolve item internally by ECS item ID
  - call legacy `UseFish` / `Grill`
  - call `ItemSystem::SyncItemStateFromLegacy(...)` after legacy mutation
- Migrated legacy item-use/campfire call sites in `ItemSystem_LegacyBridge.cpp`:
  - alive fish use now enters through `fishing::UseFishEcs(...)`
  - campfire grill now enters through `fishing::GrillFishEcs(...)`

Batch F - DragonSoul status:
- Existing 15E-24 DragonSoul ECS wrappers were rechecked.
- No additional DragonSoul caller migration was made in this pass because direct external packet refine calls were already on ECS wrappers and the remaining calls are internal legacy engine calls.

Bridge policy:
- No new public `LPITEM` exposure was added to `ItemSystem.hpp`.
- DragonSoul local bridge remains private to `DragonSoul.cpp`.
- Fishing wrapper bridge resolves `LPITEM` internally only, while call sites now use entity entry points.

Validation:
- Build gate passed after quest item/current item batch:
```powershell
cmake --build build --config RelWithDebInfo --target GameServer --parallel 8
```
- Build gate passed after fishing wrapper batch:
```powershell
cmake --build build --config RelWithDebInfo --target GameServer --parallel 8
```
- `GameServer.exe` linked successfully after both gates.
- Final validation build also passed:
```powershell
cmake --build build --config RelWithDebInfo --target GameServer --parallel 8
```
  Result: `GameServer.exe` linked successfully.
- Public `ItemSystem.hpp` pointer leak scan:
```powershell
Select-String -Path 'SRC/Server/GameServer/ecs/systems/ItemSystem.hpp' -Pattern 'LPITEM|LPCHARACTER|CHARACTER\s*\*'
```
  Result: no matches.
- Existing warning-class output remains present.
- Build output still prints the known post-step environment message:
```text
'pwsh.exe' is not recognized as an internal or external command
```
  but build commands returned exit code `0`.

Remaining blockers:
- `questlua_item.cpp`:
  - random attribute generation (`AddAttribute`, `ChangeAttribute`) still legacy-owned.
  - over9/refine/copy-before-remove paths still legacy-owned.
  - pet DB complex paths only partially migrated where ID read was safe.
- `questlua_pc.cpp`:
  - party dice reward requires `LPITEM`.
  - polymorph/book paths require legacy item pointer and/or socket mutation.
- `fishing.cpp`:
  - rod state and rod refine still legacy-owned due socket/refine/item creation/removal semantics.
  - main fish catch reward remains legacy because it writes fish length socket and highscore value.
- `cmd_gm.cpp` / `cmd_general.cpp`:
  - remaining GM commands mostly create items, mutate sockets/attributes, or force equip/refine behavior.
- `DragonSoul.cpp`:
  - legacy DS algorithms remain `LPITEM`-based internally by design.
- `input_main.cpp`:
  - remaining item paths are packet-sensitive and require separate targeted audit.

Manual WinTest checklist:
- Quest:
  - quest item use event
  - quest current item select
  - `pc.give_or_drop_item`
  - `pc.give_or_drop_item_and_select`
  - NPC item hand-in quest
  - item remove from quest script
  - item attribute get/set/clear script commands
- Fishing:
  - use fish
  - grill fish
  - fish reward branches
  - rod refine smoke test if reachable
- GM/commands:
  - GM item list command
  - GM full item set
  - mount costume unequip
  - item grant command if available
- DragonSoul:
  - activate/deactivate
  - refine grade/step/strength
  - extraction/pull-out if available
- General:
  - inventory open/relog
  - equip/unequip
  - use potion
  - refine smoke test
  - no duplicate item
  - no missing item
  - no wrong owner/window/cell
  - no delayed damage/metin collapse regression

Commit status:
- Not committed yet. User requested review before commit.

## Phase 15E-27 - Quest current item ECS migration

Date: 2026-04-27

Scope:
- Continued migration of quest current item handling toward ECS entity flow.
- Used existing `GetCurrentItemEntity()` and `SetCurrentItem(entt::entity)` bridge.
- Replaced safe current item setup/read-only ID paths.
- Kept mutation-heavy, refine, polymorph, pet, and quest-engine legacy pointer paths unchanged.

Files changed:
- `SRC/Server/GameServer/questmanager.cpp`
- `SRC/Server/GameServer/questlua_pc.cpp`
- `SRC/Server/GameServer/questlua_item.cpp`
- `docs/ecs_migration/MIGRATION_LOG_GPT.md`

Implementation:
- `CQuestManager::TakeItem`, `UseItem`, and `SIGUse` now set current item through the entity overload:
```cpp
SetCurrentItem(EntityFactory::CreateItemEntity(g_registry, item));
```
- `pc_give_or_drop_item_and_select` now grants through ECS:
```cpp
const entt::entity item = ItemSystem::AutoGiveItemEcs(chEntity, dwVnum, icount);
const uint32_t itemId = ItemSystem::GetItemID(item);
```
  then stores current item through:
```cpp
CQuestManager::Instance().SetCurrentItem(item);
```
- `questlua_item.cpp` pet death/revive paths now read current item ID through:
```cpp
ItemSystem::GetItemID(q.GetCurrentItemEntity());
```

Current item state after this pass:
- Entity current item API is used by most read-only `questlua_item.cpp` functions already.
- New current item setup points now route through `SetCurrentItem(entt::entity)` where safe.
- Legacy `GetCurrentItem()` and `SetCurrentItem(LPITEM)` remain as bridge/backing-store compatibility.

Skipped call sites and reasons:
- `questlua_pc.cpp` current item usages around NPC/current item interactions:
  - Unsafe: item is passed into quest/NPC logic as legacy pointer.
- `pc_upgrade_polymorph_book`:
  - Unsafe: `CPolymorphUtils::BookUpgrade` expects `LPITEM`.
- `questlua_item.cpp` over9 refine and refine/material paths:
  - Unsafe: refine and item mutation logic.
- `questlua_item.cpp` pet systems beyond read-only ID:
  - Partially migrated only where ID read was safe.
- Attribute/socket mutation functions:
  - Unsafe: mutation-heavy current item operations.
- `CQuestManager::GetCurrentItem()` declaration/definition:
  - Retained as legacy bridge for remaining unsafe callers.

Validation:
- Build gate passed:
```powershell
cmake --build build --config RelWithDebInfo --target GameServer --parallel 8
```
- `GameServer.exe` linked successfully.
- Existing warning-class output remains present.
- Build output still prints the known post-step environment message:
```text
'pwsh.exe' is not recognized as an internal or external command
```
  but the build command returned exit code `0`.

Manual WinTest checklist:
- Quest item use event.
- NPC item hand-in quest.
- Special item group use quest.
- Quest reward with current item select.
- Pet death/revive quest item checks if available.
- Polymorph book path smoke test, because it remains legacy.
- Verify no missing/duplicate item.
- Verify no delayed damage/metin collapse regression.

Commit status:
- Not committed yet. User requested review before commit.

## Phase 15E-26c - questlua_pc.cpp safe LPITEM cleanup

Date: 2026-04-27

Scope:
- Targeted `questlua_pc.cpp` only.
- Migrated one safe quest reward path from legacy `LPITEM` reward return to ECS item entity.
- Left quest current item, party dice, polymorph, socket mutation, item creation, DragonSoul/refine, and quest engine item handoff paths unchanged.
- No packet format, inventory layout, quest semantics, DragonSoul behavior, refine behavior, or `points[]` changes.

Files changed:
- `SRC/Server/GameServer/questlua_pc.cpp`
- `docs/ecs_migration/MIGRATION_LOG_GPT.md`

Counts:
- Initial `questlua_pc.cpp` `LPITEM` count: `53`
- Final `questlua_pc.cpp` `LPITEM` count: `52`
- Net reduction: `1`

Safe replacement performed:
- `pc_give_or_drop_item`:
  - Replaced:
```cpp
LPITEM item = ch->AutoGiveItem(dwVnum, icount);
```
  - with:
```cpp
const entt::entity item = ItemSystem::AutoGiveItemEcs(chEntity, dwVnum, icount);
const uint32_t itemId = ItemSystem::GetItemID(item);
```
  - Preserved returned Lua item ID behavior.
  - Preserved gold bar logging by using `itemId`.

Skipped call sites and reasons:
- `pc_give_or_drop_item_and_select`:
  - Unsafe: sets `CQuestManager::SetCurrentItem(item)`, quest engine still expects legacy item pointer.
- `pc_give_or_drop_item_with_dice`:
  - Unsafe: creates `LPITEM`, passes it into `FPartyDropDiceRoll`, then grants legacy item pointer to owner.
- `pc_give_random_book0`:
  - Unsafe: returned item has socket mutation.
- Polymorph/marble/book paths:
  - Unsafe: item creation and current item / polymorph-specific logic.
- Equipment read paths using `EquipmentSlots` legacy item pointers:
  - Deferred: needs separate equipment component/entity cleanup.
- Current item handlers:
  - Unsafe: quest current item bridge still supports legacy item pointer semantics.
- Inventory scan paths:
  - Deferred: some can be converted later, but require careful result parity.

Validation:
- Build gate passed:
```powershell
cmake --build build --config RelWithDebInfo --target GameServer --parallel 8
```
- `GameServer.exe` linked successfully.
- Existing warning-class output remains present.
- Build output still prints the known post-step environment message:
```text
'pwsh.exe' is not recognized as an internal or external command
```
  but the build command returned exit code `0`.

Manual WinTest checklist:
- Quest reward using `pc.give_or_drop_item`.
- Gold bar quest reward ID logging path if available.
- Quest reward failure/full inventory behavior.
- Relog persistence after quest reward.
- Verify no duplicate/missing item.
- Verify no delayed damage/metin collapse regression.

Commit status:
- Not committed yet. User requested review before commit.

## Phase 15E-26b - fishing.cpp safe LPITEM cleanup

Date: 2026-04-27

Scope:
- Targeted `fishing.cpp` only.
- Migrated safe fishing reward grants and read-only metadata access to ECS item APIs.
- Left rod/refine/socket/count mutation and fish-catch socket/highscore paths unchanged.
- No refine behavior, quest behavior, packet format, inventory layout, DragonSoul behavior, or `points[]` changes.

Files changed:
- `SRC/Server/GameServer/fishing.cpp`
- `docs/ecs_migration/MIGRATION_LOG_GPT.md`

Counts:
- Initial `fishing.cpp` `LPITEM` count: `11`
- Final `fishing.cpp` `LPITEM` count: `11`
- Net reduction: `0`

Reason count did not drop:
- Safe changes replaced `AutoGiveItem` calls and read-only `item->Get*` usage inside existing `LPITEM`-signature functions.
- Function signatures such as `UseFish(LPCHARACTER, LPITEM)`, `Grill(LPCHARACTER, LPITEM)`, and rod refine helpers remain legacy-owned and intentionally unchanged.

Safe replacements performed:
- Added explicit ECS includes for `AIHelpers`, `EntityFactory`, and `ItemSystem`.
- `UseFish(...)`:
  - Created item entity from the current fish item for read-only vnum access.
  - Replaced `item->GetVnum()` with `ItemSystem::GetItemVnum(itemEntity)`.
  - Replaced ignored-return reward grants:
    - dead fish reward
    - fish bone reward
    - shellfish reward
    - earthworm reward
    - generic used-table reward
  - New grant path uses `ItemSystem::AutoGiveItemEcs(owner, vnum)`.
- `Grill(...)`:
  - Created item entity from the source fish item for read-only metadata.
  - Replaced `item->GetVnum()` with `ItemSystem::GetItemVnum(itemEntity)`.
  - Replaced `item->GetCount()` with `ItemSystem::GetItemCount(itemEntity)`.
  - Replaced chat item name read with `ItemSystem::GetItemName(itemEntity)`.
  - Replaced ignored-return grill reward grant with `ItemSystem::AutoGiveItemEcs(owner, grillVnum, count)`.

Skipped call sites and reasons:
- Fishing rod wear checks and state:
  - Unsafe: `rod->GetSocket`, `rod->SetSocket`, `rod->GetValue`, and rod state timing are tightly coupled.
- Main fish catch grant:
  - Unsafe: returned item is used for rank logic, socket write, fish length, highscore packet, and chat formatting.
- `item->SetCount(...)` in `UseFish` and `Grill`:
  - Unsafe in this pass: source item consumption/deletion semantics remain legacy-owned.
- `RefinableRod` / `RealRefineRod`:
  - Unsafe: rod refine creates/removes items, mutates sockets, logs refine, and uses direct `ITEM_MANAGER`.
- Direct `ITEM_MANAGER::CreateItem` / `RemoveItem` paths:
  - Unsafe: item lifecycle/refine behavior.

Validation:
- Build gate passed:
```powershell
cmake --build build --config RelWithDebInfo --target GameServer --parallel 8
```
- `GameServer.exe` linked successfully.
- Existing warning-class output remains present.
- Build output still prints the known post-step environment message:
```text
'pwsh.exe' is not recognized as an internal or external command
```
  but the build command returned exit code `0`.

Manual WinTest checklist:
- Use raw fish item.
- Grill fish.
- Fishing reward grant.
- Fish bone/shellfish/earthworm reward branches if reachable.
- Relog persistence after fishing reward.
- Verify no duplicate/missing item.
- Verify no delayed damage/metin collapse regression.

Commit status:
- Not committed yet. User requested review before commit.

## Phase 15E-26a - cmd_gm.cpp safe LPITEM cleanup

Date: 2026-04-27

Scope:
- Targeted `cmd_gm.cpp` only.
- Replaced only safe LPITEM usage where existing `ItemSystem` ECS APIs preserve behavior.
- No DragonSoul, refine, quest, socket mutation, attribute mutation, packet format, inventory layout, or `points[]` changes.

Files changed:
- `SRC/Server/GameServer/cmd_gm.cpp`
- `docs/ecs_migration/MIGRATION_LOG_GPT.md`

Counts:
- Initial `cmd_gm.cpp` `LPITEM` count: `19`
- Final `cmd_gm.cpp` `LPITEM` count: `18`
- Net reduction: `1`

Safe replacements performed:
- Added `ItemSystem` include to `cmd_gm.cpp`.
- `do_get_item_id_list`:
  - Replaced read-only `LPITEM item = ch->GetInventoryItem(i)` with `ItemSystem::GetInventoryItem(owner, i)`.
  - Replaced `item->GetCell()`, `item->GetName()`, `item->GetID()` with:
    - `ItemSystem::GetItemCell(item)`
    - `ItemSystem::GetItemName(item)`
    - `ItemSystem::GetItemID(item)`
- `do_item_full_set` unequip prelude:
  - Replaced `ch->GetWear(...)` + `ch->UnequipItem(item)` with:
    - `ItemSystem::GetWearItem(owner, wearPos)`
    - `ItemSystem::UnequipItemEcs(owner, item)`
  - Legacy item creation/equip in the same command remains unchanged.

Skipped call sites and reasons:
- `ITEM_MANAGER::CreateItem(...)` paths:
  - Unsafe: newly created `LPITEM` is immediately placed/equipped or socket-mutated.
- Purge/remove loops:
  - Unsafe: direct `ITEM_MANAGER::RemoveItem(...)` side effects and quickslot sync.
- `do_refine_rod`, `do_refine_pick`, `do_max_pick`:
  - Unsafe: passed to fishing/mining legacy refine helpers.
- `ch->AutoGiveItem(...)` followed by `item->SetSocket(...)`:
  - Unsafe: returned `LPITEM` is immediately mutated.
- Weapon attribute/socket GM commands:
  - Unsafe: `ChangeAttribute`, `AddAttribute`, `AddSocket`, rare attribute mutations still legacy-owned.
- `ITEM_MANAGER::Find(...)->SetSocket(...)`:
  - Unsafe: direct GM socket mutation by item ID.
- Full equipment create/equip block:
  - Unsafe: creates legacy items and calls `EquipTo`/`FindEquipCell` directly.
- Equipment bonus/modify blocks:
  - Unsafe: direct `ModifyPoints(...)` side effects.

Validation:
- Build gate passed:
```powershell
cmake --build build --config RelWithDebInfo --target GameServer --parallel 8
```
- `GameServer.exe` linked successfully.
- Existing warning-class output remains present.
- Build output still prints the known post-step environment message:
```text
'pwsh.exe' is not recognized as an internal or external command
```
  but the build command returned exit code `0`.

Manual WinTest checklist:
- GM item ID list command.
- GM full item set command, especially pre-unequip behavior.
- Inventory open/relog after GM command usage.
- Verify no duplicate/missing item.
- Verify no delayed damage/metin collapse regression.

Commit status:
- Not committed yet. User requested review before commit.

## Phase 15E-26 - Final LPITEM purge pass, safe entry-point cleanup

Date: 2026-04-27

Scope:
- Ran a full post-ECS `LPITEM` scan and replaced only safe mutation entry points where ECS wrappers already existed.
- This was not a blind global `LPITEM` removal. Remaining `LPITEM` usage is concentrated in legacy islands that still need targeted phases.
- No packet format, inventory layout, quest behavior, DragonSoul behavior, refine behavior, shop/exchange/safebox behavior, or `points[]` logic was changed.

Files changed:
- `SRC/Server/GameServer/input_main.cpp`
- `SRC/Server/GameServer/cmd_general.cpp`
- `docs/ecs_migration/MIGRATION_LOG_GPT.md`

Full scan:
- Initial project `LPITEM` count in `SRC/Server/GameServer`: `572`
- Final project `LPITEM` count after safe replacements: `571`

Top remaining `LPITEM` islands:
- `DragonSoul.cpp`: 61
- `questlua_pc.cpp`: 53
- `char.h`: 47
- `item_manager.cpp`: 27
- `char_manager.cpp`: 19
- `cmd_gm.cpp`: 19
- `questlua_item.cpp`: 16
- `item_manager.h`: 16
- `New_PetSystem.cpp`: 14
- `DragonSoul.h`: 14
- `cmd_general.cpp`: 13
- `MountInventory.cpp`: 12
- `EntityFactory.cpp`: 12
- `input_main.cpp`: 11
- `fishing.cpp`: 11

Safe replacements performed:
- `input_main.cpp`
  - Quiz reward path changed from ignored-return legacy grant:
```cpp
ch->AutoGiveItem(vnum, count);
```
  - to ECS grant:
```cpp
ItemSystem::AutoGiveItemEcs(AIHelpers::EcsOf(ch), vnum, count);
```
  - This was safe because the return value was ignored and no socket/attribute/quest-current-item mutation followed.
- `cmd_general.cpp`
  - Mount costume unequip path changed from:
```cpp
LPITEM pkItem = ch->GetWear(WEAR_COSTUME_MOUNT);
if (pkItem) {
    ch->UnequipItem(pkItem);
    return;
}
```
  - to ECS item lookup + ECS unequip wrapper:
```cpp
const entt::entity owner = AIHelpers::EcsOf(ch);
const entt::entity item = ItemSystem::GetWearItem(owner, WEAR_COSTUME_MOUNT);
if (item != entt::null) {
    ItemSystem::UnequipItemEcs(owner, item);
    return;
}
```

Mutation entry-point scan after cleanup:
- Remaining direct legacy mutation entry calls are still present in high-risk files:
  - `fishing.cpp`: 7
  - `questlua_pc.cpp`: 7
  - `DragonSoul.cpp`: 6
  - `cmd_gm.cpp`: 4
  - dungeon/event systems: smaller counts
- These were intentionally not migrated in this pass because they involve returned `LPITEM`, socket/attribute mutation, quest state, DragonSoul, or system-specific side effects.

Public API check:
- `ItemSystem.hpp` pointer leak scan:
```powershell
Select-String -Path 'SRC/Server/GameServer/ecs/systems/ItemSystem.hpp' -Pattern 'LPITEM|LPCHARACTER|CHARACTER\s*\*'
```
  Result: no matches.

Validation:
- Build gate passed:
```powershell
cmake --build build --config RelWithDebInfo --target GameServer --parallel 8
```
- `GameServer.exe` linked successfully.
- Existing warning-class output remains present.
- Build output still prints the known post-step environment message:
```text
'pwsh.exe' is not recognized as an internal or external command
```
  but the build command returned exit code `0`.

Why `LegacyItemOf` was not killed:
- `LegacyItemOf` is still required by refine, DragonSoul, quest, equip/use, receive/give, and item-manager side-effect boundaries.
- Removing it globally now would force behavior rewrites in systems that still depend on `CItem` side effects.
- Correct next step is island-by-island cleanup, not global deletion.

Recommended next targeted phases:
- `15E-26a`: `cmd_gm.cpp` safe AutoGive/equip cleanup.
- `15E-26b`: `fishing.cpp` low-risk ignored-return AutoGive cleanup; skip metadata/socket paths.
- `15E-26c`: `questlua_pc.cpp` reward path split; only migrate ignored/null-check paths.
- `15E-26d`: DragonSoul internal LPITEM island, continuation from 15E-24.
- `15E-26e`: shop/exchange/safebox/offlineshop island audit.

Manual WinTest checklist:
- Quiz reward item grant.
- Mount costume unequip through `do_user_horse_back`.
- Inventory open/relog.
- Equip/unequip smoke test.
- Verify no duplicate/missing item.
- Verify no wrong owner/window/cell.
- Verify no delayed damage/metin collapse regression.

Commit status:
- Not committed yet. User requested review before commit.

## Phase 15E-25 - Refine system extraction, ECS-first entry control

Date: 2026-04-27

Scope:
- Started the refine system extraction by moving the packet-side refine entry point to ECS-facing `ItemSystem` wrappers.
- Added a small ECS refine model (`RefineInput`, `RefineResult`) and a thin `RefineItemEcs` wrapper.
- Legacy `CHARACTER::DoRefine*` and `CHARACTER::RefineItem` remain the refine engine for this phase.
- No refine success logic, material handling, packet format, item storage, DragonSoul behavior, or `points[]` logic was changed.

Files changed:
- `SRC/Server/GameServer/ecs/systems/ItemSystem.hpp`
- `SRC/Server/GameServer/ecs/systems/ItemSystem.cpp`
- `SRC/Server/GameServer/input_main.cpp`
- `docs/ecs_migration/MIGRATION_LOG_GPT.md`

Audit summary:
- Legacy refine engine definitions are in `ItemSystem_LegacyBridge.cpp`:
  - `CHARACTER::DoRefine(LPITEM, bool)`
  - `CHARACTER::DoRefineWithScroll(LPITEM)`
  - `CHARACTER::DoRefineItemSoul(LPITEM)`
  - `CHARACTER::RefineItem(LPITEM, LPITEM)`
- Existing ECS public wrappers were already present in `ItemSystem.hpp`:
  - `ItemSystem::DoRefine(entt::entity, entt::entity, bool)`
  - `ItemSystem::DoRefineWithScroll(entt::entity, entt::entity)`
  - `ItemSystem::DoRefineItemSoul(entt::entity, entt::entity)`
  - `ItemSystem::RefineItem(entt::entity, entt::entity, entt::entity)`
- Packet-side direct refine calls were in `input_main.cpp`:
  - `ch->DoRefine(item)`
  - `ch->DoRefineWithScroll(item)`
  - `ch->DoRefineItemSoul(item)`
  - `ch->DoRefine(item, true)`

Mutation classification:
- Read/precheck:
  - `CRefineManager::GetPercentage(...)` still requires the legacy `LPITEM`; this precheck was intentionally left unchanged.
- Mutation:
  - `DoRefine`, `DoRefineWithScroll`, `DoRefineItemSoul`, and `RefineItem` still mutate through the legacy engine.
  - The mutation is now reached from packet handling through ECS wrappers.
- Creation/destruction:
  - Refine result creation, material consumption, and item destruction remain inside legacy refine logic.
  - ECS sync occurs after successful wrapper calls through existing `ItemSystem` sync boundaries.
- DragonSoul refine:
  - Already isolated in Phase 15E-24 through `DSManager::*Ecs` wrappers.

Implementation:
- Added ECS refine model:
```cpp
struct RefineInput {
    entt::entity item = entt::null;
    std::vector<entt::entity> materials;
};

struct RefineResult {
    entt::entity resultItem = entt::null;
    bool success = false;
};
```
- Added thin wrapper:
```cpp
RefineResult RefineItemEcs(entt::entity e, const RefineInput& input, entt::entity target);
```
- `RefineItemEcs` delegates to existing `ItemSystem::RefineItem(...)` and reports the ECS result entity on success.
- `input_main.cpp` now resolves:
  - owner: `AIHelpers::EcsOf(ch)`
  - item: `ItemSystem::GetInventoryItem(owner, p->pos)`
- Packet-side refine execution now uses:
  - `ItemSystem::DoRefine(owner, itemEntity)`
  - `ItemSystem::DoRefineWithScroll(owner, itemEntity)`
  - `ItemSystem::DoRefineItemSoul(owner, itemEntity)`
  - `ItemSystem::DoRefine(owner, itemEntity, true)`

Validation:
- Build gate passed:
```powershell
cmake --build build --config RelWithDebInfo --target GameServer --parallel 8
```
- `GameServer.exe` linked successfully.
- Existing warning-class output remains present.
- Build output still prints the known post-step environment message:
```text
'pwsh.exe' is not recognized as an internal or external command
```
  but the build command returned exit code `0`.

Audit checks:
- `input_main.cpp` no longer contains direct packet-side `ch->DoRefine(...)`, `ch->DoRefineWithScroll(...)`, or `ch->DoRefineItemSoul(...)` calls.
- `ItemSystem.hpp` refine APIs remain entity-first and do not expose `LPITEM`.
- Remaining `LPITEM` usage in `ItemSystem.cpp` is internal bridge/legacy sync/refine engine support, not public API exposure.

Limitations:
- This phase does not remove `LPITEM` from the legacy refine implementation.
- Refine material handling, result creation, socket/attribute changes, destruction, and logging are still legacy-owned.
- `CRefineManager::GetPercentage(...)` still receives `LPITEM` during the packet precheck and needs a separate audit before conversion.
- `ItemSystem_LegacyBridge.cpp` remains the refine legacy island.

Manual WinTest checklist:
- Normal refine.
- Scroll refine.
- Soul refine if enabled.
- Money-only/deviltower refine.
- Failed refine.
- Successful refine.
- Refine material consumption.
- Refine result item persistence after relog.
- Verify no duplicate/missing item.
- Verify no wrong owner/window/cell.
- Verify no delayed damage/metin collapse regression.

Commit status:
- Not committed yet. User requested review before commit.

## Phase 15E-24 - DragonSoul mutation isolation wrappers

Date: 2026-04-27

Scope:
- High-risk DragonSoul mutation entry points were isolated behind ECS-facing wrappers.
- Legacy DragonSoul logic remains the execution engine for this pass.
- No DragonSoul refine/activation/extraction semantics were rewritten.
- No packet format, inventory layout, DragonSoul storage, refine behavior, or `points[]` changes were made.

Files changed:
- `SRC/Server/GameServer/DragonSoul.h`
- `SRC/Server/GameServer/DragonSoul.cpp`
- `SRC/Server/GameServer/input_main.cpp`
- `docs/ecs_migration/MIGRATION_LOG_GPT.md`

Wrappers introduced:
- `DSManager::ExtractDragonHeartEcs(entt::entity owner, entt::entity item, entt::entity extractor)`
- `DSManager::PullOutEcs(entt::entity owner, TItemPos destCell, entt::entity& item, entt::entity extractor)`
- `DSManager::DoRefineGradeEcs(entt::entity owner, TItemPos (&aItemPoses)[DRAGON_SOUL_REFINE_GRID_SIZE])`
- `DSManager::DoRefineStepEcs(entt::entity owner, TItemPos (&aItemPoses)[DRAGON_SOUL_REFINE_GRID_SIZE])`
- `DSManager::DoRefineStrengthEcs(entt::entity owner, TItemPos (&aItemPoses)[DRAGON_SOUL_REFINE_GRID_SIZE])`
- `DSManager::DoRefineAllEcs(entt::entity owner, uint8_t subheader, uint8_t type, uint8_t grade)`
- `DSManager::ActivateDragonSoulEcs(entt::entity item)`
- `DSManager::DeactivateDragonSoulEcs(entt::entity item, bool skipRefreshOwnerActiveState)`

Implementation:
- Added local DragonSoul bridge helpers in `DragonSoul.cpp`:
  - `LegacyDragonSoulItemOf(entt::entity)` resolves a legacy `LPITEM` from ECS item identity.
  - `SyncDragonSoulItemEntity(entt::entity)` syncs ECS item state from legacy after mutation.
  - `SyncDragonSoulItemPtr(LPITEM)` wraps a legacy item into ECS and syncs it.
  - `SyncDragonSoulGridItems(...)` syncs remaining refine-grid items after legacy refine.
- ECS wrappers resolve owner/item, call the existing legacy DragonSoul function unchanged, then sync ECS item state from legacy.
- The DragonSoul packet refine path in `input_main.cpp` now calls ECS wrappers:
  - `DoRefineGradeEcs(AIHelpers::EcsOf(ch), ...)`
  - `DoRefineStepEcs(AIHelpers::EcsOf(ch), ...)`
  - `DoRefineStrengthEcs(AIHelpers::EcsOf(ch), ...)`
  - `DoRefineAllEcs(AIHelpers::EcsOf(ch), ...)`

Mutation path classification:
- Refine:
  - Grade, step, strength, and refine-all now have ECS entry points.
  - Packet-side direct legacy refine calls were replaced by ECS wrapper calls.
  - Legacy `DoRefine*` functions remain internally unchanged and are called only from wrappers for this path.
- Activation:
  - `ActivateDragonSoulEcs` and `DeactivateDragonSoulEcs` were added.
  - No broad caller migration was attempted in this pass.
- Extraction:
  - `ExtractDragonHeartEcs` and `PullOutEcs` were added.
  - No broad caller migration was attempted in this pass.

Validation:
- Build gate passed:
```powershell
cmake --build build --config RelWithDebInfo --target GameServer --parallel 8
```
- `GameServer.exe` linked successfully.
- Existing warning-class output remains present.
- Build output still prints the known post-step environment message:
```text
'pwsh.exe' is not recognized as an internal or external command
```
  but the build command returned exit code `0`.

Audit checks:
- ECS DragonSoul wrapper declarations and definitions are present in `DragonSoul.h` / `DragonSoul.cpp`.
- `input_main.cpp` DragonSoul refine packet handling now calls the ECS wrapper methods.
- Direct `DoRefine*(ch, ...)` calls found after migration are only the intentional internal calls inside the ECS wrappers.

Limitations:
- This phase does not remove all `LPITEM` usage from DragonSoul internals.
- The legacy DragonSoul engine remains authoritative for mutation behavior during this phase.
- ECS wrappers are now the intended entry points for migrated callers, with legacy retained behind the wrapper boundary.

Manual WinTest checklist:
- DragonSoul refine grade.
- DragonSoul refine step.
- DragonSoul refine strength.
- DragonSoul refine-all if enabled.
- DragonSoul activation/deactivation smoke test.
- DragonHeart extraction / pull-out smoke test if available.
- Relog persistence after DS mutation.
- Verify no duplicate/missing DragonSoul item.
- Verify no wrong DS inventory slot.
- Verify no delayed damage/metin collapse regression.

Commit status:
- Not committed yet. User requested review before commit.

---

## Phase 15E-23 - DragonSoul LPITEM cleanup, ECS bridge introduction

Mode:
- Targeted subsystem extraction.
- ECS bridge introduction only.
- No commit performed.

Goal:
- Introduce entity-based DragonSoul read bridge APIs.
- Route safe DragonSoul read paths through `ItemSystem` entity APIs.
- Leave refine/byproduct/inventory mutation logic on the legacy path.

Files changed:
- `SRC/Server/GameServer/DragonSoul.h`
- `SRC/Server/GameServer/DragonSoul.cpp`
- `SRC/Server/GameServer/ecs/systems/ItemSystem.hpp`
- `SRC/Server/GameServer/ecs/systems/ItemSystem.cpp`
- `docs/ecs_migration/MIGRATION_LOG_GPT.md`

Audit summary:
- `DragonSoul.cpp` direct `LPITEM` count: `51`.
- `DragonSoul.h` direct `LPITEM` count: `14`.
- Heavy legacy zones identified:
  - DragonSoul refine grade/step/strength.
  - byproduct creation and `AutoGiveDS`.
  - `ITEM_MANAGER::CreateItem` / count mutation.
  - DS inventory routing and `AddToCharacter`.
  - activation/deactivation point modification and socket mutation.
- These zones were not rewritten in this phase.

New DragonSoul entity overloads:
```cpp
uint16_t GetBasePosition(entt::entity item) const;
bool IsValidCellForThisItem(entt::entity item, const TItemPos& Cell) const;
bool IsTimeLeftDragonSoul(entt::entity item) const;
int LeftTime(entt::entity item) const;
bool IsActiveDragonSoul(entt::entity item) const;
```

Read path migration:
- Existing LPITEM overloads remain for compatibility, but now delegate into the entity overloads through:
```cpp
EntityFactory::CreateItemEntity(g_registry, pItem)
```
- Entity overloads use `ItemSystem` reads:
  - `GetItemVnum`
  - `GetItemSubType`
  - `GetItemSocket`
  - `IsValidItem`
  - `GetItemLimitTimerBasedOnWearIndex`

New ItemSystem read API:
```cpp
int GetItemLimitTimerBasedOnWearIndex(entt::entity item);
```
- This avoids incorrectly substituting timer-based-wear proto metadata with unrelated item level-limit data.
- Implementation is read-only and uses the existing isolated internal legacy bridge fallback.

Behavior boundary:
- No DragonSoul refine behavior changed.
- No DragonSoul byproduct behavior changed.
- No DS inventory routing changed.
- No socket/count mutation semantics changed.
- No packet format changes.
- No `points[]` changes.
- Public `ItemSystem.hpp` remains pointer-clean.

Validation:
- Build gate passed:
```powershell
cmake --build build --config RelWithDebInfo --target GameServer --parallel 8
```
- `GameServer.exe` linked successfully.
- Existing warning-class output remains present.
- Build output still prints the known post-step environment message:
```text
'pwsh.exe' is not recognized as an internal or external command
```
  but the build command returned exit code `0`.

Scans:
- `ItemSystem.hpp` public pointer leak scan:
```text
LPITEM / LPCHARACTER / CHARACTER*: 0
```
- DragonSoul `LPITEM` declaration count did not drop yet because compatibility signatures remain:
```text
DragonSoul.cpp: 51
DragonSoul.h:   14
```
- Effective read migration happened inside these compatibility methods: callers can remain unchanged while read-only DS state now goes through entity ItemSystem APIs.

Remaining DragonSoul blockers:
- `RefreshItemAttributes` and `PutAttributes` mutate item attributes.
- `ExtractDragonHeart`, `PullOut`, `DoRefineGrade`, `DoRefineStep`, `DoRefineStrength`, `DoRefineAll` create/destroy/move/mutate items.
- `ActivateDragonSoul` / `DeactivateDragonSoul` still mutate sockets and call legacy point modification/event methods.
- Refine material classification and byproduct flows still pass `LPITEM`.
- DS inventory loops still collect `LPITEM` from legacy character inventory helpers.

Recommended next 15E-23 slice:
- Add entity overloads for DS refine material classification only if they can remain read-only.
- Add entity wrapper APIs around DS activation/deactivation only after socket mutation and point side effects are explicitly modeled.
- Defer grade/step/strength refine until a dedicated refine bridge phase.

Manual WinTest checklist:
- Login with DragonSoul items.
- Open DragonSoul inventory.
- Verify DS item base slot validation.
- Activate/deactivate DragonSoul deck.
- Verify remaining time display/behavior.
- Perform DS refine smoke test to confirm legacy mutation path is unchanged.
- Relog and verify DS state persists.
- Verify no delayed damage/metin collapse regression.

Commit status:
- Not committed yet. User requested review before commit.

---

## Phase 15E-22b - Complete questlua_item safe read cleanup

Mode:
- Targeted cleanup continuation.
- Safe read-only/simple metadata subset only.
- No commit performed.

Goal:
- Reduce the remaining `questlua_item.cpp` `LPITEM` surface after 15E-22.
- Add missing `ItemSystem` read APIs needed by quest Lua item helpers.
- Leave mutation-heavy quest item paths untouched.

Files changed:
- `SRC/Server/GameServer/ecs/systems/ItemSystem.hpp`
- `SRC/Server/GameServer/ecs/systems/ItemSystem.cpp`
- `SRC/Server/GameServer/questlua_item.cpp`
- `docs/ecs_migration/MIGRATION_LOG_GPT.md`

New ItemSystem public read APIs:
```cpp
int32_t GetItemValue(entt::entity item, uint32_t index);
const char* GetItemName(entt::entity item);
uint8_t GetItemSize(entt::entity item);
uint32_t GetItemRefineVnum(entt::entity item);
int GetItemRefineLevel(entt::entity item);
int GetItemLevelLimit(entt::entity item);
int32_t GetItemFlags(entt::entity item);
uint32_t GetItemWearFlags(entt::entity item);
uint32_t GetItemAntiFlags(entt::entity item);
uint32_t GetItemImmuneFlags(entt::entity item);
```

Implementation notes:
- APIs expose only `entt::entity`; no public `LPITEM` was added.
- APIs prefer ECS component data where already available.
- Metadata not yet component-backed uses the existing isolated internal legacy bridge as read-only fallback.
- No item storage, packet, refine, DragonSoul, quest reward, or `points[]` behavior was changed.

Converted `questlua_item.cpp` helpers:
- `item.has_flag`
- `item.get_value`
- `item.get_name`
- `item.get_size`
- `item.get_refine_vnum`
- `item.get_level`
- `item.get_level_limit`
- `item.get_wearflag0`
- `item.has_wearflag0`
- `item.get_antiflag0`
- `item.has_antiflag0`
- `item.get_immuneflag0`
- `item.has_immuneflag0`
- `item.is_available0`

Validation:
- Build gate passed:
```powershell
cmake --build build --config RelWithDebInfo --target GameServer --parallel 8
```
- `GameServer.exe` linked successfully.
- Existing warning-class output remains present.
- Build output still prints the known post-step environment message:
```text
'pwsh.exe' is not recognized as an internal or external command
```
  but the build command returned exit code `0`.

Scans:
- `ItemSystem.hpp` public pointer leak scan:
```text
LPITEM / LPCHARACTER / CHARACTER*: 0
```
- `questlua_item.cpp` direct `LPITEM` count:
```text
Before 15E-22: 89
After 15E-22:  29
After 15E-22b: 16
```

Remaining `questlua_item.cpp` LPITEM blockers:
- `item.remove`: direct owner validation and `ITEM_MANAGER::RemoveItem`.
- `item.set_value`: direct `SetForceAttribute`.
- `item.can_over9refine`, `item.change_to_over9`, `item.over9refine`: `COver9RefineManager` requires `LPITEM`.
- New pet functions: DB lookup/update based on legacy current item ID.
- `item.start_realtime_expire`: legacy item event method.
- `item.copy_and_give_before_remove`: legacy create/copy/remove/add flow.
- Attribute randomization helpers: `AddAttribute`, `AddRareAttribute`, `ChangeAttribute`, `ChangeRareAttribute`, `SetForceAttribute`, and attribute count/table logic.
- Commented-out legacy equip helpers still contain commented `LPITEM` text but no compiled behavior.

Runtime risk:
- Low. Converted helpers are read-only metadata or already mirrored simple socket/count writes.
- Complex mutation paths remain legacy-authoritative by design.

Recommended next 15E-22 slice:
- Design quest item mutation APIs separately:
  - `RemoveCurrentQuestItemEcs`
  - attribute generation/reroll bridge
  - over9 refine entity bridge
  - realtime expire event bridge
  - copy-and-give replacement design
- Then process `questlua_pc.cpp` low-risk reward paths with `AutoGiveItemEcs` where no returned `LPITEM` mutation is required.

Manual WinTest checklist:
- Quest item select/select_cell.
- Quest `item.get_*` metadata calls through scripts.
- Quest socket set/get.
- Quest count set if script path exists.
- Existing over9/refine/pet/copy item quests should still behave as before.
- Verify no missing item, duplicate item, wrong current item, or delayed damage/metin collapse regression.

Commit status:
- Not committed yet. User requested review before commit.

---

## Phase 15E-22 - Quest LPITEM cleanup, first safe island slice

Mode:
- Targeted subsystem rewrite.
- Partial implementation only; no forced rewrite of mutation-heavy quest item semantics.
- No commit performed.

Goal:
- Start removing quest system `LPITEM` dependency by introducing an entity current-item bridge.
- Convert safe `questlua_item.cpp` read-only/simple write helpers to `ItemSystem` entity APIs.
- Preserve all legacy quest behavior where item mutation/refine/pet/copy semantics still require `CItem*`.

Files changed:
- `SRC/Server/GameServer/questmanager.h`
- `SRC/Server/GameServer/questmanager.cpp`
- `SRC/Server/GameServer/questlua_item.cpp`
- `docs/ecs_migration/MIGRATION_LOG_GPT.md`

Implementation:
- Added parallel current item entity bridge to `CQuestManager`:
```cpp
entt::entity GetCurrentItemEntity();
void SetCurrentItem(entt::entity item);
```
- `GetCurrentItemEntity()` wraps the existing legacy quest item pointer through `EntityFactory::CreateItemEntity(g_registry, GetCurrentItem())`.
- `SetCurrentItem(entt::entity)` resolves the item ID through `ItemSystem::GetItemID(item)` and stores the existing legacy quest item pointer via the old `SetCurrentItem(LPITEM)` bridge.
- Existing legacy APIs remain in place:
```cpp
LPITEM GetCurrentItem();
void SetCurrentItem(LPITEM item);
```
  These are still required by mutation-heavy quest paths.

Converted `questlua_item.cpp` helpers:
- `item.get_cell` now reads `ItemSystem::GetItemCell(currentItemEntity)`.
- `item.select_cell` now selects through `ItemSystem::GetInventoryItem(currentPCEntity, cell)`.
- `item.select` now selects through `ItemSystem::FindItemByID(currentPCEntity, id)`.
- `item.get_id` now reads `ItemSystem::GetItemID`.
- `item.get_socket` now reads `ItemSystem::GetItemSocket`.
- `item.set_socket` now writes through `ItemSystem::SetItemSocket`.
- `item.get_vnum` now reads `ItemSystem::GetItemVnum`.
- `item.get_count` now reads `ItemSystem::GetItemCount`.
- `item.get_type` now reads `ItemSystem::GetItemType`.
- `item.get_sub_type` now reads `ItemSystem::GetItemSubType`.
- `item.set_count0` now writes through `ItemSystem::SetItemCount`.

Behavior boundary:
- No quest script API names changed.
- No packet format changes.
- No inventory layout changes.
- No DragonSoul/refine behavior changes.
- No quest reward behavior rewrite.
- No `points[]` changes.
- No attempt was made to rewrite item copy/refine/pet/attribute generation semantics.

Validation:
- Build gate passed:
```powershell
cmake --build build --config RelWithDebInfo --target GameServer --parallel 8
```
- `GameServer.exe` linked successfully.
- Existing warning-class output remains present.
- Build output still prints the known post-step environment message:
```text
'pwsh.exe' is not recognized as an internal or external command
```
  but the build command returned exit code `0`.

LPITEM scan after this slice:
```text
questlua_pc.cpp:    53
questlua_item.cpp:  29
questmanager.h:      5
questmanager.cpp:    5
```
- `questlua_item.cpp` direct `LPITEM` count after this slice: `29`.
- The safe helper conversions removed 10 direct `LPITEM` uses from `questlua_item.cpp`.

Blockers left intentionally:
- `item.remove` still calls `ITEM_MANAGER::RemoveItem(item)`.
- `item.get_value`, `item.set_value`, `item.get_name`, `item.get_size`, `item.get_refine_vnum`, `item.get_level`, `item.get_level_limit` still need additional read APIs or legacy fallback decisions.
- `item.can_over9refine`, `item.change_to_over9`, `item.over9refine` still pass `LPITEM` into `COver9RefineManager`.
- New pet functions still use item ID through the current legacy item.
- `item.start_realtime_expire` still calls a legacy `CItem` event method.
- `item.copy_and_give_before_remove` creates/copies/removes legacy items and must not be rewritten without a dedicated design.
- `ENABLE_NEWSTUFF` attribute helpers still call legacy attribute generation/mutation APIs (`AddAttribute`, `ChangeAttribute`, `SetForceAttribute`, etc.).

Runtime risk:
- Low-to-medium. Converted Lua functions use existing ItemSystem ECS APIs that already mirror/sync legacy item state.
- The old quest current item pointer remains authoritative for complex mutation paths, so existing scripts relying on side effects should continue to behave as before.

Recommended next 15E-22 slice:
- Add entity read APIs for item flag/value/name/size/refine metadata if safe.
- Convert remaining read-only `questlua_item.cpp` helpers.
- Leave over9/refine/pet/copy/attribute mutation helpers as separate high-risk subphases.
- After `questlua_item.cpp`, process low-risk `questlua_pc.cpp` reward paths using `AutoGiveItemEcs` only where the return is ignored or metadata-only.

Manual WinTest checklist:
- Quest item select by cell and by item ID.
- Quest item socket read/write.
- Quest item count set through Lua if available.
- Quest reward dialog that gives an item.
- Quest that uses current item vnum/type/subtype.
- Relog after quest item mutation.
- Verify no duplicate/missing item and no delayed damage/metin collapse regression.

Commit status:
- Not committed yet. User requested review before commit.

---

## Phase 15E-21 - Split legacy CHARACTER/CItem code out of ItemSystem.cpp

Mode:
- Structural refactor.
- No gameplay semantics changed.
- No commit performed.

Goal:
- Physically isolate legacy `CItem::` / `CHARACTER::` method bodies from `ItemSystem.cpp`.
- Reduce `LPITEM` noise in the ECS-facing ItemSystem implementation file.
- Keep `ItemSystem.cpp` focused on ECS APIs and minimal bridge helpers.

Files changed:
- `SRC/Server/GameServer/ecs/systems/ItemSystem.cpp`
- `SRC/Server/GameServer/ecs/systems/ItemSystem_LegacyBridge.cpp`
- `docs/ecs_migration/MIGRATION_LOG_GPT.md`

Implementation:
- Created new legacy bridge translation unit:
```text
SRC/Server/GameServer/ecs/systems/ItemSystem_LegacyBridge.cpp
```
- Moved the legacy tail after `} // namespace ItemSystem` from `ItemSystem.cpp` into the new bridge file.
- The moved block contains legacy `CItem::` and `CHARACTER::` method bodies, including item core, inventory, equip, use, refine, exchange, special group, and event-related legacy code.
- Kept the required local include/helper/event declaration context in the bridge file so the moved method bodies link without behavior changes.
- Left `ItemSystem.cpp` with the ECS `namespace ItemSystem` implementation block and the existing ECS bridge helpers only.

Behavior boundary:
- No packet format changes.
- No inventory layout changes.
- No DragonSoul/refine/quest/shop/exchange behavior changes.
- No `points[]` changes.
- No public API signature changes.
- This is a physical source split only.

Validation:
- First build reran CMake because the glob detected the new file, then link failed because the newly generated target had not compiled the new translation unit in that same invocation.
- Second build passed:
```powershell
cmake --build build --config RelWithDebInfo --target GameServer --parallel 8
```
- `ItemSystem_LegacyBridge.cpp` compiled and `GameServer.exe` linked successfully.
- Existing warning-class output remains present after the split.
- Build output still prints the known post-step environment message:
```text
'pwsh.exe' is not recognized as an internal or external command
```
  but the successful build returned exit code `0`.

Scans:
- `ItemSystem.hpp` public pointer leak scan:
```powershell
Select-String -Path 'SRC/Server/GameServer/ecs/systems/ItemSystem.hpp' -Pattern 'LPITEM|LPCHARACTER|CHARACTER\s*\*'
```
  Result: `0` matches.
- `ItemSystem.cpp` legacy method scan:
```powershell
Select-String -Path 'SRC/Server/GameServer/ecs/systems/ItemSystem.cpp' -Pattern 'CHARACTER::|CItem::'
```
  Result: `0` matches.
- `ItemSystem.cpp` `LPITEM` count:
```text
Before: 260
After:  62
```
- `ItemSystem_LegacyBridge.cpp` `LPITEM` count:
```text
213
```

Result:
- `ItemSystem.cpp` `LPITEM` surface dropped by 198 references.
- Legacy item/character method bodies are now physically isolated in `ItemSystem_LegacyBridge.cpp`.
- `ItemSystem.hpp` remains clean with zero public `LPITEM` / `LPCHARACTER` / `CHARACTER*` exposure.

Runtime risk:
- Low-to-medium. This is a translation-unit split with no intentional behavior change.
- Risk is primarily linkage/include/build-system related, already covered by the successful build.
- Runtime-sensitive legacy code still exists, but it is now isolated for later LPITEM island cleanup.

Manual WinTest checklist:
- Login/relog.
- Open main inventory and extra inventory.
- Item grant and stack merge.
- Use consumable.
- Equip/unequip weapon and armor.
- Refine smoke test.
- DragonSoul smoke test if available.
- Box/special group open.
- Verify no duplicate item, missing item, wrong owner, wrong slot.
- Verify no delayed damage/metin collapse regression.

Commit status:
- Not committed yet. User requested review before commit.

## 2026-04-26 - Phase 15E-16: ReceiveItem / ownership transfer ECS boundary

Scope:
- High-risk subsystem extraction, narrow implementation.
- Add ECS-facing ownership transfer primitives.
- Keep legacy `CHARACTER::ReceiveItem` side effects intact for dungeon, quest, fishing, blacksmith/refine and horse-feed paths.
- Migrate only the safe `CHARACTER::GiveItem` boundary to call the ECS receive wrapper.

Files changed:
- `SRC/Server/GameServer/ecs/systems/ItemSystem.hpp`
- `SRC/Server/GameServer/ecs/systems/ItemSystem.cpp`
- `docs/ecs_migration/MIGRATION_LOG_GPT.md`

Audit summary:
- `CHARACTER::CanReceiveItem` is not a pure ownership check. It validates:
  - receiver must be NPC/non-PC
  - giver distance
  - blacksmith/refine NPC type rules
  - horse NPC revive/feed item rules
  - fishing NPC/campfire rules
  - quest give fallback
- `CHARACTER::ReceiveItem` is not a pure transfer. It triggers:
  - Rune/Halloween/Viking dungeon `OnNpcTakeItem` hooks
  - fishing grill
  - `quest::CQuestManager::TakeItem`
  - blacksmith refine UI setup
  - horse revive/feed actions
  - item count consumption
- Conclusion: full replacement is not safe yet. The correct step is an ECS boundary that delegates side effects to legacy and owns ECS sync after the call.

New public ECS APIs:
```cpp
bool TransferItemOwnership(entt::entity item, entt::entity from, entt::entity to);
bool ReceiveItemEcs(entt::entity receiver, entt::entity from, entt::entity item);
```

Implementation details:
- `TransferItemOwnership(...)` updates `ecs::ItemOwner`:
  - resolves player IDs from `ecs::PlayerID` first
  - falls back to `LegacyCharOf(e)->GetPlayerID()`
  - refuses transfer when the target has no player ID
  - updates `ownerPID`, `lastOwnerPID`, and `ownershipPID`
- `ReceiveItemEcs(...)`:
  - resolves receiver/giver through `LegacyCharOf`
  - resolves item through `.cpp`-local `LegacyItemOf`
  - calls legacy `CanReceiveItem`
  - calls legacy `ReceiveItem` for all runtime side effects
  - if legacy item still exists, calls `SyncItemStateFromLegacy(item)`
  - if legacy item was consumed/removed, unregisters the item ID from `CItemRegistry` and destroys the ECS item entity
- Existing `ReceiveItem(...)` wrapper now delegates to `ReceiveItemEcs(...)`.

Caller migrated:
- `CHARACTER::GiveItem(LPCHARACTER victim, TItemPos Cell)` now wraps the `LPITEM` into an item entity and calls:
```cpp
ItemSystem::ReceiveItemEcs(AIHelpers::EcsOf(victim),
                           AIHelpers::EcsOf(this),
                           itemEntity)
```
- This is the safe boundary because it previously called the same legacy validation and side-effect path directly.

Explicitly not migrated:
- Quest item current-item paths.
- DragonSoul paths.
- Refine internals.
- Dungeon hook logic.
- Packet formats.
- Inventory storage layout.
- `points[]`.

Validation:
- Build gate passed:
```powershell
cmake --build build --config RelWithDebInfo --target GameServer --parallel 8
```
- `GameServer.exe` linked successfully.
- Existing warning-class output remains present.
- Known post-step environment message remains:
```text
'pwsh.exe' is not recognized as an internal or external command
```
  Build returned exit code `0`.

Scans:
- `ItemSystem.hpp` public pointer leak scan:
```powershell
Select-String -Path 'SRC/Server/GameServer/ecs/systems/ItemSystem.hpp' -Pattern 'LPITEM|LPCHARACTER|CHARACTER\s*\*'
```
  Result: no matches.
- Receive/ownership API scan confirms:
  - `TransferItemOwnership` declaration + definition present.
  - `ReceiveItemEcs` declaration + definition present.
  - `ReceiveItem` wrapper delegates to `ReceiveItemEcs`.
  - `CHARACTER::GiveItem` migrated to the ECS boundary.

Remaining legacy dependency:
- `ReceiveItemEcs` still calls legacy `CanReceiveItem` and `ReceiveItem` intentionally.
- This is required until dungeon/quest/refine/fishing/horse side effects have separate ECS equivalents.

Manual WinTest checklist:
- Drag valid item onto blacksmith NPC and verify refine window opens.
- Drag invalid item onto blacksmith and verify rejection message.
- Drag fish onto campfire/fisher if available.
- Horse feed/revive item on horse NPC.
- Rune/Halloween/Viking dungeon NPC item hand-in if accessible.
- Generic quest NPC item hand-in.
- Verify consumed item disappears exactly once.
- Verify non-consumed item remains in correct slot.
- Relog and verify inventory state.
- Check syserr for duplicate/missing item, wrong owner, wrong cell/window.

Commit status:
- Not committed yet. User requested review before commit.

## 2026-04-26 - Phase 15E-17: Item location / inventory authority ECS boundary

Scope:
- High-risk subsystem extraction, narrow implementation.
- Add ECS-facing item location placement/removal APIs.
- Move the ECS AutoGive placement helper from direct `LPITEM::AddToCharacter` calls to `ItemSystem::PlaceItemEcs`.
- Do not touch equip, refine, DragonSoul internals, quest item paths, packet formats, inventory storage layout, or `points[]`.

Files changed:
- `SRC/Server/GameServer/ecs/systems/ItemSystem.hpp`
- `SRC/Server/GameServer/ecs/systems/ItemSystem.cpp`
- `docs/ecs_migration/MIGRATION_LOG_GPT.md`

Audit summary:
- Legacy location authority is spread across:
  - `CItem::AddToCharacter`
  - `CItem::RemoveFromCharacter`
  - `SetWindow`
  - `SetCell`
  - `GetWindow`
  - `GetCell`
- Runtime-sensitive call sites include:
  - inventory move/swap
  - equipment
  - DragonSoul refine
  - shop/exchange
  - safebox/offlineshop
  - quest item replacement
  - refine replacement item flows
- Conclusion: broad replacement is not safe. The correct first step is an ECS location boundary used only by already-ECS AutoGive placement.

Existing ECS model:
```cpp
struct ItemLocation {
    uint8_t window;
    uint16_t cell;
};
```

New public ECS APIs:
```cpp
bool PlaceItemEcs(entt::entity owner, entt::entity item, uint8_t window, uint16_t cell);
bool RemoveItemEcs(entt::entity item);
```

Implementation details:
- `PlaceItemEcs(...)`:
  - validates owner and item resolution
  - validates supported window/cell bounds
  - rejects duplicate ECS placement for the same owner/window/cell
  - updates ECS owner/location/equipped state first
  - calls legacy `AddToCharacter` as sync/side-effect layer
  - rolls ECS state back if legacy placement fails
  - calls `SyncItemStateFromLegacy(item)` after success
- `RemoveItemEcs(...)`:
  - updates ECS location to `RESERVED_WINDOW`
  - clears ECS owner/equipped state
  - calls legacy `RemoveFromCharacter` if the legacy item still has an owner
  - syncs ECS state from legacy after removal

Caller migrated:
- Internal ECS `PlaceItemInInventory(...)` now uses `PlaceItemEcs(...)` for:
  - DragonSoul inventory placement
  - extra inventory placement
  - main inventory placement
- The only remaining direct `legacyItem->AddToCharacter(ch, ...)` in this path is inside `PlaceItemEcs(...)`, which is the intended sync boundary.

Explicitly not migrated:
- `CItem::AddToCharacter` internals.
- `CItem::RemoveFromCharacter` internals.
- Inventory swap/move paths.
- Equipment paths.
- Refine replacement item paths.
- DragonSoul internals.
- Shop/exchange/safebox/offlineshop paths.
- Quest item replacement paths.
- Packet formats.
- `points[]`.

Validation:
- Build gate passed:
```powershell
cmake --build build --config RelWithDebInfo --target GameServer --parallel 8
```
- `GameServer.exe` linked successfully.
- Existing warning-class output remains present.
- Known post-step environment message remains:
```text
'pwsh.exe' is not recognized as an internal or external command
```
  Build returned exit code `0`.

Scans:
- `ItemSystem.hpp` public pointer leak scan:
```powershell
Select-String -Path 'SRC/Server/GameServer/ecs/systems/ItemSystem.hpp' -Pattern 'LPITEM|LPCHARACTER|CHARACTER\s*\*'
```
  Result: no matches.
- Location API scan confirms:
  - `PlaceItemEcs` declaration + definition present.
  - `RemoveItemEcs` declaration + definition present.
  - `PlaceItemInInventory` calls `PlaceItemEcs` for all three supported inventory routes.
  - Direct `legacyItem->AddToCharacter(ch, ...)` remains only inside `PlaceItemEcs`.

Runtime risk:
- Medium-high. `AutoGiveItemEcs` now depends on the ECS placement boundary.
- Rollback protection exists inside `PlaceItemEcs` if legacy placement fails.
- Broad inventory movement remains legacy to avoid slot corruption.

Manual WinTest checklist:
- GM/simple item grant.
- Stackable grant and merge.
- Extra inventory item grant.
- DragonSoul item grant if available.
- Full inventory fallback.
- Move item after grant.
- Relog and verify item window/cell persistence.
- Verify no duplicated item, missing item, wrong owner, wrong cell/window.
- Verify no delayed damage/metin collapse regression.

Commit status:
- Not committed yet. User requested review before commit.

## 2026-04-26 - Phase 15E-18: Equip / unequip ECS boundary

Scope:
- High-risk subsystem extraction, narrow implementation.
- Add ECS-facing equip/unequip APIs.
- Preserve legacy `CHARACTER::EquipItem` / `CHARACTER::UnequipItem` behavior for stat, affect, packet, costume, mount, DragonSoul and special item side effects.
- Do not rewrite equipment semantics.

Files changed:
- `SRC/Server/GameServer/ecs/components/item_components.hpp`
- `SRC/Server/GameServer/ecs/EntityFactory.cpp`
- `SRC/Server/GameServer/ecs/systems/InventorySystem.cpp`
- `SRC/Server/GameServer/ecs/systems/ItemSystem.hpp`
- `SRC/Server/GameServer/ecs/systems/ItemSystem.cpp`
- `docs/ecs_migration/MIGRATION_LOG_GPT.md`

Audit summary:
- `CHARACTER::EquipItem` is not a simple location move. It handles:
  - `CanEquipNow`
  - wear slot lookup via `FindEquipCell`
  - costume weapon compatibility
  - weapon/costume auto-unequip
  - polymorph/riding/sex checks
  - DragonSoul equip handling
  - quickslot sync
  - timed socket initialization
  - stat/affect recalculation through `EquipTo`
- `CHARACTER::UnequipItem` handles:
  - `CanUnequipNow`
  - empty inventory/DragonSoul slot lookup
  - weapon costume auto-unequip
  - `RemoveFromCharacter`
  - `AddToCharacter`
  - max point checks and affect cleanup
- `CItem::EquipTo` / `CItem::Unequip` contain core side effects:
  - `ModifyPoints`
  - `ComputeBattlePoints`
  - `UpdatePacket`
  - mount/costume/pet skin updates
  - unique/accessory expire timers
  - `EvItemEquipped` / `EvItemUnequipped`
- Conclusion: full semantic replacement is not safe yet. The correct step is an ECS-first boundary with rollback and legacy side-effect delegation.

Component update:
```cpp
struct ItemEquipped {
    bool equipped { false };
    uint8_t slot { 0 };
};
```
- `EntityFactory::MakeItemEquipped` now populates `slot` from `GetCell() - INVENTORY_MAX_NUM` when the item is equipped.
- `InventorySystem::SyncItemEquipped` now preserves the wear slot when legacy item state is available.
- `ItemSystem::SyncItemLocationFromLegacy` now syncs both `equipped` and `slot`.

New public ECS APIs:
```cpp
bool UnequipItemEcs(entt::entity owner, entt::entity item);
bool EquipItemEcs(entt::entity owner, entt::entity item, int candidateCell = -1);
```

Implementation details:
- Existing public wrappers now delegate:
```cpp
bool UnequipItem(entt::entity e, entt::entity item) {
    return UnequipItemEcs(e, item);
}

bool EquipItem(entt::entity e, entt::entity item, int candidateCell) {
    return EquipItemEcs(e, item, candidateCell);
}
```
- `EquipItemEcs(...)`:
  - resolves owner and item internally
  - calculates intended wear slot using legacy `FindEquipCell`
  - snapshots old ECS owner/location/equipped state
  - updates ECS owner/location/equipped first
  - calls legacy `ch->EquipItem(...)` for side effects
  - rolls ECS state back if legacy equip fails
  - syncs ECS from legacy on success
- `UnequipItemEcs(...)`:
  - resolves target inventory/DragonSoul slot using legacy empty-slot helpers
  - snapshots old ECS owner/location/equipped state
  - updates ECS owner/location/equipped first
  - calls legacy `ch->UnequipItem(...)` for side effects
  - rolls ECS state back if legacy unequip fails
  - syncs ECS from legacy on success

Explicitly not migrated:
- `CHARACTER::EquipItem` internals.
- `CHARACTER::UnequipItem` internals.
- `CItem::EquipTo` internals.
- `CItem::Unequip` internals.
- Quest-driven equip paths.
- DragonSoul equip/refine internals.
- GM direct `EquipTo` helper paths.
- Packet formats.
- Stat/affect recalculation.
- `points[]`.

Validation:
- Build gate passed:
```powershell
cmake --build build --config RelWithDebInfo --target GameServer --parallel 8
```
- `GameServer.exe` linked successfully.
- Existing warning-class output remains present.
- Known post-step environment message remains:
```text
'pwsh.exe' is not recognized as an internal or external command
```
  Build returned exit code `0`.

Scans:
- `ItemSystem.hpp` public pointer leak scan:
```powershell
Select-String -Path 'SRC/Server/GameServer/ecs/systems/ItemSystem.hpp' -Pattern 'LPITEM|LPCHARACTER|CHARACTER\s*\*'
```
  Result: no matches.
- Equip API scan confirms:
  - `EquipItemEcs` declaration + definition present.
  - `UnequipItemEcs` declaration + definition present.
  - existing `EquipItem` / `UnequipItem` wrappers delegate to ECS boundary.
  - `ItemEquipped` now contains slot metadata.

Runtime risk:
- High. Equip/unequip touches stats, affects, packets, costumes, mounts and DragonSoul.
- Risk is reduced by keeping legacy semantic functions as the side-effect layer and using rollback on legacy failure.
- Manual WinTest is required before expanding caller migration.

Manual WinTest checklist:
- Equip weapon.
- Unequip weapon.
- Equip armor.
- Unequip armor.
- Equip/unequip costume body.
- Equip/unequip costume weapon with matching weapon subtype.
- Equip incompatible costume weapon and verify rejection.
- Equip/unequip mount costume if available.
- Equip/unequip DragonSoul item if available.
- Relog and verify equipment cells are correct.
- Verify HP/SP/attack/defense/stat changes.
- Verify equipment window shows items in correct slots.
- Verify inventory cells are not duplicated or corrupted.
- Verify no delayed damage/metin collapse regression.

Commit status:
- Not committed yet. User requested review before commit.

## 2026-04-26 - Phase 15E-19: UseItemEx ECS dispatcher boundary

Scope:
- Extreme-complexity subsystem extraction, narrow implementation.
- Add an ECS-facing `UseItemEcs` dispatcher.
- Route only simple equip-toggle item types through ECS equip/unequip boundaries.
- Keep quest, refine, DragonSoul, special systems and complex item-use branches on the legacy `CHARACTER::UseItemEx` bridge.

Files changed:
- `SRC/Server/GameServer/ecs/systems/ItemSystem.hpp`
- `SRC/Server/GameServer/ecs/systems/ItemSystem.cpp`
- `docs/ecs_migration/MIGRATION_LOG_GPT.md`

Audit summary:
- `CHARACTER::UseItemEx` contains many branch classes:
  - direct equip toggle: `ITEM_COSTUME`, `ITEM_WEAPON`, `ITEM_ARMOR`, `ITEM_ROD`, `ITEM_RING`, `ITEM_BELT`, `ITEM_PICK`, `ITEM_SPECIAL_DS`
  - DragonSoul pullout: `ITEM_DS`
  - consumables and potions: `ITEM_USE`
  - blend items: `ITEM_BLEND`
  - special item systems: `ITEM_SPECIAL`
  - fish / treasure / boxes / skill books / refine scrolls / acce / switchbot / quest-like behavior
  - refine and socket/attribute mutation branches
- Several consumable count decrements were already routed through `ItemSystem::ConsumeItem` in Phase 15E-10.
- Equip/unequip boundaries were already introduced in Phase 15E-18.
- Conclusion: only simple equip-toggle dispatch is safe to extract now. Everything else remains legacy bridge with ECS sync.

New public ECS API:
```cpp
bool UseItemEcs(entt::entity owner, entt::entity item, TItemPos destCell = NPOS);
```

Implementation:
- Existing `UseItemEx(entt::entity, entt::entity, TItemPos)` now delegates to:
```cpp
return UseItemEcs(e, item, destCell);
```
- `UseItemEcs(...)` handles simple equip-toggle items directly when `destCell == NPOS`:
```cpp
ITEM_COSTUME
ITEM_WEAPON
ITEM_ARMOR
ITEM_ROD
ITEM_RING
ITEM_BELT
ITEM_PICK
ITEM_SPECIAL_DS
```
- The simple path calls:
```cpp
IsItemEquipped(item) ? UnequipItemEcs(owner, item)
                     : EquipItemEcs(owner, item);
```
- Complex branches call legacy:
```cpp
ch->UseItemEx(legacyItem, destCell);
```
- After legacy use:
  - if the legacy item still exists, `SyncItemStateFromLegacy(item)` runs
  - if the legacy item was consumed/deleted, the ECS item entity is destroyed and `CItemRegistry` is unregistered

Explicitly not migrated:
- `ITEM_DS` DragonSoul pullout.
- `ITEM_USE` complex branches beyond existing `ConsumeItem` mutations.
- Quest item logic.
- Refine scrolls.
- Socket/attribute mutation branches.
- `ITEM_BLEND`.
- `ITEM_SPECIAL`.
- Fish/box/special reward systems.
- Packet formats.
- `points[]`.

Validation:
- Build gate passed:
```powershell
cmake --build build --config RelWithDebInfo --target GameServer --parallel 8
```
- `GameServer.exe` linked successfully.
- Existing warning-class output remains present.
- Known post-step environment message remains:
```text
'pwsh.exe' is not recognized as an internal or external command
```
  Build returned exit code `0`.

Scans:
- `ItemSystem.hpp` public pointer leak scan:
```powershell
Select-String -Path 'SRC/Server/GameServer/ecs/systems/ItemSystem.hpp' -Pattern 'LPITEM|LPCHARACTER|CHARACTER\s*\*'
```
  Result: no matches.
- Dispatcher scan confirms:
  - `UseItemEcs` declaration + definition present.
  - `UseItemEx` wrapper delegates to `UseItemEcs`.
  - simple equip-toggle classifier exists.
  - legacy `CHARACTER::UseItemEx` still exists for complex branches.

Runtime risk:
- High. Item use is the largest gameplay dispatcher.
- Risk is constrained by extracting only simple equip-toggle dispatch and leaving complex branches on legacy.
- Manual WinTest is mandatory before expanding more branches.

Manual WinTest checklist:
- Double-click weapon equip/unequip.
- Double-click armor equip/unequip.
- Double-click costume body equip/unequip.
- Double-click costume weapon equip/unequip.
- Double-click belt/ring/rod/pick if available.
- Use potion and verify count decrement.
- Use buff/simple consumable.
- Use DragonSoul item if available and verify legacy behavior unchanged.
- Use refine scroll if available and verify legacy behavior unchanged.
- Relog and verify item state persists.
- Verify no duplicated/missing item, wrong owner, wrong slot/window.
- Verify no delayed damage/metin collapse regression.

Commit status:
- Not committed yet. User requested review before commit.

## 2026-04-26 - Phase 15E-20: LPITEM purge audit + ItemSystem LegacyItemOf isolation

Scope:
- Large cleanup/audit phase.
- Code changes limited to safe ItemSystem boundaries.
- No gameplay semantic rewrites.
- No quest, DragonSoul, refine, shop/exchange/safebox/offlineshop, packet, inventory layout, or `points[]` changes.

Files changed:
- `SRC/Server/GameServer/ecs/systems/ItemSystem.hpp`
- `SRC/Server/GameServer/ecs/systems/ItemSystem.cpp`
- `docs/ecs_migration/MIGRATION_LOG_GPT.md`

Project-wide LPITEM baseline:
- Total project `LPITEM` count before this pass: `977`.
- Total project `LPITEM` count after this pass: `978`.
- The count increased by one because bridge isolation introduced explicit private helper signatures. This is acceptable for this phase because the direct `LegacyItemOf` scatter was reduced and categorized.

LPITEM island map after this pass:
```text
369 Other / needs triage
286 ItemSystem bridge island
 91 Item core legacy island
 71 Quest / questlua island
 71 DragonSoul island
 39 Shop / exchange / safebox / offlineshop island
 32 Command / GM island
 20 Input / packet island
 14 Refine island
 13 Dungeon / event island
```

Top files by LPITEM count:
```text
260 SRC/Server/GameServer/ecs/systems/ItemSystem.cpp
 53 SRC/Server/GameServer/questlua_pc.cpp
 51 SRC/Server/GameServer/DragonSoul.cpp
 47 SRC/Server/GameServer/char.h
 36 SRC/Server/GameServer/questlua_item.cpp
 36 SRC/Server/GameServer/ecs/systems/PlayerRuntimeSystem.cpp
 27 SRC/Server/GameServer/ecs/systems/CombatSystem.cpp
 27 SRC/Server/GameServer/item_manager.cpp
 19 SRC/Server/GameServer/cmd_gm.cpp
 19 SRC/Server/GameServer/char_manager.cpp
```

ItemSystem baseline:
- Initial `ItemSystem.cpp` `LPITEM` count: `259`.
- Final `ItemSystem.cpp` `LPITEM` count: `260`.
- Initial direct `LegacyItemOf(` count: `36`.
- Final direct `LegacyItemOf(` count: `4`.
- Final bridge helper counts:
```text
ResolveLegacyItemForSync(: 1
ResolveLegacyItemForLegacySideEffect(: 35
ResolveLegacyItemForDestruction(: 2
DestroyItemEntityAndLegacy(: 4
```

Code changes:
- Removed unused public compatibility API:
```cpp
entt::entity AutoGiveItemEntity(entt::entity owner, uint32_t itemVnum,
                                uint32_t count = 1, int rarePct = -1,
                                bool sendMessage = true);
```
- Removed its definition from `ItemSystem.cpp`.
- Scan confirmed no external `AutoGiveItemEntity` call sites existed.

Legacy bridge isolation:
- Added private `.cpp`-local bridge helpers:
```cpp
static LPITEM ResolveLegacyItemForSync(entt::entity itemEntity);
static LPITEM ResolveLegacyItemForLegacySideEffect(entt::entity itemEntity);
static LPITEM ResolveLegacyItemForDestruction(entt::entity itemEntity);
```
- Replaced scattered direct `LegacyItemOf(...)` calls with bridge helper calls.
- Behavior remains unchanged; this is naming/isolation, not semantic migration.

Destruction helper:
- Added private `.cpp`-local helper:
```cpp
static bool DestroyItemEntityAndLegacy(entt::entity itemEntity, const char* reason);
```
- It:
  - validates ECS item entity
  - reads `ecs::ItemIdentity` when available
  - unregisters `CItemRegistry`
  - destroys ECS entity
  - removes legacy item only if it still exists
  - supports a reason string for legacy `ITEM_MANAGER::RemoveItem`
- Used in:
  - `ConsumeItem`
  - `UseItemEcs` consumed/deleted legacy branch
  - `ReceiveItemEcs` consumed/deleted legacy branch
- Not used for blend merge `M2_DESTROY_ITEM` path because that legacy destruction semantic is different and intentionally left untouched.

Read-only replacement result:
- No additional broad read-only replacements were performed in this pass.
- Reason: the remaining readable LPITEM usages in `ItemSystem.cpp` are mostly adjacent to side effects, sync, deletion, refine, DragonSoul, quest or legacy packet behavior.
- Existing public ECS item read APIs remain the intended replacement surface:
  - `GetItemID`
  - `GetItemVnum`
  - `GetItemOriginalVnum`
  - `GetItemType`
  - `GetItemSubType`
  - `GetItemCount`
  - `GetItemSocket`
  - `GetItemAttributeType`
  - `GetItemAttributeValue`
  - `GetItemWindow`
  - `GetItemCell`
  - `IsItemEquipped`
  - `GetItemOwner`

Validation:
- Build gate passed:
```powershell
cmake --build build --config RelWithDebInfo --target GameServer --parallel 8
```
- `GameServer.exe` linked successfully.
- Existing warning-class output remains present.
- Known post-step environment message remains:
```text
'pwsh.exe' is not recognized as an internal or external command
```
  Build returned exit code `0`.

Required scans:
- `ItemSystem.hpp` pointer leak scan:
```powershell
Select-String -Path 'SRC/Server/GameServer/ecs/systems/ItemSystem.hpp' -Pattern 'LPITEM|LPCHARACTER|CHARACTER\s*\*'
```
  Result: no matches.
- `AutoGiveItemEntity` scan:
  Result: no matches.
- `ItemSystem.cpp` direct `LegacyItemOf(` count:
  Result: `4`, limited to the resolver itself and the three named resolver helpers.

Remaining blockers:
- `ItemSystem.cpp` still has many LPITEM declarations because it still contains legacy `CHARACTER` and `CItem` method bodies in the same file.
- Full LPITEM removal requires splitting/moving legacy item code out of `ItemSystem.cpp` or migrating those method bodies, not just replacing wrapper calls.
- The largest non-ItemSystem islands are questlua, DragonSoul, CItem core, PlayerRuntime/Combat and storage/trade paths.

Next LPITEM purge roadmap:
- `15E-21 Quest LPITEM cleanup`
  - quest current item bridge
  - `questlua_pc` reward paths
  - `questlua_item` direct `LPITEM` calls
  - quest manager item references
- `15E-22 DragonSoul LPITEM cleanup`
  - DS inventory/refine/byproduct paths
  - `DSManager` item parameter cleanup
  - DS packet boundary entity conversion
- `15E-23 Refine LPITEM cleanup`
  - `DoRefine` / `RefineItem` internals
  - scroll/refine socket mutation bridges
  - refine result item creation
- `15E-24 Trade/storage LPITEM cleanup`
  - exchange
  - shop/shopEx
  - safebox/mall
  - new offlineshop
- `15E-25 Global LPITEM final purge`
  - item manager/core CItem island
  - command/GM residual calls
  - input/packet residual calls
  - dungeon/event residual calls
  - battle/combat weapon access

Manual WinTest checklist:
- Login/relog.
- Item grant.
- Stack merge.
- Full inventory fallback.
- Use potion.
- Equip/unequip weapon.
- Equip/unequip armor.
- NPC item hand-in.
- Refine smoke test.
- DragonSoul smoke test if available.
- Box/special item open.
- Verify no duplicate item.
- Verify no missing item.
- Verify no wrong owner.
- Verify no wrong window/cell.
- Verify no delayed damage/metin collapse regression.

Commit status:
- Not committed yet. User requested review before commit.

## 2026-04-26 - Phase 15E-14: Migrate first real AutoGiveItem callers to AutoGiveItemEcs

Scope:
- Controlled production migration of low-risk grant-by-vnum callers.
- Do not migrate questlua, DragonSoul, refine, socket/attribute mutation, or special-group paths.

Files changed:
- `SRC/Server/GameServer/cmd_general.cpp`
- `SRC/Server/GameServer/ecs/systems/ActivitySystem.cpp`
- `SRC/Server/GameServer/ecs/systems/ItemSystem.cpp`
- `docs/ecs_migration/MIGRATION_LOG_GPT.md`

Batch A - candidate classification:
- LOW candidates selected:
  - return ignored.
  - or return only checked against `entt::null`.
  - no LPITEM storage.
  - no socket/attribute mutation.
  - no quest current item.
  - no DragonSoul/refine/special-group behavior.
- HIGH candidates left untouched:
  - questlua reward paths.
  - DragonSoul byproduct/reward paths.
  - fishing/activity paths that still use LPITEM metadata or mutation.
  - special item group internals.
  - callers using `LPITEM` return for ID/vnum/socket/attribute/current-item.

Batch B/C - migrated callers:
- Replaced previous entity-return legacy bridge calls:
```cpp
ItemSystem::AutoGiveItemEntity(...)
```
- With ECS-native grant path:
```cpp
ItemSystem::AutoGiveItemEcs(...)
```
- Migrated locations:
  - `cmd_general.cpp:241`
  - `cmd_general.cpp:3124`
  - `cmd_general.cpp:3176`
  - `ActivitySystem.cpp:480`
  - `ItemSystem.cpp:8198`
  - `ItemSystem.cpp:8219`
  - `ItemSystem.cpp:8226`
  - `ItemSystem.cpp:8233`
  - `ItemSystem.cpp:11705`
  - `ItemSystem.cpp:11786`
  - `ItemSystem.cpp:13464`

Batch D - left untouched:
- Direct legacy `ch->AutoGiveItem(...)` callers remain.
- `AutoGiveItemEntity(...)` remains declared/defined as compatibility bridge but now has no external call sites outside its own declaration/definition.

Validation:
- Build gate passed:
```powershell
cmake --build build --config RelWithDebInfo --target GameServer --parallel 8
```
- `ItemSystem.hpp` pointer leak scan remains clean:
```powershell
LPITEM|LPCHARACTER|CHARACTER* => 0
```
- `AutoGiveItemEntity` remaining references:
  - declaration
  - definition
- `AutoGiveItemEcs` is now used in production callers.

Runtime risk:
- Medium. These are the first real production callers using the new ECS grant path.
- Manual test must focus on grant, stack merge, inventory placement, full inventory fallback, and relog persistence.

Manual WinTest checklist:
- GM/simple give command.
- Stone craft single reward.
- Stone craft bulk reward.
- Activity reward.
- Internal use-item grant paths touched in `ItemSystem.cpp`.
- Full inventory case.
- Stack merge case.
- Relog persistence.
- Verify no duplicate/missing item.
- Verify no wrong slot/window/owner.

Commit status:
- Not committed yet. User requested review before commit.

## 2026-04-26 - Phase 15E-13: Complete AutoGiveItemEcs parity with legacy

Scope:
- Improve `AutoGiveItemEcs` behavior parity with legacy `CHARACTER::AutoGiveItem(vnum)`.
- Keep the ECS API additive.
- Do not migrate existing callers.
- Do not modify legacy `CHARACTER::AutoGiveItem`.

Files changed:
- `SRC/Server/GameServer/ecs/systems/ItemSystem.cpp`
- `docs/ecs_migration/MIGRATION_LOG_GPT.md`

Batch A - blend item merge:
- Added internal helper:
```cpp
HandleBlendItemMerge(entt::entity owner, entt::entity item)
```
- Mirrors legacy blend merge behavior:
  - only handles `ITEM_BLEND`.
  - scans owner inventory item entities.
  - matches same vnum.
  - matches sockets 0, 1, 2.
  - requires existing stack below `g_bItemCountLimit`.
  - merges count into existing stack via `SetItemCount`.
  - destroys/unregisters temporary incoming item with legacy destroy.
- Existing legacy blend behavior remains untouched.

Batch B - chat/log parity:
- Added internal helper:
```cpp
SendAutoGiveMessage(entt::entity owner, uint32_t count, LPITEM item)
```
- ECS grant path now sends the same `TEXTS_IMPROVEMENT` item gain message format used by legacy:
```cpp
ChatSystem::SendNew(..., 102, "%d#%s", count, itemName)
```
- Merge messages use the original requested count, not the remaining count after merge.
- Existing `LogManager::ItemLog` calls in placement already matched legacy for inventory/ground paths.

Batch C - quickslot auto-fill:
- `PlaceItemInInventory` now mirrors the legacy potion quickslot behavior:
  - if granted item is `ITEM_USE` + `USE_POTION`
  - and quickslot 0 is empty
  - assign quickslot 0 to the item cell.

Batch D - full inventory fallback:
- Ground fallback remains legacy-assisted:
  - `AddToGround`
  - destroy timer
  - ownership timer
  - `SYSTEM_DROP` log
- ECS state is refreshed after fallback using `SyncItemStateFromLegacy`.

Batch E - DragonSoul routing:
- DragonSoul placement still uses legacy `GetEmptyDragonSoulInventory` and `AddToCharacter` to preserve storage/packet behavior.
- No DragonSoul semantics were changed.

Batch F - merge correctness:
- ECS merge path already handles:
  - partial merge through remaining count.
  - count limit.
  - socket default match.
  - extra vs main inventory separation.
- Blend special merge was added after creation to mirror the legacy post-creation blend branch.

Batch G - internal validation:
- No temporary dual-run compare was added.
- Reason: calling legacy and ECS grant paths together would grant twice. A non-mutating simulation path is needed before safe automated comparison can exist.

Validation:
- Build gate passed:
```powershell
cmake --build build --config RelWithDebInfo --target GameServer --parallel 8
```
- `ItemSystem.hpp` pointer leak scan remains clean:
```powershell
LPITEM|LPCHARACTER|CHARACTER* => 0
```
- `LegacyItemOf(` count after this phase: 34 matches including helper definition.
- Increase is from explicit parity helpers and legacy sync boundaries; no existing gameplay caller was switched.

Remaining known gaps:
- `sendMessage` false is respected by item gain message helper, but some legacy side effects may still differ in rare paths.
- `rarePct` is still accepted but not applied because the audited legacy grant function does not visibly use it.
- Full automated compare still requires a non-mutating simulation path.

Manual WinTest checklist before caller migration:
- Give stackable item repeatedly.
- Verify count message and stack count.
- Test partial merge at stack limit.
- Test blend item merge.
- Test potion grant into empty quickslot 0.
- Test full inventory ground fallback and ownership timer.
- Test DragonSoul item routing.
- Relog and verify persistence.
- Verify no duplicate/missing/wrong-slot item.

Commit status:
- Not committed yet. User requested review before commit.

## 2026-04-26 - Phase 15E-12: Implement ECS-native AutoGiveItem(vnum) path (parallel rewrite)

Scope:
- Add an additive ECS-native grant-by-vnum API.
- Do not replace legacy `CHARACTER::AutoGiveItem`.
- Do not modify existing reward/drop/quest callers.
- Preserve packet formats, inventory layout, DragonSoul behavior, refine behavior, and `points[]`.

Files changed:
- `SRC/Server/GameServer/ecs/systems/ItemSystem.hpp`
- `SRC/Server/GameServer/ecs/systems/ItemSystem.cpp`
- `docs/ecs_migration/MIGRATION_LOG_GPT.md`

New public API:
```cpp
entt::entity AutoGiveItemEcs(entt::entity owner,
                             uint32_t itemVnum,
                             uint32_t count = 1,
                             int rarePct = -1,
                             bool sendMessage = true);
```

Batch A - legacy behavior extracted:
- Legacy `CHARACTER::AutoGiveItem(vnum)` flow:
  - proto lookup through `ITEM_MANAGER::GetTable`.
  - stack merge before new item creation.
  - extra inventory stack branch for extra items.
  - main inventory stack branch for normal stackable items.
  - match by vnum/original vnum semantics and socket default checks.
  - respect `ITEM_FLAG_MAKECOUNT`.
  - respect `g_bItemCountLimit`.
  - create item only for remaining count.
  - special blend item merge happens after creation in legacy.
  - route to DragonSoul / extra inventory / main inventory.
  - ground fallback with ownership timer.
  - chat/log side effects.

Batch B/C - ECS inventory scan and stack merge:
- Added internal helpers:
```cpp
FindMergeTarget(...)
MergeIntoStack(...)
TryMergeItemVnum(...)
```
- ECS scan uses:
  - `ItemIdentity`
  - `ItemOwner`
  - `ItemLocation`
  - `ItemCount`
  - `ItemSockets`
- Merge matching currently supports:
  - owner PID match.
  - target inventory window match.
  - vnum match.
  - socket default match against item proto.
  - count below `g_bItemCountLimit`.
- Merge updates the existing stack through `SetItemCount`, keeping ECS and legacy count synchronized.

Batch D - item creation:
- Added internal helper:
```cpp
CreateItemEntityFromProto(...)
```
- Uses `ITEM_MANAGER::CreateItem(...)` for object creation, then immediately wraps/syncs with `EntityFactory::CreateItemEntity`.
- `rarePct` is accepted but not applied because the current legacy `CHARACTER::AutoGiveItem(vnum)` path does not visibly use `iRarePct` in the audited function body.

Batch E - placement/routing:
- Added internal helper:
```cpp
PlaceItemInInventory(...)
```
- Placement still uses legacy `AddToCharacter` / ground fallback as the sync boundary because packet/storage side effects are still legacy-owned.
- Routing order matches legacy intent:
  - DragonSoul
  - Extra inventory
  - Main inventory
  - Ground fallback
- After placement, `SyncItemStateFromLegacy` refreshes ECS state.

Batch F - full additive flow:
- `AutoGiveItemEcs` now:
  - resolves owner.
  - resolves proto.
  - logs money drop like legacy.
  - applies `ITEM_FLAG_MAKECOUNT`.
  - tries ECS stack merge before creation for extra/main stackable items.
  - creates only remaining count.
  - places created item through legacy sync boundary.
  - returns merged or created item entity.

Batch G - compare/debug:
- No temporary dual-run comparison was added.
- Reason: calling both legacy `AutoGiveItemEntity` and `AutoGiveItemEcs` would grant items twice unless wrapped in a non-mutating simulation, which does not exist yet.

Known gaps vs legacy:
- Blend item post-creation special merge is not yet implemented in the ECS path.
- Chat message side effects for stack merge/new grant are not fully mirrored.
- Quickslot auto-fill for potion is not mirrored.
- Placement still depends on legacy `LPITEM` and `AddToCharacter` for packet/storage correctness.
- DragonSoul routing is delegated to legacy placement helper and not ECS-native.
- Ground fallback still uses legacy ground/ownership behavior.

Validation:
- Build gate passed:
```powershell
cmake --build build --config RelWithDebInfo --target GameServer --parallel 8
```
- `ItemSystem.hpp` pointer leak scan:
```powershell
LPITEM|LPCHARACTER|CHARACTER* => 0
```
- `AutoGiveItemEcs` exists only as additive API; no existing callers were migrated.
- `LegacyItemOf(` count after this phase: 31 matches including helper definition.

Risk assessment:
- Low runtime risk because the new API is not wired into existing gameplay callers.
- Medium future risk if callers are migrated before closing blend/chat/quickslot gaps.
- High risk if used for DragonSoul or full-inventory edge cases before manual testing.

Manual WinTest checklist before any caller migration:
- Give stackable item repeatedly through a temporary controlled command.
- Verify merge into existing stack.
- Verify partial merge at stack limit.
- Verify no duplicate item.
- Verify correct count after relog.
- Verify full inventory fallback.
- Verify extra inventory item stack routing.
- Verify DragonSoul item fallback if tested.
- Verify blend item behavior remains legacy until explicitly supported.

Commit status:
- Not committed yet. User requested review before commit.

## 2026-04-26 - Phase 15E-11: Extract stack merge flow to ECS-first model

Scope:
- Audit `CHARACTER::AutoGiveItem` stack merge behavior.
- Determine whether `MergeItemStack(owner, incomingItem)` can safely be integrated into `AutoGiveItemEntity`.
- Do not change stack merge behavior, packets, inventory layout, DragonSoul, quest, refine, or `points[]`.

Files changed:
- `docs/ecs_migration/MIGRATION_LOG_GPT.md`

Code changes:
- None in this phase.

Batch A - legacy merge logic identified:
- `CHARACTER::AutoGiveItem(LPITEM item, ...)` existing-item path:
  - rejects null item and item with owner.
  - extra inventory stack branch:
    - `item->IsExtraItem()`
    - `item->IsStackable()`
    - `!ITEM_ANTIFLAG_STACK`
    - scans `EXTRA_INVENTORY_MAX_NUM`
    - matches same vnum
    - requires every socket to match
    - increments existing stack up to `g_bItemCountLimit`
    - decrements incoming count
    - destroys incoming item if fully merged
  - main inventory branch:
    - same stackable / anti-stack checks
    - scans `INVENTORY_MAX_NUM`
    - matches same vnum
    - requires socket equality
    - increments existing stack up to `g_bItemCountLimit`
    - decrements or destroys incoming item
  - then routes remaining item to DragonSoul, extra inventory, main inventory, or ground.
- `CHARACTER::AutoGiveItem(vnum, ...)` vnum path:
  - reads proto table first.
  - before creating any new item, scans existing extra/main inventory stacks.
  - if fully merged, increments existing stack and returns that existing `LPITEM`.
  - only creates a new item when stack merge did not fully consume the requested count.
  - blend item has an additional separate stack/merge behavior.

Batch B/C - ECS merge API design:
- Proposed API:
```cpp
entt::entity MergeItemStack(entt::entity owner, entt::entity incomingItem);
```
- Safe only for an actual incoming item entity.
- Not sufficient for the current critical `AutoGiveItem(vnum)` merge path because that path merges before creating an incoming item.
- Implementing it against `AutoGiveItemEntity` after `ch->AutoGiveItem(...)` would be too late: legacy merge has already happened.

Batch D - integration decision:
- `AutoGiveItemEntity(...)` currently delegates to:
```cpp
LPITEM item = ch->AutoGiveItem(itemVnum, count, rarePct, sendMessage);
```
- That legacy call already performs stack merge and returns either:
  - existing merged item,
  - newly created item,
  - null.
- Running an ECS merge after this would risk double merge or wrong count.
- Bypassing legacy merge would require replacing item creation, stack search, make-count handling, blend behavior, empty-cell routing, AddToCharacter, ground fallback, ownership timers, and chat/log behavior.
- That is outside this phase's safe scope.

Conclusion:
- ECS merge was not implemented.
- No legacy merge was bypassed.
- No runtime behavior was changed.
- Safe next design is a lower-level planned API, not direct replacement:
```cpp
FindMergeTarget(owner, itemVnum, sockets, inventoryClass)
ApplyStackMerge(targetEntity, count)
CreateOrGrantItemEntity(...)
```
- This should be done only after entity-backed inventory scan no longer depends on `LPITEM pItems[]` arrays.

Validation:
- Build gate passed:
```powershell
cmake --build build --config RelWithDebInfo --target GameServer --parallel 8
```
- Build produced `GameServer.exe` successfully.

Risk areas if attempted prematurely:
- double merge
- wrong count after legacy already merged
- duplicate item if temporary item destruction is mishandled
- missing item if ECS entity is destroyed before legacy path finishes
- wrong inventory class for extra inventory vs main inventory
- blend item behavior mismatch
- lost chat/log side effects
- ground fallback / ownership timer mismatch

Manual WinTest checklist:
- No runtime code changed in this phase.
- Existing item grant behavior should remain unchanged.
- If testing after previous phases:
  - give stackable item repeatedly
  - verify merge count
  - verify no duplicate item
  - verify full inventory fallback
  - relog persistence

Commit status:
- Not committed yet. User requested review before commit.

## 2026-04-26 - Phase 15E-10: Extract consumable item flow to ECS-first model

Scope:
- Extract one narrow item mutation flow: simple consumable stack decrement.
- Introduce an ECS-first consume API.
- Do not touch refine, DragonSoul, quest items, socket/attribute logic, ownership/location, packet formats, or `points[]`.

Files changed:
- `SRC/Server/GameServer/ecs/systems/ItemSystem.hpp`
- `SRC/Server/GameServer/ecs/systems/ItemSystem.cpp`
- `docs/ecs_migration/MIGRATION_LOG_GPT.md`

Batch A - consumable path audit:
- Searched `ITEM_USE` / `UseItemEx` branches for:
```cpp
item->SetCount(item->GetCount() - 1)
```
- Remaining direct `item->SetCount(item->GetCount() - 1)` count after migration: 95.
- Safe candidates selected:
  - `USE_CLEAR`
  - `USE_POTION`
  - `USE_POTION_CONTINUE`
  - `USE_ABILITY_UP`
- Skipped categories:
  - socket mutations
  - attribute mutations
  - refine / scroll refine
  - DragonSoul
  - quest/current item flows
  - pet/mount/metin/recall/special systems
  - branches with manual `RemoveItem` in the same flow
  - branches with item deletion or packet-sensitive side effects

Batch B - ECS consumable API:
- Added:
```cpp
bool ConsumeItem(entt::entity item, uint32_t amount = 1);
```
- Behavior:
  - Reads current count from ECS via `GetItemCount`.
  - If `count > amount`, calls `SetItemCount(item, count - amount)`.
  - If `count <= amount`, destroys/unregisters the item entity and calls legacy `ITEM_MANAGER::RemoveItem(...)` as the sync layer.
  - Rejects invalid entity and zero amount.
- This is the first ECS-first consumable mutation API.

Batch C - migrated safe consumable flows:
- Replaced four direct legacy count decrements:
```cpp
item->SetCount(item->GetCount() - 1);
```
- With:
```cpp
ItemSystem::ConsumeItem(EntityFactory::CreateItemEntity(g_registry, item));
```
- Migrated line sites after current edits:
  - `ItemSystem.cpp:9476`
  - `ItemSystem.cpp:9663`
  - `ItemSystem.cpp:9684`
  - `ItemSystem.cpp:9740`

Batch D/E - consistency:
- For positive remaining count, `SetItemCount` keeps ECS and legacy count synced.
- For zero count, entity is destroyed/unregistered before legacy removal.
- Packet behavior remains legacy-managed through the existing legacy item removal/count mechanisms.

LegacyItemOf result:
- Before this phase: 29 matches including helper definition.
- After this phase: 30 matches including helper definition.
- Reason for increase: new `ConsumeItem` API owns the bridge internally for zero-count legacy sync.
- Direct legacy mutation paths were reduced by 4 even though the central bridge count increased by 1.

Validation:
- Build gate passed:
```powershell
cmake --build build --config RelWithDebInfo --target GameServer --parallel 8
```
- `ItemSystem.hpp` public pointer leak scan:
```powershell
LPITEM|LPCHARACTER|CHARACTER* => 0
```
- Existing warning-class output only; `GameServer.exe` linked successfully.

Risks:
- `ConsumeItem` now destroys the ECS item entity when the stack reaches zero and then lets legacy remove the item.
- Manual testing must verify that legacy removal does not need the ECS item entity after this point.
- The migrated branches should be tested with count 1 and count > 1 stacks.

Manual WinTest checklist:
- Use `USE_CLEAR` item with stack > 1 and stack = 1.
- Use normal potion stack repeatedly.
- Use potion continue item.
- Use ability-up potion/item.
- Verify stack decrement.
- Verify item disappears at zero.
- Relog and verify persistence.
- Verify no duplicate or missing item.
- Verify no delayed damage/metin collapse regression.

Commit status:
- Not committed yet. User requested review before commit.

## 2026-04-26 - Phase 15E-9: Replace legacy item mutation paths with ECS-first write model

Scope:
- Audit legacy item mutation sites for possible ECS-first replacement.
- Only replace call sites that are standalone, entity-backed, and not tied to refine, DragonSoul, quest, packets, ownership/location, deletion, equip, or legacy `CHARACTER/CItem` semantics.

Files changed:
- `docs/ecs_migration/MIGRATION_LOG_GPT.md`

Code changes:
- None in this phase.
- Reason: no safe mutation call site met the replacement criteria.

Batch A - mutation audit:
- Mutation patterns scanned:
  - `item->SetCount(...)`
  - `item->SetSocket(...)`
  - `item->SetAttribute(...)`
- Total direct mutation hits in `ItemSystem.cpp`: 207.
- Classification:
  - SAFE: 0 direct legacy call sites.
  - UNSAFE: all remaining direct legacy mutation sites.
- Main unsafe categories:
  - Stack merge / split / consume paths with packet, deletion, or inventory side effects.
  - Use-item branches with quest/script/affect/special-item behavior.
  - Socket-based timed items, auto-potion, mount/pet/metin/recall behavior.
  - Refine and scroll refine transaction paths.
  - DragonSoul and special inventory routing.
  - Add/remove/equip ownership and location transitions.
  - Event/timer item expiration flows.

Batch B - count mutation replacement:
- No `item->SetCount(...)` call was replaced.
- Reason: every audited candidate was coupled to legacy item lifecycle, deletion, packet sync, stack merge, or item-use semantics.
- Existing ECS-first API remains available:
```cpp
void SetItemCount(entt::entity item, uint32_t count);
```

Batch C - socket mutation replacement:
- No `item->SetSocket(...)` call was replaced.
- Reason: socket writes are tied to timed sockets, refine/metin behavior, pet/mount data, auto-potion state, recall coordinates, or event timers.
- Existing ECS-first API remains available:
```cpp
bool SetItemSocket(entt::entity item, int index, uint32_t value);
```

Batch D - attribute mutation replacement:
- No `item->SetAttribute(...)` call was replaced.
- Reason: direct attribute writes are inside legacy randomization/refine-style `CItem` logic and `CItem::SetAttribute` is private.
- Existing ECS-first API remains available:
```cpp
bool SetItemAttribute(entt::entity item, int index, int type, int value);
```

Batch E/F - ECS-first mutation pattern:
- No new production flow was flipped in this phase.
- The safe ECS-first APIs from 15E-7/15E-8 remain the migration target for future isolated call sites.
- Legacy remains primary in the audited direct mutation sites until a dedicated semantic phase extracts each flow.

Batch G - validation:
- `LegacyItemOf(` count before: 29.
- `LegacyItemOf(` count after: 29.
- `ItemSystem.hpp` public pointer leak scan:
```powershell
LPITEM|LPCHARACTER|CHARACTER* => 0
```
- Build gate passed:
```powershell
cmake --build build --config RelWithDebInfo --target GameServer --parallel 8
```
- Build reused existing objects and produced `GameServer.exe` successfully.

Conclusion:
- Safe replacement count is zero for this phase.
- Proceeding with automated replacements here would risk item deletion, packet sync, stack merge, timed socket, refine, DragonSoul, or quest behavior.
- Recommended next step is not broad replacement; it is extraction of one narrow semantic flow at a time:
  - consumable count decrement flow
  - stack merge count flow
  - auto-potion socket flow
  - recall item socket flow
  - timed unique item socket flow
  - refine transaction bridge

Manual WinTest checklist:
- No runtime code changed in this phase.
- After previous 15E-8 code changes still test:
  - item count changes
  - socket-based items
  - attribute-based items
  - relog persistence
  - no duplicate/missing items
  - no owner/cell/window desync

Commit status:
- Not committed yet. User requested review before commit.

## 2026-04-26 - Phase 15E-8: Item ECS authority expansion + LegacyItemOf isolation

Scope:
- Expand ECS item data authority with prototype metadata, socket/attribute write APIs, location/equipped reads, and explicit sync APIs.
- Keep `ItemSystem.hpp` free of `LPITEM`.
- Do not change packet formats, inventory layout, `CHARACTER::UseItemEx`, equip/unequip semantics, refine semantics, DragonSoul behavior, or `points[]`.

Files changed:
- `SRC/Server/GameServer/ecs/components/item_components.hpp`
- `SRC/Server/GameServer/ecs/EntityFactory.cpp`
- `SRC/Server/GameServer/ecs/systems/ItemSystem.hpp`
- `SRC/Server/GameServer/ecs/systems/ItemSystem.cpp`
- `docs/ecs_migration/MIGRATION_LOG_GPT.md`

Batch A - item authority audit:
- ECS item fields already mirrored:
  - `ItemIdentity`: id, original vnum, VID, mask vnum.
  - `ItemLocation`: window, cell.
  - `ItemCount`: count.
  - `ItemOwner`: owner PID, last owner PID, ownership PID.
  - `ItemEquipped`: equipped flag.
  - `ItemFlags`: flags, exchanging, skipSave, locked.
  - `ItemSockets`: socket array.
  - `ItemAttributes`: attribute array.
- Missing before this phase:
  - prototype type/subtype metadata.
- Stale-risk areas:
  - legacy direct `SetSocket`, `SetCount`, `SetAttributes`, `AddToCharacter`, `RemoveFromCharacter`, `EquipTo`, refine remove/create flows.

Batch B - prototype metadata component:
- Added:
```cpp
struct ItemPrototypeMeta {
    uint8_t type = 0;
    uint8_t subType = 0;
};
```
- Populated in `EntityFactory::SyncItemEntity` from:
```cpp
item->GetType()
item->GetSubType()
```
- Updated:
```cpp
GetItemType(entt::entity item)
GetItemSubType(entt::entity item)
```
- These now read ECS `ItemPrototypeMeta` directly.

Batch C - socket write/sync APIs:
- Added:
```cpp
bool SetItemSocket(entt::entity item, int index, uint32_t value);
bool SyncItemSocketsFromLegacy(entt::entity item);
bool SyncLegacySocketsFromEcs(entt::entity item);
```
- `SetItemSocket` updates ECS `ItemSockets` first and mirrors to legacy if present.
- No existing high-risk socket mutation call sites were migrated.

Batch D - attribute write/sync APIs:
- Added:
```cpp
bool SetItemAttribute(entt::entity item, int index, int type, int value);
bool ClearItemAttribute(entt::entity item, int index);
bool SyncItemAttributesFromLegacy(entt::entity item);
bool SyncLegacyAttributesFromEcs(entt::entity item);
```
- `SetItemAttribute` updates ECS `ItemAttributes` first and mirrors to legacy through public `SetAttributes(...)`.
- Direct `CItem::SetAttribute(...)` was not usable because it is private.
- No random attribute generation/refine/acce/costume transfer semantics were changed.

Batch E - location/equipped read APIs:
- Added:
```cpp
uint8_t GetItemWindow(entt::entity item);
uint16_t GetItemCell(entt::entity item);
bool IsItemEquipped(entt::entity item);
bool IsItemInInventory(entt::entity item);
bool IsItemInExtraInventory(entt::entity item);
bool IsItemInDragonSoulInventory(entt::entity item);
```
- These prefer ECS `ItemLocation` / `ItemEquipped`.
- Legacy fallback remains if component coverage is missing.

Batch F - controlled sync APIs:
- Added:
```cpp
bool SyncItemLocationFromLegacy(entt::entity item);
bool SyncItemOwnerFromLegacy(entt::entity item);
bool SyncItemStateFromLegacy(entt::entity item);
```
- These read legacy `CItem` and update ECS components only.
- They do not send packets, save directly, or mutate gameplay behavior.
- Added post-success sync at wrapper boundaries:
  - `SetWearItem`
  - `UnequipItem`
  - `EquipItem`
  - `UseItemEx`
  - existing-item `AutoGiveItem`
  - `AutoGiveDS`
  - `ReceiveItem`
  - `DoRefine`
  - `DoRefineWithScroll`
  - `DoRefineItemSoul`
  - `RefineItem`

Batch G - isolated socket/attribute callers:
- No direct legacy call sites were migrated.
- Reason: available `SetSocket`, `SetAttribute`, `SetCount` sites are overwhelmingly inside legacy `LPITEM`-based item-use/refine/timer/quest/DragonSoul-sensitive flows.
- Moving those requires dedicated semantic phases.

LegacyItemOf result:
- Initial count before this phase: 17 matches including helper definition.
- Final count after this phase: 29 matches including helper definition.
- The raw count increased because new sync APIs intentionally isolate bridge usage inside explicit ECS sync boundaries.
- Effective result: `LegacyItemOf` is more centralized and purpose-classified, not removed.

Remaining categories:
- Equip/unequip bridge.
- `UseItemEx` bridge.
- Existing-item AutoGive/Receive ownership transfer bridge.
- Refine bridge.
- Socket/attribute legacy sync bridge.
- Location/owner/state sync bridge.
- Special systems still embedded in legacy `CHARACTER/CItem` methods.

Recommended next phases:
- Migrate isolated count mutation callers to `SetItemCount`.
- Add item packet sync API before touching packet-sensitive item mutations.
- Add ownership/location mutation API before replacing `AddToCharacter` / `RemoveFromCharacter`.
- Add dedicated refine bridge phase.
- Add dedicated DragonSoul bridge phase.
- Add quest current item bridge before quest item pointer cleanup.

Validation:
- Build gate passed after implementation:
```powershell
cmake --build build --config RelWithDebInfo --target GameServer --parallel 8
```
- `ItemSystem.hpp` public pointer leak scan:
```powershell
LPITEM|LPCHARACTER|CHARACTER* => 0
```
- New API scan confirms:
  - `SetItemSocket`
  - `SetItemAttribute`
  - `SyncItemStateFromLegacy`
  - `GetItemWindow`
- Existing warning-class output only; `GameServer.exe` linked successfully.

Manual WinTest checklist:
- Login/relog.
- Open inventory.
- Grant normal item.
- Grant stackable item and verify merge.
- Modify item count through safe path if available.
- Use consumable.
- Equip/unequip.
- Item with socket behavior.
- Item with attributes.
- Box/special group open.
- Refine smoke test.
- DragonSoul smoke test if available.
- Verify no duplicate item.
- Verify no missing item.
- Verify no wrong owner.
- Verify no wrong cell/window.
- Verify no delayed damage/metin collapse regression.

Commit status:
- Not committed yet. User requested review before commit.

## 2026-04-26 - Phase 15E-7: Introduce ECS item write + ownership API and reduce LegacyItemOf scope

Scope:
- Introduce the first safe ECS-native item mutation/read ownership APIs.
- Do not remove `LegacyItemOf` yet.
- Do not change item storage, packets, DragonSoul, refine semantics, or `points[]`.

Files changed:
- `SRC/Server/GameServer/ecs/systems/ItemSystem.hpp`
- `SRC/Server/GameServer/ecs/systems/ItemSystem.cpp`
- `docs/ecs_migration/MIGRATION_LOG_GPT.md`

Batch A - write operation audit:
- Initial `LegacyItemOf(` count: 18 matches including the helper definition.
- Mutation categories found in `ItemSystem.cpp`:
  - Socket write: many `SetSocket(...)` paths, including use-item, timers, refine, metin, recall, auto-potion, unique item expiry.
  - Attribute write: `SetAttribute(...)`, `SetAttributes(...)`, attr copy/refine style flows.
  - Count/stack: many `SetCount(...)` paths in consume, stack merge, rewards, refine cost, quest/item-use paths.
  - Ownership/location: `AddToCharacter`, `RemoveFromCharacter`, `SetWindow`, `SetCell`, `SetSkipSave`.
  - Equip state: `EquipTo`, equip/unequip wrapper paths.
  - Refine-sensitive: `DoRefine`, `DoRefineWithScroll`, `DoRefineItemSoul`, `RefineItem`, remove/create/add-to-character sequences.
  - DragonSoul-sensitive: inventory routing and special storage paths.
- Only count and ownership read were selected for implementation.

Batch B - safe ECS write API:
- Added:
```cpp
void SetItemCount(entt::entity item, uint32_t count);
```
- Behavior:
  - Updates `ecs::ItemCount` if the item entity is valid.
  - Updates legacy `CItem` count through the existing bridge when present.
  - Keeps ECS and legacy count in sync.
- No existing count mutation paths were redirected yet because most are embedded in legacy behavior-sensitive flows.

Batch C - ownership read API:
- Added:
```cpp
entt::entity GetItemOwner(entt::entity item);
```
- Behavior:
  - Reads `ecs::ItemOwner.ownerPID` first.
  - Resolves the owner character through `CHARACTER_MANAGER::instance().FindByPID(...)` and `AIHelpers::EcsOf(...)`.
  - Falls back to legacy item owner lookup only if ECS owner component resolution is unavailable.
- Ownership behavior is read-only and unchanged.

Additional LegacyItemOf reduction:
- `GetItemType` and `GetItemSubType` no longer use `LegacyItemOf`.
- They now read static prototype metadata through:
```cpp
ITEM_MANAGER::instance().GetTable(GetItemVnum(item))
```
- This avoids item pointer fallback for type/subtype reads.

Batch D - trivial flow replacement:
- No broad count-flow migration was performed.
- Reason: most `SetCount` flows are inside legacy item-use/refine/reward paths and are coupled to deletion, packets, ownership, or quest behavior.
- The new `SetItemCount` API is available for future isolated call-site migration.

Batch E - remaining blockers:
- Final `LegacyItemOf(` count: 17 matches including the helper definition.
- Remaining categories:
  - Equip/unequip wrappers: need dedicated equip-state bridge.
  - `UseItemEx`: item script and behavior dispatch; still legacy-sensitive.
  - Existing-item AutoGive/Receive: ownership transfer semantics.
  - Refine wrappers: refine transaction semantics.
  - `RefineItem`: requires item and target legacy pointer internally.
  - `SetItemCount`: intentionally keeps legacy sync until full item storage authority moves to ECS.

Missing APIs/components for full removal:
- Socket write API.
- Attribute write API.
- Ownership/location mutation API.
- Equip state mutation API.
- Item packet sync API.
- Refine bridge.
- DragonSoul bridge.
- Quest current item bridge.

Validation:
- Build gate passed after API implementation:
```powershell
cmake --build build --config RelWithDebInfo --target GameServer --parallel 8
```
- `ItemSystem.hpp` `LPITEM` scan result: 0.
- Existing warning-class output only; `GameServer.exe` linked successfully.
- Known post-build environment warning remains: `pwsh.exe` not recognized.

Manual WinTest checklist:
- Login/relog.
- Normal inventory open.
- Item grant and stack merge.
- Use item.
- Equip/unequip.
- Box/special group open.
- Refine smoke test.
- Verify no duplicate, missing item, wrong owner, wrong slot, or persistence regression.

Commit status:
- Not committed yet. User requested review before commit.

## 2026-04-26 - Phase 15E-6: Reduce LegacyItemOf usage by moving item reads to ECS components

Scope:
- Reduce internal `LegacyItemOf(entt::entity)` dependency in `ItemSystem.cpp`.
- Prefer pure ECS component reads only where component coverage already exists.
- Preserve all item storage, packet, ownership, merge, refine, DragonSoul, and `points[]` behavior.

Files changed:
- `SRC/Server/GameServer/ecs/systems/ItemSystem.hpp`
- `SRC/Server/GameServer/ecs/systems/ItemSystem.cpp`
- `docs/ecs_migration/MIGRATION_LOG_GPT.md`

Batch A - audit result:
- Initial `LegacyItemOf(` count in `ItemSystem.cpp`: 22 matches, including the helper definition.
- Usage classifications:
  - Read-only metadata: `GetItemID`, `GetItemVnum`, `GetItemOriginalVnum`, `GetItemCount`, `GetItemType`, `GetItemSubType`.
  - Read-only socket/attribute: component-backed by `ecs::ItemSockets` and `ecs::ItemAttributes`.
  - Ownership/location/equip/use mutation wrappers: kept on `LegacyItemOf`.
  - AutoGive/Receive wrappers: kept on `LegacyItemOf`.
  - Refine wrappers: kept on `LegacyItemOf`.
  - Special-group internals: legacy `LPITEM` vector remains internal only.

Batch B - pure ECS read APIs:
- Removed `LegacyItemOf` fallback from these existing read APIs:
```cpp
uint32_t GetItemID(entt::entity item);
uint32_t GetItemVnum(entt::entity item);
uint32_t GetItemOriginalVnum(entt::entity item);
uint32_t GetItemCount(entt::entity item);
```
- These now read only ECS components:
  - `ecs::ItemIdentity`
  - `ecs::ItemCount`
- `GetItemType` and `GetItemSubType` still use `LegacyItemOf` because no ECS component currently stores type/subtype.

Batch C - socket/attribute read APIs:
- Added read-only ECS item APIs:
```cpp
uint32_t GetItemSocket(entt::entity item, int index);
bool HasItemSocket(entt::entity item, int index);
int GetItemAttributeType(entt::entity item, int index);
int GetItemAttributeValue(entt::entity item, int index);
```
- Implementations read only:
  - `ecs::ItemSockets`
  - `ecs::ItemAttributes`
- No write APIs were added.
- No socket/attribute mutation path was changed.

Batch D - metadata-only caller migration:
- No additional internal callers were migrated in this pass.
- Reason: the remaining simple candidates need type/subtype or are embedded in legacy mutation-sensitive flows.
- Avoided broad changes inside legacy `CHARACTER` item methods in this phase.

Batch E - remaining blockers:
- Final `LegacyItemOf(` count in `ItemSystem.cpp`: 18 matches, including the helper definition.
- Remaining blocker categories:
  - `SetWearItem`, `UnequipItem`, `EquipItem`, `CanEquipNow`, `CanUnequipNow`: equipment mutation/delegation.
  - `UseItemEx`: legacy item behavior and item script dispatch.
  - `AutoGiveItem(entt::entity, entt::entity)`, `AutoGiveDS`, `CanReceiveItem`, `ReceiveItem`: ownership/transfer semantics.
  - `GetItemType`, `GetItemSubType`: missing ECS type/subtype component coverage.
  - Refine wrappers: refine transaction semantics must stay legacy until a dedicated bridge phase.
  - `RefineItem`: requires both item and target legacy item pointers internally.

Missing ECS component coverage:
- Item type.
- Item subtype.
- Potentially current vnum vs original/masked vnum semantics if these need to differ in future APIs.

Recommended next phases:
- Add component-backed item prototype metadata (`type`, `subType`) during `EntityFactory::CreateItemEntity` sync.
- Add read-only APIs for item flags/location/owner if needed.
- Keep write/mutation APIs separate:
  - socket write API
  - attribute write API
  - item ownership/location API
  - item packet sync API
  - refine bridge
  - DragonSoul bridge
  - quest current item bridge

Validation:
- Build gate passed:
```powershell
cmake --build build --config RelWithDebInfo --target GameServer --parallel 8
```
- `ItemSystem.hpp` still has zero `LPITEM`.
- Existing warning-class output only; `GameServer.exe` linked successfully.
- Known post-build environment warning remains: `pwsh.exe` not recognized.

Manual WinTest checklist:
- Login/relog.
- Open normal inventory.
- Grant item and verify stack merge.
- Use consumable/item.
- Equip and unequip.
- Open box/special group.
- Refine smoke test.
- Verify no duplicate/missing item, wrong owner, wrong slot, or relog persistence regression.

Commit status:
- Not committed yet. User requested review before commit.

## 2026-04-26 - Phase 15E-5: Finish ItemSystem.hpp public LPITEM purge

Scope:
- Complex code task with validation gates.
- Remove the remaining public `LPITEM` exposure from `ItemSystem.hpp` where safe.
- Preserve legacy runtime behavior by keeping all `LPITEM` resolution inside `ItemSystem.cpp`.

Files changed:
- `SRC/Server/GameServer/ecs/systems/ItemSystem.hpp`
- `SRC/Server/GameServer/ecs/systems/ItemSystem.cpp`

Batch A - `AutoGiveItem(vnum)` public LPITEM wrapper:
- Removed the unused public ECS wrapper:
```cpp
LPITEM AutoGiveItem(entt::entity e, uint32_t itemVnum, ...);
```
- Kept the entity-return API as the only public grant-by-vnum ECS API:
```cpp
entt::entity AutoGiveItemEntity(entt::entity owner,
                                uint32_t itemVnum,
                                uint32_t count = 1,
                                int rarePct = -1,
                                bool sendMessage = true);
```
- Removed the matching `LPITEM AutoGiveItem(...)` wrapper definition from `ItemSystem.cpp`.
- Did not change `CHARACTER::AutoGiveItem`.
- Did not migrate high-risk legacy reward/quest callers.

Batch B - Special item group public result:
- Replaced the public `std::vector<LPITEM>& itemGets` output with an entity-safe result type:
```cpp
struct SpecialItemGroupResult {
    std::vector<entt::entity> itemEntities;
    std::vector<uint32_t> itemVnums;
    std::vector<uint32_t> itemCounts;
    int count = 0;
};

SpecialItemGroupResult GiveItemFromSpecialItemGroup(entt::entity e,
                                                    uint32_t groupNum);
```
- Implementation still delegates internally to:
```cpp
ch->GiveItemFromSpecialItemGroup(...)
```
- Legacy `LPITEM` outputs are converted inside `.cpp` using:
```cpp
EntityFactory::CreateItemEntity(g_registry, item)
```
- No legacy box/special-group behavior was changed.

Batch C - Refine public API signatures:
- Converted public refine wrappers from `LPITEM` item parameters to `entt::entity` item parameters:
```cpp
bool DoRefine(entt::entity e, entt::entity item, bool moneyOnly = false);
bool DoRefineWithScroll(entt::entity e, entt::entity item);
bool DoRefineItemSoul(entt::entity e, entt::entity item);
bool RefineItem(entt::entity e, entt::entity item, entt::entity target);
```
- Implementation resolves item entities with the existing `.cpp`-local helper:
```cpp
static LPITEM LegacyItemOf(entt::entity itemEntity)
```
- Delegation remains unchanged to legacy `CHARACTER` refine methods.
- No refine semantics, packet behavior, item storage, DragonSoul behavior, or `points[]` logic was touched.

Validation:
- Build gate passed:
```powershell
cmake --build build --config RelWithDebInfo --target GameServer --parallel 8
```
- Build linked `GameServer.exe` successfully.
- Existing warning-class output remained:
  - C4805 unsafe bool/int comparisons
  - C4311 pointer truncation warnings
  - C4715 missing return warning
  - LNK4075 incremental/LTCG warning
  - post-build environment warning: `pwsh.exe` not recognized
- Final header scan:
```powershell
Select-String -Path 'SRC/Server/GameServer/ecs/systems/ItemSystem.hpp' -Pattern 'LPITEM'
```
- Result: no matches. `ItemSystem.hpp` now exposes zero `LPITEM`.
- Direct public wrapper call-site scan found no external `ItemSystem::AutoGiveItem`, `ItemSystem::GiveItemFromSpecialItemGroup`, or `ItemSystem::DoRefine/RefineItem` call sites requiring migration.

Explicitly not touched:
- `CHARACTER::AutoGiveItem`
- `CHARACTER::GiveItemFromSpecialItemGroup`
- `CHARACTER::DoRefine`
- `CHARACTER::DoRefineWithScroll`
- `CHARACTER::DoRefineItemSoul`
- `CHARACTER::RefineItem`
- Quest/reward high-risk LPITEM paths
- Packet formats
- Inventory storage layout
- DragonSoul behavior
- `points[]`

Runtime risk:
- Low for public wrapper cleanup because no external direct call sites were present.
- Medium for future use of the new special-group result wrapper because it delegates to legacy and converts returned `LPITEM` to item entities; manual testing is still required before using it broadly.
- High-risk refine semantics remain in legacy code and were only wrapped with entity-first public signatures.

Manual WinTest checklist:
- Login and verify normal inventory opens.
- Use reward/grant paths previously migrated to `AutoGiveItemEntity`.
- Open boxes/special item groups through existing legacy paths.
- Open refine UI and perform safe refine test.
- Verify no duplicated/missing item, no inventory corruption, and no delayed damage/metin collapse regression.
- Relog and verify item persistence.

Commit status:
- Not committed yet. User requested review before commit.

## 2026-04-26 - Phase 15E-4: ItemSystem LPITEM public API purge mini-epic

Mode:
- Complex code task with validation gates.
- No commit was made.
- No `points[]`, packet format, inventory layout, DragonSoul behavior, refine semantics, `CHARACTER::AutoGiveItem` semantics, or `CHARACTER::UseItemEx` semantics were changed.

Files changed:
- `SRC/Server/GameServer/ecs/systems/ItemSystem.hpp`
- `SRC/Server/GameServer/ecs/systems/ItemSystem.cpp`
- `SRC/Server/GameServer/cmd_general.cpp`
- `SRC/Server/GameServer/ecs/systems/ActivitySystem.cpp`
- `docs/ecs_migration/MIGRATION_LOG_GPT.md`

Batch 0 - AutoGiveItemEntity:
- Status: complete.
- Added public additive API:
```cpp
entt::entity AutoGiveItemEntity(entt::entity owner, uint32_t itemVnum,
                                uint32_t count = 1, int rarePct = -1,
                                bool sendMessage = true);
```
- Implementation delegates to legacy `ch->AutoGiveItem(...)`, then maps the returned item with:
```cpp
EntityFactory::CreateItemEntity(g_registry, item)
```
- Existing API remains unchanged:
```cpp
LPITEM AutoGiveItem(entt::entity e, uint32_t itemVnum, ...);
```

Batch A - Low / selected medium AutoGiveItem caller migration:
- Status: complete.
- Migrated low-risk return-ignored callers:
  - `SRC/Server/GameServer/cmd_general.cpp:241`
  - `SRC/Server/GameServer/ecs/systems/ItemSystem.cpp:7550`
  - `SRC/Server/GameServer/ecs/systems/ItemSystem.cpp:7571`
  - `SRC/Server/GameServer/ecs/systems/ItemSystem.cpp:7578`
  - `SRC/Server/GameServer/ecs/systems/ItemSystem.cpp:7585`
  - `SRC/Server/GameServer/ecs/systems/ItemSystem.cpp:11057`
  - `SRC/Server/GameServer/ecs/systems/ItemSystem.cpp:11138`
  - `SRC/Server/GameServer/ecs/systems/ItemSystem.cpp:12816`
- Migrated selected medium null-check-only callers:
  - `SRC/Server/GameServer/cmd_general.cpp:3124`
  - `SRC/Server/GameServer/cmd_general.cpp:3176`
- Deliberately left untouched:
  - callers that use `item->GetID()`
  - callers that use `item->GetName()`
  - callers that use `item->GetVnum()` / `GetType()` except the safe ActivitySystem metadata case
  - callers that call `SetSocket` / mutate the returned item
  - `CQuestManager::SetCurrentItem(item)`
  - DragonSoul byproduct/refine paths
  - fishing paths that mutate sockets
  - `GiveItemFromSpecialItemGroup` internals

Batch B - Read-only item entity accessors:
- Status: complete.
- Added public read-only APIs with no `LPITEM` exposure in the header:
```cpp
bool IsValidItem(entt::entity item);
uint32_t GetItemID(entt::entity item);
uint32_t GetItemVnum(entt::entity item);
uint32_t GetItemOriginalVnum(entt::entity item);
uint8_t GetItemType(entt::entity item);
uint8_t GetItemSubType(entt::entity item);
uint32_t GetItemCount(entt::entity item);
```
- Implementation prefers ECS components where available:
  - `ecs::ItemIdentity` for ID/vnum
  - `ecs::ItemCount` for count
- Fallback uses `.cpp`-local `LegacyItemOf(entt::entity)` for legacy-only metadata.
- No write APIs were added.

Batch C - Simple metadata-only caller migration:
- Status: complete.
- Migrated `SRC/Server/GameServer/ecs/systems/ActivitySystem.cpp:480`.
- Old path:
```cpp
LPITEM pReward = ch->AutoGiveItem(itemVnum, 1, -1, false);
const uint32_t rewardVnum = pReward ? pReward->GetVnum() : 0;
```
- New path:
```cpp
const entt::entity reward = ItemSystem::AutoGiveItemEntity(AIHelpers::EcsOf(ch), itemVnum, 1, -1, false);
const uint32_t rewardVnum = ItemSystem::GetItemVnum(reward);
```
- This caller does not mutate or pass the returned item pointer onward.

Batch D - Refine API audit only:
- Status: complete, no code changes.
- Public leaks still present:
```cpp
bool DoRefine(entt::entity e, LPITEM item, bool moneyOnly = false);
bool DoRefineWithScroll(entt::entity e, LPITEM item);
bool DoRefineItemSoul(entt::entity e, LPITEM item);
bool RefineItem(entt::entity e, LPITEM item, LPITEM target);
```
- Definitions:
  - `ItemSystem.cpp:1040` - `DoRefine`
  - `ItemSystem.cpp:1046` - `DoRefineWithScroll`
  - `ItemSystem.cpp:1052` - `DoRefineItemSoul`
  - `ItemSystem.cpp:1064` - `RefineItem`
- Runtime legacy implementations:
  - `CHARACTER::DoRefine`
  - `CHARACTER::DoRefineWithScroll`
  - `CHARACTER::DoRefineItemSoul`
  - `CHARACTER::RefineItem`
- Call sites include:
  - `input_main.cpp:4572`
  - `input_main.cpp:4577`
  - `input_main.cpp:4584`
  - `input_main.cpp:4593`
  - `input_main.cpp:4597`
  - internal `ItemSystem.cpp:10010` / `10020`
- Risk classification: HIGH / CRITICAL.
- Recommended split:
  - first convert only public wrappers to `entt::entity item` / `entt::entity target` using `LegacyItemOf`;
  - do not change refine internals;
  - do not migrate `input_main` refine call sites until wrapper build and focused refine WinTest are clean.

Batch E - GiveItemFromSpecialItemGroup design only:
- Status: complete, no code changes.
- Current public leak:
```cpp
bool GiveItemFromSpecialItemGroup(entt::entity e, uint32_t groupNum,
                                  std::vector<uint32_t>& itemVnums,
                                  std::vector<uint32_t>& itemCounts,
                                  std::vector<LPITEM>& itemGets,
                                  int& count);
```
- Current callers:
  - internal box/opening paths in `ItemSystem.cpp`
  - `questlua_pc.cpp:529`
- Proposed future result:
```cpp
struct SpecialItemGroupResult {
    std::vector<entt::entity> itemEntities;
    std::vector<uint32_t> itemVnums;
    std::vector<uint32_t> itemCounts;
    int count = 0;
};
```
- Design conclusion:
  - sufficient for item entity return, vnum/count reporting and count tracking;
  - not sufficient alone for callers that still need immediate `LPITEM` names/IDs unless paired with read-only item accessors;
  - should be a separate phase because special groups can grant items, gold, EXP, mobs, affects and mob groups.

Validation:
- Build after Batch A: passed.
- Build after Batch B: passed.
- Build after Batch C: passed.
- Final `GameServer` build result: passed.
- Header scan remaining `LPITEM` public leaks in `ItemSystem.hpp`:
```cpp
LPITEM AutoGiveItem(entt::entity e, uint32_t itemVnum, ...);
std::vector<LPITEM>& itemGets;
bool DoRefine(entt::entity e, LPITEM item, bool moneyOnly = false);
bool DoRefineWithScroll(entt::entity e, LPITEM item);
bool DoRefineItemSoul(entt::entity e, LPITEM item);
bool RefineItem(entt::entity e, LPITEM item, LPITEM target);
```

Manual WinTest checklist:
- Login / relog.
- Daily reward / reward command path.
- Stone craft reward success and full-inventory failure.
- Fishing ActivitySystem reward, including legendary fish notice path.
- Item grants that were left legacy should still behave unchanged.
- Inventory full fallback and ground ownership timer.
- Quest reward grant.
- DragonSoul byproduct/refine reward.
- Box/special item group opening.
- Refine with normal item, scroll and item soul.
- Confirm no duplicate item, missing item, wrong slot, wrong owner, packet desync or delayed damage/metin collapse regression.

Commit status:
- Not committed yet. User requested review before commit.

## 2026-04-26 - Phase 15E-3f-impl: Introduce AutoGiveItemEntity parallel API

Scope:
- Code change, safe additive change.
- Introduce a parallel entity-return API for future migration of `AutoGiveItem(vnum)` callers.
- Do not remove or modify the existing `LPITEM AutoGiveItem(...)` API.

Files changed:
- `SRC/Server/GameServer/ecs/systems/ItemSystem.hpp`
- `SRC/Server/GameServer/ecs/systems/ItemSystem.cpp`

New public API:
```cpp
entt::entity AutoGiveItemEntity(entt::entity owner, uint32_t itemVnum,
                                uint32_t count = 1, int rarePct = -1,
                                bool sendMessage = true);
```

Implementation:
```cpp
entt::entity AutoGiveItemEntity(entt::entity owner, uint32_t itemVnum,
                                uint32_t count, int rarePct,
                                bool sendMessage)
{
    auto* ch = LegacyCharOf(owner);
    if (!ch)
        return entt::null;

    LPITEM item = ch->AutoGiveItem(itemVnum, count, rarePct, sendMessage);
    if (!item)
        return entt::null;

    return EntityFactory::CreateItemEntity(g_registry, item);
}
```

Old API status:
- Existing public API is unchanged and still available:
```cpp
LPITEM AutoGiveItem(entt::entity e, uint32_t itemVnum, ...);
```
- No call sites were modified.
- No questlua, reward, DragonSoul, packet, storage, ownership, merge or `points[]` behavior changed.

Validation:
- Build gate passed:
```powershell
cmake --build build --config RelWithDebInfo --target GameServer --parallel 8
```
- Build produced existing warning-class output only; `GameServer.exe` linked successfully.
- Search confirmed only the new declaration and definition:
```text
SRC/Server/GameServer/ecs/systems/ItemSystem.hpp: AutoGiveItemEntity(...)
SRC/Server/GameServer/ecs/systems/ItemSystem.cpp: AutoGiveItemEntity(...)
```
- Search confirmed old `LPITEM AutoGiveItem(entt::entity e, uint32_t itemVnum, ...)` remains present.

Runtime risk:
- Low. This is additive and unused by existing callers.
- The wrapper delegates to the existing legacy `ch->AutoGiveItem(...)`, then maps the returned `LPITEM` with the already-used idempotent `EntityFactory::CreateItemEntity(g_registry, item)`.

Next migration use:
- Future low-risk callers can move from:
```cpp
LPITEM item = AutoGiveItem(...);
```
to:
```cpp
entt::entity item = AutoGiveItemEntity(...);
```
only when they no longer need immediate `LPITEM` methods.

Commit status:
- Not committed yet. User requested review before commit.

## 2026-04-26 - Phase 15E-3f: ItemSystem AutoGiveItem(vnum) migration design

Mode:
- Audit and design.
- Optional implementation was evaluated and deliberately not performed.
- No code changes were made in this phase.
- No commit was made.

Target API:
```cpp
LPITEM AutoGiveItem(entt::entity e, uint32_t itemVnum, ...);
```

Desired future API:
```cpp
entt::entity AutoGiveItem(entt::entity e, uint32_t itemVnum, ...);
```

Declaration / definition:
- `SRC/Server/GameServer/ecs/systems/ItemSystem.hpp:73`
```cpp
LPITEM AutoGiveItem(entt::entity e, uint32_t itemVnum, ...);
```
- `SRC/Server/GameServer/ecs/systems/ItemSystem.cpp:902`
```cpp
LPITEM AutoGiveItem(entt::entity e, uint32_t itemVnum, ...)
```
- Legacy implementation:
```cpp
SRC/Server/GameServer/ecs/systems/ItemSystem.cpp:13224
LPITEM CHARACTER::AutoGiveItem(uint32_t dwItemVnum, ...)
```

Key call-site classification:

| File | Line | Usage type | Risk |
|---|---:|---|---|
| `questlua_pc.cpp` | 652 | stores `LPITEM`, uses `item->GetID()` for gold bar log | HIGH |
| `questlua_pc.cpp` | 768 | stores `LPITEM`, passes to `CQuestManager::SetCurrentItem(item)` | HIGH |
| `questlua_pc.cpp` | 4605 | stores `LPITEM`, mutates sockets | HIGH |
| `cmd_general.cpp` | 240 | ignores return value | LOW |
| `cmd_general.cpp` | 3123 | stores `LPITEM`, null-checks for inventory space | MEDIUM |
| `cmd_general.cpp` | 3175 | stores `LPITEM`, null-checks for inventory space | MEDIUM |
| `cmd_gm.cpp` | 2377 | stores `LPITEM`, immediately mutates socket | HIGH |
| `cmd_gm.cpp` | 3088 | stores `LPITEM`, then socket setup loop | HIGH |
| `DragonSoul.cpp` | 517 | stores `LPITEM`, uses name in chat | HIGH |
| `fishing.cpp` | 557 | stores `LPITEM`, uses type/vnum for ranking/reward logic | HIGH |
| `input_main.cpp` | 857 | stores `LPITEM`, likely pet/item socket initialization | HIGH |
| `input_main.cpp` | 1189 | ignores return, reward grant | LOW |
| `ActivitySystem.cpp` | 479 | stores `LPITEM`, uses reward metadata | HIGH |
| `PlayerRuntimeSystem.cpp` | 2438 / 2555 | stores `LPITEM` from legacy method inside `CHARACTER` code | HIGH |
| `PlayerRuntimeSystem.cpp` | 3263 | stores `LPITEM`, random skill book socket setup | HIGH |
| `ItemSystem.cpp` | 5948 / 6019 / 8288 | stores `LPITEM`, item-specific behavior | HIGH |
| `ItemSystem.cpp` | 7484 / 7505 / 7512 / 7519 / 10991 / 11072 / 12750 | ignores return or simple grant | LOW-MEDIUM |
| `ItemSystem.cpp` | 13967 | stores `LPITEM` as special-group output | HIGH |

Broader usage categories:
- Questlua path: heavy and high risk because returned `LPITEM` is used for logs, current quest item, sockets and item identity.
- Reward/drop systems: mixed risk; some ignore return, others inspect or mutate returned item.
- DragonSoul: high risk because returned item may be shown in packets/chat or used as byproduct result.
- Commands/GM: mixed risk; many returned items are immediately mutated.
- Packet/input paths: high risk where returned item is configured after creation.
- Internal special-group path: high risk because output remains `std::vector<LPITEM>`.

Behavior summary for `CHARACTER::AutoGiveItem(vnum)`:
- Failure path:
  - missing proto or failed `ITEM_MANAGER::CreateItem` returns `nullptr`.
- Stack merge path:
  - if stackable, scans extra inventory or main inventory.
  - merges count into an existing matching stack.
  - returns the existing stack item when fully merged.
- New item path:
  - creates item with `ITEM_MANAGER::CreateItem`.
  - finds empty DragonSoul, extra inventory or main inventory slot.
  - adds item to character with `AddToCharacter`.
  - sends chat/packet side effects through legacy item/inventory code.
  - returns the newly inserted item.
- Ground fallback path:
  - if no inventory slot exists, adds item to ground.
  - starts destroy event.
  - sets ownership timer.
  - returns the item pointer even though it is not in inventory.
- Special merge path:
  - blend and stackable logic can return an already-existing inventory item instead of the newly created object.

Return value can be:
- `nullptr`
- newly created item in inventory/extra inventory/DragonSoul inventory
- existing stack item after merge
- newly created item dropped to ground with ownership

Entity mapping strategy:
- Use:
```cpp
EntityFactory::CreateItemEntity(g_registry, item)
```
- This is idempotent:
  - if item has valid ID and mapped entity exists, it syncs and returns existing entity.
  - if no mapping exists, it creates/syncs/registers a new item entity.
  - if item is null or has ID 0, it returns `entt::null`.
- Mapping cases:
  - new item: create/sync/register entity.
  - merged item: return/sync existing stack entity.
  - ground fallback: create/sync entity for ground item.
  - null: return `entt::null`.

API compatibility issue:
- C++ cannot overload only by return type.
- Therefore this cannot be added safely next to the existing API:
```cpp
LPITEM AutoGiveItem(entt::entity e, uint32_t itemVnum, ...);
entt::entity AutoGiveItem(entt::entity e, uint32_t itemVnum, ...);
```
- Replacing the old signature immediately would break many call sites that expect `LPITEM`.

Compatibility strategy:
- Best next step is a new temporary entity-return function with a distinct name, for example:
```cpp
entt::entity AutoGiveItemEntity(entt::entity e, uint32_t itemVnum, ...);
```
- Internally:
```cpp
LPITEM item = ch->AutoGiveItem(itemVnum, count, rarePct, sendMessage, ...);
return EntityFactory::CreateItemEntity(g_registry, item);
```
- Keep the old `LPITEM AutoGiveItem(...)` until call sites are migrated in risk-ranked batches.
- After all callers are entity-ready, either:
  - rename `AutoGiveItemEntity` to `AutoGiveItem`, or
  - remove the old `LPITEM` API and update the public name.

Optional implementation decision:
- Not implemented in this phase.
- Reason: the requested final API name cannot coexist with the old API, and replacing it now would force unsafe broad caller migration.
- Also, adding `AutoGiveItemEntity` is technically safe, but it introduces a new public API name that should be confirmed before implementation.

Recommended migration plan:
1. Add `AutoGiveItemEntity(...)` in a dedicated implementation phase.
2. Keep legacy `LPITEM AutoGiveItem(...)` unchanged.
3. Migrate LOW-risk callers first:
   - callers that ignore return value.
   - callers that only null-check.
4. Migrate HIGH-risk callers later:
   - callers that mutate sockets.
   - callers that pass item to quest/current-item systems.
   - callers that read `GetID`, `GetName`, `GetType`, `GetVnum`.
5. Introduce item entity accessors as needed for `ItemIdentity`, name/vnum/type/count/socket/attribute reads.
6. Only after all callers stop requiring `LPITEM`, remove/rename the old API.

Risk assessment:
- Mapping itself is low risk because `CreateItemEntity` is idempotent.
- Immediate signature replacement is high risk because many callers use `LPITEM` behavior directly.
- Special-group and quest current item paths are blockers for a simple mass migration.
- No runtime behavior should be changed until caller batches are isolated and tested.

Remaining blockers:
- Need item entity read/write APIs for common post-grant operations:
  - item ID
  - vnum/original vnum
  - name
  - type/subtype
  - sockets
  - count
- `CQuestManager::SetCurrentItem(LPITEM)` remains legacy.
- `GiveItemFromSpecialItemGroup(... std::vector<LPITEM>&)` remains legacy.
- Several reward systems still expect immediate `LPITEM` mutation after creation.

Validation:
- Build gate passed on the current worktree:
```powershell
cmake --build build --config RelWithDebInfo --target GameServer --parallel 8
```
- No new code was introduced by 15E-3f; the build validates the already pending 15E-3a through 15E-3e ItemSystem changes plus this audit/log update.

Conclusion:
- Safe to proceed with a separate `AutoGiveItemEntity` bridge API phase.
- Not safe to replace `LPITEM AutoGiveItem(...)` with `entt::entity AutoGiveItem(...)` in one step.
- Do not change `CHARACTER::AutoGiveItem`, questlua, reward systems, DragonSoul, packets, storage or `points[]` yet.

## 2026-04-26 - Phase 15E-3e-pre: ItemSystem AutoGive / receive API audit

Mode:
- Audit only.
- No code changes were made for this audit.
- No commit was made.

Target public APIs:
```cpp
void AutoGiveItem(entt::entity e, LPITEM item, bool longOwnerShip = false);
bool AutoGiveDS(entt::entity e, LPITEM item, bool longOwnerShip = false);
LPITEM AutoGiveItem(entt::entity e, uint32_t itemVnum, ...);
bool CanReceiveItem(entt::entity receiver, entt::entity from, LPITEM item);
void ReceiveItem(entt::entity receiver, entt::entity from, LPITEM item);
bool GiveItemFromSpecialItemGroup(..., std::vector<LPITEM>& itemGets, ...);
```

Declaration locations:
- `SRC/Server/GameServer/ecs/systems/ItemSystem.hpp:65` - `AutoGiveItem(entt::entity e, LPITEM item, ...)`
- `SRC/Server/GameServer/ecs/systems/ItemSystem.hpp:71` - `AutoGiveDS(entt::entity e, LPITEM item, ...)`
- `SRC/Server/GameServer/ecs/systems/ItemSystem.hpp:73` - `LPITEM AutoGiveItem(entt::entity e, uint32_t itemVnum, ...)`
- `SRC/Server/GameServer/ecs/systems/ItemSystem.hpp:85` - `CanReceiveItem(..., LPITEM item)`
- `SRC/Server/GameServer/ecs/systems/ItemSystem.hpp:86` - `ReceiveItem(..., LPITEM item)`
- `SRC/Server/GameServer/ecs/systems/ItemSystem.hpp:87-90` - `GiveItemFromSpecialItemGroup(..., std::vector<LPITEM>& itemGets, ...)`

Definition locations:
- `SRC/Server/GameServer/ecs/systems/ItemSystem.cpp:877` - `void AutoGiveItem(entt::entity e, LPITEM item, ...)`
- `SRC/Server/GameServer/ecs/systems/ItemSystem.cpp:892` - `bool AutoGiveDS(entt::entity e, LPITEM item, ...)`
- `SRC/Server/GameServer/ecs/systems/ItemSystem.cpp:899` - `LPITEM AutoGiveItem(entt::entity e, uint32_t itemVnum, ...)`
- `SRC/Server/GameServer/ecs/systems/ItemSystem.cpp:926` - `bool CanReceiveItem(..., LPITEM item)`
- `SRC/Server/GameServer/ecs/systems/ItemSystem.cpp:933` - `void ReceiveItem(..., LPITEM item)`
- `SRC/Server/GameServer/ecs/systems/ItemSystem.cpp:941` - `bool GiveItemFromSpecialItemGroup(..., std::vector<LPITEM>& itemGets, ...)`

Legacy implementation locations:
- `SRC/Server/GameServer/ecs/systems/ItemSystem.cpp:12983` - `CHARACTER::AutoGiveItem(LPITEM item, ...)`
- `SRC/Server/GameServer/ecs/systems/ItemSystem.cpp:13169` - `CHARACTER::AutoGiveDS(LPITEM item, ...)`
- `SRC/Server/GameServer/ecs/systems/ItemSystem.cpp:13219` - `CHARACTER::AutoGiveItem(uint32_t dwItemVnum, ...)`
- `SRC/Server/GameServer/ecs/systems/ItemSystem.cpp:13496` - `CHARACTER::CanReceiveItem(LPCHARACTER from, LPITEM item)`
- `SRC/Server/GameServer/ecs/systems/ItemSystem.cpp:13688` - `CHARACTER::ReceiveItem(LPCHARACTER from, LPITEM item)`
- `SRC/Server/GameServer/ecs/systems/ItemSystem.cpp:13865` - `CHARACTER::GiveItemFromSpecialItemGroup(..., std::vector<LPITEM>& item_gets, ...)`

Direct public `ItemSystem::...` call sites:
- None found for:
  - `ItemSystem::AutoGiveItem`
  - `ItemSystem::AutoGiveDS`
  - `ItemSystem::CanReceiveItem`
  - `ItemSystem::ReceiveItem`
  - `ItemSystem::GiveItemFromSpecialItemGroup`
- Current practical usage is through legacy `CHARACTER` methods and internal `CHARACTER` calls.

High-volume legacy usage files:
- `SRC/Server/GameServer/questlua_pc.cpp` - quest reward / item grant paths.
- `SRC/Server/GameServer/DragonSoul.cpp` - DragonSoul refine/byproduct/give paths.
- `SRC/Server/GameServer/fishing.cpp` - fishing reward paths.
- `SRC/Server/GameServer/ecs/systems/PlayerRuntimeSystem.cpp` - acce / attr transfer style item movement and giveback paths.
- `SRC/Server/GameServer/RuneDungeon.cpp` and `VikingDungeon.cpp` - dungeon key/reward paths.
- `SRC/Server/GameServer/input_main.cpp` - packet/input reward or event grant paths.
- `SRC/Server/GameServer/cmd_general.cpp`, `cmd_gm.cpp` - command-driven grants.
- `SRC/Server/GameServer/battle_pass.cpp`, `OXEvent.cpp`, `wheel_of_destiny.cpp`, `mining.cpp`, `polymorph.cpp`, `db.cpp`, `char_manager.cpp` - reward and item grant paths.

Representative legacy call sites:
- `SRC/Server/GameServer/questlua_pc.cpp:652,768` - `LPITEM item = ch->AutoGiveItem(...)`
- `SRC/Server/GameServer/questlua_pc.cpp:712,715` - `AutoGiveItem(item)` with existing `LPITEM`
- `SRC/Server/GameServer/questlua_pc.cpp:529` - `GiveItemFromSpecialItemGroup(...)`
- `SRC/Server/GameServer/DragonSoul.cpp:408,680,842,1029,1057` - `ch->AutoGiveItem(LPITEM, true)`
- `SRC/Server/GameServer/DragonSoul.cpp:1191,1303` - `ch->AutoGiveDS(itemres, true)`
- `SRC/Server/GameServer/fishing.cpp:557` - `LPITEM item = ch->AutoGiveItem(...)`
- `SRC/Server/GameServer/input_main.cpp:857` - `LPITEM item = ch->AutoGiveItem(...)`
- `SRC/Server/GameServer/input_main.cpp:1189` - `ch->AutoGiveItem(vnum, count)`
- `SRC/Server/GameServer/ecs/systems/ItemSystem.cpp:13486-13488` - `victim->CanReceiveItem(this, item)` then `victim->ReceiveItem(this, item)`
- `SRC/Server/GameServer/ecs/systems/ItemSystem.cpp:13962` - `item_get = AutoGiveItem(dwVnum, dwCount, iRarePct)`

Call-site classification:
- Already has item `entt::entity`: none found for target public `ItemSystem` APIs.
- Only has `LPITEM`: all existing item-object input paths.
- Legacy `CHARACTER` wrapper: yes, all behaviorful paths delegate to `CHARACTER`.
- Questlua path: yes, heavy use through `questlua_pc.cpp`.
- Drop/reward path: yes, dungeon rewards, fishing, OX, battle pass, wheel, mining, DB grants.
- Exchange/trade/receive path: yes, `GiveItem`, `CanReceiveItem`, `ReceiveItem` and NPC take-item logic.
- Packet/input path: yes, `input_main.cpp` reward/event paths and normal item interaction can reach receive logic.

Mapping status:
- Existing `LPITEM -> entt::entity` mapping is safe and already used:
```cpp
EntityFactory::CreateItemEntity(g_registry, item)
```
- Existing `.cpp`-local `entt::entity -> LPITEM` resolver from 15E-3c is reusable:
```cpp
static LPITEM LegacyItemOf(entt::entity itemEntity)
```
- These are enough for signature-only cleanup of APIs that receive an existing item.

Risk classification per API:
- `void AutoGiveItem(entt::entity e, LPITEM item, ...)`
  - Risk: MEDIUM-HIGH.
  - Reason: ownership transfer, stack merge, empty slot resolution, DragonSoul/extra/main inventory routing, ground fallback and ownership timer.
  - Signature-only cleanup possible: YES, if changed to `entt::entity item` and internally resolved with `LegacyItemOf`.
- `bool AutoGiveDS(entt::entity e, LPITEM item, ...)`
  - Risk: MEDIUM-HIGH.
  - Reason: DragonSoul-only routing, DragonSoul inventory slot selection, ground fallback and ownership timer.
  - Signature-only cleanup possible: YES, if changed to `entt::entity item` and internally resolved with `LegacyItemOf`.
- `LPITEM AutoGiveItem(entt::entity e, uint32_t itemVnum, ...)`
  - Risk: HIGH.
  - Reason: creates or merges item internally and returns newly created/merged `LPITEM`.
  - Signature-only cleanup possible: NO.
  - Requires separate design to return `entt::entity`, mapping returned `LPITEM` with `EntityFactory::CreateItemEntity(g_registry, item)`.
- `bool CanReceiveItem(entt::entity receiver, entt::entity from, LPITEM item)`
  - Risk: MEDIUM.
  - Reason: validation depends on NPC race, distance, item type/refine state, quest/dungeon/NPC behavior.
  - Signature-only cleanup possible: YES, if changed to `entt::entity item` and internally resolved with `LegacyItemOf`.
- `void ReceiveItem(entt::entity receiver, entt::entity from, LPITEM item)`
  - Risk: HIGH.
  - Reason: NPC take-item logic can consume items, trigger refine, quest hooks, dungeon progression, fishing grill and item count changes.
  - Signature-only cleanup possible: YES mechanically, but should be paired with `CanReceiveItem` and tested carefully.
- `GiveItemFromSpecialItemGroup(... std::vector<LPITEM>& itemGets ...)`
  - Risk: HIGH.
  - Reason: output vector exposes legacy item pointers and function may grant gold, EXP, mobs, affects, spawned groups, and newly created items.
  - Signature-only cleanup possible: NO.
  - Requires separate result-design, likely `std::vector<entt::entity>& itemGets` or a richer reward result struct.

Behavior risks:
- Item ownership transfer: HIGH for AutoGive/Receive.
- Inventory insertion: HIGH, includes main, extra and DragonSoul inventory.
- Empty slot resolution: HIGH, uses item size and inventory-specific rules.
- Stack merge: HIGH, can merge into existing stack and destroy/zero incoming item.
- Special inventory routing: HIGH for extra inventory and DragonSoul.
- DragonSoul inventory routing: HIGH for `AutoGiveDS` and DragonSoul reward/refine byproducts.
- Anti-duplication: HIGH, because item identity/entity mapping must track created, merged and destroyed items correctly.
- Packet sync: HIGH, through `AddToCharacter`, `SetCount`, `RemoveItem`, `AddToGround`, chat messages and quickslot updates.
- Persistence/relog: HIGH, because item location/count/owner changes must save correctly.
- Quest reward behavior: HIGH, `questlua_pc` heavily uses returned `LPITEM` IDs and names.
- Drop ownership timer: MEDIUM-HIGH, ground fallback sets ownership and destroy timers.

Recommended smallest 15E-3e implementation batch:
1. Convert only existing-item input wrappers:
```cpp
void AutoGiveItem(entt::entity e, entt::entity item, bool longOwnerShip = false, ...);
bool AutoGiveDS(entt::entity e, entt::entity item, bool longOwnerShip = false);
bool CanReceiveItem(entt::entity receiver, entt::entity from, entt::entity item);
void ReceiveItem(entt::entity receiver, entt::entity from, entt::entity item);
```
2. Reuse `.cpp`-local `LegacyItemOf(entt::entity)` and keep behavior as pure legacy delegation.
3. Do not change `LPITEM AutoGiveItem(entt::entity e, uint32_t itemVnum, ...)` in the same batch.
4. Do not change `GiveItemFromSpecialItemGroup(... std::vector<LPITEM>& ...)` in the same batch.
5. Build and run focused tests for:
   - DragonSoul byproduct giveback
   - quest reward item grant
   - NPC receive/take item
   - stack merge
   - full inventory ground fallback
   - relog persistence

Separate design required:
- `AutoGiveItem(itemVnum...)` should become:
```cpp
entt::entity AutoGiveItem(entt::entity e, uint32_t itemVnum, ...);
```
  - Internally call legacy `ch->AutoGiveItem(...)`.
  - Convert returned `LPITEM` to entity with `EntityFactory::CreateItemEntity(g_registry, item)`.
  - Must audit callers that depend on `LPITEM` methods immediately after return.
- `GiveItemFromSpecialItemGroup` should not be changed until a result type is designed:
```cpp
struct SpecialItemGroupResult {
    std::vector<uint32_t> itemVnums;
    std::vector<uint32_t> itemCounts;
    std::vector<entt::entity> itemEntities;
    int count;
};
```
  - This needs a separate phase because questlua and box opening code may inspect returned item names/IDs/counts.

Proceed recommendation:
- Safe to proceed with a narrow 15E-3e implementation only for existing-item input wrappers:
  - `AutoGiveItem(entt::entity, entt::entity, ...)`
  - `AutoGiveDS(entt::entity, entt::entity, ...)`
  - `CanReceiveItem(..., entt::entity item)`
  - `ReceiveItem(..., entt::entity item)`
- Not safe to include `AutoGiveItem(itemVnum...)` return conversion in the same batch.
- Not safe to include `GiveItemFromSpecialItemGroup` in the same batch.
- Do not change storage, packet behavior, item creation semantics, ownership transfer semantics, quest rewards, DragonSoul behavior or `points[]`.

## 2026-04-26 - Phase 15E-3e: ItemSystem AutoGive/Receive existing-item signature cleanup

Scope:
- Code change, narrow scope.
- Remove `LPITEM` exposure only from existing-item AutoGive/Receive public wrappers.
- Preserve legacy ownership, insertion, receive, DragonSoul and quest behavior exactly.

Files changed:
- `SRC/Server/GameServer/ecs/systems/ItemSystem.hpp`
- `SRC/Server/GameServer/ecs/systems/ItemSystem.cpp`

Target APIs migrated:
```cpp
void AutoGiveItem(entt::entity e, LPITEM item, bool longOwnerShip = false);
bool AutoGiveDS(entt::entity e, LPITEM item, bool longOwnerShip = false);
bool CanReceiveItem(entt::entity receiver, entt::entity from, LPITEM item);
void ReceiveItem(entt::entity receiver, entt::entity from, LPITEM item);
```

New signatures:
```cpp
void AutoGiveItem(entt::entity e, entt::entity item, bool longOwnerShip = false);
bool AutoGiveDS(entt::entity e, entt::entity item, bool longOwnerShip = false);
bool CanReceiveItem(entt::entity receiver, entt::entity from, entt::entity item);
void ReceiveItem(entt::entity receiver, entt::entity from, entt::entity item);
```

Implementation:
- Reused the existing `.cpp`-local resolver:
```cpp
static LPITEM LegacyItemOf(entt::entity itemEntity)
```
- Each wrapper resolves:
  - owner/receiver/from through `LegacyCharOf(...)`
  - item through `LegacyItemOf(item)`
- Delegation remains unchanged:
```cpp
ch->AutoGiveItem(legacyItem, longOwnerShip, ...);
ch->AutoGiveDS(legacyItem, longOwnerShip);
receiverCh->CanReceiveItem(fromCh, legacyItem);
receiverCh->ReceiveItem(fromCh, legacyItem);
```
- Missing owner/from/item resolution now fails safely:
  - `AutoGiveItem`: no-op
  - `AutoGiveDS`: `false`
  - `CanReceiveItem`: `false`
  - `ReceiveItem`: no-op

Validation:
- Build gate passed:
```powershell
cmake --build build --config RelWithDebInfo --target GameServer --parallel 8
```
- Build produced existing warning-class output only; `GameServer.exe` linked successfully.
- Targeted grep confirmed the four migrated signatures now use `entt::entity item`.
- Targeted negative grep found no remaining `LPITEM` exposure on these four target signatures.

Explicitly left unchanged:
- `LPITEM AutoGiveItem(entt::entity e, uint32_t itemVnum, ...)`
- `GiveItemFromSpecialItemGroup(... std::vector<LPITEM>& itemGets, ...)`
- `CHARACTER::AutoGiveItem` internals.
- `CHARACTER::AutoGiveDS` internals.
- `CHARACTER::CanReceiveItem` / `ReceiveItem` internals.
- Storage layout.
- Packet behavior.
- Item creation.
- Ownership transfer semantics.
- Quest rewards.
- DragonSoul behavior.
- `points[]`.

Remaining known `LPITEM` public API leaks in `ItemSystem.hpp` after 15E-3e:
- `LPITEM AutoGiveItem(entt::entity e, uint32_t itemVnum, ...)`
- `GiveItemFromSpecialItemGroup(... std::vector<LPITEM>& itemGets, ...)`
- Refine APIs:
  - `DoRefine`
  - `DoRefineWithScroll`
  - `DoRefineItemSoul`
  - `RefineItem`

Recommended next split:
- `15E-3f-pre`: audit `AutoGiveItem(itemVnum...)` return conversion to `entt::entity`.
- `15E-3g-pre`: audit `GiveItemFromSpecialItemGroup` result redesign.
- `15E-3h-pre`: audit refine APIs.

Manual WinTest checklist:
- Quest reward item grant.
- DragonSoul byproduct giveback.
- NPC receive/take item.
- Stack merge into existing stack.
- Full inventory ground fallback with ownership timer.
- Extra inventory and DragonSoul inventory routing.
- Relog persistence.
- Verify no duplicate item, missing item, wrong slot, wrong owner or delayed damage/metin collapse regression.

Commit status:
- Not committed yet. User requested review before commit.

## 2026-04-26 - Phase 15E-3d-pre: ItemSystem UseItemEx API audit

Mode:
- Audit only.
- No code changes were made for this audit.
- No commit was made.

Target API:
```cpp
bool UseItemEx(entt::entity e, LPITEM item, TItemPos destCell = NPOS);
```

Declaration / definition:
- `SRC/Server/GameServer/ecs/systems/ItemSystem.hpp:61`
```cpp
bool UseItemEx(entt::entity e, LPITEM item, TItemPos destCell = NPOS);
```
- `SRC/Server/GameServer/ecs/systems/ItemSystem.cpp:863`
```cpp
bool UseItemEx(entt::entity e, LPITEM item, TItemPos destCell)
{
    auto* ch = LegacyCharOf(e);
    return ch ? ch->UseItemEx(item, destCell) : false;
}
```
- Legacy declaration: `SRC/Server/GameServer/char.h:1420`
```cpp
bool UseItemEx(LPITEM item, TItemPos DestCell);
```
- Legacy implementation: `SRC/Server/GameServer/ecs/systems/ItemSystem.cpp:5625`
```cpp
bool CHARACTER::UseItemEx(LPITEM item, TItemPos DestCell)
```

Direct `ItemSystem::UseItemEx` call sites:
- None found.
- No external direct call site currently calls `ItemSystem::UseItemEx(...)`.

Internal / legacy call sites:
- `SRC/Server/GameServer/ecs/systems/ItemSystem.cpp:5526`
```cpp
bool ret = UseItemEx(item, DestCell);
```
  - Classification: legacy `CHARACTER::UseItem` internal path.
  - Context: packet/input item-use path after item lookup and validation.
  - Item handle: only `LPITEM`.
- `SRC/Server/GameServer/ecs/systems/ItemSystem.cpp:5540`
```cpp
return UseItemEx(item, DestCell);
```
  - Classification: legacy `CHARACTER::UseItem` internal path.
  - Context: fallback/direct item-use execution.
  - Item handle: only `LPITEM`.
- `SRC/Server/GameServer/ecs/systems/ItemSystem.cpp:5625`
```cpp
bool CHARACTER::UseItemEx(LPITEM item, TItemPos DestCell)
```
  - Classification: legacy implementation body.
  - Context: actual item behavior dispatcher.
  - Item handle: only `LPITEM`.
- `SRC/Server/GameServer/input_main.cpp:1512`
```cpp
ch->UseItem(((struct command_item_use *) data)->Cell);
```
  - Classification: packet/input path.
  - Calls `CHARACTER::UseItem`, which may call `CHARACTER::UseItemEx`.
- `SRC/Server/GameServer/input_main.cpp:1525`
```cpp
ch->UseItem(p->Cell, p->TargetCell);
```
  - Classification: packet/input path with target cell.
  - Calls `CHARACTER::UseItem`, which may call `CHARACTER::UseItemEx`.
- `SRC/Server/GameServer/cmd_general.cpp:3615`
```cpp
ch->UseItem(TItemPos(INVENTORY, i));
```
  - Classification: command path.
  - Calls `CHARACTER::UseItem`.
- `SRC/Server/GameServer/cmd_gm.cpp:4685`
```cpp
ch->UseItem(TItemPos(INVENTORY, cell));
```
  - Classification: GM command path.
  - Calls `CHARACTER::UseItem`.
- `SRC/Server/GameServer/questmanager.cpp:816`
```cpp
bool CQuestManager::UseItem(unsigned int pc, LPITEM item, bool bReceiveAll)
```
  - Classification: quest manager item script path.
  - This is not `ItemSystem::UseItemEx`, but `CHARACTER::UseItemEx` calls quest item hooks.

Call-site classification summary:
- Already has item `entt::entity` available: none found.
- Only has `LPITEM`: all practical `UseItemEx` paths.
- Legacy `CHARACTER` wrapper: yes, `CHARACTER::UseItem` and `CHARACTER::UseItemEx`.
- Packet/input path: yes, `input_main.cpp` item-use packets call `ch->UseItem(...)`.
- Questlua / quest path: indirect, through `quest::CQuestManager::UseItem(...)`, `SIGUse(...)`, and item script hooks inside `CHARACTER::UseItemEx`.
- Item script / special behavior path: yes, `CHARACTER::UseItemEx` is the main dispatcher for many item categories.

`LegacyItemOf` availability:
- 15E-3c already added a reusable `.cpp`-local resolver in `ItemSystem.cpp`:
```cpp
static LPITEM LegacyItemOf(entt::entity itemEntity)
{
    if (itemEntity == entt::null || !g_registry.valid(itemEntity))
        return nullptr;

    const auto* identity = g_registry.try_get<ecs::ItemIdentity>(itemEntity);
    if (!identity || identity->id == 0)
        return nullptr;

    return ITEM_MANAGER::instance().Find(identity->id);
}
```
- This resolver is directly reusable for a signature-only `UseItemEx` public API cleanup.

Behavior surface inside `CHARACTER::UseItemEx`:
- Equipment toggle:
  - item types such as `ITEM_COSTUME`, `ITEM_WEAPON`, `ITEM_ARMOR`, `ITEM_ROD`, `ITEM_RING`, `ITEM_BELT`, `ITEM_PICK`, `ITEM_SPECIAL_DS`, `ITEM_TOTEM` call `EquipItem(item)` / `UnequipItem(item)`.
- Consumables:
  - many `ITEM_USE` subtypes add affects, remove affects, consume/remove items, alter recovery, bonuses and timers.
- Quest items:
  - `ITEM_QUEST` calls `quest::CQuestManager::UseItem(...)` and `SIGUse(...)`.
- DragonSoul:
  - `ITEM_DS`, DragonSoul duration/extract/attribute paths, `DSManager::PullOut`, `ExtractDragonHeart`, `PutAttributes`.
- Refine / special item triggers:
  - `RefineItem(item, item2)`, metin insertion/removal, attribute change/add, acce/costume/pendant style operations.
- Packet sync:
  - indirect through `AddToCharacter`, `RemoveFromCharacter`, `ITEM_MANAGER::RemoveItem`, equip/unequip, item count changes and chat packets.
- Cooldowns / quest flags:
  - uses quest event flags, item-use timers and anti-spam checks in several paths.
- Affect/stat recalculation:
  - many `AddAffect` / `RemoveAffect` paths touch stat and combat behavior.
- Ownership/location validation:
  - checks exchanging, equipped state, target cell, destination item, inventory/extra inventory placement and special inventories.

Risk assessment:
- Public signature-only cleanup risk: LOW-MEDIUM.
  - No direct external `ItemSystem::UseItemEx` call sites exist.
  - The wrapper can resolve `entt::entity item` to `LPITEM` and delegate unchanged to `ch->UseItemEx(...)`.
- Legacy behavior migration risk: CRITICAL.
  - `CHARACTER::UseItemEx` is a broad item behavior dispatcher.
  - Any semantic change can affect combat, equipment, consumables, quest scripts, DragonSoul, refine, packets, item deletion and affects.
- Stale item entity risk: MEDIUM.
  - `LegacyItemOf` resolves through `ItemIdentity.id` and `ITEM_MANAGER::Find(id)`.
  - If an item was deleted, it returns null and the wrapper should fail safely.
- Runtime regression risk if only public wrapper is changed: low, because no direct external call sites currently use it.

Recommended 15E-3d implementation plan:
1. Change only the public `ItemSystem` signature:
```cpp
bool UseItemEx(entt::entity owner, entt::entity item, TItemPos destCell = NPOS);
```
2. Reuse the existing `.cpp`-local `LegacyItemOf(entt::entity)` from 15E-3c.
3. Keep implementation as a pure bridge:
```cpp
bool UseItemEx(entt::entity owner, entt::entity item, TItemPos destCell)
{
    auto* ch = LegacyCharOf(owner);
    LPITEM legacyItem = LegacyItemOf(item);
    return ch && legacyItem ? ch->UseItemEx(legacyItem, destCell) : false;
}
```
4. Do not change `CHARACTER::UseItem` or `CHARACTER::UseItemEx`.
5. Do not route internal legacy calls through `ItemSystem::UseItemEx`.
6. Build and run focused item-use regression.

Proceed recommendation:
- Safe to proceed with signature-only cleanup of the public `ItemSystem::UseItemEx` wrapper.
- Do not proceed with any semantic migration of `CHARACTER::UseItemEx`.
- Do not touch item scripts, packets, storage, equip/unequip internals, refine, DragonSoul or `points[]`.

Recommended manual WinTest checklist after implementation:
- Login and use normal consumables.
- Equip/unequip by double-click through item use.
- Use quest item.
- Use buff/potion item and verify affects/stat changes.
- Use DragonSoul-related item if available.
- Use/refine/metin/attribute item if available.
- Verify item deletion/count changes are correct.
- Verify no delayed damage/metin collapse regression.
- Relog and verify inventory/equipment state persists.

## 2026-04-26 - Phase 15E-3d: ItemSystem UseItemEx public signature cleanup

Scope:
- Code change, narrow scope.
- Remove `LPITEM` exposure from public `ItemSystem::UseItemEx`.
- Preserve legacy `CHARACTER::UseItemEx` behavior exactly.

Files changed:
- `SRC/Server/GameServer/ecs/systems/ItemSystem.hpp`
- `SRC/Server/GameServer/ecs/systems/ItemSystem.cpp`

Old public signature:
```cpp
bool UseItemEx(entt::entity e, LPITEM item, TItemPos destCell = NPOS);
```

New public signature:
```cpp
bool UseItemEx(entt::entity e, entt::entity item, TItemPos destCell = NPOS);
```

Implementation:
- Reused the existing `.cpp`-local resolver added in 15E-3c:
```cpp
static LPITEM LegacyItemOf(entt::entity itemEntity)
```
- Wrapper now resolves:
  - owner with `LegacyCharOf(e)`
  - item with `LegacyItemOf(item)`
- Delegation remains unchanged:
```cpp
return ch && legacyItem ? ch->UseItemEx(legacyItem, destCell) : false;
```

Validation:
- Build gate passed:
```powershell
cmake --build build --config RelWithDebInfo --target GameServer --parallel 8
```
- Build produced existing warning-class output only; `GameServer.exe` linked successfully.
- `UseItemEx` grep confirmed:
```cpp
bool UseItemEx(entt::entity e, entt::entity item, TItemPos destCell = NPOS);
bool UseItemEx(entt::entity e, entt::entity item, TItemPos destCell)
```
- Targeted header grep found no `LPITEM` exposure on `ItemSystem::UseItemEx`.

Explicitly not touched:
- `CHARACTER::UseItem`
- `CHARACTER::UseItemEx`
- Legacy internal `UseItemEx(item, DestCell)` calls
- Item scripts / quest hooks
- Packets
- Storage
- Equip / unequip internals
- Refine
- DragonSoul
- `points[]`

Runtime risk:
- Public wrapper risk remains low because no direct external `ItemSystem::UseItemEx` call sites were found.
- Legacy semantic risk remains critical and intentionally untouched.

Manual WinTest checklist:
- Login and use normal consumables.
- Equip/unequip by double-click through item use.
- Use quest item.
- Use buff/potion item and verify affects/stat changes.
- Use DragonSoul-related item if available.
- Use/refine/metin/attribute item if available.
- Verify item deletion/count changes are correct.
- Verify no delayed damage/metin collapse regression.
- Relog and verify inventory/equipment state persists.

Commit status:
- Not committed yet. User requested review before commit.

## 2026-04-26 - Phase 15E-15: Replace AutoGiveItemEntity default path with ECS AutoGiveItemEcs

Scope:
- Code change, controlled production switch.
- Keep the compatibility API name `AutoGiveItemEntity`.
- Change its default implementation from legacy `CHARACTER::AutoGiveItem(vnum)` to ECS-native `ItemSystem::AutoGiveItemEcs`.
- Do not migrate high-risk direct legacy callers in this pass.

Files changed:
- `SRC/Server/GameServer/ecs/systems/ItemSystem.cpp`
- `docs/ecs_migration/MIGRATION_LOG_GPT.md`

Implementation:
- `AutoGiveItemEntity(...)` now defaults to:
```cpp
const entt::entity item =
    AutoGiveItemEcs(owner, itemVnum, count, rarePct, sendMessage);
return item;
```
- The old legacy path remains available only behind an explicit compile-time switch:
```cpp
#ifdef USE_LEGACY_AUTOGIVE
```
- Optional debug fallback is available only behind:
```cpp
#ifdef DEBUG_AUTOGIVE_ECS_FALLBACK
```
- If debug fallback is enabled and ECS grant fails, the code logs:
```cpp
AUTOGIVE_ECS_FAIL owner=%u vnum=%u count=%u
```
  then calls the legacy `ch->AutoGiveItem(...)` as a diagnostic fallback.

Behavior boundary:
- Production/default path no longer calls `ch->AutoGiveItem(itemVnum, ...)` through `AutoGiveItemEntity`.
- Direct legacy `ch->AutoGiveItem(...)` callers outside this wrapper were not migrated if they are high-risk or still need separate review.
- Existing `AutoGiveItemEcs` behavior is unchanged.
- No packet, inventory layout, DragonSoul, refine, quest, storage, or `points[]` changes.

Validation:
- Build gate passed:
```powershell
cmake --build build --config RelWithDebInfo --target GameServer --parallel 8
```
- `GameServer.exe` linked successfully.
- Existing warning-class output remains present.
- Build output still prints the known post-step environment message:
```text
'pwsh.exe' is not recognized as an internal or external command
```
  but the build command returned exit code `0`.

Scans:
- `ItemSystem.hpp` public pointer leak scan:
```powershell
Select-String -Path 'SRC/Server/GameServer/ecs/systems/ItemSystem.hpp' -Pattern 'LPITEM|LPCHARACTER|CHARACTER\s*\*'
```
  Result: no matches.
- `AutoGiveItemEntity` scan:
  - `ItemSystem.hpp`: declaration only.
  - `ItemSystem.cpp`: definition only.
  - No external call sites found.
- Controlled legacy fallback scan:
  - `USE_LEGACY_AUTOGIVE` present only inside `AutoGiveItemEntity`.
  - `DEBUG_AUTOGIVE_ECS_FALLBACK` present only inside `AutoGiveItemEntity`.
  - `ch->AutoGiveItem(itemVnum, ...)` remains only in the explicitly gated legacy/debug branches of this wrapper.

Callers deliberately left untouched:
- Direct `ch->AutoGiveItem(...)` calls in legacy/high-risk systems remain for later targeted phases.
- Examples include GM/quest/fishing/DragonSoul/refine/special reward paths where the returned `LPITEM` is used, passed onward, or tied to special semantics.

Runtime risk:
- Medium. The compatibility wrapper now uses the ECS grant path by default, so any remaining user of `AutoGiveItemEntity` will exercise ECS grant behavior.
- Current scan found no external `AutoGiveItemEntity` call sites, so immediate runtime impact should be low.
- Direct legacy callers remain unchanged and should behave as before.

Manual WinTest checklist:
- GM give command / simple item reward.
- Stackable grant and merge.
- Full inventory fallback.
- Extra inventory route for eligible items.
- DragonSoul item smoke test if available.
- Relog persistence.
- Verify no duplicate item, missing item, wrong owner, wrong slot.
- Verify no delayed damage/metin collapse regression.

Commit status:
- Not committed yet. User requested review before commit.


## Phase 15E-35 - Full count engine migration (safe subset)

Date: 2026-04-27

Mode:
- Core engine migration, safe subset only.
- Code change.
- Not committed yet.

Goal:
- Move remaining simple `item->SetCount(item->GetCount() - X)` count-consume paths toward ECS-controlled count mutation.
- Keep unsafe legacy count paths unchanged.

Files changed:
- `SRC/Server/GameServer/ecs/systems/ItemSystem_LegacyBridge.cpp`
- `docs/ecs_migration/MIGRATION_LOG_GPT.md`

Implementation:
- Replaced 33 additional simple one-count consumable branches with:
```cpp
ItemSystem::ConsumeItemEcs(EntityFactory::CreateItemEntity(g_registry, item));
```
- Replaced one full-stack consume branch with:
```cpp
ItemSystem::ConsumeItemEcs(EntityFactory::CreateItemEntity(g_registry, item), count);
```
- The migrated branches are simple consume-style flows such as potion/affect/firework/alignment/battle-pass/hair/simple reward use paths.
- Existing 15E-34 migrated branches remain on `ConsumeItemEcs`.

Encoding / compile repair:
- During the safe replacements, the legacy bridge file exposed old Korean/mojibake comment continuations as standalone source lines after line normalization.
- These were converted back into comments only.
- No gameplay logic was changed by this repair.

Skipped as unsafe:
- DragonSoul count/refine paths.
- Refine scroll count paths.
- Pet duration / pet state paths.
- Horse/pet-like use paths.
- Skill book/book stackfix paths.
- Special item group / box internals.
- Attribute/socket scroll flows.
- Stack merge and inventory move count balancing.
- Timed/socket/location-dependent item flows.
- Polymorph/book-special flows.

Counts after this pass:
- `ConsumeItemEcs` call sites in `ItemSystem_LegacyBridge.cpp`: 38.
- Remaining direct `item->SetCount(item->GetCount() - 1)` call sites in `ItemSystem_LegacyBridge.cpp`: 58.
- Remaining simple `item->SetCount(0)` / `item->SetCount(variable)` style call sites in `ItemSystem_LegacyBridge.cpp`: 11.

Reason remaining count paths were not converted:
- They are not simple count engine operations; most are tied to merge, split, refine, DragonSoul, pet, special group, socket/attribute, or item creation/deletion semantics.
- Converting those blindly would risk duplicated items, missing items, wrong stack count, or broken special item behavior.

Validation:
- Build gate passed:
```powershell
cmake --build build --config RelWithDebInfo --target GameServer --parallel 8
```
- `GameServer.exe` linked successfully.
- Existing warning-class output remains present.
- Build output still prints the known post-step environment message:
```text
'pwsh.exe' is not recognized as an internal or external command
```
  but the build command returned exit code `0`.

Header scan:
```powershell
Select-String -Path 'SRC/Server/GameServer/ecs/systems/ItemSystem.hpp' -Pattern 'LPITEM|LPCHARACTER|CHARACTER\s*\*'
```
Result: no matches.

Manual WinTest checklist:
- Use normal potion / no-delay potion.
- Use affect consumable.
- Use firework items.
- Use battle-pass consumable if available.
- Use alignment/point consumables.
- Use hair item path if available.
- Verify stack count decrements correctly.
- Verify item disappears at zero count.
- Relog and verify no duplicate/missing item.
- Verify no wrong owner/window/cell.
- Verify no delayed damage/metin collapse regression.

Commit status:
- Not committed yet. User requested review before commit.


## Phase 15E-35 critical continuation - count engine migration expansion

Date: 2026-04-27

Mode:
- Core count engine migration.
- Code change, still safe-gated.
- Not committed yet.

Goal:
- Continue eliminating remaining simple `LPITEM::SetCount` usage.
- Push all safe one-count consume paths through `ItemSystem::ConsumeItemEcs(...)`.
- Keep explicitly unsafe count semantics legacy until dedicated wrappers exist.

Files changed:
- `SRC/Server/GameServer/ecs/systems/ItemSystem_LegacyBridge.cpp`
- `docs/ecs_migration/MIGRATION_LOG_GPT.md`

Implementation:
- Replaced 40 more simple successful-use decrement paths with:
```cpp
ItemSystem::ConsumeItemEcs(EntityFactory::CreateItemEntity(g_registry, item));
```
- These include simple post-success consumes for book-stackfix, special reward/box success, attribute/costume/pet-enchant style consumable scrolls, and similar one-item use paths where the consumed item itself is not the mutation target.
- Existing ECS count APIs from 15E-34 remain the mutation boundary.

Current count scan:
- `ConsumeItemEcs` call sites in `ItemSystem_LegacyBridge.cpp`: 78.
- Remaining direct `item->SetCount(item->GetCount() - 1)` call sites in `ItemSystem_LegacyBridge.cpp`: 18.
- Remaining direct/variable `item->SetCount(...)` count mutation sites in `ItemSystem_LegacyBridge.cpp`: 15.

Remaining skipped count paths and reason:
- New pet duration item: pet DB/state semantics.
- DragonSoul charging/enchant/refine paths: DragonSoul mutation semantics.
- Socket/metin/weapon socket flows: socket mutation and item target state coupling.
- Horse feed/revive: horse subsystem side effects.
- Warp/recall/timed socket items: socket/timed/location semantics.
- Polymorph/book-special paths: special gameplay state and legacy use behavior.
- Drop/split/merge/autogive stack paths: count update is coupled to item creation, merge, destroy, and inventory placement.

Validation:
- Build gate passed:
```powershell
cmake --build build --config RelWithDebInfo --target GameServer --parallel 8
```
- `GameServer.exe` linked successfully.
- Existing warning-class output remains present.
- Build output still prints the known post-step environment message:
```text
'pwsh.exe' is not recognized as an internal or external command
```
  but the build command returned exit code `0`.

Header scan:
```powershell
Select-String -Path 'SRC/Server/GameServer/ecs/systems/ItemSystem.hpp' -Pattern 'LPITEM|LPCHARACTER|CHARACTER\s*\*'
```
Result: no matches.

Manual WinTest checklist:
- Skill book consume with stackfix enabled.
- Box/special reward item use.
- Attribute/costume scroll consume smoke test.
- Pet enchant/revive consumable smoke test if available, watching for count decrement only.
- Normal potion and affect consumables from the previous slice.
- Relog persistence after using migrated consumables.
- Verify no duplicate item, missing item, wrong count, wrong owner/window/cell.
- Verify no delayed damage/metin collapse regression.

Commit status:
- Not committed yet. User requested review before commit.


## Phase 15E-35 complete pass - count engine migration outside unsafe subsystems

Date: 2026-04-27

Mode:
- Core engine extraction.
- Code change, no commit.

Goal:
- Eliminate remaining simple `LPITEM::SetCount` mutations outside explicitly unsafe subsystems.
- Keep unsafe subsystem count transitions legacy until dedicated transactional ECS wrappers exist.

Files changed:
- `SRC/Server/GameServer/ecs/systems/ItemSystem_LegacyBridge.cpp`
- `docs/ecs_migration/MIGRATION_LOG_GPT.md`

Additional implementation after prior 15E-35 slices:
- Replaced 12 more simple one-count consume paths with `ItemSystem::ConsumeItemEcs(...)`.
- Replaced 2 additional partial-count paths with `ItemSystem::ConsumeItemEcs(..., amount)`:
  - `RemoveSpecifyItem` partial remove path.
  - Dragon Coin partial stack conversion path.
- Total `ConsumeItemEcs` usage in `ItemSystem_LegacyBridge.cpp` is now 96.

Remaining direct `item->SetCount(...)` sites:
- Count: 19 non-comment sites in `ItemSystem_LegacyBridge.cpp`.
- These are intentionally retained because they are unsafe/transactional:
  - Drop/split stack path: count update is coupled to dropped item creation and delayed save.
  - Inventory stack merge/split path: count update is coupled to target stack mutation and new item creation.
  - AutoGive stack merge paths: count update is coupled to merge, item destroy, and placement decisions.
  - New pet duration path: pet DB/state semantics.
  - DragonSoul charging/enchant/refine paths: DragonSoul mutation semantics.
  - Timed/recall item paths: socket/location/timed state semantics.

Remaining direct sites:
```text
3500: item->SetCount(item->GetCount() - bCount);
3896: item->SetCount(item->GetCount() - count);
4140: item->SetCount(item->GetCount() - count);
4390: item->SetCount(bCount);
4477: item->SetCount(bCount);
4674: item->SetCount(bCount);
4761: item->SetCount(bCount);
5454: item->SetCount(item->GetCount() - 1);
6617: item->SetCount(item->GetCount() - 1);
6663: item->SetCount(item->GetCount() - 1);
9418: item->SetCount(item->GetCount() - 1);
12791: item->SetCount(0);
12796: item->SetCount(bCount);
12837: item->SetCount(0);
12842: item->SetCount(bCount);
13019: item->SetCount(item->GetCount() + bCount2);
13074: item->SetCount(item->GetCount() + bCount2);
15145: item->SetCount(item->GetCount() - 1);
15211: item->SetCount(item->GetCount() - 1);
```

Validation:
- Build gate passed:
```powershell
cmake --build build --config RelWithDebInfo --target GameServer --parallel 8
```
- `GameServer.exe` linked successfully.
- Existing warning-class output remains present.
- Build output still prints the known post-step environment message:
```text
'pwsh.exe' is not recognized as an internal or external command
```
  but the build command returned exit code `0`.

Header scan:
```powershell
Select-String -Path 'SRC/Server/GameServer/ecs/systems/ItemSystem.hpp' -Pattern 'LPITEM|LPCHARACTER|CHARACTER\s*\*'
```
Result: no matches.

Manual WinTest checklist:
- RemoveSpecifyItem-style quest/item hand-in.
- Dragon Coin partial stack consume path.
- Consumable scroll paths migrated in previous 15E-35 slices.
- Attribute/socket consumable smoke tests.
- Inventory stack split/drop/autogive stack merge smoke tests, because these remain legacy and must still behave unchanged.
- DragonSoul smoke test, because DS remains legacy for count transitions.
- Pet duration item smoke test, because it remains legacy.
- Timed/recall item smoke test, because it remains legacy.
- Relog persistence after count changes.
- Verify no duplicate item, missing item, wrong count, wrong owner/window/cell.
- Verify no delayed damage/metin collapse regression.

Commit status:
- Not committed yet. User requested review before commit.


## Phase 15E-35 final audit - full count extraction boundary

Date: 2026-04-27

Mode:
- Audit/final boundary confirmation after count extraction.
- No additional gameplay code changes in this pass.
- Not committed yet.

Goal:
- Verify whether any remaining `item->SetCount(...)` sites are still simple count mutations outside unsafe subsystems.

Result:
- No simple `item->SetCount(...)` sites remain outside the explicit unsafe/transactional categories.
- ECS count mutation is now used for all migrated simple consume paths through `ItemSystem::ConsumeItemEcs(...)`.
- Legacy count mutation remains only where count is coupled to broader item engine behavior.

Current count authority state:
- ECS-driven simple consume call sites in `ItemSystem_LegacyBridge.cpp`: 96.
- Remaining non-comment direct `item->SetCount(...)` sites in `ItemSystem_LegacyBridge.cpp`: 19.
- Remaining sites are not simple count writes; they are transactional legacy item engine boundaries.

Remaining direct `SetCount` categories:
- Drop/split path:
  - `3500: item->SetCount(item->GetCount() - bCount);`
  - Coupled to dropped item creation, socket copy, delayed save, and ground placement.
- Inventory stack/split path:
  - `3896: item->SetCount(item->GetCount() - count);`
  - `4140: item->SetCount(item->GetCount() - count);`
  - Coupled to target stack mutation, new item creation, socket copy, grid validation, and placement.
- AutoGive stack merge path:
  - `4390`, `4477`, `4674`, `4761`, `12791`, `12796`, `12837`, `12842`, `13019`, `13074`.
  - Coupled to stack merge, partial merge, item destroy, inventory routing, and placement decisions.
- Pet duration path:
  - `5454: item->SetCount(item->GetCount() - 1);`
  - Coupled to `new_petsystem` DB duration mutation.
- DragonSoul / DS charge / DS enchant path:
  - `6617`, `6663`, `9418`.
  - Coupled to DragonSoul charge/enchant state and logs.
- Timed / recall item path:
  - `15145`, `15211`.
  - Coupled to socket location state, item creation, and warp/recall behavior.

Why these were not converted:
- They require dedicated transactional ECS wrappers, not direct `ConsumeItemEcs` / `SetItemCountEcs` replacement.
- Blind conversion risks duplicated items, missing items, wrong stack counts, stale socket/location state, or broken DragonSoul/pet behavior.

Next required phases before these can be removed:
- ECS stack merge transaction wrapper.
- ECS drop/split transaction wrapper.
- ECS AutoGive stack merge replacement.
- ECS DragonSoul count/charge bridge.
- ECS pet duration item bridge.
- ECS timed/recall item bridge.

Validation status:
- Last build gate passed after the final code changes in this phase:
```powershell
cmake --build build --config RelWithDebInfo --target GameServer --parallel 8
```
- `GameServer.exe` linked successfully.
- `ItemSystem.hpp` remains free of `LPITEM`, `LPCHARACTER`, and `CHARACTER*` exposure.

Commit status:
- Not committed yet. User requested review before commit.


## Phase 15E-35 aggressive global SetCount pass

Date: 2026-04-27

Mode:
- Aggressive engine migration.
- Code change, no commit.

Goal:
- Scan all GameServer sources for `->SetCount(...)`, not only `ItemSystem_LegacyBridge.cpp`.
- Migrate every safe count mutation outside the explicitly unsafe subsystems.

Files changed in this pass:
- `SRC/Server/GameServer/ecs/systems/GayaSystem.cpp`
- `SRC/Server/GameServer/ItemUse.cpp`
- `SRC/Server/GameServer/fishing.cpp`
- `SRC/Server/GameServer/Halloween2022Dungeon.cpp`
- `SRC/Server/GameServer/LostCastleDungeon.cpp`
- `SRC/Server/GameServer/RuneDungeon.cpp`
- `SRC/Server/GameServer/new_switchbot.cpp`
- `SRC/Server/GameServer/guild_renewal.cpp`
- `SRC/Server/GameServer/cmd_general.cpp`
- `SRC/Server/GameServer/shop_manager.cpp`
- `docs/ecs_migration/MIGRATION_LOG_GPT.md`

Migrated safe count paths:
- Gaya material consume.
- Dragon Coin / Yang consumable paths in `ItemUse.cpp`.
- Fish use and grill consume paths.
- Halloween / Lost Castle / Rune dungeon item hand-in consumes.
- Switchbot price item consumes.
- Guild renewal item contribution deductions.
- Guild deposit partial item deduction.
- NPC shop sell partial stack deduction.

Implementation pattern:
```cpp
ItemSystem::ConsumeItemEcs(EntityFactory::CreateItemEntity(g_registry, item), amount);
ItemSystem::SetItemCountEcs(EntityFactory::CreateItemEntity(g_registry, item), newCount);
ItemSystem::DestroyItemEntityEcs(EntityFactory::CreateItemEntity(g_registry, item), reason);
```

Validation:
- Build gate passed after the global changes:
```powershell
cmake --build build --config RelWithDebInfo --target GameServer --parallel 8
```
- `GameServer.exe` linked successfully.
- Existing post-build environment message remains:
```text
'pwsh.exe' is not recognized as an internal or external command
```
  but the build returned exit code `0`.

Global scan after this pass:
- Remaining `->SetCount(...)` matches across GameServer: 80.
- Remaining `ConsumeItemEcs` / `SetItemCountEcs` / `DestroyItemEntityEcs` references across GameServer: 122.

Remaining `SetCount` files by category:
- `DragonSoul.cpp`: skipped by explicit rule.
- `New_PetSystem.cpp`: skipped by explicit rule.
- `cuberenewal.cpp`, `mining.cpp`, refine scroll sections in `ItemSystem_LegacyBridge.cpp`: refine/crafting-like internals, skipped.
- `shop.cpp`, `safebox.cpp`, `CombatSystem.cpp`, parts of `ItemSystem_LegacyBridge.cpp`: stack merge/split/placement transactions, skipped.
- `attr_transfer.cpp`, `PlayerRuntimeSystem.cpp`, `cmd_general.cpp` bottle split path: attribute/special unique item logic, skipped.
- `item_manager.cpp` and `ItemSystem.cpp`: core legacy item engine / legacy mirror boundary, allowed for now.

Why the remaining count is not below 20 yet:
- The global scan includes all explicitly skipped unsafe subsystems and core item engine boundaries.
- Reducing below 20 requires dedicated transactional ECS wrappers for stack merge/split, shop/safebox placement, DragonSoul, refine/cube/mining, pet books, and special unique item paths.
- Blind replacement would break ownership, placement, item destruction, stack merge, or special subsystem behavior.

Manual WinTest checklist:
- Gaya craft/consume path.
- Dragon Coin and Yang consumable items.
- Fish use and grill.
- Halloween / Lost Castle / Rune hand-in flows.
- Switchbot roll with price item consumption.
- Guild renewal contribution and guild deposit item path.
- NPC shop partial sell.
- Inventory relog persistence after each item count mutation.
- Verify no duplicate item, missing item, wrong count, wrong owner/window/cell.
- Verify no delayed damage/metin collapse regression.

Commit status:
- Not committed yet. User requested review before commit.

## Phase 15E-35 multi-engine LPITEM replacement sprint

Date: 2026-04-27

Mode:
- Large aggressive ECS engine migration.
- Code change, no commit.

Scope completed in this pass:
- Added ECS-facing attribute mutation wrapper APIs to `ItemSystem`.
- Migrated safe quest current-item socket/count/attribute mutations to ECS APIs.
- Migrated switchbot attribute reroll calls through ECS wrapper while preserving legacy reroll semantics.
- Migrated combat arrow count mutation to `SetItemCountEcs`.
- Left DragonSoul, refine, pet duration, timed unique, stack merge, shop/exchange/safebox/offlineshop transaction paths untouched as required.

Files changed in this pass:
- `SRC/Server/GameServer/ecs/systems/ItemSystem.hpp`
- `SRC/Server/GameServer/ecs/systems/ItemSystem.cpp`
- `SRC/Server/GameServer/questlua_item.cpp`
- `SRC/Server/GameServer/new_switchbot.cpp`
- `SRC/Server/GameServer/ecs/systems/CombatSystem.cpp`
- `docs/ecs_migration/MIGRATION_LOG_GPT.md`

New ECS APIs added:
```cpp
bool SetItemForceAttributeEcs(entt::entity item, int index, uint8_t type, int16_t value);
bool AddItemAttributeEcs(entt::entity item);
bool ChangeItemAttributeEcs(entt::entity item);
bool ClearItemAttributesEcs(entt::entity item);
```

Implementation notes:
- `SetItemForceAttributeEcs` routes through existing `SetItemAttribute`, so ECS component data is updated and legacy is mirrored through the existing bridge.
- `AddItemAttributeEcs` and `ChangeItemAttributeEcs` preserve existing legacy random attribute generation by calling the legacy CItem methods internally, then syncing ECS attributes from legacy.
- `ClearItemAttributesEcs` clears ECS attributes and mirrors the zeroed attribute array back to legacy if available.
- No public `LPITEM` API was introduced.

Migrated call sites:
- `questlua_item.cpp`
  - `item.set_value` now uses `SetItemForceAttributeEcs`.
  - `item.set_attr` now uses `SetItemForceAttributeEcs`.
  - `item.set_socket` now uses `SetItemSocketEcs`.
  - `item.set_count` now uses `SetItemCountEcs`.
  - normal attribute add/change branches now use `AddItemAttributeEcs` / `ChangeItemAttributeEcs`.
  - rare attribute add/change remains legacy because rare attribute wrapper is not defined yet and must preserve exact rare table behavior.
- `new_switchbot.cpp`
  - Three `pkItem->ChangeAttribute()` reroll points now call `ChangeItemAttributeEcs(EntityFactory::CreateItemEntity(g_registry, pkItem))`.
  - Direct `ITEM_MANAGER::Find` and LPITEM switchbot helper signatures remain because `CheckItem`, `SendItemUpdate`, and ownership/packet update logic are still LPITEM-heavy.
- `CombatSystem.cpp`
  - `CHARACTER::UseArrow` now uses `SetItemCountEcs(EntityFactory::CreateItemEntity(g_registry, pkArrow), iCount)` instead of direct `pkArrow->SetCount(iCount)`.
  - Stack merge/drop logic in the same file was not touched because it is explicitly stack merge core.

Target scan counts:

Baseline from sprint start:
```text
LPITEM total in primary targets: 497
SetCount pattern total in primary targets: 58
SetSocket pattern total in primary targets: 110
Attribute mutation pattern total in primary targets: 71
ITEM_MANAGER Find pattern total in primary targets: 16
```

After this pass:
```text
LPITEM total in primary targets: 500
SetCount pattern total in primary targets: 57
Direct ->SetCount total in primary targets: 56
SetSocket pattern total in primary targets: 110
Direct ->SetSocket total in primary targets: 95
Attribute mutation pattern total in primary targets: 69
Direct attribute mutation pattern total in primary targets: 47
ITEM_MANAGER Find pattern total in primary targets: 16
```

Why LPITEM did not drop in this pass:
- This sprint added ECS wrappers but intentionally kept legacy bridge implementation internal, which adds a few `LPITEM` bridge references in `ItemSystem.cpp`.
- The useful progress is mutation authority movement, not pointer-count reduction in this batch.
- Public ECS header remained pointer-clean.

Skipped blockers:
- `ItemSystem_LegacyBridge.cpp` remaining count mutations are mostly stack merge/drop/autogive, pet duration DB state, DragonSoul charge/enchant, refine scroll/material, timed/recall/special unique paths.
- `DragonSoul.cpp` remains skipped by rule: DS internals and refine/activation socket/count semantics need dedicated DS wrappers.
- `PlayerRuntimeSystem.cpp` remaining count mutation is acce/costume-sensitive and was not touched.
- `CombatSystem.cpp` remaining count mutations are stack merge / reward drop transaction logic and were not touched.
- `questlua_item.cpp` rare attribute add/change remains legacy until rare-attribute ECS wrappers exist.
- `new_switchbot.cpp` direct lookup remains because replacing `ITEM_MANAGER::Find` safely requires a switchbot-specific entity lookup/mutation wrapper that still preserves `CheckItem` and packet update behavior.

Build gates:
```powershell
cmake --build build --config RelWithDebInfo --target GameServer --parallel 8
```
- Passed after adding ItemSystem attribute APIs and quest mutation migration.
- Passed after switchbot and combat count/attribute migration.
- Passed after final quest socket/count API rename.
- `GameServer.exe` linked successfully each time.
- Existing post-build message remained:
```text
'pwsh.exe' is not recognized as an internal or external command
```
  but build exit code was `0`.

Header validation:
```powershell
Select-String -Path 'SRC/Server/GameServer/ecs/systems/ItemSystem.hpp' -Pattern 'LPITEM|LPCHARACTER|CHARACTER\s*\*'
```
- Result: no matches.

Manual WinTest checklist for this pass:
- Quest item socket set/get.
- Quest item set_count.
- Quest normal attribute add/change/set/clear.
- Quest rare attribute add/change smoke test because rare path still uses legacy.
- Switchbot start/reroll/stop and relog attribute persistence.
- Bow/arrow combat consume path.
- Inventory open/relog after each mutation path.
- Verify no duplicate item, missing item, wrong count, wrong owner/window/cell.
- Verify no delayed damage/metin collapse regression.

Commit status:
- Not committed. User requested review before commit.

---

## Phase 15E-46 - CItem ownership + proto read accessors

Date: 2026-04-28

Mode:
- Entity-first read accessor migration.
- Code changes committed in small per-file commits.

Goal:
- Move next-tier CItem read accessors behind `ItemSystem` ECS APIs.
- Keep public `ItemSystem.hpp` entity-only and pointer-clean.

New / verified ECS read APIs:
- `ItemSystem::GetItemCell(entt::entity)`
- `ItemSystem::GetItemWindow(entt::entity)`
- `ItemSystem::IsItemEquipped(entt::entity)`
- `ItemSystem::GetItemOwnerEntity(entt::entity)`
- `ItemSystem::GetItemAntiFlag(entt::entity)`
- `ItemSystem::GetItemWearFlag(entt::entity)`
- `ItemSystem::GetItemProto(entt::entity)`
- `ItemSystem::GetItemAttribute(entt::entity, int)`

Files changed:
- `SRC/Server/GameServer/ecs/systems/ItemSystem.hpp`
- `SRC/Server/GameServer/ecs/systems/ItemSystem.cpp`
- `SRC/Server/GameServer/DragonSoul.cpp`
- `SRC/Server/GameServer/buff_on_attributes.cpp`
- `SRC/Server/GameServer/mining.cpp`
- `SRC/Server/GameServer/new_offlineshop.cpp`
- `SRC/Server/GameServer/new_switchbot.cpp`
- `SRC/Server/GameServer/ecs/systems/CombatSystem.cpp`
- `SRC/Server/GameServer/ecs/systems/MovementSystem.cpp`
- `SRC/Server/GameServer/ecs/systems/NetworkSyncSystem.cpp`
- `SRC/Server/GameServer/ecs/systems/PlayerRuntimeSystem.cpp`
- `SRC/Server/GameServer/ecs/systems/StatSystem.cpp`
- `SRC/Server/GameServer/ecs/systems/MountSystem.cpp`
- `SRC/Server/GameServer/char_manager.cpp`
- `SRC/Server/GameServer/shop.cpp`
- `SRC/Server/GameServer/MountInventory.cpp`
- `SRC/Server/GameServer/new_offlineshop.h`
- `SRC/Server/GameServer/war_map.cpp`
- `SRC/Server/GameServer/ecs/systems/InventorySystem.cpp`
- `SRC/Server/GameServer/exchange.cpp`
- `SRC/Server/GameServer/questlua_item.cpp`

Migrations performed:
- `GetCell` / `GetWindow` caller-side item position reads moved to `ItemSystem`.
- `IsEquipped` caller-side equipment checks moved to `ItemSystem`.
- `GetAntiFlag` / `GetWearFlag` proto-flag reads moved to `ItemSystem`.
- `GetAttribute` item attribute reads moved to `ItemSystem`.
- `GetOwner` item-owner reads now use `GetItemOwnerEntity`; legacy `LPCHARACTER` is recovered only at bridge boundaries via `ecs::LegacyCharOf`.
- `GetProto` reads now use `GetItemProto` with null checks before dereference.

Final bridge-excluded scan:
```text
GetCell remaining:       8
IsEquipped remaining:    1
GetOwner remaining:      29
GetAntiFlag remaining:   0
GetProto remaining:      1
GetWindow remaining:     5
GetWearFlag remaining:   0
GetAttribute remaining:  6
```

Remaining blockers / false positives:
- `item_manager.cpp`: core legacy item persistence/destruction internals still intentionally call CItem accessors.
- `building.cpp`, `cmd_gm.cpp`: land owner false positives, not CItem ownership.
- `exchange.cpp`, `input_main.cpp`: exchange company owner false positives, not CItem ownership.
- `MountSystem.cpp`, `New_PetSystem.cpp`, `PetSystem.cpp`: actor/system owner false positives, not CItem ownership.
- `ActivitySystem.cpp`, `CombatSystem.cpp`, `SkillSystem.cpp`, `char_manager.cpp`, `cmd_gm.cpp`: `CSectree::GetAttribute` false positives, not item attributes.

Validation:
```powershell
cmake --build build --config RelWithDebInfo --target GameServer --parallel 8
```
- Build passed after owner batch.
- Build passed after proto batch.
- Build passed after final remaining cell/window/attribute cleanup.
- Existing warnings remain unrelated.
- Existing post-build message remained:
```text
'pwsh.exe' is not recognized as an internal or external command
```
  but build exit code was `0`.

Header validation:
```powershell
Select-String -Path SRC/Server/GameServer/ecs/systems/ItemSystem.hpp -Pattern 'LPITEM|LPCHARACTER|CHARACTER\s*\*'
```
- Result: no matches.

Manual WinTest checklist:
- Login/relog.
- Inventory open and item tooltip display.
- Equip/unequip weapon and armor.
- Move item between inventory slots.
- Drop item and pick it back up.
- Trade item / ownership transfer smoke test.
- DragonSoul activate/deactivate and refine smoke test.
- Mount inventory expiration smoke test.
- Offline shop item expiration display.
- Quest copy/remove item path.
- Verify no wrong owner, wrong window/cell, missing item, duplicate item, or VID drift.

Commit status:
- Committed as per-file Phase 15E-46 commits.

## Phase 15E-45: CItem read accessor migration batch

Date: 2026-04-28

Mode:
- Read-only call-site migration.
- Commit after each file.
- `ItemSystem_LegacyBridge.cpp` skipped by design because it is the legacy bridge.

Goal:
- Move caller-side CItem read access from `item->GetVnum/GetValue/GetSocket/GetID/GetType/GetCount/GetSubType` to entity-first `ItemSystem` accessors.
- Keep behavior read-only and preserve all lifecycle/storage semantics.

Accessor status:
- Existing entity accessors were present and reused:
```cpp
ItemSystem::GetItemID
ItemSystem::GetItemVnum
ItemSystem::GetItemType
ItemSystem::GetItemSubType
ItemSystem::GetItemCount
ItemSystem::GetItemValue
ItemSystem::GetItemSocket
```
- No public `LPITEM` API was added.

Files migrated in this batch:
- `SRC/Server/GameServer/PlayerRuntimeSystem.cpp`
- `SRC/Server/GameServer/ecs/systems/CombatSystem.cpp`
- `SRC/Server/GameServer/cmd_general.cpp`
- `SRC/Server/GameServer/questlua_pc.cpp`
- `SRC/Server/GameServer/new_switchbot.cpp`
- `SRC/Server/GameServer/shop.cpp`
- `SRC/Server/GameServer/DragonSoul.cpp`
- `SRC/Server/GameServer/fishing.cpp`
- `SRC/Server/GameServer/ecs/systems/StatSystem.cpp`
- `SRC/Server/GameServer/ecs/systems/InventorySystem.cpp`
- `SRC/Server/GameServer/ecs/systems/ActivitySystem.cpp`
- `SRC/Server/GameServer/ecs/systems/MountSystem.cpp`
- `SRC/Server/GameServer/cuberenewal.cpp`
- `SRC/Server/GameServer/New_PetSystem.cpp`
- `SRC/Server/GameServer/attr_transfer.cpp`
- `SRC/Server/GameServer/safebox.cpp`
- `SRC/Server/GameServer/mining.cpp`
- `SRC/Server/GameServer/shopEx.cpp`
- `SRC/Server/GameServer/ecs/systems/SkillSystem.cpp`
- `SRC/Server/GameServer/ecs/systems/NetworkSyncSystem.cpp`
- `SRC/Server/GameServer/new_offlineshop_manager.cpp`
- `SRC/Server/GameServer/exchange.cpp`
- `SRC/Server/GameServer/new_offlineshop.cpp`
- `SRC/Server/GameServer/MountInventory.cpp`
- `SRC/Server/GameServer/PetSystem.cpp`
- `SRC/Server/GameServer/ItemUse.cpp`

Counts:
```text
Bridge-excluded target read calls before this batch: 929
Bridge-excluded target read calls after this batch:  381
```

Skipped / deferred:
- `SRC/Server/GameServer/ecs/systems/ItemSystem_LegacyBridge.cpp`: intentionally skipped; it is the legacy CItem bridge.
- `SRC/Server/GameServer/item_manager.cpp`: core item manager / lifecycle island, deferred.
- `SRC/Server/GameServer/ecs/systems/ItemSystem.cpp`: internal bridge/system implementation, deferred.
- `guild_manager.cpp`, `guild_renewal.cpp`, `cmd_gm.cpp`, `battle_pass.cpp`, `dragon_soul_table.cpp`, `building.cpp`: current grep hits are largely non-CItem APIs such as guild/table/row/building `GetID/GetValue/GetType`; skipped to avoid false-positive migration.
- `CItem` declarations/implementations were not removed.

Validation:
```powershell
cmake --build build --config RelWithDebInfo --target GameServer --parallel 8
```
- Build passed after each implemented group.
- `GameServer.exe` linked successfully.
- Earlier batch builds still showed the existing post-build message:
```text
'pwsh.exe' is not recognized as an internal or external command
```
  but build exit code was `0`.
- The final build completed without that post-build message.

Known warnings:
- Existing C4805/C4804/C4244 warnings remain in unrelated legacy areas.

Manual WinTest checklist:
- Login and inventory item display.
- Item tooltip vnum/count/type/socket/value display.
- Equip weapon/armor and verify stat behavior.
- Pet/mount summon and mount inventory persistence.
- Safebox deposit/withdraw and stack split.
- Shop/offlineshop/exchange item display and transaction smoke test.
- DragonSoul/fishing/cube/refine smoke tests for read-path regressions.
- Verify no duplicate item, no missing item, no wrong owner/window/cell, no VID drift.

Commit status:
- File-by-file commits created for the migrated files.

### Phase 15E-45 continuation: remaining caller-side CItem read cleanup

Date: 2026-04-28

Mode:
- Read-only call-site migration continuation.
- Build gate after each migration group.
- File-by-file commits.

Additional files migrated:
- `SRC/Server/GameServer/input_main.cpp`
- `SRC/Server/GameServer/shop_manager.cpp`
- `SRC/Server/GameServer/battle.cpp`
- `SRC/Server/GameServer/char_manager.cpp`
- `SRC/Server/GameServer/guild_renewal.cpp`
- `SRC/Server/GameServer/ecs/systems/DragonSoulSystem.cpp`
- `SRC/Server/GameServer/blend_item.cpp`
- `SRC/Server/GameServer/questmanager.cpp`
- `SRC/Server/GameServer/VikingDungeon.cpp`
- `SRC/Server/GameServer/LostCastleDungeon.cpp`
- `SRC/Server/GameServer/ani.cpp`
- `SRC/Server/GameServer/polymorph.cpp`
- `SRC/Server/GameServer/db.cpp`
- `SRC/Server/GameServer/input_db.cpp`
- `SRC/Server/GameServer/over9refine.cpp`
- `SRC/Server/GameServer/RuneDungeon.cpp`
- `SRC/Server/GameServer/Halloween2022Dungeon.cpp`
- `SRC/Server/GameServer/questlua_item.cpp`
- `SRC/Server/GameServer/mount_inventory_helper.h`
- `SRC/Server/GameServer/belt_inventory_helper.h`
- `SRC/Server/GameServer/buff_on_attributes.cpp`
- `SRC/Server/GameServer/new_offlineshop.h`
- `SRC/Server/GameServer/ecs/systems/GayaSystem.cpp`
- `SRC/Server/GameServer/refine.cpp`

Counts:
```text
Bridge-excluded target read calls after prior 15E-45 batch: 381
Bridge-excluded target read calls after continuation:       245
```

Remaining scan notes:
- `item_manager.cpp`: core item manager/lifecycle island; deferred because entity wrapping during item creation/save/remove requires a separate core design.
- `InventorySystem.cpp`: remaining hits are `this->Get...` inside legacy `CItem` method bodies; deferred with bridge/core work.
- `ItemSystem.cpp`, `EntityFactory.cpp`, `ItemRegistry*`, and `ItemSystem_LegacyBridge.cpp`: intentionally excluded bridge/internal files.
- Most remaining non-excluded matches are false positives for this phase: guild/building/shop/table parser/descriptor APIs such as `CGuild::GetID`, `CShopEx::GetVnum`, `CGroupNode::GetValue`, `DESC::GetSocket`, `CEntity::GetType`.
- `CItem` method declarations and implementations were not removed.

Validation:
```powershell
cmake --build build --config RelWithDebInfo --target GameServer --parallel 8
```
- Build passed after each group and final verification.
- `ItemSystem.hpp` pointer scan stayed clean.

Manual WinTest checklist:
- Login and inventory item display.
- Tooltip vnum/count/type/socket/value display.
- Equip weapon/armor and verify stat behavior.
- Shop/offlineshop/exchange item display and transaction smoke test.
- Quest item use/reward checks.
- Dungeon key/item gates.
- DragonSoul, polymorph, refine, Gaya, pet/mount smoke tests.
- Verify no duplicate/missing item, wrong owner/window/cell, or VID drift.

Commit status:
- Additional file-by-file commits created.

## Phase 15E-45-prep: Full pointer typedef audit

Date: 2026-04-28

Mode:
- AUDIT ONLY.
- No code/gameplay changes.

Goal:
- Identify all `LPxxx` pointer-style typedefs that matter before claiming full EnTT migration.
- Categorize each as GAME ENTITY, SERVICE OBJECT, INFRASTRUCTURE, or DATA STRUCTURE.

Report:
- Saved comprehensive audit to:
```text
docs/ecs_migration/phase15e_45_pointer_audit.txt
```

Findings:
```text
Unique LP types found: 22
GameServer LP types: 17
Core/infrastructure LP types additionally found under SRC/Server: 5
```

Largest remaining pointer surfaces by token refs:
```text
LPCHARACTER  2020
LPITEM       1038
LPDESC        370
LPDUNGEON     208
LPEVENT       203
LPENTITY      135
LPPARTY        94
LPSECTREE_MAP  85
LPSECTREE      64
LPFDWATCH      61
LPBUFFER       53
```

Category totals:
```text
GAME ENTITY:     LPCHARACTER, LPITEM, LPENTITY, LPOBJECT
SERVICE OBJECT:  LPPARTY, LPSHOP, LPSHOPEX, LPDUNGEON
INFRASTRUCTURE:  LPDESC, LPCLIENT_DESC, LPDESC_P2P, LPEVENT, LPBUFFER, LPFDWATCH, LPKEVENT, LPHEART, LPLOGFILE
DATA STRUCTURE:  LPSECTREE, LPSECTREE_LIST, LPSECTREE_MAP, LPREGEN, LPREGEN_EXCEPTION
```

Roadmap conclusion:
- Full EnTT requires finishing `LPCHARACTER` and `LPITEM`, then migrating generic `LPENTITY` and `LPOBJECT` public handles.
- Party/shop/dungeon should remain service classes but expose entity-first APIs.
- Descriptor/buffer/fdwatch/heart/logfile should remain infrastructure, not gameplay entities.
- `LPEVENT` needs separate scheduler/Flecs design.
- Sectree and regen should remain data structures initially, with entity-facing adapter APIs.

Validation:
```powershell
cmake --build build --config RelWithDebInfo --target GameServer --parallel 8
```
- Build passed.
- `GameServer.exe` linked successfully.

Commit status:
- Committed as:
```text
Phase 15E-45-prep: Full pointer typedef audit
```

## Phase 15E-38: Timed/recall item count consume to ECS

Date: 2026-04-28

Mode:
- Audit + verification.
- No code change required.
- Documentation-only phase.

Context:
- The timed/recall `SetCount` sites from the earlier audit were already migrated during the consolidated 15E-21 through 15E-36d work.
- This phase verified the current code state and socket-state safety instead of reapplying an already completed migration.

Audit result:
- `ItemSystem_LegacyBridge.cpp` direct `->SetCount` count: 1.
- Remaining direct `SetCount` is `pItem->SetCount(pItem->GetCount() + count)` inside core `CItem::SetCount` internals and is outside Phase 15E-38 scope.
- `GiveRecallItem` uses `ItemSystem::ConsumeItemEcs(EntityFactory::CreateItemEntity(g_registry, item))`.
- `ProcessRecallItem` uses `ItemSystem::ConsumeItemEcs(EntityFactory::CreateItemEntity(g_registry, item))`.

Socket preservation:
- `GiveRecallItem` preserves recall coordinates before consume. For single-count items it writes socket 0/1 on the current item; for stacked items it creates a split recall item with copied coordinates, then consumes one count from the original stack through ECS.
- `ProcessRecallItem` reads socket 0/1 for map/warp coordinates before ECS consume.
- No additional socket clearing or socket sync hook was needed because `ConsumeItemEcs` runs only after coordinate state is used or copied.

Validation:
```powershell
cmake --build build --config RelWithDebInfo --target GameServer --parallel 8
```
- Build passed.
- `ItemSystem.hpp` remains pointer-clean: no `LPITEM`, `LPCHARACTER`, or `CHARACTER*` public exposure.

Manual WinTest checklist:
- Use recall item with count 1 and verify warp target is correct and the item is destroyed at zero.
- Use stacked recall item and verify split/copy coordinates are preserved while the original stack decrements.
- Use timed item if available and verify expiration/count behavior.
- Logout/login with timed/recall items.
- Check `syserr.txt` for socket, destroy, registry, and `VID_DRIFT` errors.
- Verify no delayed damage/metin collapse regression.

Commit status:
- Documentation-only commit planned.

## Phase 15E-39: Per-subsystem direct SetCount / M2_DESTROY_ITEM audit

Date: 2026-04-28

Mode:
- Audit only.
- No code changes.
- Report generated for remaining LPITEM purge work.

Generated report:
- `docs/ecs_migration/phase15e_39_lpitem_remaining_audit.txt`

Tree-wide counts:
- Direct `->SetCount(...)` matches: 44.
- `M2_DESTROY_ITEM(...)` matches including macro definitions: 107.
- Real `M2_DESTROY_ITEM(...)` call sites excluding macro definitions: 105.

Top remaining `->SetCount` subsystems:
- DragonSoul: 11.
- Shop/storage/trade/offlineshop: 10.
- Refine/crafting/material consume: 8.
- Combat: 6.
- Pet: 2.
- Command/GM: 1.
- Other gameplay: 1.
- Core item engine: 1.
- Quest/questlua: 1.
- ItemSystem ECS bridge/sync: 1.

Important classification:
- `ItemSystem_LegacyBridge.cpp` line 826 is core `CItem::SetCount` boundary logic, not caller-side count consume.
- `ItemSystem.cpp` direct legacy `SetCount` and `M2_DESTROY_ITEM(legacyItem)` are ECS bridge/mirror internals behind public entity APIs.
- DragonSoul, shop/storage/offlineshop, refine/crafting, and transaction flows must not be blindly converted; they need subsystem wrappers because count/destroy is tied to ownership, stack merge, packet sync, or refine semantics.

Top remaining `M2_DESTROY_ITEM` subsystems:
- Command/GM: 60.
- Shop/storage/trade/offlineshop: 12.
- DragonSoul: 7.
- ECS system cleanup paths: 6.
- World/dungeon/sectree cleanup: 5.
- Combat: 3.
- ItemSystem legacy bridge / CItem boundary: 3.
- Refine/crafting/material consume: 2.
- Mount: 2.
- Quest/questlua: 2.

Recommended next priorities:
- Phase 15E-40: DragonSoul count/destroy bridge, wrapper-first.
- Phase 15E-41: shop/storage/cube stack split/merge wrappers.
- Phase 15E-42: CombatSystem/PlayerRuntime direct count cleanup.
- Phase 15E-43: command/GM and world cleanup destroy conversion.
- Continue replacing high-frequency read-only `CItem` methods with `ItemSystem` accessors before attempting CItem surface deletion.

Validation:
- Audit-only phase; no source code was changed.
- Build gate run after report generation.

Commit status:
- Documentation/report commit planned.

## Phase 15E-40: Combat ECS cleanup + cmd_gm bulk destroy migration

Date: 2026-04-28

Mode:
- Code change with build gates.
- ECS system cleanup first, then GM bulk destroy and world cleanup paths.
- DragonSoul, shop/trade/safebox, refine/cube, and pet internals intentionally left for later phases.

Part A - ECS system internal cleanup:
- `CombatSystem.cpp`
  - Replaced stack merge target `SetCount(+bCount2)` with `ItemSystem::AddItemCountEcs`.
  - Replaced `SetCount(0) + M2_DESTROY_ITEM(item)` full consume branches with `ItemSystem::ConsumeItemEcs`.
  - Replaced partial remaining-count assignment with `ItemSystem::SetItemCountEcs`.
  - Replaced shared dungeon drop template destroy with `ItemSystem::DestroyItemEntityEcs`.
- `PlayerRuntimeSystem.cpp`
  - Replaced acce clean-attr material decrement with `ItemSystem::ConsumeItemEcs`.
- `GayaSystem.cpp`
  - Replaced temporary glimmerstone destroy paths with `ItemSystem::DestroyItemEntityEcs`.
  - Preserved item name before destroy in the missing-material chat path to avoid post-destroy pointer read.
- `InventorySystem.cpp`
  - Replaced rune add failure `M2_DESTROY_ITEM(this)` with `ItemSystem::DestroyItemEntityEcs`.
- `SessionSystem.cpp`
  - Replaced safebox/mall load add-failure destroys with `ItemSystem::DestroyItemEntityEcs`.
- `MountSystem.cpp`
  - Replaced mount inventory load add-failure destroy with `ItemSystem::DestroyItemEntityEcs`.

Part B - `cmd_gm.cpp` bulk destroy migration:
- Replaced all 60 direct `M2_DESTROY_ITEM(item)` calls with:
```cpp
ItemSystem::DestroyItemEntityEcs(
    EntityFactory::CreateItemEntity(g_registry, item),
    "GM_CMD_DESTROY");
```
- Added `ecs/EntityFactory.hpp`.
- Direct `M2_DESTROY_ITEM(item)` count in `cmd_gm.cpp`: 60 -> 0.

Part C - World cleanup destroy paths:
- `dungeon.cpp`: dungeon entity item cleanup now uses `DestroyItemEntityEcs`.
- `sectree.cpp`: sectree destroy item cleanup now uses `DestroyItemEntityEcs`.
- `sectree_manager.cpp`: private-map and attr cleanup item destroy now uses `DestroyItemEntityEcs`.
- `MountInventory.cpp`: `RemoveFromCharacter()` is still executed first, then the removed item is destroyed through `DestroyItemEntityEcs`.
- `char_manager.cpp` audit hit is inside a commented-out legacy mall block and was not changed.

Validation:
- Build passed after each modified ECS system file in Part A.
- Build passed after `cmd_gm.cpp` bulk migration.
- Build passed after world cleanup migration.
- Final build passed:
```powershell
cmake --build build --config RelWithDebInfo --target GameServer --parallel 8
```

Count results:
```text
Tree-wide ->SetCount before Phase 15E-40: 44
Tree-wide ->SetCount after Phase 15E-40:  37

Tree-wide M2_DESTROY_ITEM real call sites before Phase 15E-40: 105
Tree-wide M2_DESTROY_ITEM real call sites after Phase 15E-40:   30
```

Remaining `->SetCount` hot files:
- `DragonSoul.cpp`: 11.
- `shop.cpp`: 8.
- `cuberenewal.cpp`: 5.
- `New_PetSystem.cpp`: 2.
- `attr_transfer.cpp`: 2.
- `safebox.cpp`: 2.
- Single remaining matches in `ItemSystem.cpp`, `ItemSystem_LegacyBridge.cpp`, `item_manager.cpp`, `cmd_general.cpp`, `mining.cpp`, `questlua_item.cpp`, and commented `ItemUse.cpp`.

Remaining `M2_DESTROY_ITEM` hot files:
- `DragonSoul.cpp`: 7.
- `shop.cpp`: 5.
- `new_offlineshop_manager.cpp`: 5.
- `ItemSystem_LegacyBridge.cpp`: 3 core CItem boundary calls.
- `ItemSystem.cpp`: 2 bridge calls.
- `questlua_pc.cpp`: 2.
- `cuberenewal.cpp`: 2.
- Single remaining matches in `shopEx.cpp`, `safebox.cpp`, `char_manager.cpp` commented block, and `item_manager.cpp`.

Commits:
- `Phase 15E-40 PART A: CombatSystem ECS legacy mutation cleanup`
- `Phase 15E-40 PART A: PlayerRuntimeSystem ECS count cleanup`
- `Phase 15E-40 PART A: GayaSystem ECS destroy cleanup`
- `Phase 15E-40 PART A: InventorySystem ECS destroy cleanup`
- `Phase 15E-40 PART A: SessionSystem ECS destroy cleanup`
- `Phase 15E-40 PART A: MountSystem ECS destroy cleanup`
- `Phase 15E-40 PART B: cmd_gm.cpp bulk M2_DESTROY_ITEM migration`
- `Phase 15E-40 PART C: World cleanup destroy paths to ECS`

Manual WinTest checklist:
- Combat: mob death, drop appears, pickup to inventory, stack merge, full-stack source disappears, partial stack source remains correct.
- GM: `/item <vnum>`, job/full item commands, create+destroy failure paths, no orphan registry entries.
- World cleanup: ground item cleanup, dungeon clear, sectree cleanup, mount inventory destroy path, safebox/mall load failure if reproducible.
- Check `syserr.txt` for item destroy errors, ECS orphan warnings, double destruction, and `VID_DRIFT`.
- Verify no delayed damage/metin collapse regression.

Commit status:
- Code commits completed.
- This log update committed separately as Phase 15E-40 completion documentation.

## Phase 15E-41: Quest + Safebox subsystem cleanup

Date: 2026-04-28

Mode:
- Code change with build gates.
- Scope limited to `questlua_pc.cpp` and `safebox.cpp`.
- Shop/offlineshop, cube/refine, DragonSoul, and pet paths intentionally untouched.

Part A - `questlua_pc.cpp` destroy migration:
- Replaced quest item insertion failure destroy with:
```cpp
ItemSystem::DestroyItemEntityEcs(
    EntityFactory::CreateItemEntity(g_registry, item),
    "QUEST_ITEM_FAIL");
```
- Replaced quest-created item cleanup failure destroy for `pkNewItem` with the same ECS destroy path.
- Added `ecs/EntityFactory.hpp`.
- Build passed after Part A.

Part B - `safebox.cpp` transaction cleanup:
- Replaced destructor cleanup:
```cpp
LPITEM removed = m_pkItems[i]->RemoveFromCharacter();
ItemSystem::DestroyItemEntityEcs(
    EntityFactory::CreateItemEntity(g_registry, removed),
    "SAFEBOX_DESTRUCT");
```
- Replaced safebox stack split source decrement with `ItemSystem::ConsumeItemEcs`.
- Replaced safebox stack split target increment with `ItemSystem::AddItemCountEcs`.
- Preserved `RemoveFromCharacter()` before destroy in destructor cleanup.
- Build passed after Part B.

Verification:
```text
questlua_pc.cpp SetCount=0 M2_DESTROY_ITEM=0
safebox.cpp     SetCount=0 M2_DESTROY_ITEM=0

Tree-wide ->SetCount after Phase 15E-41: 35
Tree-wide M2_DESTROY_ITEM real call sites after Phase 15E-41: 27
```

Final validation:
```powershell
cmake --build build --config RelWithDebInfo --target GameServer --parallel 8
```
- Build passed.

Manual WinTest checklist:
- Quest item reward path.
- Quest-created item cleanup/failure path if reproducible.
- Safebox open/close.
- Deposit/withdraw normal item.
- Deposit/withdraw stackable item with partial split.
- Logout/relogin and verify safebox persistence.
- Check `syserr.txt` for `QUEST_ITEM_FAIL`, `SAFEBOX_DESTRUCT`, transaction mismatch, orphan item, double-destroy, and `VID_DRIFT`.
- Verify no delayed damage/metin collapse regression.

Commits:
- `Phase 15E-41 PART A: questlua_pc.cpp destroy paths to ECS`
- `Phase 15E-41 PART B: safebox.cpp count and destroy to ECS`

Commit status:
- Code commits completed.
- This log update committed separately as Phase 15E-41 completion documentation.

## Phase 15E-42: Shop / OfflineShop / shopEx subsystem cleanup

Date: 2026-04-28

Mode:
- Code change with build gates.
- Scope limited to `shop.cpp`, `new_offlineshop_manager.cpp`, and `shopEx.cpp`.
- Cube/refine/attr_transfer, DragonSoul, and pet paths intentionally untouched.

Part A - `shop.cpp` NPC shop transactions:
- Replaced NPC shop stack merge target increments with `ItemSystem::AddItemCountEcs`.
- Replaced partial remaining source count assignments with `ItemSystem::SetItemCountEcs`.
- Replaced full source stack cleanup and temporary item cleanup with `ItemSystem::DestroyItemEntityEcs`.
- Added `ecs/EntityFactory.hpp` and `ecs/Registry.hpp`.
- Target file result: `shop.cpp SetCount=0 M2_DESTROY_ITEM=0`.
- Build passed after Part A.

Part B - `new_offlineshop_manager.cpp` player offline shop destroys:
- Replaced temp item cleanup with `ItemSystem::DestroyItemEntityEcs(..., "OFFLINESHOP_TEMP")`.
- Replaced sell/remove cleanup with:
```cpp
LPITEM removed = item->RemoveFromCharacter();
ItemSystem::DestroyItemEntityEcs(
    EntityFactory::CreateItemEntity(g_registry, removed),
    "OFFLINESHOP_SELL");
```
- Preserved `RemoveFromCharacter()` before destruction for attached items.
- Target file result: `new_offlineshop_manager.cpp SetCount=0 M2_DESTROY_ITEM=0`.
- Build passed after Part B.

Part C - `shopEx.cpp` destroy migration:
- Replaced the extended shop inventory-full temporary item destroy with `ItemSystem::DestroyItemEntityEcs(..., "SHOP_EX_TRANSACTION")`.
- Target file result: `shopEx.cpp SetCount=0 M2_DESTROY_ITEM=0`.
- Build passed after Part C.

Final count verification:
```text
shop.cpp                    SetCount=0 M2_DESTROY_ITEM=0
new_offlineshop_manager.cpp SetCount=0 M2_DESTROY_ITEM=0
shopEx.cpp                  SetCount=0 M2_DESTROY_ITEM=0

Tree-wide ->SetCount after Phase 15E-42: 27
Tree-wide M2_DESTROY_ITEM real call sites after Phase 15E-42: 16
```

Final validation:
```powershell
cmake --build build --config RelWithDebInfo --target GameServer --parallel 8
```
- Build passed.

Manual WinTest checklist:
- NPC shop: buy stackable merge, buy stackable new slot, buy non-stackable item.
- NPC shop: sell stackable partial, sell stackable full consume, sell non-stackable item.
- Verify gold balance after each transaction.
- Offline shop: create shop, buy item from another player, close shop, return items, logout/login owner state.
- Extended shop: inventory-full buy failure and normal buy if available.
- Check `syserr.txt` for `SHOP_TRANSACTION`, `SHOP_EX_TRANSACTION`, `OFFLINESHOP_TEMP`, `OFFLINESHOP_SELL`, item count desync, duplicate/missing items, gold mismatch, and `VID_DRIFT`.
- Verify no delayed damage/metin collapse regression.

Commits:
- `Phase 15E-42 PART A: shop.cpp NPC shop transactions to ECS`
- `Phase 15E-42 PART B: new_offlineshop_manager.cpp destroys to ECS`
- `Phase 15E-42 PART C: shopEx.cpp destroy to ECS`

Commit status:
- Code commits completed.
- This log update committed separately as Phase 15E-42 completion documentation.

## Phase 15E-43: Cube / Refine / Attr_transfer / Mining material consume cleanup

Date: 2026-04-28

Mode:
- Code change with build gates after every part.
- Scope limited to material consume paths in `cuberenewal.cpp`, `attr_transfer.cpp`, `mining.cpp`, and `cmd_general.cpp`.
- DragonSoul and pet paths intentionally untouched.

Part A - `cuberenewal.cpp` cube refining:
- Replaced improve material partial decrement with `ItemSystem::ConsumeItemEcs`.
- Replaced cube stack merge target increments with `ItemSystem::AddItemCountEcs`.
- Replaced cube partial remaining source count with `ItemSystem::SetItemCountEcs`.
- Replaced full source cleanup with `ItemSystem::DestroyItemEntityEcs(..., "CUBE_REFINING_CONSUME")`.
- Target file result: `cuberenewal.cpp SetCount=0 M2_DESTROY_ITEM=0`.
- Build passed after Part A.
- Note: first build attempt timed out due to stale MSBuild processes; stale processes were stopped and the clean rerun passed.

Part B - `attr_transfer.cpp` material consume:
- Replaced source attribute item decrement with `ItemSystem::ConsumeItemEcs`.
- Replaced material item decrement with `ItemSystem::ConsumeItemEcs`.
- Target file result: `attr_transfer.cpp SetCount=0 M2_DESTROY_ITEM=0`.
- Build passed after Part B.

Part C - `mining.cpp` ore refining:
- Replaced `item->SetCount(item->GetCount() - ORE_COUNT_FOR_REFINE)` with `ItemSystem::ConsumeItemEcs`.
- Target file result: `mining.cpp SetCount=0 M2_DESTROY_ITEM=0`.
- Build passed after Part C.

Part D - `cmd_general.cpp` bottle consume:
- Replaced `pkBottle->SetCount(pkBottle->GetCount() - 1)` with `ItemSystem::ConsumeItemEcs`.
- Target file result: `cmd_general.cpp SetCount=0 M2_DESTROY_ITEM=0`.
- Build passed after Part D.

Final count verification:
```text
cuberenewal.cpp  SetCount=0 M2_DESTROY_ITEM=0
attr_transfer.cpp SetCount=0 M2_DESTROY_ITEM=0
mining.cpp       SetCount=0 M2_DESTROY_ITEM=0
cmd_general.cpp  SetCount=0 M2_DESTROY_ITEM=0

Tree-wide ->SetCount after Phase 15E-43: 18
Tree-wide M2_DESTROY_ITEM real call sites after Phase 15E-43: 14
```

Final validation:
```powershell
cmake --build build --config RelWithDebInfo --target GameServer --parallel 8
```
- Build passed.

Manual WinTest checklist:
- Cube refining: material consume partial/full, result item creation, excess material return, stack merge behavior.
- Attribute transfer: source and material item consume independently, target receives attributes.
- Mining: ore stack decreases by `ORE_COUNT_FOR_REFINE`, refined ore created.
- Bottle/water command: bottle count decreases and disappears at zero.
- Check `syserr.txt` for `CUBE_REFINING_CONSUME`, item count mismatch, stack merge errors, orphan/double-destroy, and `VID_DRIFT`.
- Verify no delayed damage/metin collapse regression.

Commits:
- `Phase 15E-43 PART A: cuberenewal.cpp cube refining to ECS`
- `Phase 15E-43 PART B: attr_transfer.cpp material consume to ECS`
- `Phase 15E-43 PART C: mining.cpp ore refining to ECS`
- `Phase 15E-43 PART D: cmd_general.cpp bottle consume to ECS`

Commit status:
- Code commits completed.
- This log update committed separately as Phase 15E-43 completion documentation.

## Phase 15E-44: Pet duration consume + DragonSoul charge/refine

Date: 2026-04-28

Mode:
- Code change with build gates.
- Pet first, then DragonSoul segmented cleanup.
- DragonSoul persistence/refine semantics preserved; legacy DS algorithms were not rewritten.

Part A - `New_PetSystem.cpp` pet book consume:
- Replaced both `bookItem->SetCount(bookItem->GetCount() - 1)` calls with `ItemSystem::ConsumeItemEcs`.
- Existing summon success/failure control flow was preserved; consume still happens only where legacy code consumed the book.
- Target file result: `New_PetSystem.cpp SetCount=0 M2_DESTROY_ITEM=0`.
- Build passed after Part A.

Part B segment 1 - DragonSoul extract / pull-out paths:
- Replaced DS extract source decrement with `ItemSystem::ConsumeItemEcs`.
- Replaced extractor decrement with `ItemSystem::ConsumeItemEcs`.
- Replaced pull-out failure byproduct/source cleanup with `ItemSystem::DestroyItemEntityEcs(..., "DRAGON_SOUL_BYPRODUCT")`.
- Build passed after segment 1.

Part B segment 2 - DragonSoul grade/step refine material consume:
- Replaced full material consume branches with `RemoveFromCharacter()` followed by `ItemSystem::DestroyItemEntityEcs(..., "DRAGON_SOUL_REFINE_CONSUME")`.
- Replaced partial/final consume branches with `ItemSystem::ConsumeItemEcs`.
- This preserves auto-destroy when `left_count` consumes the full stack.
- Build passed after segment 2.

Part B segment 3 - DragonSoul strength refine consume:
- Replaced `pDragonSoul` success/failure decrement with `ItemSystem::ConsumeItemEcs`.
- Replaced `pRefineStone` success/failure decrement with `ItemSystem::ConsumeItemEcs`.
- Build passed after segment 3.

Part B segment 4 - DragonSoul invalid destroy paths:
- Replaced invalid DS pair destroys with `RemoveFromCharacter()` followed by `ItemSystem::DestroyItemEntityEcs(..., "DRAGON_SOUL_INVALID")`.
- Preserved legacy remove-before-destroy ordering.
- Build passed after segment 4.

Final count verification:
```text
New_PetSystem.cpp SetCount=0 M2_DESTROY_ITEM=0
DragonSoul.cpp    SetCount=0 M2_DESTROY_ITEM=0

Tree-wide ->SetCount after Phase 15E-44: 5
Tree-wide M2_DESTROY_ITEM real call sites after Phase 15E-44: 7
```

Final validation:
```powershell
cmake --build build --config RelWithDebInfo --target GameServer --parallel 8
```
- Build passed.

Manual WinTest checklist:
- Pet: summon with pet book, count decreases or item disappears at one count, failure path does not consume unexpectedly.
- DragonSoul extract: DS/extractor counts decrease correctly, extractor one-count destroy works, result heart item appears.
- DragonSoul pull-out: success returns item, failure destroys source and gives byproduct if applicable.
- DragonSoul grade/step refine: source materials consumed, result DS appears, invalid paths do not crash.
- DragonSoul strength refine: DS and refine stone consumed on success/failure, result behavior unchanged.
- Logout/login after DS operations to verify persistence.
- Check `syserr.txt` for `DRAGON_SOUL_INVALID`, `DRAGON_SOUL_BYPRODUCT`, `DRAGON_SOUL_REFINE_CONSUME`, DS count desync, pet book loss/duplication, orphan/double-destroy, and `VID_DRIFT`.
- Verify no delayed damage/metin collapse regression.

Commits:
- `Phase 15E-44 PART A: New_PetSystem.cpp pet book consume to ECS`
- `Phase 15E-44 PART B segment 1: DragonSoul extract paths to ECS`
- `Phase 15E-44 PART B segment 2: DragonSoul refine material consume to ECS`
- `Phase 15E-44 PART B segment 3: DragonSoul strength refine consume to ECS`
- `Phase 15E-44 PART B segment 4: DragonSoul invalid destroy paths to ECS`

Commit status:
- Code commits completed.
- This log update committed separately as Phase 15E-44 completion documentation.

## Phase 15E-36a: Count authority split

Date: 2026-04-28

Mode:
- Code change.
- Narrow core safety fix.
- No commit.

Files changed:
- `SRC/Server/GameServer/ecs/systems/ItemSystem.cpp`

Goal:
- Prevent public ECS count writes from reaching legacy `CItem::SetCount(0)`.
- Make zero-count item removal use the centralized ECS destruction path.
- Keep nonzero count updates mirrored to legacy for packet/save compatibility.

Changes made:
- Added internal component-only helper:
```cpp
static void SetItemCountComponentOnly(entt::entity item, uint32_t count)
```
- Added internal nonzero-only legacy mirror helper:
```cpp
static void MirrorItemCountToLegacyNonDestroy(entt::entity item, uint32_t count)
```
- Updated `ItemSystem::SetItemCount(...)`:
```text
invalid entity -> no-op
count == 0    -> DestroyItemEntityAndLegacy(item, "SET_ITEM_COUNT_ZERO")
count > 0     -> update ecs::ItemCount, then mirror to legacy SetCount(count)
```
- Updated `ItemSystem::SetItemCountEcs(...)`:
```text
invalid entity -> false
count == 0    -> DestroyItemEntityAndLegacy(item, "SET_ITEM_COUNT_ECS_ZERO")
count > 0     -> SetItemCount(item, count)
```

Why this matters:
- Before this phase, `SetItemCountEcs(item, 0)` could call `legacyItem->SetCount(0)`.
- Legacy `CItem::SetCount(0)` can run `RemoveFromCharacter()` / `M2_DESTROY_ITEM(...)` directly.
- That bypasses the centralized ECS destruction helper and risks stale ECS entities or double deletion.
- After this phase, zero-count destruction has one ECS-owned entry point from ItemSystem count APIs.

Current count path after change:
```text
SetItemCountEcs(item, 0)
  -> DestroyItemEntityAndLegacy(item, "SET_ITEM_COUNT_ECS_ZERO")

SetItemCountEcs(item, nonzero)
  -> SetItemCount(item, nonzero)
     -> SetItemCountComponentOnly(item, nonzero)
     -> MirrorItemCountToLegacyNonDestroy(item, nonzero)
        -> legacyItem->SetCount(nonzero)
```

Validation scans:
```text
ItemSystem_LegacyBridge.cpp direct ->SetCount:       9
ItemSystem_LegacyBridge.cpp direct M2_DESTROY_ITEM:  3
ItemSystem.cpp legacy SetCount mirror sites:         1
ItemSystem.cpp zero-count destroy guards:            2
```

Zero-count public call-site scan:
```powershell
Get-ChildItem -Path SRC/Server/GameServer -Recurse -Include *.cpp,*.h,*.hpp |
  Select-String -Pattern 'SetItemCountEcs\([^,]+,\s*0\)|SetItemCount\([^,]+,\s*0\)'
```
- Result: no direct zero-count call sites found.

Header validation:
```powershell
Select-String -Path 'SRC/Server/GameServer/ecs/systems/ItemSystem.hpp' -Pattern 'LPITEM|LPCHARACTER|CHARACTER\s*\*'
```
- Result: no matches.

Build validation:
```powershell
cmake --build build --config RelWithDebInfo --target GameServer --parallel 8
```
- Build passed.
- `GameServer.exe` linked successfully.
- Existing post-build `pwsh.exe` warning remained, build exit code was `0`.

Remaining blockers:
- `CItem::SetCount` still contains legacy zero-count destruction internals.
- Pet duration path still consumes via legacy direct `SetCount`.
- DragonSoul charge/enchant paths still consume via legacy direct `SetCount`.
- Refine scroll/material paths still consume via legacy direct `SetCount`.

Recommended next phase:
```text
Phase 15E-36b: Pet duration consume wrapper
Phase 15E-36c: DragonSoul consume wrapper
Phase 15E-36d: Refine scroll/material consume wrapper
```

Manual WinTest checklist:
- Consume normal item stack to zero.
- Use item with `SetItemCountEcs` path if reachable, especially quest item count set.
- Arrow consume / combat count path.
- Guild renewal item count paths.
- Switchbot price consume.
- Inventory open/relog.
- Verify no duplicate item, missing item, stale entity, wrong owner/window/cell.
- Verify no delayed damage/metin collapse regression.

Commit status:
- Not committed.

## Phase 15E-37: Cleanup destroy paths to ECS

Date: 2026-04-28

Mode:
- Audit + verification.
- No code change required in this pass.
- Documentation commit only.

Context:
- Phase 15E-21 through 15E-36d were consolidated in commit `4a9477b`.
- The cleanup destroy migrations requested for 15E-37 were already included in that consolidated state.

Target:
- `SRC/Server/GameServer/ecs/systems/ItemSystem_LegacyBridge.cpp`

Audit result:
```text
M2_DESTROY_ITEM(item): 0
M2_DESTROY_ITEM(all direct bridge matches): 3
```

Remaining direct `M2_DESTROY_ITEM` matches:
```text
827: M2_DESTROY_ITEM(this)
832: M2_DESTROY_ITEM(RemoveFromCharacter())
848: M2_DESTROY_ITEM(RemoveFromCharacter())
```
- All three remaining matches are inside core `CItem::SetCount` internals.
- They are not ground gold pickup, inventory cleanup, logout/disconnect cleanup, or item destroy event paths.
- They are out of scope for Phase 15E-37 and need a later core `CItem::SetCount` isolation/removal phase.

Cleanup paths verified already on ECS destroy:
```text
Ground gold pickup:
  ItemSystem::DestroyItemEntityEcs(CreateItemEntity(...), "PICKUP_GOLD")

CHARACTER::ClearItem inventory loop:
  ItemSystem::DestroyItemEntityEcs(CreateItemEntity(...), "CLEAR_ITEM_INVENTORY")

CHARACTER::ClearItem DragonSoul inventory loop:
  ItemSystem::DestroyItemEntityEcs(CreateItemEntity(...), "CLEAR_ITEM_DRAGON_SOUL")

CHARACTER::ClearItem extra inventory loop:
  ItemSystem::DestroyItemEntityEcs(CreateItemEntity(...), "CLEAR_ITEM_EXTRA_INVENTORY")

CHARACTER::ClearItem switchbot loop:
  ItemSystem::DestroyItemEntityEcs(CreateItemEntity(...), "CLEAR_ITEM_SWITCHBOT")

item_destroy_event:
  ItemSystem::DestroyItemEntityEcs(CreateItemEntity(...), "ITEM_DESTROY_EVENT")
```

Destroy API verification:
```text
DestroyItemEntityEcs(item, reason)
  -> DestroyItemEntityAndLegacy(item, reason)
     -> EntityFactory::DestroyItemEntity(g_registry, legacyItem)
     -> ITEM_MANAGER::RemoveItem(legacyItem, reason)
```
- Handles ECS unregister/destroy before legacy item removal.
- Does not expose LPITEM publicly.
- Does not require caller-side packet formatting.
- Works for cleanup contexts where item has already been removed from character before destroy.

Header validation:
```powershell
Select-String -Path 'SRC/Server/GameServer/ecs/systems/ItemSystem.hpp' -Pattern 'LPITEM|LPCHARACTER|CHARACTER\s*\*'
```
- Result: no matches.

Build validation:
```powershell
cmake --build build --config RelWithDebInfo --target GameServer --parallel 8
```
- Build passed.
- `GameServer.exe` linked successfully.
- CMake reconfigured because unrelated root `CMakeLists.txt` is newer in the dirty worktree.
- The Phase 15E-37 GameServer target still built successfully.

Manual WinTest checklist:
- Login -> existing items present.
- Pick up ground gold -> gold added, ground item gone.
- Logout -> no cleanup syserr.
- Re-login -> inventory items persist.
- Force-quit client -> cleanup path clean.
- Verify no duplicate item, missing item, stale ECS item, wrong owner/window/cell.
- Verify no delayed damage/metin collapse regression.

Commit status:
- Documentation-only commit for Phase 15E-37.

## Phase 15E-36b/c/d: Pet, DragonSoul and refine count consume migration

Date: 2026-04-28

Mode:
- Code change.
- Multi-subsystem count consume pass.
- No commit.

Files changed:
- `SRC/Server/GameServer/ecs/systems/ItemSystem_LegacyBridge.cpp`

Prerequisite:
- Phase 15E-36a split zero-count authority so `SetItemCountEcs(..., 0)` routes to centralized ECS destruction instead of legacy `CItem::SetCount(0)`.

Goal:
- Remove remaining direct caller-side `SetCount(GetCount() - 1)` paths outside core `CItem::SetCount`.
- Preserve existing side-effect order by replacing each decrement at the exact old decrement point.

Changes made:
```text
1. Pet duration item consume
   Old: item->SetCount(item->GetCount() - 1)
   New: ItemSystem::ConsumeItemEcs(EntityFactory::CreateItemEntity(g_registry, item))
   Context: after pet duration DB update and success chat.

2. DragonSoul time charge percent consume
   Old: item->SetCount(item->GetCount() - 1)
   New: ItemSystem::ConsumeItemEcs(EntityFactory::CreateItemEntity(g_registry, item))
   Context: after successful GiveMoreTime_Per mutation.

3. DragonSoul time charge fix consume
   Old: item->SetCount(item->GetCount() - 1)
   New: ItemSystem::ConsumeItemEcs(EntityFactory::CreateItemEntity(g_registry, item))
   Context: after successful GiveMoreTime_Fix mutation.

4. DragonSoul enchant consume
   Old: item->SetCount(item->GetCount() - 1)
   New: ItemSystem::ConsumeItemEcs(EntityFactory::CreateItemEntity(g_registry, item))
   Context: after successful attribute put and log/chat.

5. Refine scroll consume in DoRefineWithScroll path A
   Old: pkItemScroll->SetCount(pkItemScroll->GetCount() - 1)
   New: ItemSystem::ConsumeItemEcs(EntityFactory::CreateItemEntity(g_registry, pkItemScroll))

6. Refine scroll consume in DoRefineWithScroll path B
   Old: pkItemScroll->SetCount(pkItemScroll->GetCount() - 1)
   New: ItemSystem::ConsumeItemEcs(EntityFactory::CreateItemEntity(g_registry, pkItemScroll))

7. Refine soul scroll consume in DoRefineItemSoul
   Old: pkItemScroll->SetCount(pkItemScroll->GetCount() - 1)
   New: ItemSystem::ConsumeItemEcs(EntityFactory::CreateItemEntity(g_registry, pkItemScroll))
```

Counts:
```text
ItemSystem_LegacyBridge.cpp direct ->SetCount before this pass:      9
ItemSystem_LegacyBridge.cpp direct ->SetCount after this pass:       1
ItemSystem_LegacyBridge.cpp direct M2_DESTROY_ITEM before/after:     3 -> 3
Refine scroll consume calls now using ECS:                           3
Pet duration consume calls now using ECS:                            1
```

Remaining direct legacy count/destruction:
```text
826: pItem->SetCount(pItem->GetCount() + count)
827: M2_DESTROY_ITEM(this)
832: M2_DESTROY_ITEM(RemoveFromCharacter())
848: M2_DESTROY_ITEM(RemoveFromCharacter())
```
- All remaining direct sites are inside `CItem::SetCount` core internals.
- No gameplay caller-side `item->SetCount(item->GetCount() - 1)` remains in `ItemSystem_LegacyBridge.cpp`.

Validation:
```powershell
cmake --build build --config RelWithDebInfo --target GameServer --parallel 8
```
- Build passed.
- `GameServer.exe` linked successfully.
- Warnings only; no compile/link errors.
- Existing post-build `pwsh.exe` warning remained, build exit code was `0`.

Header validation:
```powershell
Select-String -Path 'SRC/Server/GameServer/ecs/systems/ItemSystem.hpp' -Pattern 'LPITEM|LPCHARACTER|CHARACTER\s*\*'
```
- Result: no matches.

Runtime risk notes:
- DragonSoul and refine semantics were not rewritten; only the final count decrement was routed through ECS consume.
- Existing log/chat/order was preserved at the old decrement point.
- If the consumed item count reaches zero, ECS destruction now owns entity unregister/destroy before legacy remove.

Manual WinTest checklist:
- Pet duration item use.
- DragonSoul charge percent item.
- DragonSoul charge fix item.
- DragonSoul enchant item.
- Refine with scroll success and fail.
- Refine item soul success and fail.
- Inventory open/relog after each.
- Verify no duplicate item, missing item, stale entity, wrong owner/window/cell.
- Verify no delayed damage/metin collapse regression.

Commit status:
- Not committed.

## Phase 15E-36: CItem::SetCount isolation audit

Date: 2026-04-28

Mode:
- Audit + design only.
- No code changes.
- No commit.

Scope:
- `SRC/Server/GameServer/ecs/systems/ItemSystem.cpp`
- `SRC/Server/GameServer/ecs/systems/ItemSystem_LegacyBridge.cpp`
- `SRC/Server/GameServer/ecs/EntityFactory.cpp`
- `SRC/Server/GameServer/ecs/ItemRegistry.*`

Goal:
- Identify the current count authority call graph.
- Identify recursion / double destruction / stale ECS entity risks.
- Define the next safe implementation phase before migrating DragonSoul, refine, pet duration, and core count paths.

Current count authority call graph:
```text
ItemSystem::SetItemCountEcs(item, count)
  -> ItemSystem::SetItemCount(item, count)
     -> g_registry.emplace_or_replace<ecs::ItemCount>(item, count)
     -> ResolveLegacyItemForLegacySideEffect(item)
     -> legacyItem->SetCount(count)
        -> CItem::SetCount(count)
           -> clamp count
           -> m_dwCount = count
           -> SyncItemCountComponent(this, m_dwCount)
              -> ItemEntityOf(this)
              -> g_registry.emplace_or_replace<ecs::ItemCount>(entity, m_dwCount)
           -> if count == 0 && m_pOwner:
              -> legacy RemoveFromCharacter / M2_DESTROY_ITEM paths
           -> else:
              -> UpdatePacket()
              -> Save()
```

Consume / destruction path:
```text
ItemSystem::ConsumeItemEcs(item, amount)
  -> ItemSystem::ConsumeItem(item, amount)
     -> GetItemCount(item)
     -> if current count > amount:
        -> SetItemCount(item, current - amount)
        -> legacyItem->SetCount(nonzero)
     -> else:
        -> DestroyItemEntityAndLegacy(item, "CONSUME_ITEM")
           -> EntityFactory::DestroyItemEntity(g_registry, legacyItem)
              -> CItemRegistry::Unregister(itemID)
              -> g_registry.destroy(entity)
           -> ITEM_MANAGER::instance().RemoveItem(legacyItem, reason)
```

Destroy helper behavior:
```text
ItemSystem::DestroyItemEntityEcs(item, reason)
  -> DestroyItemEntityAndLegacy(item, reason)
     -> ResolveLegacyItemForDestruction(item)
     -> if legacy item exists:
        -> destroy/unregister ECS entity first
        -> ITEM_MANAGER::RemoveItem(legacyItem, reason)
     -> else:
        -> unregister by ItemIdentity.id
        -> destroy ECS entity
```

Important finding:
- `SetItemCountEcs(item, 0)` is not equivalent to `ConsumeItemEcs(item, amount)` or `DestroyItemEntityEcs(item, reason)`.
- `SetItemCountEcs(0)` currently enters legacy `CItem::SetCount(0)`, which can destroy the item through legacy `M2_DESTROY_ITEM` without necessarily going through the centralized ECS destruction helper.
- This is the main reason remaining core/refine/DragonSoul/pet paths should not be blindly converted to `SetItemCountEcs(0)`.

Remaining direct count/destruction sites in `ItemSystem_LegacyBridge.cpp`:
```text
Direct ->SetCount: 9
Direct M2_DESTROY_ITEM(...): 3
```

Remaining direct `SetCount` classification:
```text
826: CItem::SetCount internal pItem->SetCount(...)
     Risk: core count/delete internals; recursion risk if replaced with ECS API.

5452: CHARACTER::UseItemEx pet duration item consume
     Risk: pet DB state update + item consume must be atomic; needs dedicated wrapper.

6615: CHARACTER::UseItemEx DragonSoul time-charge-percent consume
6661: CHARACTER::UseItemEx DragonSoul time-charge-fix consume
9416: CHARACTER::UseItemEx DragonSoul enchant consume
     Risk: DragonSoul mutation / socket / log semantics; needs DS wrapper.

7904: commented legacy line
     Risk: none; no executable code.

11569: CHARACTER::DoRefineWithScroll scroll consume
11966: CHARACTER::DoRefineWithScroll scroll consume
12218: CHARACTER::DoRefineItemSoul scroll consume
     Risk: refine success/fail/material ordering; needs refine wrapper.
```

Remaining direct `M2_DESTROY_ITEM` classification:
```text
827: CItem::SetCount internal M2_DESTROY_ITEM(this)
832: CItem::SetCount internal M2_DESTROY_ITEM(RemoveFromCharacter())
848: CItem::SetCount fallback M2_DESTROY_ITEM(RemoveFromCharacter())
```

Risk assessment:
- High: reentrancy/recursion if `CItem::SetCount` internals call public ECS count APIs, because public ECS count APIs currently mirror back to legacy `SetCount`.
- High: stale ECS item entity if legacy `CItem::SetCount(0)` destroys an item without `DestroyItemEntityAndLegacy`.
- Medium: double deletion if a migrated caller calls `ConsumeItemEcs` and legacy side effects also call `ITEM_MANAGER::RemoveItem` afterward.
- Medium: packet/save ordering changes if count writes bypass legacy `UpdatePacket()` / `Save()` before packet authority is fully ECS-owned.
- Medium: `DestroyItemEntityAndLegacy` destroys the ECS entity before `ITEM_MANAGER::RemoveItem`; this is currently intentional, but any legacy callback that expects a live ECS item during remove would fail.

Recommended next implementation phase:
```text
Phase 15E-36a: Count authority split

1. Add private/internal helper in ItemSystem.cpp:
   - SetItemCountComponentOnly(entt::entity item, uint32_t count)
   - MirrorItemCountToLegacyNonDestroy(entt::entity item, uint32_t count)

2. Enforce rule:
   - SetItemCountEcs(item, 0) must route to DestroyItemEntityEcs or return false.
   - Nonzero count writes may mirror to legacy SetCount.
   - Zero-count destruction must have exactly one path: DestroyItemEntityAndLegacy.

3. Keep CItem::SetCount as legacy mirror/core boundary for now.
   - Do not replace internals yet.
   - Add audit guard/log only if needed.

4. After 36a build + WinTest:
   - 36b: Pet duration consume wrapper.
   - 36c: DragonSoul consume wrapper.
   - 36d: Refine scroll/material consume wrapper.
```

Concrete next wrappers:
```text
ConsumePetDurationItemEcs(owner, durationItem, petItem)
  - DB update must succeed first.
  - Then ConsumeItemEcs(durationItem, 1).

ConsumeDragonSoulChargeItemEcs(owner, chargeItem, targetDsItem, reason)
  - Legacy DS mutation/log remains inside wrapper.
  - ConsumeItemEcs only after DS mutation succeeds.
  - Sync target DS item state afterward.

ConsumeRefineMaterialEcs(owner, scrollItem, refinedItem, reason)
  - Preserve existing success/fail ordering.
  - Consume scroll/material after probability decision where legacy currently does it.
```

Validation performed:
```powershell
cmake --build build --config RelWithDebInfo --target GameServer --parallel 8
```
- Build passed.
- `GameServer.exe` linked successfully.

Header validation:
```powershell
Select-String -Path 'SRC/Server/GameServer/ecs/systems/ItemSystem.hpp' -Pattern 'LPITEM|LPCHARACTER|CHARACTER\s*\*'
```
- Result: no matches.

Manual WinTest focus before 36a:
- Consume normal item stack to zero.
- Use recall item.
- Gold pickup.
- Character logout/relog with items.
- Verify no stale item entity / duplicate item / missing item.
- Verify no delayed damage/metin collapse regression.

Commit status:
- Not committed.

## Phase 15E-35c: Mandatory consume/destroy block rewrite

Date: 2026-04-28

Mode:
- Strict transformation task.
- Target file only.
- Code change, no commit.

Target file:
- `SRC/Server/GameServer/ecs/systems/ItemSystem_LegacyBridge.cpp`

Goal:
- Rewrite at least 10 distinct consumption/destroy blocks if safe.
- Skip only DragonSoul, refine, pet duration/state, timed unique/recall-sensitive logic.
- Do not leave partial `SetCount` / `M2_DESTROY_ITEM` logic inside migrated blocks.

Result:
- Modified blocks in this pass: 5.
- The hard target of 10 modified blocks could not be met safely because only 5 non-skip direct destroy blocks remained after the prior 15E-35b pass.
- All remaining direct `SetCount` candidates are either explicit skip categories or core `CItem::SetCount` internals.

Blocks migrated in this pass:
```text
1. CHARACTER::ClearItem inventory cleanup
   Old: item->RemoveFromCharacter(); M2_DESTROY_ITEM(item);
   New: item->RemoveFromCharacter(); DestroyItemEntityEcs(CreateItemEntity(...), "CLEAR_ITEM_INVENTORY");

2. CHARACTER::ClearItem DragonSoul inventory cleanup
   Old: item->RemoveFromCharacter(); M2_DESTROY_ITEM(item);
   New: item->RemoveFromCharacter(); DestroyItemEntityEcs(CreateItemEntity(...), "CLEAR_ITEM_DRAGON_SOUL");

3. CHARACTER::ClearItem extra inventory cleanup
   Old: item->RemoveFromCharacter(); M2_DESTROY_ITEM(item);
   New: item->RemoveFromCharacter(); DestroyItemEntityEcs(CreateItemEntity(...), "CLEAR_ITEM_EXTRA_INVENTORY");

4. CHARACTER::ClearItem switchbot cleanup
   Old: item->RemoveFromCharacter(); M2_DESTROY_ITEM(item);
   New: item->RemoveFromCharacter(); DestroyItemEntityEcs(CreateItemEntity(...), "CLEAR_ITEM_SWITCHBOT");

5. item_destroy_event
   Old: M2_DESTROY_ITEM(pkItem);
   New: DestroyItemEntityEcs(CreateItemEntity(...), "ITEM_DESTROY_EVENT");
```

Blocks previously migrated by the immediately preceding 15E-35b pass and still part of the concrete consume/destroy rewrite sequence:
```text
1. CHARACTER::PickupItem ground gold destroy -> DestroyItemEntityEcs(..., "PICKUP_GOLD")
2. CHARACTER::GiveRecallItem split-stack decrement -> ConsumeItemEcs(...)
3. CHARACTER::ProcessRecallItem use decrement -> ConsumeItemEcs(...)
```

Counts for this pass:
```text
ItemSystem_LegacyBridge.cpp direct ->SetCount before:             9
ItemSystem_LegacyBridge.cpp direct ->SetCount after:              9
ItemSystem_LegacyBridge.cpp direct M2_DESTROY_ITEM(item) before:  4
ItemSystem_LegacyBridge.cpp direct M2_DESTROY_ITEM(item) after:   0
ItemSystem_LegacyBridge.cpp all M2_DESTROY_ITEM calls before:     8
ItemSystem_LegacyBridge.cpp all M2_DESTROY_ITEM calls after:      3
```

Remaining direct `SetCount` / `M2_DESTROY_ITEM` candidates and rejection reasons:
```text
1. CItem::SetCount, pItem->SetCount(pItem->GetCount() + count)
   Rejected: core CItem zero-count internals; replacing this would recurse through ECS SetItemCountEcs/legacy mirror.

2. CItem::SetCount, M2_DESTROY_ITEM(this)
   Rejected: core CItem destruction internals; not a caller consume block.

3. CItem::SetCount, M2_DESTROY_ITEM(RemoveFromCharacter())
   Rejected: core CItem destruction internals; not a caller consume block.

4. CItem::SetCount fallback M2_DESTROY_ITEM(RemoveFromCharacter())
   Rejected: core CItem destruction internals; not a caller consume block.

5. CHARACTER::UseItemEx pet duration path
   Rejected: pet duration/state path, explicit skip category.

6. CHARACTER::UseItemEx DragonSoul time charge percent
   Rejected: DragonSoul path, explicit skip category.

7. CHARACTER::UseItemEx DragonSoul time charge fix
   Rejected: DragonSoul path, explicit skip category.

8. CHARACTER::UseItemEx commented mythical peach SetCount
   Rejected: commented legacy line, no executable block.

9. CHARACTER::UseItemEx DragonSoul enchant
   Rejected: DragonSoul path, explicit skip category.

10. CHARACTER::DoRefineWithScroll scroll consume
    Rejected: refine path, explicit skip category.

11. CHARACTER::DoRefineWithScroll scroll consume
    Rejected: refine path, explicit skip category.

12. CHARACTER::DoRefineItemSoul scroll consume
    Rejected: refine path, explicit skip category.
```

Validation:
```powershell
cmake --build build --config RelWithDebInfo --target GameServer --parallel 8
```
- Build passed.
- `GameServer.exe` linked successfully.
- Warnings only; no compile/link errors.
- Existing post-build `pwsh.exe` warning remains, build exit code was `0`.

Header validation:
```powershell
Select-String -Path 'SRC/Server/GameServer/ecs/systems/ItemSystem.hpp' -Pattern 'LPITEM|LPCHARACTER|CHARACTER\s*\*'
```
- Result: no matches.

Manual WinTest checklist:
- Character logout / cleanup path with inventory items.
- DragonSoul inventory cleanup path.
- Extra inventory cleanup path.
- Switchbot item cleanup path.
- Ground item destroy event after ownership timer.
- Gold pickup.
- Recall item split/use.
- Inventory open/relog.
- Verify no duplicate item, missing item, stale entity, wrong owner/window/cell.
- Verify no delayed damage/metin collapse regression.

Commit status:
- Not committed. User requested review before commit.

## Phase 15E-35b: Top concrete consume-block rewrite

Date: 2026-04-28

Mode:
- Concrete implementation task.
- Target file only.
- Code change, no commit.

Target file:
- `SRC/Server/GameServer/ecs/systems/ItemSystem_LegacyBridge.cpp`

Goal:
- Find concrete blocks containing `GetCount()` with `SetCount(...)` or `M2_DESTROY_ITEM(...)`.
- Migrate the simplest real consume/destroy blocks to ECS consume/destruction APIs.
- If fewer than 10 safe blocks exist, document every candidate and rejection reason.

Candidate scan result:
- Total candidates found: 14.
- Safe candidates found: 3.
- Safe candidates migrated: 3.
- Fewer than 10 safe blocks exist because the remaining candidates are core `CItem::SetCount`, pet duration/state, DragonSoul, refine, or commented legacy code.

Candidates considered:
```text
1. CItem::SetCount, pItem, amount=count add, entity=no, risk=LEGACY CORE ONLY
   Rejected: core CItem zero-count/delete internals, not a caller consume block.

2. CItem::SetCount, this, amount=destroy, entity=no, risk=LEGACY CORE ONLY
   Rejected: core CItem deletion internals.

3. CItem::SetCount, RemoveFromCharacter(), amount=destroy, entity=no, risk=LEGACY CORE ONLY
   Rejected: core CItem deletion internals.

4. CHARACTER::PickupItem, item, amount=all/gold pickup, entity=no
   Migrated: ground gold destroy after GiveGold now uses DestroyItemEntityEcs(CreateItemEntity(...), "PICKUP_GOLD").

5. CHARACTER::UseItemEx, item, amount=1, entity=no, risk=PET DURATION/STATE
   Rejected: new pet duration DB state path, explicitly skipped.

6. CHARACTER::UseItemEx, item, amount=1, entity=no, risk=DRAGONSOUL
   Rejected: DragonSoul charge path, explicitly skipped.

7. CHARACTER::UseItemEx, item, amount=1, entity=no, risk=DRAGONSOUL
   Rejected: DragonSoul charge path, explicitly skipped.

8. CHARACTER::UseItemEx, item, amount=1, entity=no, risk=COMMENTED LEGACY
   Rejected: commented-out legacy SetCount line only.

9. CHARACTER::UseItemEx, item, amount=1, entity=no, risk=DRAGONSOUL
   Rejected: DragonSoul enchant path, explicitly skipped.

10. CHARACTER::DoRefineWithScroll, pkItemScroll, amount=1, entity=no, risk=REFINE
    Rejected: refine scroll path, explicitly skipped.

11. CHARACTER::DoRefineWithScroll, pkItemScroll, amount=1, entity=no, risk=REFINE
    Rejected: refine scroll path, explicitly skipped.

12. CHARACTER::DoRefineItemSoul, pkItemScroll, amount=1, entity=no, risk=REFINE
    Rejected: refine soul path, explicitly skipped.

13. CHARACTER::GiveRecallItem, item, amount=1, entity=no
    Migrated: recall item split stack decrement now uses ConsumeItemEcs(CreateItemEntity(...)).

14. CHARACTER::ProcessRecallItem, item, amount=1, entity=no
    Migrated: recall item use decrement after WarpSet now uses ConsumeItemEcs(CreateItemEntity(...)).
```

Blocks migrated:
```cpp
ItemSystem::DestroyItemEntityEcs(
    EntityFactory::CreateItemEntity(g_registry, item),
    "PICKUP_GOLD");
```
```cpp
ItemSystem::ConsumeItemEcs(EntityFactory::CreateItemEntity(g_registry, item));
```

Counts:
```text
ItemSystem_LegacyBridge.cpp direct ->SetCount before:             11
ItemSystem_LegacyBridge.cpp direct ->SetCount after:               9
ItemSystem_LegacyBridge.cpp direct M2_DESTROY_ITEM(item) before:   5
ItemSystem_LegacyBridge.cpp direct M2_DESTROY_ITEM(item) after:    4
ItemSystem_LegacyBridge.cpp all M2_DESTROY_ITEM calls before:      9
ItemSystem_LegacyBridge.cpp all M2_DESTROY_ITEM calls after:       8
```

Remaining candidate scan after migration:
```text
Remaining candidates: 11
- CItem::SetCount core internals: 3
- Pet duration/state: 1
- DragonSoul: 3
- Refine: 3
- Commented legacy line: 1
```

Validation:
```powershell
cmake --build build --config RelWithDebInfo --target GameServer --parallel 8
```
- Build passed.
- `GameServer.exe` linked successfully.
- Warnings only; no compile/link errors.
- Existing post-build `pwsh.exe` warning remains, build exit code was `0`.

Header validation:
```powershell
Select-String -Path 'SRC/Server/GameServer/ecs/systems/ItemSystem.hpp' -Pattern 'LPITEM|LPCHARACTER|CHARACTER\s*\*'
```
- Result: no matches.

Manual WinTest checklist:
- Pick up gold from ground and verify gold amount/ranking side effect/save behavior.
- Use recall item with stack count > 1 and verify one item is split/consumed correctly.
- Use recall item to warp and verify stack decrements or item disappears at zero.
- Inventory open/relog after recall use.
- Verify no duplicate item, missing item, wrong owner/window/cell.
- Verify no delayed damage/metin collapse regression.

Commit status:
- Not committed. User requested review before commit.

## Phase 15E-35b: Aggressive consume block follow-up

Date: 2026-04-28

Mode:
- Aggressive logic-level migration.
- Target file only.
- Audit/validation pass, no code change.
- No commit.

Target file:
- `SRC/Server/GameServer/ecs/systems/ItemSystem_LegacyBridge.cpp`

Goal:
- Replace remaining full item consumption blocks with `ItemSystem::ConsumeItemEcs(...)`.
- Remove matching `SetCount` / `M2_DESTROY_ITEM(item)` branch logic only when the whole block is safe.

Result:
- No additional full consumption blocks were converted in this pass.
- The safe full stack-consume blocks had already been migrated in the previous 15E-35a+ pass.
- Remaining direct `SetCount` / `M2_DESTROY_ITEM(item)` sites are either not full count-consume blocks or are explicitly skipped risk areas.

Current counts:
```text
ItemSystem_LegacyBridge.cpp direct ->SetCount:             11
ItemSystem_LegacyBridge.cpp direct M2_DESTROY_ITEM(item):   5
ItemSystem_LegacyBridge.cpp all M2_DESTROY_ITEM calls:      9
```

Remaining direct `SetCount` classification:
- `826`: core `CItem::SetCount` boundary / delete-on-zero internals.
- `5450`: pet duration DB path, skipped by phase rule.
- `6613`, `6659`, `9414`: DragonSoul charge/enchant paths, skipped by phase rule.
- `11567`, `11964`, `12216`: refine scroll/material paths, skipped by phase rule.
- `15133`, `15199`: recall / coordinate item split-use paths; not a full consume+destroy block, left unchanged.
- `7902`: commented-out legacy line.

Remaining direct `M2_DESTROY_ITEM(item)` classification:
- Ground gold pickup destroy: lifecycle destroy, not a count-consume branch.
- Character inventory / DragonSoul / extra inventory / switchbot cleanup loops: lifecycle cleanup, not count-consume branches.

Validation:
```powershell
cmake --build build --config RelWithDebInfo --target GameServer --parallel 8
```
- Build passed.
- `GameServer.exe` linked successfully.

Header validation:
```powershell
Select-String -Path 'SRC/Server/GameServer/ecs/systems/ItemSystem.hpp' -Pattern 'LPITEM|LPCHARACTER|CHARACTER\s*\*'
```
- Result: no matches.

Manual WinTest checklist:
- Use consumable stack to verify count decreases correctly.
- Consume item to zero and verify no duplicate / missing item.
- Verify recall item behavior if reachable.
- Verify pet duration item behavior if reachable.
- DragonSoul charge/enchant smoke test remains important because those paths were intentionally skipped.
- Refine scroll smoke test remains important because those paths were intentionally skipped.
- Verify no delayed damage/metin collapse regression.

Commit status:
- Not committed. User requested review before commit.

## Phase 15E-35a focused SetCount engine elimination

Date: 2026-04-27

Mode:
- Aggressive but narrow.
- Code change, no commit.

Target:
- `SRC/Server/GameServer/ecs/systems/ItemSystem_LegacyBridge.cpp`
- Optional `PlayerRuntimeSystem.cpp` / `CombatSystem.cpp` were scanned only; no optional changes were made in this focused slice.

Goal:
- Reduce simple LPITEM `SetCount` mutation paths without touching DragonSoul, refine, pet duration, timed items, stack merge, polymorph/book, or transaction-heavy logic.

Changes made:
- Migrated the `USE_RECIPE` simple ingredient consume cluster from direct `SetCount` to ECS count consumption:
```cpp
ItemSystem::ConsumeItemEcs(EntityFactory::CreateItemEntity(g_registry, pSource1), dwSourceCount1);
ItemSystem::ConsumeItemEcs(EntityFactory::CreateItemEntity(g_registry, pSource2), dwSourceCount2);
ItemSystem::ConsumeItemEcs(EntityFactory::CreateItemEntity(g_registry, pBottle));
```

Why this cluster was safe:
- It only consumes recipe source items and one bottle after count validation.
- It already grants the result through `ItemSystem::AutoGiveItemEcs`.
- It is not DragonSoul, refine, pet duration, timed item, stack merge, polymorph/book, shop/exchange/safebox/offlineshop transaction logic.

Counts:
```text
ItemSystem_LegacyBridge.cpp direct ->SetCount before: 36
ItemSystem_LegacyBridge.cpp direct ->SetCount after:  33
Global GameServer direct ->SetCount before:           79
Global GameServer direct ->SetCount after:            76
```

Skipped remaining `ItemSystem_LegacyBridge.cpp` groups:
- Drop/split and move stack logic: stack/split transaction, creates or destroys items.
- Stack merge / AutoGive merge logic: stack merge core.
- Pet duration item path: DB-backed pet duration semantics.
- DragonSoul charge/enchant paths: DragonSoul state and logging.
- Refine scroll/material paths: refine internals.
- Timed/recall item paths: socket/location/warp semantics.
- Metin/socket mutation path: socket mutation plus item return logic.

Validation:
```powershell
cmake --build build --config RelWithDebInfo --target GameServer --parallel 8
```
- Build passed.
- `GameServer.exe` linked successfully.
- Existing post-build message remained:
```text
'pwsh.exe' is not recognized as an internal or external command
```
  but build exit code was `0`.

Header validation:
```powershell
Select-String -Path 'SRC/Server/GameServer/ecs/systems/ItemSystem.hpp' -Pattern 'LPITEM|LPCHARACTER|CHARACTER\s*\*'
```
- Result: no matches.

Manual WinTest checklist:
- Recipe item use path with one source item.
- Recipe item use path with two source items.
- Missing source item / missing bottle failure cases.
- Successful recipe result grant.
- Relog after recipe consume.
- Verify no duplicate item, missing item, wrong count, wrong owner/window/cell.
- Verify no delayed damage/metin collapse regression.

Commit status:
- Not committed. User requested review before commit.

## Phase 15E-35a brutal targeted SetCount pass

Date: 2026-04-27

Mode:
- Aggressive, narrow, target-file only.
- Code change, no commit.

Target file:
- `SRC/Server/GameServer/ecs/systems/ItemSystem_LegacyBridge.cpp`

Additional replacements in this pass:
- Drop/split decrement:
```cpp
item->SetCount(item->GetCount() - bCount);
```
changed to:
```cpp
ItemSystem::ConsumeItemEcs(EntityFactory::CreateItemEntity(g_registry, item), bCount);
```
- Inventory split decrement:
```cpp
item->SetCount(item->GetCount() - count);
```
changed to:
```cpp
ItemSystem::ConsumeItemEcs(EntityFactory::CreateItemEntity(g_registry, item), count);
```
- Metin/scroll consume decrement:
```cpp
pkItem->SetCount(pkItem->GetCount() - 1);
```
changed to:
```cpp
ItemSystem::ConsumeItemEcs(EntityFactory::CreateItemEntity(g_registry, pkItem));
```

Counts after this brutal targeted pass:
```text
ItemSystem_LegacyBridge.cpp direct ->SetCount before this pass: 33
ItemSystem_LegacyBridge.cpp direct ->SetCount after this pass:  30
Global GameServer direct ->SetCount after this pass:            73
```

Remaining target-file `SetCount` categories:
- Stack merge / stack split core.
- AutoGive stack merge core.
- Blend item merge core.
- DragonSoul charge/enchant paths.
- Refine scroll/material paths.
- Pet duration DB path.
- Timed/recall item path.

These were skipped because the user rule allowed skipping DragonSoul, refine, pet duration, timed item, and stack merge. The remaining target-file matches fall into those categories.

Validation:
```powershell
cmake --build build --config RelWithDebInfo --target GameServer --parallel 8
```
- Build passed.
- `GameServer.exe` linked successfully.
- Existing post-build message remained:
```text
'pwsh.exe' is not recognized as an internal or external command
```
  but build exit code was `0`.

Header validation:
```powershell
Select-String -Path 'SRC/Server/GameServer/ecs/systems/ItemSystem.hpp' -Pattern 'LPITEM|LPCHARACTER|CHARACTER\s*\*'
```
- Result: no matches.

Manual WinTest checklist:
- Drop partial stack split.
- Inventory item split/move partial stack.
- Metin/scroll consume path touched by `pkItem` decrement.
- Recipe item use path from prior 15E-35a slice.
- Relog after each mutation.
- Verify no duplicate item, missing item, wrong count, wrong owner/window/cell.
- Verify no delayed damage/metin collapse regression.

Commit status:
- Not committed. User requested review before commit.

## Phase 15E-35a SetCount kill follow-up audit

Date: 2026-04-28

Mode:
- Aggressive targeted cleanup review.
- No additional code changes in this follow-up because the remaining target-file matches all fall into explicit skip categories.

Target file:
- `SRC/Server/GameServer/ecs/systems/ItemSystem_LegacyBridge.cpp`

Current count:
```text
Direct ->SetCount in ItemSystem_LegacyBridge.cpp: 30
```

Success target status:
- The requested success target was under 60.
- Current target-file count is 30, so the target is already met.

Remaining direct `SetCount` groups and reasons:
- `826`: core `CItem::SetCount` / delete-on-zero handling, includes ownership removal and duplicate stack handling. Legacy core boundary.
- `3896`, `3897`: explicit stack merge on item move. Skipped by stack merge rule.
- `4369`, `4390`, `4456`, `4477`, `4653`, `4674`, `4740`, `4761`: stack merge / partial count retention blocks. Skipped by stack merge rule.
- `5454`: pet duration DB path. Skipped by pet duration rule.
- `6617`, `6663`, `9418`: DragonSoul charge/enchant paths. Skipped by DragonSoul rule.
- `11571`, `11968`, `12220`: refine scroll/material paths. Skipped by refine rule.
- `12789`, `12791`, `12796`, `12835`, `12837`, `12842`, `13019`, `13074`, `13123`: AutoGive / blend / inventory stack merge core. Skipped by stack merge rule.
- `15145`, `15211`: timed/recall item paths with socket coordinate state and warp semantics. Skipped by timed item rule.

Validation:
```powershell
cmake --build build --config RelWithDebInfo --target GameServer --parallel 8
```
- Build passed.
- `GameServer.exe` linked successfully.

Conclusion:
- No safe additional direct `SetCount` replacements remain in the target file under the current skip rules.
- Further reduction requires dedicated ECS wrappers for stack merge, refine, DragonSoul, pet duration, and timed/recall item logic.

Commit status:
- Not committed. User requested review before commit.

## Phase 15E-35b targeted SetSocket kill

Date: 2026-04-28

Mode:
- Aggressive narrow pass.
- Target file only.
- Code change, no commit.

Target file:
- `SRC/Server/GameServer/ecs/systems/ItemSystem_LegacyBridge.cpp`

Goal:
- Reduce direct LPITEM `SetSocket` usage by migrating obvious socket mutations to ECS socket API.

Counts:
```text
Direct ->SetSocket before: 68
Direct ->SetSocket after:  35
Reduction:                 33
```

Migration pattern:
```cpp
item->SetSocket(index, value);
```
changed to:
```cpp
ItemSystem::SetItemSocketEcs(EntityFactory::CreateItemEntity(g_registry, item), index, value);
```

Migrated call-site groups:
- Generic socket copy helper:
  - `FN_copy_item_socket(dest, src)` now writes destination sockets through `SetItemSocketEcs`.
- New potion / affect active flag sockets:
  - Active/inactive socket flags on `item`, `pkItem`, `old`, and `currentItem` now route through ECS socket writes.
- Stone detector/simple counter socket:
  - Counter increment now routes through `SetItemSocketEcs`.
- Detachment / clean socket / socket clear flows:
  - `USE_DETACHMENT_ONE`, `CLEAN_SOCKET`, and related simple socket writes now route through ECS socket writes.
- Weapon/metin socket writes:
  - Simple weapon socket assignment and metin socket/broken metin writes now route through ECS socket writes.
- Rune activation/deactivation state socket:
  - Rune bonus active flag writes now route through ECS socket writes.

Skipped remaining `SetSocket` groups:
- Timed unique / realtime expire sockets:
  - `2918`, `2922`, `5406`, `5411`, `17419`, `17458`, `17479`, `17492`, `17534`, `17643`, `17649`.
- Pet sockets / pet DB state:
  - `5539`, `5540`, `5543`, `5633`, `5644`, `5645`, `5654`, `9514`, `9515`.
- DragonSoul / duration bottle state:
  - `6580`, `6583`, `6586`.
- Refine / accessory refine socket transfer:
  - `12633`, `12646`, `12657`.
- Recall item coordinate sockets:
  - `15131`, `15132`, `15141`, `15142`, `15204`, `15205`.
- Auto recovery amount-used sockets with logging flag overload:
  - `15771`, `15773` kept because the current ECS API does not preserve the third `bLog` argument semantics.
- Comment-only match:
  - `12492` is commented code and remains unchanged.

Validation:
```powershell
cmake --build build --config RelWithDebInfo --target GameServer --parallel 8
```
- Build passed.
- `GameServer.exe` linked successfully.
- Existing post-build message remained:
```text
'pwsh.exe' is not recognized as an internal or external command
```
  but build exit code was `0`.

Header validation:
```powershell
Select-String -Path 'SRC/Server/GameServer/ecs/systems/ItemSystem.hpp' -Pattern 'LPITEM|LPCHARACTER|CHARACTER\s*\*'
```
- Result: no matches.

Manual WinTest checklist:
- Potion / new potion activate and deactivate.
- Auto recovery item activate/deactivate smoke.
- Stone detector usage.
- Metin socket insert and broken metin outcome.
- Detachment / clean socket item use.
- Rune activate/deactivate if available.
- Pet, DS, refine, timed/recall smoke tests to verify skipped legacy paths still behave unchanged.
- Inventory open/relog.
- Verify no duplicate item, missing item, wrong socket, wrong owner/window/cell.
- Verify no delayed damage/metin collapse regression.

Commit status:
- Not committed. User requested review before commit.

## Phase 15E-35c targeted attribute mutation cleanup

Date: 2026-04-28

Mode:
- Targeted pass.
- Code change, no commit.

Target files:
- `SRC/Server/GameServer/ecs/systems/ItemSystem_LegacyBridge.cpp`
- `SRC/Server/GameServer/questlua_item.cpp`

Goal:
- Replace safe direct LPITEM attribute writes with ECS attribute APIs.

Counts:
```text
ItemSystem_LegacyBridge.cpp direct attribute writes before: 28
ItemSystem_LegacyBridge.cpp direct attribute writes after:  22
questlua_item.cpp direct attribute writes before:           0
questlua_item.cpp direct attribute writes after:            0
```

Migrated call sites:
- Six normal item `item2->AddAttribute()` calls in the attribute-add scroll paths now route through:
```cpp
ItemSystem::AddItemAttributeEcs(EntityFactory::CreateItemEntity(g_registry, item2));
```

Why these were safe:
- They are normal item attribute-add paths.
- They do not belong to DragonSoul, refine, costume/acce transfer, or pet attribute state.
- The wrapper preserves legacy random attribute generation internally and syncs ECS attributes after the legacy mutation.

Skipped remaining direct attribute writes:
- Core CItem boundary:
  - `pItem->SetAttributes(m_aAttr)` inside legacy item internals.
- Pet attributes:
  - Pet box and pet item attribute/socket-derived stat transfer blocks.
- Costume/acce transfer:
  - `ADD_COSTUME_ATTR`, `REMOVE_COSTUME_ATTR`, sash/costume generated attribute paths, costume reset/change paths.
- DragonSoul:
  - DS enchant attribute clear/regeneration path.
- Pet enchant DB-backed attribute path:
  - Requires DB save semantics preservation before ECS migration.

Validation:
```powershell
cmake --build build --config RelWithDebInfo --target GameServer --parallel 8
```
- Build passed.
- `GameServer.exe` linked successfully.
- Existing post-build message remained:
```text
'pwsh.exe' is not recognized as an internal or external command
```
  but build exit code was `0`.

Header validation:
```powershell
Select-String -Path 'SRC/Server/GameServer/ecs/systems/ItemSystem.hpp' -Pattern 'LPITEM|LPCHARACTER|CHARACTER\s*\*'
```
- Result: no matches.

Manual WinTest checklist:
- Add normal bonus to weapon/armor/accessory where supported.
- Add talisman/pendant bonus path if reachable.
- Verify item attributes persist after relog.
- Smoke test skipped paths: pet item, costume/acce transfer, DragonSoul enchant, refine.
- Verify no duplicate item, missing item, wrong attributes, wrong owner/window/cell.
- Verify no delayed damage/metin collapse regression.

Commit status:
- Not committed. User requested review before commit.

## Phase 15E-35a+ loop-level count migration audit

Date: 2026-04-28

Mode:
- Aggressive but controlled.
- Target file only.
- No additional code changes in this follow-up.

Target file:
- `SRC/Server/GameServer/ecs/systems/ItemSystem_LegacyBridge.cpp`

Goal:
- Replace full item consumption blocks, not just single `SetCount` lines, when they are obvious count decrement / manual destroy flows.

Audit result:
- No additional safe full-block replacement was found under the current rules.
- Existing non-skipped consume blocks around normal item use already route through `ItemSystem::ConsumeItemEcs`.
- Remaining full-block candidates are all excluded by rule or are not count-consume semantics.

Skipped full-block groups:
- DragonSoul charge / duration bottle blocks.
- Pet duration / pet item DB-backed blocks.
- Timed / realtime / recall item socket blocks.
- Stack merge / AutoGive merge blocks with `SetCount(0)` plus `M2_DESTROY_ITEM`.
- Inventory clear / character cleanup blocks that call `M2_DESTROY_ITEM` without item count semantics.
- Core `CItem::SetCount` delete-on-zero boundary.

Current counts:
```text
ItemSystem_LegacyBridge.cpp direct ->SetCount:      30
ItemSystem_LegacyBridge.cpp direct M2_DESTROY_ITEM: 15
```

Validation:
```powershell
cmake --build build --config RelWithDebInfo --target GameServer --parallel 8
```
- Build passed.
- `GameServer.exe` linked successfully.

Conclusion:
- No safe loop-level count block remains in `ItemSystem_LegacyBridge.cpp` without entering DragonSoul, refine, pet duration, timed unique/recall, stack merge, or core cleanup semantics.
- Further reduction requires dedicated subsystem wrappers, not broad block replacement.

Commit status:
- Not committed. User requested review before commit.

## Phase 15E-35a+ logic-level consume block migration

Date: 2026-04-28

Mode:
- Aggressive but controlled.
- Target file only.
- Code change, no commit.

Target file:
- `SRC/Server/GameServer/ecs/systems/ItemSystem_LegacyBridge.cpp`

Goal:
- Replace full stack consume / manual destroy blocks with ECS count APIs.
- Remove branching where `ConsumeItemEcs` can own the zero-count destruction.

Changes made:
- Stack merge target increments now use:
```cpp
ItemSystem::AddItemCountEcs(EntityFactory::CreateItemEntity(g_registry, item2), bCount2);
```
- Source stack consumption in the same merge blocks now uses:
```cpp
ItemSystem::ConsumeItemEcs(EntityFactory::CreateItemEntity(g_registry, item), bCount2);
```
- Manual `SetCount(0) + M2_DESTROY_ITEM(item)` branches in these merge blocks were removed; `ConsumeItemEcs` now owns zero-count destruction.
- Existing stack merge return/chat behavior was preserved.
- Existing `AutoGiveItem` stack merge increments now use `AddItemCountEcs` for normal, extra inventory, and blend merge targets.

Counts:
```text
ItemSystem_LegacyBridge.cpp direct ->SetCount before:      30
ItemSystem_LegacyBridge.cpp direct ->SetCount after:       11
ItemSystem_LegacyBridge.cpp direct M2_DESTROY_ITEM(item) before: 15
ItemSystem_LegacyBridge.cpp direct M2_DESTROY_ITEM(item) after:   5
```

Remaining direct `SetCount` matches:
- `826`: core `CItem::SetCount` boundary / delete-on-zero internals.
- `5450`: pet duration DB path.
- `6613`, `6659`, `9414`: DragonSoul charge/enchant paths.
- `11567`, `11964`, `12216`: refine scroll/material paths.
- `15133`, `15199`: timed/recall item coordinate paths.
- One remaining match is a commented-out legacy line.

Remaining direct `M2_DESTROY_ITEM(item)` matches:
- Ground gold pickup destroy.
- Character inventory cleanup / logout destroy loops.
- These are not count-consume branches and were left unchanged.

Validation:
```powershell
cmake --build build --config RelWithDebInfo --target GameServer --parallel 8
```
- Build passed.
- `GameServer.exe` linked successfully.
- Existing post-build message remained:
```text
'pwsh.exe' is not recognized as an internal or external command
```
  but build exit code was `0`.

Header validation:
```powershell
Select-String -Path 'SRC/Server/GameServer/ecs/systems/ItemSystem.hpp' -Pattern 'LPITEM|LPCHARACTER|CHARACTER\s*\*'
```
- Result: no matches.

Manual WinTest checklist:
- Pick up stackable item that merges into inventory stack.
- Pick up stackable extra-inventory item that merges into extra stack.
- AutoGive stack merge into existing stack.
- AutoGive blend item merge.
- Full merge case where source item should disappear.
- Partial merge case where source item should remain with correct count.
- Relog after merge.
- Verify no duplicate item, missing item, wrong count, wrong owner/window/cell.
- Verify no delayed damage/metin collapse regression.

Commit status:
- Not committed. User requested review before commit.

## Phase 15E-47 state mutation accessor migration

Date: 2026-04-29

Mode:
- Entity-first CItem state setter migration.
- Public ECS API remains pointer-clean.

Goal:
- Move caller-side CItem state mutation calls behind `ecs::ItemSystem` entity APIs.
- Leave only core item-manager initialization, comments, and non-CItem false positives outside the legacy bridge.

New / extended ECS APIs:
```cpp
bool SetItemExchanging(entt::entity item, bool flag);
bool LockItem(entt::entity item, bool locked = true);
bool UnlockItem(entt::entity item);
bool SetItemSkipSave(entt::entity item, bool flag);
bool SetItemWindow(entt::entity item, uint8_t window);
bool SetItemCell(entt::entity item, entt::entity owner, uint16_t cell);
bool AlterItemToMagicItem(entt::entity item);
```
Existing ECS setter APIs reused:
```cpp
SetItemSocket
SetItemForceAttributeEcs
ClearItemAttributesEcs
SetItemAttributeEcs
```

Files changed:
- `SRC/Server/GameServer/ecs/systems/ItemSystem.hpp`
- `SRC/Server/GameServer/ecs/systems/ItemSystem.cpp`
- `SRC/Server/GameServer/ecs/systems/ActivitySystem.cpp`
- `SRC/Server/GameServer/ecs/systems/AffectSystem.cpp`
- `SRC/Server/GameServer/ecs/systems/CombatSystem.cpp`
- `SRC/Server/GameServer/ecs/systems/DragonSoulSystem.cpp`
- `SRC/Server/GameServer/ecs/systems/MountSystem.cpp`
- `SRC/Server/GameServer/ecs/systems/PlayerRuntimeSystem.cpp`
- `SRC/Server/GameServer/ecs/systems/SessionSystem.cpp`
- `SRC/Server/GameServer/ecs/systems/SocialSystem.cpp`
- `SRC/Server/GameServer/attr_transfer.cpp`
- `SRC/Server/GameServer/blend_item.cpp`
- `SRC/Server/GameServer/char_manager.cpp`
- `SRC/Server/GameServer/cmd_general.cpp`
- `SRC/Server/GameServer/cmd_gm.cpp`
- `SRC/Server/GameServer/cuberenewal.cpp`
- `SRC/Server/GameServer/db.cpp`
- `SRC/Server/GameServer/DragonSoul.cpp`
- `SRC/Server/GameServer/exchange.cpp`
- `SRC/Server/GameServer/fishing.cpp`
- `SRC/Server/GameServer/input_db.cpp`
- `SRC/Server/GameServer/input_main.cpp`
- `SRC/Server/GameServer/LostCastleDungeon.cpp`
- `SRC/Server/GameServer/marriage.cpp`
- `SRC/Server/GameServer/MountInventory.cpp`
- `SRC/Server/GameServer/New_PetSystem.cpp`
- `SRC/Server/GameServer/PetSystem.cpp`
- `SRC/Server/GameServer/polymorph.cpp`
- `SRC/Server/GameServer/questlua_pc.cpp`
- `SRC/Server/GameServer/safebox.cpp`

Initial setter audit, excluding ItemSystem internals and bridge:
```text
SetSocket:          76
SetAttribute:        1
SetForceAttribute:  85
SetVnum:             0
SetExchanging:       4
Lock:               18
Unlock:              0
SetSkipSave:        17
SetWindow:           3
SetCell:             3
SetOwner:            3
StartUseDelay:       0
SetCreator:          0
AddProtoFlag:        0
RemoveProtoFlag:     0
Set6thAttribute:     0
MoveItemSlot:        0
AlterToMagicItem:    4
AlterToSocketItem:   1
```

Final setter audit, excluding ItemSystem internals and bridge:
```text
SetSocket:          24
SetAttribute:        1
SetForceAttribute:   1
SetVnum:             0
SetExchanging:       0
Lock:                1
Unlock:              0
SetSkipSave:         3
SetWindow:           1
SetCell:             0
SetOwner:            3
StartUseDelay:       0
SetCreator:          0
AddProtoFlag:        0
RemoveProtoFlag:     0
Set6thAttribute:     0
MoveItemSlot:        0
AlterToMagicItem:    2
AlterToSocketItem:   1
```

Final remaining classification:
- `item_manager.cpp`: core item creation / initialization / alter logic; intentionally left as legacy core boundary.
- `sectree_manager.cpp`: `pSec->SetAttribute`, not CItem.
- `new_offlineshop.cpp`: `CShopItem::SetWindow`, not CItem.
- `PlayerRuntimeSystem.cpp`: safebox/shop owner setters, not CItem.
- `guild.cpp`: commented land owner setter, not CItem.
- `AffectSystem.cpp`: commented legacy item lock/socket lines.

Migrated systems:
- Quest reward item socket/attribute setup.
- DragonSoul state socket and attribute writes through ECS wrappers.
- Polymorph book socket state through ECS wrappers.
- GM item socket/attribute commands.
- Exchange lock state.
- Safebox/mount inventory window/cell/skip-save state.
- Pet summon item locking and pet item attribute setup.
- Cube, blend, fishing, combat, session, input, DB restore and runtime setter paths.

Build results:
- Build passed after API addition.
- Build passed after each file/batch migration.
- Final build command:
```powershell
cmake --build build --config RelWithDebInfo --target GameServer --parallel 8
```
- `GameServer.exe` linked successfully.
- Existing post-build message remained:
```text
'pwsh.exe' is not recognized as an internal or external command
```
  but build exit code was `0`.

Header validation:
```powershell
Select-String -Path 'SRC/Server/GameServer/ecs/systems/ItemSystem.hpp' -Pattern 'LPITEM|LPCHARACTER|CHARACTER\s*\*'
```
- Result: no matches.

Manual WinTest checklist:
- Refine an item and verify socket/attribute state.
- DragonSoul activate/deactivate and refine smoke test.
- Polymorph book generate / practice / upgrade smoke test.
- GM item socket/attribute commands.
- Trade/exchange lock/unlock behavior.
- Safebox deposit/withdraw/relog.
- Mount inventory item move/relog.
- Pet summon book and pet attribute setup.
- Fishing rod/fish socket state.
- Quest reward item with sockets/attributes.
- Inventory open/relog.
- Verify no duplicate/missing item, wrong socket, wrong attribute, wrong owner/window/cell.
- Verify VID_DRIFT remains zero.

Commit status:
- Committed in multiple Phase 15E-47 commits; no squash.

## Phase 15E-48 partial signature migration: P1/P2/P3 safe helpers

Date: 2026-04-29

Mode:
- LPITEM parameter signature migration.
- Partial safe batch only; no core `char_item.cpp`, `item_manager.cpp`, or `ItemSystem_LegacyBridge.cpp` internals touched.

Scope completed:
- P1 internal helpers:
  - `Halloween2022Dungeon.cpp`: `RemoveOneGivenItem(entt::entity, ...)`
  - `LostCastleDungeon.cpp`: `ConsumeOneGivenItem(entt::entity, ...)`
  - `MountInventory.cpp`: `StartMountExpireIfNeeded(entt::entity)`
- P2 ECS helpers:
  - `InventorySystem.cpp`: `SyncItemLocation(entt::entity)`
  - `CombatSystem.cpp`: `MakeItemLink(entt::entity, ...)`
- P3 free functions:
  - `fishing.cpp/.h`: `UseFish(entt::entity, entt::entity)` and `Grill(entt::entity, entt::entity)`
  - `battle.cpp`: `Item_GetDamage(entt::entity, ...)`
  - `battle.cpp/.h`: `CalcArrowDamage(..., entt::entity bow, entt::entity arrow, ...)`
- Small P4-compatible read-only cleanup:
  - `DragonSoul.cpp/.h`: removed LPITEM compatibility wrappers for `LeftTime`, `IsTimeLeftDragonSoul`, and `IsActiveDragonSoul`; callers now use existing entity overloads.

Initial LPITEM parameter count for this scoped audit:
```text
Excluding ItemSystem internals, EntityFactory, ItemRegistry, item_manager.cpp, char.h:
179
```

Final LPITEM parameter count for the same scoped audit:
```text
161
```

Top remaining files after this batch:
```text
DragonSoul.cpp: 9
DragonSoul.h: 8
MountSystem.cpp: 7
CombatSystem.cpp: 6
MountSystem.h: 5
New_PetSystem.cpp: 5
PlayerRuntimeSystem.cpp: 5
new_switchbot.cpp: 5
New_PetSystem.h: 5
MountInventory.cpp/h: 4 each
questmanager.cpp/h: 4 each
mining.cpp: 4
InventorySystem.cpp: 4
```

Remaining blockers / deferred groups:
- DragonSoul mutation entry points: still class/public API and side-effect heavy; defer to dedicated DS signature phase.
- Mount/Pet systems: class methods and DB/state semantics; defer to subsystem batch.
- Quest manager current item APIs: cross-subsystem callers; defer to quest current item signature batch.
- Mining/fishing rod refine: legacy refine/remove paths remain; requires wrapper-level conversion.
- Combat reward/drop helpers: still use `AddToCharacter`, `AddToGround`, ownership timers; leave until location/ownership signatures are converted.
- `item_manager.h`, `item.h`: core CItem/item-manager island; explicitly not touched in 15E-48.

Build results:
- Build passed after each migrated file/group.
- One intermediate build failed with one missing DragonSoulSystem caller after removing LPITEM time wrappers; fixed immediately by converting the caller to entity overload.
- Final validation command:
```powershell
cmake --build build --config RelWithDebInfo --target GameServer --parallel 8
```

Header validation:
```powershell
Select-String -Path 'SRC/Server/GameServer/ecs/systems/ItemSystem.hpp' -Pattern 'LPITEM|LPCHARACTER|CHARACTER\s*\*'
```
- Result: no matches.

Manual WinTest checklist for this batch:
- Halloween dungeon item hand-in consumes correct count.
- Lost Castle key/tile hand-in consumes correct count.
- Mount inventory add/relog starts mount realtime expiration correctly.
- Fishing use/grill still grants/consumes correctly.
- Bow/arrow combat and arrow skills produce expected damage.
- DragonSoul active deck stat recompute still respects remaining time.
- Rare drop notice item links still show sockets/attributes/name correctly.

Commit status:
- Committed in separate Phase 15E-48 commits; no squash.

## Phase 15E-49 — LPITEM Signature Migration: Subsystem Batches

Mode:
- Subsystem-by-subsystem LPITEM parameter signature migration.
- No `char_item.cpp`, `item_manager.cpp`, or core CItem deletion attempted.
- `ItemSystem.hpp` remained pointer-clean.

Completed batches:
- PART A Switchbot:
  - `new_switchbot.cpp/.h` switchbot item helper signatures changed from `LPITEM` to `entt::entity`.
  - Commit: `292610e Phase 15E-49 PART A: Switchbot signatures entity-first`
- PART B MountInventory:
  - `CMountInventory::Add`, `RemoveByItem`, `DetachSlot`, and `SaveItem` item parameters changed to `entt::entity`.
  - Commit: `a0cd8f8 Phase 15E-49 PART B: MountInventory signatures entity-first`
- PART C QuestManager:
  - `UseItem`, `SIGUse`, and `TakeItem` now take `entt::entity`.
  - Removed public `SetCurrentItem(LPITEM)` overload; `SetCurrentItem(entt::entity)` keeps the legacy quest item pointer bridge internally.
  - Commit: `b6ef0b6 Phase 15E-49 PART C: QuestManager item signatures`
- PART D Mining:
  - `RealRefinePick`, `CHEAT_MAX_PICK`, and `OreRefine` now take `entt::entity`.
  - Mining internals still resolve legacy CItem privately because pick/refine helpers still operate on `CItem&`.
  - Commit: `b5aaaa2 Phase 15E-49 PART D: Mining signatures entity-first`
- PART E InventorySystem:
  - Internal `SyncCharacterEquipmentSlot` now takes `entt::entity`.
  - `EquipmentSlots` still stores `LPITEM`; this is an ECS component storage cleanup blocker for a later phase.
  - Commit: `e755f5e Phase 15E-49 PART E: InventorySystem signatures entity-first`
- PART F PlayerRuntimeSystem:
  - Local inventory-sort quickslot checker lambdas now take `entt::entity`.
  - Class APIs `SetQuestItemPtr`, `CanTakeInventoryItem`, and `CleanAcceAttr` intentionally deferred.
  - Commit: `97bae2d Phase 15E-49 PART F: PlayerRuntimeSystem signatures entity-first`
- PART G CombatSystem:
  - Static reward/drop helpers `__TryAutoGiveRewardItem` and `__GiveRewardItemToCharacterOrDrop` now take `entt::entity`.
  - Legacy CItem is resolved privately inside the helper for AddToCharacter/AddToGround/log side effects.
  - Commit: `1fe5c13 Phase 15E-49 PART G: CombatSystem signatures entity-first`
- PART H Mount/Pet:
  - `CMountActor`, `CMountSystem`, `CPetActor`, `CPetSystem`, `CNewPetActor`, and `CNewPetSystem` summon/mount/book item parameters changed to `entt::entity` where safe.
  - Legacy CItem resolution remains private inside the subsystem implementations for DB/socket/state side effects.
  - Commits:
    - `ce501d5 Phase 15E-49 PART H seg 1: MountSystem signatures`
    - `17dacfd Phase 15E-49 PART H seg 2: PetSystem signatures`
    - `0c0693e Phase 15E-49 PART H seg 3: NewPetSystem signatures`

Counts:
```text
Scoped LPITEM parameter count before 15E-49: 161
Scoped LPITEM parameter count after 15E-49:  90
Reduction: 71
```

Top remaining scoped LPITEM parameter files:
```text
DragonSoul.cpp: 9
DragonSoul.h: 8
item.h: 4
CombatSystem.cpp: 4
new_offlineshop.h: 3
polymorph.cpp/h: 3 each
exchange.cpp: 3
PlayerRuntimeSystem.cpp: 3
InventorySystem.cpp: 3
```

DragonSoul status:
- PART I was audited but not migrated in this pass.
- A broad local edit attempt was rejected before commit because it touched DS refine/extract internals too widely.
- `DragonSoul.cpp/.h` were restored to HEAD for this phase; no DS behavior change shipped.
- Remaining DS LPITEM signatures are still legacy engine boundaries: `PullOut`, `ExtractDragonHeart`, `DragonSoulItemInitialize`, `PutAttributes`, `RefreshItemAttributes`, activation/deactivation legacy entry points, and refine material helper.

Remaining blockers:
- DragonSoul needs a dedicated DS-only signature phase with segment commits.
- `InventorySystem` component storage still contains `std::array<LPITEM, ...>` and needs an entity-storage phase.
- `PlayerRuntimeSystem` remaining signatures are `CHARACTER::` class APIs and require caller-wide migration.
- `CombatSystem::GetArrowAndBow` and `UseArrow` still expose LPITEM through core combat/weapon APIs.
- Offlineshop/exchange/polymorph/refine/file-core islands remain intentionally out of this batch.

Build results:
- Build passed after every committed batch.
- One MountSystem build failed with two missed same-file callers; fixed before commit.
- One PetSystem build failed with three missed same-file callers; fixed before commit.
- DragonSoul broad edit was restored before build/commit.
- Final successful command used repeatedly:
```powershell
cmake --build build --config RelWithDebInfo --target GameServer --parallel 8
```

Manual WinTest checklist:
- Switchbot configure/run/stop.
- Mount summon/mount/unmount/skin update.
- Pet summon/unsummon and old pet quest summon.
- NewPet summon/book skill/evolution/rename skin refresh.
- Mining pick refine and ore refine with metinstone.
- Quest item use/take current item flow.
- Combat reward drop and instant inventory merge path.
- Inventory equip/unequip quickslot sync.
- Full login/relog persistence.

Commit status:
- Code batches committed individually.
- WinTest not run in this environment.

## Phase 16-2 Hotfix - Trace Logging Split and Combat Attack Timestamp

Mode:
- Post Batch 3 WinTest regression follow-up.
- No broad logging call-site migration in this hotfix.
- Goal was to reduce newly migrated hot-path `LOG_INFO` noise and fix the metin/combat delayed-damage regression observed on WinTest.

Problem observed:
- WinTest initially felt like the character was on a "zombie core"; chat input disconnected the client.
- After the first logging cleanup, gameplay became playable, but metin stones received damage only after the player stopped attacking, then collapsed 1-2 seconds later.
- After the attack timestamp fix, the user confirmed gameplay was working normally again.

Trace logging split:
- `SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE` was added to the server compile definitions so `LOG_TRACE` call sites are compiled in.
- Runtime logger level was changed back to `info`, so trace calls are available for diagnostics but suppressed by default.
- Hot-path debug logs moved from `LOG_INFO` to `LOG_TRACE`:
  - `DBCLIENT: header ...` in `input_db.cpp`
  - `DB_BYTES_READ` in `main.cpp`
  - combat/death/drop debug logs in `ecs/systems/CombatSystem.cpp`
  - motion loading / normal attack duration debug logs in `motion.cpp`
- This removed the visible INFO-level spam introduced by the mechanical `sys_log(1)` to `LOG_INFO` conversion.

Combat regression root cause:
- The `HEADER_CG_ATTACK` path called `ch->Attack(...)` without refreshing the legacy last-attack timestamp.
- `battle_melee_attack()` / `battle_hit()` use the `ENABLE_CHECK_BATTLE` guard:
```cpp
const bool bAttacking =
    (get_dword_time() - ch->GetLastAttackTime()) <
    (ch->IsRiding() ? 800 : 750);
```
- Because `m_dwLastAttackTime` was stale, damage application was rejected while attacking and later appeared delayed.

Combat fix:
- `input_main.cpp` now calls `ch->OnMove(true);` immediately before `ch->Attack(victim, packMelee->bType);` in the attack packet path.
- This restores the same attack-timestamp side effect expected by the battle guard without changing damage formulas, packets, or death scheduling.

Files changed:
- `SRC/Server/CMakeLists.txt`
- `SRC/Server/Core/Logging.cpp`
- `SRC/Server/GameServer/input_db.cpp`
- `SRC/Server/GameServer/main.cpp`
- `SRC/Server/GameServer/motion.cpp`
- `SRC/Server/GameServer/input_main.cpp`
- `SRC/Server/GameServer/ecs/systems/CombatSystem.cpp`

Build/deploy:
- Build passed:
```powershell
cmake --build build --config RelWithDebInfo --target GameServer --parallel 8
```
- MSBuild emitted a post-build warning that `pwsh.exe` was not recognized, but the build completed successfully and produced `GameServer.exe`.
- Built binary was deployed to:
```text
C:\AurigaGlobal-WinTest\srv1\share\bin\GameServer.exe
```
- A backup was created before deployment:
```text
C:\AurigaGlobal-WinTest\srv1\share\bin\GameServer.exe.pre_attack_time_fix_20260430_114710
```
- Deployed binary hash matched the built binary:
```text
515B98EBC864D2B8A6CC2A41CC36118B1E21A50CB758F1B9FBEC2F534C5DFF1E
```

WinTest result:
- User confirmed the game is playable after the fixes.
- User confirmed the metin delayed-damage problem is resolved.

Log review after fix:
- No fresh critical combat/ECS errors were found.
- No `VID_DRIFT` was found.
- No fmt formatting exceptions were found.
- No new ECS orphan/ghost/duplicate item indicators were found.
- No new `DBCLIENT: header` or `DB_BYTES_READ` INFO spam was found after the trace split.
- No new `DEAD` / `DEAD_EVENT_CREATE` INFO spam was found after moving combat debug logs to `LOG_TRACE`.

Remaining oddities found in WinTest logs:
- `ch99/core99/log/syserr.txt` contains repeated quest-state warnings:
```text
QUEST __status invalid: pid=6 quest=sash val=668443392 -> start=0
```
- `auth/log/syserr.txt` still reports startup data/config issues:
```text
Cube_Init failed
<Blend_Item_init> fail
```
- Disconnect/restart noise remains in `ch1/core*/syserr.txt` and `db/syserr.txt`, including socket read / peer receive failures. These looked like connection lifecycle noise, not new crash evidence.
- PTS logs still show event/write spikes on `ch99/core99` and `ch1/core*`; because gameplay now works, these are not the metin damage blocker but should be profiled later.
- Syslog remains noisy from non-critical INFO paths such as `MOB_SPAWN`, quest kill/drop logs, `BattlePassInfo`, and `ITEM_SAVE`.

Recommended follow-up:
- Move remaining high-frequency debug-only logs (`MOB_SPAWN`, quest kill/drop traces, `BattlePassInfo`, selected `ITEM_SAVE`) to `LOG_TRACE` or `LOG_DEBUG`.
- Investigate the saved `sash` quest state separately if it persists after relog or affects quest behavior.
- Investigate `Cube_Init failed` / `<Blend_Item_init> fail` as data/config startup cleanup, separate from logging and combat.
- Profile PTS event/write spikes only after the logging hot paths are quiet.

Hotfix commit consolidation:
- The 7 hotfix files were reviewed and split into 3 logical commits after WinTest confirmation.
- Pre-commit build passed before staging the hotfix groups.
- Final build passed after all 3 commits.
- Final working tree after the hotfix commits: clean.

Hotfix commits:
- `72e3b24 Phase 16-2 Hotfix: Enable SPDLOG_ACTIVE_LEVEL=TRACE compile-time`
  - `SRC/Server/CMakeLists.txt`
  - `SRC/Server/Core/Logging.cpp`
  - Trace-level logs are compiled in while runtime default level remains `info`.
- `2d7def9 Phase 16-2 Hotfix: Demote hot-path debug logs to LOG_TRACE`
  - `SRC/Server/GameServer/input_db.cpp`
  - `SRC/Server/GameServer/main.cpp`
  - `SRC/Server/GameServer/motion.cpp`
  - `SRC/Server/GameServer/ecs/systems/CombatSystem.cpp`
  - DBCLIENT headers, DB byte reads, combat death/drop debug, and motion loading logs now use `LOG_TRACE`.
- `cb1e4b9 Phase 16-2 Hotfix: Restore OnMove(true) before Attack`
  - `SRC/Server/GameServer/input_main.cpp`
  - Restores `ch->OnMove(true)` immediately before `ch->Attack(...)` in the `HEADER_CG_ATTACK` path.
  - User-confirmed WinTest result: metin damage and gameplay are normal again.

Post-hotfix build:
```powershell
cmake --build build --config RelWithDebInfo --target GameServer --parallel 8
```
- Result: passed.

Post-hotfix logging audit:
```text
GameServer sys_log: 407
GameServer sys_err: 723
GameServer _sys_err: 0
```

Current next logging candidates after Batch 4:
```text
33 questlua.cpp
25 arena.cpp
25 war_map.cpp
24 input_auth.cpp
24 ecs/systems/SessionSystem.cpp
23 battle_pass.cpp
22 DragonSoul.cpp
20 MarkManager.cpp
20 ecs/systems/AffectSystem.cpp
20 ecs/systems/InventorySystem.cpp
```

Quest-prefix verification:
- `questlua.cpp` has a local `sys_err` redirect:
```cpp
#define sys_err(fmt, args...) quest::CQuestManager::instance().QuestError(__FUNCTION__, __LINE__, fmt, ##args)
#define sys_err(fmt, ...) quest::CQuestManager::instance().QuestError(__FUNCTION__, __LINE__, fmt, __VA_ARGS__)
```
- `questlua.cpp` must remain deferred to the dedicated QuestError modernization pass.
- `questpc.cpp` and `questnpc.cpp` had no local `sys_err` override and were already migrated in Batch 4.

## Phase 16-1 - spdlog + fmt Integration

Mode:
- Foundation-only logging modernization.
- No call-site migration in this phase.
- Existing `sys_log` / `sys_err` names and printf-style format strings remain compatible.

Tooling/library decision:
- Project C++ standard: C++23 (`CMAKE_CXX_STANDARD 23`).
- `fmt` is still preferred over `std::format` because spdlog integrates with fmt directly and the transition layer needs stable printf-compatible formatting support.
- Vendored header-only dependencies:
  - `extern/fmt/include/fmt` from fmt 10.2.1.
  - `extern/spdlog/include/spdlog` from spdlog 1.13.0.

Files changed:
- `SRC/Server/CMakeLists.txt`
- `SRC/Server/Core/Logging.hpp`
- `SRC/Server/Core/Logging.cpp`
- `SRC/Server/Core/log.cpp`
- `extern/fmt/include/fmt/*`
- `extern/spdlog/include/spdlog/*`

Implementation summary:
- Added `logging::Init`, `logging::Shutdown`, `logging::GetLogger`, and `logging::GetErrorLogger`.
- Added modern `LOG_TRACE`, `LOG_DEBUG`, `LOG_INFO`, `LOG_WARN`, and `LOG_ERROR` macros.
- Configured async spdlog loggers with rotating file sinks:
  - `./log/syslog.txt`
  - `./log/syserr.txt`
  - 50 MB per file, 10 rotated files.
- Routed legacy `sys_log` through the async syslog logger.
- Routed legacy `sys_err` / `_sys_err` through the async syserr logger and mirrored errors to syslog.
- Kept legacy `PTS.txt` handling on the old `pt_log` path.
- Avoided double-opening `syslog.txt` / `syserr.txt` with legacy `FILE*`; spdlog now owns those files.

Build results:
- Build passed after vendored dependency integration.
- Build passed after adding the logging infrastructure.
- Build passed after routing `sys_log` / `sys_err` through spdlog.
- Final successful command:
```powershell
cmake --build build --config RelWithDebInfo --target GameServer --parallel 8
```

Manual WinTest checklist:
- Server starts cleanly.
- `./log/syslog.txt` and `./log/syserr.txt` are created.
- Existing calls such as `sys_log(0, "user %s", name)` still format correctly.
- Error entries flush promptly to `syserr.txt`.
- File rotation works at the configured size threshold.
- Shutdown flushes pending async log entries.
- High-frequency logging does not block the game thread.

Commit status:
- `44e595c Phase 16-1.1: Integrate fmt + spdlog headers`
- `1a77bb1 Phase 16-1.2: Add modern logging infrastructure`
- `124ae3e Phase 16-1.3: Route sys_log sys_err through spdlog`
- WinTest not run in this environment.

## Phase 16-2 - sys_log/sys_err Call Site Format Modernization

Mode:
- Call-site migration from legacy printf-style logging to fmt-style `LOG_*` macros.
- Keep the Phase 16-1 `sys_log` / `sys_err` compatibility bridge until all call sites are migrated.
- No behavior changes outside logging format syntax.

Initial audit:
```text
GameServer sys_log: 1057
GameServer sys_err: 1308
GameServer _sys_err: 2
Total GameServer legacy log call sites: 2367
```

Top files by legacy logging call count:
```text
134 input_db.cpp
117 ecs/systems/ItemSystem_LegacyBridge.cpp
 87 ecs/systems/SkillSystem.cpp
 87 questlua_pc.cpp
 85 dragon_soul_table.cpp
 80 questlua_dungeon.cpp
 70 ecs/systems/CombatSystem.cpp
 68 questmanager.cpp
 62 input_main.cpp
 49 item_manager_read_tables.cpp
 49 sectree_manager.cpp
 48 item_manager.cpp
 47 db.cpp
 43 questlua_global.cpp
 38 input_login.cpp
 38 dungeon.cpp
 36 guild.cpp
 35 char_manager.cpp
 34 main.cpp
 33 building.cpp
```

Migration approach:
- Use direct `LOG_INFO` / `LOG_ERROR` migration rather than overloaded `sys_log` format detection.
- Convert format strings from `%` placeholders to `{}` fmt placeholders.
- Keep the old `sys_log` / `sys_err` functions for unmigrated files.
- Build after every migrated file.

Commit status:
- `Phase 16-2.1` audit/approach commit in progress.

Batch 1 completed:
- `input_db.cpp`: 134 legacy log call sites migrated to `LOG_INFO` / `LOG_ERROR`.
- `ecs/systems/ItemSystem_LegacyBridge.cpp`: 117 legacy log call sites migrated; commented legacy examples renamed to avoid grep false positives.
- `ecs/systems/SkillSystem.cpp`: 87 legacy log call sites migrated.
- `dragon_soul_table.cpp`: 85 legacy log call sites migrated.
- `ecs/systems/CombatSystem.cpp`: 70 legacy log call sites migrated.

Compatibility helpers added during Batch 1:
- `Logging.hpp` now undefines pre-existing `LOG_*` macros before defining the modern spdlog macros, avoiding the legacy `dev_log.h` `LOG_INFO` / `LOG_WARN` name collision.
- `Logging.hpp` formats enum values numerically, preserving old `%d` enum logging semantics.
- `Logging.hpp` formats non-character object pointers as `const void*`, preserving old `%p` pointer logging semantics where direct casts are not practical.

Quest-specific skip:
- `questlua_pc.cpp` and `questlua_dungeon.cpp` intentionally skipped in this batch.
- Both files locally redefine `sys_err` to `quest::CQuestManager::QuestError(...)`.
- Those call sites are not the Core `sys_err` compatibility bridge and need a separate QuestError modernization pass.

Counts after Batch 1:
```text
GameServer sys_log: 807
GameServer sys_err: 1067
GameServer _sys_err: 2
Total GameServer legacy log call sites: 1876
Batch 1 reduction: 491
```

Build results:
- Build passed after every migrated file.
- Final successful command:
```powershell
cmake --build build --config RelWithDebInfo --target GameServer --parallel 8
```

Commit status:
- `bc189d3 Phase 16-2.1: Confirm LOG macro logging migration approach`
- `c9de540 Phase 16-2.2: Expose LOG macros to GameServer sources`
- `716dde9 Phase 16-2.3: Avoid dev_log LOG macro collision`
- `fbf4938 Phase 16-2: Migrate logging format in input_db.cpp`
- `ffe93ef Phase 16-2: Migrate logging format in ItemSystem_LegacyBridge.cpp`
- `23e23f6 Phase 16-2: Migrate logging format in SkillSystem.cpp`
- `e7f8ef5 Phase 16-2: Migrate logging format in dragon_soul_table.cpp`
- `d33c38c Phase 16-2: Migrate logging format in CombatSystem.cpp`
- WinTest not run in this environment.

Batch 2 completed:
- `questmanager.cpp`: 68 legacy log call sites migrated to `LOG_INFO` / `LOG_ERROR`.
- `input_main.cpp`: 62 legacy log call sites migrated.
- `item_manager_read_tables.cpp`: 49 legacy log call sites migrated.
- `sectree_manager.cpp`: 49 legacy log call sites migrated.
- `item_manager.cpp`: 48 legacy log call sites migrated.
- `db.cpp`: 47 legacy log call sites migrated.

Batch 2 gotchas:
- `item_manager_read_tables.cpp` includes `dev_log.h`; `Core/Logging.hpp` must be included after it so the modern `LOG_*` macros win.
- `sectree_manager.cpp` uses bitfield-like sectree coordinate members; fmt/spdlog needed explicit integer casts for those log arguments.
- `db.cpp` had two legacy `sys_err(0, "...")` calls; these were normalized to `LOG_ERROR("...", ...)`.
- `questlua_pc.cpp` and `questlua_dungeon.cpp` remain deferred because their local `sys_err` macro redirects to `QuestError`.

Counts after Batch 2:
```text
Before Batch 2:
GameServer sys_log: 807
GameServer sys_err: 1067
GameServer _sys_err: 2
Total: 1876

After Batch 2:
GameServer sys_log: 631
GameServer sys_err: 922
GameServer _sys_err: 0
Total: 1553

Batch 2 reduction: 323
Phase 16-2 cumulative reduction: 814
```

Remaining top files:
```text
87 questlua_pc.cpp
80 questlua_dungeon.cpp
43 questlua_global.cpp
38 input_login.cpp
38 dungeon.cpp
36 guild.cpp
35 char_manager.cpp
34 main.cpp
33 building.cpp
33 questlua.cpp
32 party.cpp
32 questpc.cpp
30 questnpc.cpp
30 ecs/systems/PlayerRuntimeSystem.cpp
29 guild_war.cpp
```

Build results:
- Build passed after every Batch 2 migrated file.
- Final successful command:
```powershell
cmake --build build --config RelWithDebInfo --target GameServer --parallel 8
```

Commit status:
- `f884d6f Phase 16-2: Migrate logging format in questmanager.cpp`
- `03f69dc Phase 16-2: Migrate logging format in input_main.cpp`
- `aafd760 Phase 16-2: Migrate logging format in item_manager_read_tables.cpp`
- `061cbc4 Phase 16-2: Migrate logging format in sectree_manager.cpp`
- `1481d76 Phase 16-2: Migrate logging format in item_manager.cpp`
- `4700a2c Phase 16-2: Migrate logging format in db.cpp`
- WinTest not run in this environment.

Batch 3 completed:
- `input_login.cpp`: 38 legacy log call sites migrated.
- `dungeon.cpp`: 38 legacy log call sites migrated.
- `guild.cpp`: 36 legacy log call sites migrated.
- `char_manager.cpp`: 35 legacy log call sites migrated.
- `main.cpp`: 34 legacy log call sites migrated.
- `building.cpp`: 33 legacy log call sites migrated.

Batch 3 gotchas:
- `input_login.cpp` exposed an old format/argument mismatch in `player_prototype error`; the missing face/shape argument was restored as `pinfo->shape`.
- `char_manager.cpp` had a legacy `sys_err(0, "...")` form and needed manual normalization to `LOG_ERROR("...")`.
- Block-comment examples in `dungeon.cpp` / `guild.cpp` were renamed to avoid grep false positives.

Counts after Batch 3:
```text
Before Batch 3:
GameServer sys_log: 631
GameServer sys_err: 922
GameServer _sys_err: 0
Total: 1553

After Batch 3:
GameServer sys_log: 517
GameServer sys_err: 822
GameServer _sys_err: 0
Total: 1339

Batch 3 reduction: 214
Phase 16-2 cumulative reduction: 1028
```

Batch 4 candidates, excluding `questlua_*`:
```text
33 questlua.cpp
32 questpc.cpp
32 party.cpp
30 ecs/systems/PlayerRuntimeSystem.cpp
30 questnpc.cpp
29 guild_war.cpp
28 shop_manager.cpp
26 desc.cpp
25 war_map.cpp
25 arena.cpp
24 input_auth.cpp
24 ecs/systems/SessionSystem.cpp
23 battle_pass.cpp
22 DragonSoul.cpp
20 ecs/systems/InventorySystem.cpp
```

Build results:
- Build passed after every Batch 3 migrated file.
- Final successful command:
```powershell
cmake --build build --config RelWithDebInfo --target GameServer --parallel 8
```

Commit status:
- `e42aed0 Phase 16-2: Migrate logging format in input_login.cpp`
- `50913a7 Phase 16-2: Migrate logging format in dungeon.cpp`
- `6765eba Phase 16-2: Migrate logging format in guild.cpp`
- `d7ccaf3 Phase 16-2: Migrate logging format in char_manager.cpp`
- `210a451 Phase 16-2: Migrate logging format in main.cpp`
- `b722349 Phase 16-2: Migrate logging format in building.cpp`
- WinTest not run in this environment.

Batch 4 verification:
- `questlua.cpp` was checked first because of the quest-prefix naming.
- `questlua.cpp` locally redefines `sys_err` to `quest::CQuestManager::QuestError(...)`, so it was skipped and deferred to the dedicated QuestError modernization pass.
- `questpc.cpp` and `questnpc.cpp` had no local `sys_err` override and were migrated normally.

Batch 4 completed:
- `questpc.cpp`: 32 legacy log call sites migrated.
- `party.cpp`: 32 legacy log call sites migrated.
- `ecs/systems/PlayerRuntimeSystem.cpp`: 30 legacy log call sites migrated.
- `questnpc.cpp`: 30 legacy log call sites migrated.
- `guild_war.cpp`: 29 legacy log call sites migrated.
- `shop_manager.cpp`: 28 legacy log call sites migrated.
- `desc.cpp`: 26 legacy log call sites migrated.
- `questlua.cpp`: 33 call sites skipped because of the local `QuestError` redirect.

Batch 4 gotchas:
- `questpc.cpp`, `party.cpp`, `questnpc.cpp`, and `desc.cpp` contained commented legacy examples; these were normalized to `LOG_INFO` / `LOG_ERROR` examples so grep-based verification stays clean.
- `questnpc.cpp`, `shop_manager.cpp`, and `desc.cpp` contain non-UTF-8 legacy bytes, so small comment/cast fixes had to be applied with byte-preserving edits.
- `party.cpp`, `guild_war.cpp`, `shop_manager.cpp`, and `PlayerRuntimeSystem.cpp` needed explicit `static_cast<int>(...)` for byte-sized fields to preserve old `%d` numeric rendering under fmt.
- `desc.cpp` had two legacy calls with extra arguments that were ignored by `sys_log`; the extra arguments were removed rather than carried into fmt-style calls.
- `PlayerRuntimeSystem.cpp` emitted an existing non-logging MSVC warning around `EditMyInven` (`uint16_t` to `uint8_t` map insert), but the build passed.

Counts after Batch 4:
```text
Before Batch 4:
GameServer sys_log: 517
GameServer sys_err: 822
GameServer _sys_err: 0
Total: 1339

After Batch 4:
GameServer sys_log: 407
GameServer sys_err: 723
GameServer _sys_err: 0
Total: 1130

Batch 4 reduction: 209
Phase 16-2 cumulative reduction: 1237
```

Batch 5 candidates, excluding `questlua_*`:
```text
33 questlua.cpp
25 arena.cpp
25 war_map.cpp
24 input_auth.cpp
24 ecs/systems/SessionSystem.cpp
23 battle_pass.cpp
22 DragonSoul.cpp
20 MarkManager.cpp
20 ecs/systems/AffectSystem.cpp
20 ecs/systems/InventorySystem.cpp
19 ecs/systems/PointSystem.cpp
18 shop.cpp
18 cmd_gm.cpp
17 text_file_loader.cpp
17 skill.cpp
16 ecs/systems/NetworkSyncSystem.cpp
16 ecs/systems/SocialSystem.cpp
14 cmd_general.cpp
14 ecs/systems/MovementSystem.cpp
13 input_p2p.cpp
```

Questlua group remaining:
```text
87 questlua_pc.cpp
80 questlua_dungeon.cpp
43 questlua_global.cpp
20 questlua_marriage.cpp
12 questlua_item.cpp
12 questlua_party.cpp
11 questlua_affect.cpp
10 questlua_guild.cpp
9 questlua_npc.cpp
9 questlua_quest.cpp
6 questlua_target.cpp
4 questlua_building.cpp
4 questlua_horse.cpp
4 questlua_game.cpp
3 questlua_dragonsoul.cpp
2 questlua_pet.cpp
2 questlua_petnew.cpp
2 questlua_arena.cpp
```

Build results:
- Build passed after every Batch 4 migrated file.
- Final successful command used repeatedly:
```powershell
cmake --build build --config RelWithDebInfo --target GameServer --parallel 8
```
- The existing post-build warning remains:
```text
'pwsh.exe' is not recognized as an internal or external command
```

Commit status:
- `3505337 Phase 16-2: Migrate logging format in questpc.cpp`
- `83018d5 Phase 16-2: Migrate logging format in party.cpp`
- `597223c Phase 16-2: Normalize party logging numeric casts`
- `57b9fc0 Phase 16-2: Migrate logging format in PlayerRuntimeSystem.cpp`
- `f91536d Phase 16-2: Migrate logging format in questnpc.cpp`
- `a7676e3 Phase 16-2: Migrate logging format in guild_war.cpp`
- `77eb640 Phase 16-2: Migrate logging format in shop_manager.cpp`
- `f29a441 Phase 16-2: Migrate logging format in desc.cpp`
- WinTest not run for Batch 4 per phase instruction.

Batch 5 verification:
- Checked all Batch 5 target files for local `sys_err` redirects before migration.
- No local `sys_err` override was found in:
  - `arena.cpp`
  - `war_map.cpp`
  - `input_auth.cpp`
  - `ecs/systems/SessionSystem.cpp`
  - `battle_pass.cpp`
  - `DragonSoul.cpp`
  - `MarkManager.cpp`
  - `ecs/systems/AffectSystem.cpp`
  - `ecs/systems/InventorySystem.cpp`
- `questlua.cpp` remains skipped because it routes `sys_err` to `QuestError`.

Batch 5 completed:
- `arena.cpp`: 25 legacy log call sites migrated.
- `war_map.cpp`: 25 legacy log call sites migrated.
- `input_auth.cpp`: 24 legacy log call sites migrated.
- `ecs/systems/SessionSystem.cpp`: 24 legacy log call sites migrated.
- `battle_pass.cpp`: 23 legacy log call sites migrated.
- `DragonSoul.cpp`: 22 legacy log call sites migrated.
- `MarkManager.cpp`: 20 legacy log call sites migrated.
- `ecs/systems/AffectSystem.cpp`: 20 legacy log call sites migrated.
- `ecs/systems/InventorySystem.cpp`: 20 legacy log call sites migrated.

Batch 5 hot-path decisions:
- `war_map.cpp`: routine member/observer count debug logs moved to `LOG_TRACE`; war lifecycle and errors remain INFO/ERROR.
- `input_auth.cpp`: per-packet auth header trace moved to `LOG_TRACE`; auth lifecycle and invalid access errors remain INFO/ERROR.
- `SessionSystem.cpp`: routine `SAVE`, `SHOW`, and same-sectree debug logs moved to `LOG_TRACE`; warp/disconnect/safebox events remain INFO/ERROR.
- `battle_pass.cpp`: `BattlePassInfo` config detail logs moved to `LOG_TRACE`; config load failures remain ERROR.
- `AffectSystem.cpp`: affect event/save/remove routine debug logs moved to `LOG_TRACE`; invalid affect/load errors remain ERROR.
- `InventorySystem.cpp`: migrated hots were error paths only, so they remain `LOG_ERROR`.

Batch 5 gotchas:
- Several legacy files still contain non-UTF-8 bytes; byte-preserving edits were used where `apply_patch` could not read the file.
- Byte-sized fields were explicitly cast to `int` where old `%d` semantics would otherwise render as characters under fmt.
- `desc.cpp`-style extra-argument issues were also found in this batch:
  - `AffectSystem.cpp` had `Character::AddAffect lDuration == 0 type {}` with an extra argument after conversion; it was normalized to include both duration and type.
- `arena.cpp` build emitted existing pointer-cast warnings around arena info chat; unrelated to logging migration.

Counts after Batch 5:
```text
Before Batch 5:
GameServer sys_log: 407
GameServer sys_err: 723
GameServer _sys_err: 0
Total: 1130

After Batch 5:
GameServer sys_log: 332
GameServer sys_err: 595
GameServer _sys_err: 0
Total: 927

Batch 5 reduction: 203
Phase 16-2 cumulative reduction: 1440
```

Batch 6 candidates, excluding `questlua*.cpp`:
```text
19 ecs/systems/PointSystem.cpp
18 shop.cpp
18 cmd_gm.cpp
17 skill.cpp
17 text_file_loader.cpp
16 ecs/systems/NetworkSyncSystem.cpp
16 ecs/systems/SocialSystem.cpp
14 cmd_general.cpp
14 ecs/systems/MovementSystem.cpp
13 marriage.cpp
13 input_p2p.cpp
13 motion.cpp
12 priv_manager.cpp
12 input.cpp
12 guild_manager.cpp
11 New_PetSystem.cpp
11 wedding.cpp
10 desc_client.cpp
10 mob_manager.cpp
10 MarkImage.cpp
```

Questlua group remaining:
```text
87 questlua_pc.cpp
80 questlua_dungeon.cpp
43 questlua_global.cpp
33 questlua.cpp
20 questlua_marriage.cpp
12 questlua_party.cpp
12 questlua_item.cpp
11 questlua_affect.cpp
10 questlua_guild.cpp
9 questlua_npc.cpp
9 questlua_quest.cpp
6 questlua_target.cpp
4 questlua_game.cpp
4 questlua_building.cpp
4 questlua_horse.cpp
3 questlua_dragonsoul.cpp
2 questlua_pet.cpp
2 questlua_arena.cpp
2 questlua_petnew.cpp
```

Build results:
- Build passed after every Batch 5 migrated file.
- Final successful command used repeatedly:
```powershell
cmake --build build --config RelWithDebInfo --target GameServer --parallel 8
```
- The existing post-build warning remains:
```text
'pwsh.exe' is not recognized as an internal or external command
```

Commit status:
- `901cc13 Phase 16-2: Migrate logging format in arena.cpp`
- `c633398 Phase 16-2: Migrate logging format in war_map.cpp`
- `e524771 Phase 16-2: Migrate logging format in input_auth.cpp`
- `0aa8b65 Phase 16-2: Migrate logging format in SessionSystem.cpp`
- `7896da6 Phase 16-2: Migrate logging format in battle_pass.cpp`
- `06f46a4 Phase 16-2: Migrate logging format in DragonSoul.cpp`
- `6927fb6 Phase 16-2: Migrate logging format in MarkManager.cpp`
- `86129b2 Phase 16-2: Migrate logging format in AffectSystem.cpp`
- `386e873 Phase 16-2: Migrate logging format in InventorySystem.cpp`
- WinTest not run for Batch 5 per phase instruction.

Batch 6 verification:
- Checked all Batch 6 target files for local `sys_err` redirects before migration.
- No local `sys_err` override was found in:
  - `ecs/systems/PointSystem.cpp`
  - `shop.cpp`
  - `cmd_gm.cpp`
  - `skill.cpp`
  - `text_file_loader.cpp`
  - `ecs/systems/NetworkSyncSystem.cpp`
  - `ecs/systems/SocialSystem.cpp`
  - `cmd_general.cpp`
  - `ecs/systems/MovementSystem.cpp`
  - `marriage.cpp`

Batch 6 completed:
- `ecs/systems/PointSystem.cpp`: 19 legacy log call sites migrated.
- `shop.cpp`: 18 legacy log call sites migrated.
- `cmd_gm.cpp`: 18 legacy log call sites migrated.
- `skill.cpp`: 17 legacy log call sites migrated.
- `text_file_loader.cpp`: 17 legacy log call sites migrated.
- `ecs/systems/NetworkSyncSystem.cpp`: 16 legacy log call sites migrated.
- `ecs/systems/SocialSystem.cpp`: 16 legacy log call sites migrated.
- `cmd_general.cpp`: 14 legacy log call sites migrated.
- `ecs/systems/MovementSystem.cpp`: 14 legacy log call sites migrated.
- `marriage.cpp`: 13 legacy log call sites migrated.

Batch 6 hot-path decisions:
- `PointSystem.cpp`: EXP negative clamp and skill-apply debug logs moved to `LOG_TRACE`; level-up remains `LOG_INFO`, errors remain `LOG_ERROR`.
- `skill.cpp`: skill proto dump/detail logs moved to `LOG_TRACE`.
- `text_file_loader.cpp`: optional token lookup misses moved to `LOG_TRACE`; syntax/value failures remain `LOG_ERROR` or `LOG_INFO`.
- `NetworkSyncSystem.cpp`: routine entity insert/remove, sync owner, sync release, remove/update packet logs moved to `LOG_TRACE`; BGM lifecycle logs remain `LOG_INFO`.
- `MovementSystem.cpp`: NPC goto, per-position detail, save event, and recovery event logs moved to `LOG_TRACE`; sectree mismatch and movement errors remain INFO/ERROR.
- `marriage.cpp`: periodic near-check logging moved to `LOG_TRACE`; marriage/wedding lifecycle and errors remain INFO/ERROR.

Batch 6 gotchas:
- `skill.cpp`: old `%-3d` printf formatting was normalized to fmt left alignment `{:<3}`.
- `MovementSystem.cpp`: `SECTREEID` coordinate fields required local value copies before fmt/spdlog formatting to avoid reference binding issues.
- `marriage.cpp`: a legacy `NearCheck` log had one extra argument not represented by the old format string; the fmt conversion now includes the missing marriage point placeholder.
- `cmd_general.cpp`: two malformed `LOG_ERROR(0, "...%s")` conversions were normalized to `LOG_ERROR("...{}", ...)`.

Counts after Batch 6:
```text
Before Batch 6:
GameServer sys_log: 332
GameServer sys_err: 595
GameServer _sys_err: 0
Total: 927

After Batch 6:
GameServer sys_log: 245
GameServer sys_err: 520
GameServer _sys_err: 0
Total: 765

Batch 6 reduction: 162
Phase 16-2 cumulative reduction: 1602
```

Batch 7 candidates, excluding `questlua*.cpp`:
```text
13 input_p2p.cpp
13 motion.cpp
12 input.cpp
12 guild_manager.cpp
12 priv_manager.cpp
11 New_PetSystem.cpp
11 wedding.cpp
10 desc_client.cpp
10 target.cpp
10 mob_manager.cpp
10 MarkImage.cpp
10 safebox.cpp
10 regen.cpp
10 MountSystem.cpp
9 pvp.cpp
```

Remaining `questlua*.cpp` group:
```text
87 questlua_pc.cpp
80 questlua_dungeon.cpp
43 questlua_global.cpp
33 questlua.cpp
20 questlua_marriage.cpp
12 questlua_party.cpp
12 questlua_item.cpp
11 questlua_affect.cpp
10 questlua_guild.cpp
9 questlua_npc.cpp
9 questlua_quest.cpp
6 questlua_target.cpp
4 questlua_game.cpp
4 questlua_building.cpp
4 questlua_horse.cpp
3 questlua_dragonsoul.cpp
2 questlua_pet.cpp
2 questlua_arena.cpp
2 questlua_petnew.cpp
0 questlua_oxevent.cpp
0 questlua_ba.cpp
0 questlua_danceevent.cpp
```

Build results:
- Build passed after every Batch 6 migrated file before commit.
- `MovementSystem.cpp` initially failed on one fmt/spdlog argument binding issue; local coordinate copies fixed it, then the build passed.
- Final successful command used repeatedly:
```powershell
cmake --build build --config RelWithDebInfo --target GameServer --parallel 8
```
- Existing unrelated warnings remain:
  - `MovementSystem.cpp`: `CHARACTER::GetPosition` / `GetRotation` not all paths return a value.
  - Post-build environment warning: `'pwsh.exe' is not recognized as an internal or external command`.

Commit status:
- `63e6bed Phase 16-2: Migrate logging format in PointSystem.cpp`
- `3e093a7 Phase 16-2: Migrate logging format in shop.cpp`
- `894275a Phase 16-2: Migrate logging format in cmd_gm.cpp`
- `b896439 Phase 16-2: Migrate logging format in skill.cpp`
- `ad0a242 Phase 16-2: Migrate logging format in text_file_loader.cpp`
- `81e64ba Phase 16-2: Migrate logging format in NetworkSyncSystem.cpp`
- `d8ea3b2 Phase 16-2: Migrate logging format in SocialSystem.cpp`
- `f4c6f18 Phase 16-2: Migrate logging format in cmd_general.cpp`
- `a47d775 Phase 16-2: Migrate logging format in MovementSystem.cpp`
- `d3d2895 Phase 16-2: Migrate logging format in marriage.cpp`
- WinTest not run for Batch 6 per phase instruction.

Batch 7 verification:
- Checked all Batch 7 target files for local `sys_err` redirects before migration.
- No local `sys_err` override was found in:
  - `input_p2p.cpp`
  - `motion.cpp`
  - `input.cpp`
  - `guild_manager.cpp`
  - `priv_manager.cpp`
  - `New_PetSystem.cpp`
  - `wedding.cpp`
  - `desc_client.cpp`
  - `target.cpp`
  - `mob_manager.cpp`
  - `MarkImage.cpp`
  - `safebox.cpp`
  - `regen.cpp`
  - `MountSystem.cpp`
  - `pvp.cpp`
- `MountSystem.cpp` was resolved to the root `SRC/Server/GameServer/MountSystem.cpp` because that file held the listed 10 call sites. `ecs/systems/MountSystem.cpp` still has 4 remaining calls and is a Batch 8/tail candidate.

Batch 7 completed:
- `input_p2p.cpp`: 13 legacy log call sites migrated.
- `motion.cpp`: 13 legacy log call sites migrated.
- `input.cpp`: 12 legacy log call sites migrated.
- `guild_manager.cpp`: 12 legacy log call sites migrated.
- `priv_manager.cpp`: 12 legacy log call sites migrated.
- `New_PetSystem.cpp`: 11 legacy log call sites migrated.
- `wedding.cpp`: 11 legacy log call sites migrated.
- `desc_client.cpp`: 10 legacy log call sites migrated.
- `target.cpp`: 10 legacy log call sites migrated.
- `mob_manager.cpp`: 10 legacy log call sites migrated.
- `MarkImage.cpp`: 10 legacy log call sites migrated.
- `safebox.cpp`: 10 legacy log call sites migrated.
- `regen.cpp`: 10 legacy log call sites migrated.
- `MountSystem.cpp`: 10 legacy log call sites migrated.
- `pvp.cpp`: 9 legacy log call sites migrated.

Batch 7 hot-path decisions:
- `input_p2p.cpp`: relay/analyze packet diagnostics moved to `LOG_TRACE`; P2P setup, messenger, guild, block-chat, and shutdown events remain INFO/ERROR.
- `motion.cpp`: existing motion duration/detail diagnostics stay `LOG_TRACE`; motion parse/load failures remain `LOG_ERROR`.
- `input.cpp`: packet analyze and PONG debug logs moved to `LOG_TRACE`; handshake/key agreement failures are `LOG_ERROR`.
- `target.cpp`: target update/create/replace debug logs moved to `LOG_TRACE`; target event null/error paths remain `LOG_ERROR`.
- `mob_manager.cpp`: mob proto and group dump logs moved to `LOG_TRACE`; syntax/load errors remain `LOG_ERROR`.
- `desc_client.cpp`: high-frequency `DB_PACKET` logging moved to `LOG_TRACE`; connection/phase lifecycle remains `LOG_INFO`.
- `pvp.cpp`: mount-can-attack test-server detail and PVP list send detail moved to `LOG_TRACE`; duel/PVP lifecycle remains INFO/ERROR.

Batch 7 gotchas:
- `New_PetSystem.cpp` and `MountSystem.cpp`: legacy pointer `%x` logging was normalized to pointer-safe `{}` with `static_cast<const void*>`.
- `safebox.cpp`: two legacy `sys_err("...%s %d", count)` calls had two printf placeholders but only one argument. The fmt migration fixed them to a single `{}` placeholder for `count`.
- `mob_manager.cpp`: the mechanical converter initially put an `#ifdef ENABLE_MULTI_NAMES` branch inside a variadic logging macro argument list. This was replaced with a local `const char* mobName` selected before the `LOG_TRACE` call.
- `mob_manager.cpp`: legacy `%-5d` formatting was normalized to fmt left alignment `{:<5}`.
- `input.cpp`: byte/header values that used old `%d/%u` semantics are explicitly cast to `int` where needed.
- `priv_manager.cpp`: `uint8_t` privilege and empire fields are explicitly cast to `int` to preserve numeric output under fmt.
- `guild_manager.cpp`: the byte-sized guild war result type is explicitly cast to `int`.

Counts after Batch 7:
```text
Before Batch 7:
GameServer sys_log: 245
GameServer sys_err: 520
GameServer _sys_err: 0
Total: 765

After Batch 7:
GameServer sys_log: 177
GameServer sys_err: 425
GameServer _sys_err: 0
Total: 602

Batch 7 reduction: 163
Phase 16-2 cumulative reduction: 1765
```

Batch 8 top non-questlua candidates:
```text
9 horse_rider.cpp
9 gm.cpp
9 battle.cpp
9 PetSystem.cpp
9 p2p.cpp
8 MarkConvert.cpp
8 input_udp.cpp
7 item_manager_idrange.cpp
7 cuberenewal.cpp
7 desc_manager.cpp
7 new_switchbot.cpp
6 new_offlineshop_manager.cpp
6 sectree.cpp
6 mining.cpp
6 shopEx.cpp
5 hwidmanager.cpp
5 desc_p2p.cpp
5 map_location.cpp
5 fishing.cpp
5 exchange.cpp
5 guild_renewal.cpp
5 attr_transfer.cpp
5 config.cpp
5 sectree.h
4 ecs/systems/MountSystem.cpp
4 questevent.cpp
4 messenger_manager.cpp
4 locale_service.cpp
4 cmd.cpp
4 ecs/systems/GayaSystem.cpp
```

Batch 8 recommendation:
- Use strategy B, bulk closeout for all remaining non-questlua files, but still keep the Batch 7 discipline: migrate one file, build, commit, continue.
- Reason: remaining non-questlua files are now mostly 1-9 calls each, so splitting into several more fixed-size batches adds overhead without reducing per-file risk.
- Keep `questlua*.cpp` deferred to the dedicated QuestError modernization pass because `questlua.cpp` and related Lua bindings need special handling around quest error routing.

Remaining `questlua*.cpp` group:
```text
87 questlua_pc.cpp
80 questlua_dungeon.cpp
43 questlua_global.cpp
33 questlua.cpp
20 questlua_marriage.cpp
12 questlua_party.cpp
12 questlua_item.cpp
11 questlua_affect.cpp
10 questlua_guild.cpp
9 questlua_npc.cpp
9 questlua_quest.cpp
6 questlua_target.cpp
4 questlua_game.cpp
4 questlua_building.cpp
4 questlua_horse.cpp
3 questlua_dragonsoul.cpp
2 questlua_pet.cpp
2 questlua_arena.cpp
2 questlua_petnew.cpp
0 questlua_oxevent.cpp
0 questlua_ba.cpp
0 questlua_danceevent.cpp
```

Build results:
- Build passed after every Batch 7 migrated file before commit.
- `mob_manager.cpp` initially failed because a preprocessor branch was inside a logging macro argument list; local `mobName` selection fixed it, then the build passed.
- Final successful command used repeatedly:
```powershell
cmake --build build --config RelWithDebInfo --target GameServer --parallel 8
```
- Existing post-build environment warning remains in incremental builds:
```text
'pwsh.exe' is not recognized as an internal or external command
```

Commit status:
- `f49d85c Phase 16-2: Migrate logging format in input_p2p.cpp`
- `6d67065 Phase 16-2: Migrate logging format in motion.cpp`
- `1007fbf Phase 16-2: Migrate logging format in input.cpp`
- `dbf5c68 Phase 16-2: Migrate logging format in guild_manager.cpp`
- `82bf9ff Phase 16-2: Migrate logging format in priv_manager.cpp`
- `c1835b2 Phase 16-2: Migrate logging format in New_PetSystem.cpp`
- `2976b37 Phase 16-2: Migrate logging format in wedding.cpp`
- `36b0da2 Phase 16-2: Migrate logging format in desc_client.cpp`
- `2f579a4 Phase 16-2: Migrate logging format in target.cpp`
- `f6dc2e2 Phase 16-2: Migrate logging format in mob_manager.cpp`
- `a5b42b9 Phase 16-2: Migrate logging format in MarkImage.cpp`
- `cc4896b Phase 16-2: Migrate logging format in safebox.cpp`
- `79f8a4c Phase 16-2: Migrate logging format in regen.cpp`
- `dbadf5f Phase 16-2: Migrate logging format in MountSystem.cpp`
- `caaeb73 Phase 16-2: Migrate logging format in pvp.cpp`
- WinTest not run for Batch 7 per phase instruction.

Batch 8 bulk closeout completed:
- Strategy B executed: all remaining non-`questlua` files were migrated with file-by-file build and commit discipline.
- Initial Batch 8 non-`questlua` scope: 68 files, 245 legacy `sys_log` / `sys_err` call sites.
- Final non-`questlua` scope: 0 legacy `sys_log` / `sys_err` call sites.
- `questlua*.cpp` remains deferred to the dedicated QuestError modernization pass.
- `log.cpp` remains intentionally outside this call-site pass because it is legacy logging infrastructure / compatibility scope.

Batch 8 migrated groups:
```text
8a: battle.cpp, gm.cpp, horse_rider.cpp, p2p.cpp, PetSystem.cpp,
    input_udp.cpp, MarkConvert.cpp, cuberenewal.cpp, desc_manager.cpp,
    item_manager_idrange.cpp, new_switchbot.cpp, mining.cpp,
    new_offlineshop_manager.cpp, sectree.cpp, shopEx.cpp.

8b: attr_transfer.cpp, config.cpp, desc_p2p.cpp, exchange.cpp,
    fishing.cpp, guild_renewal.cpp, hwidmanager.cpp, map_location.cpp,
    sectree.h, cmd.cpp, GayaSystem.cpp, ecs/systems/MountSystem.cpp,
    locale_service.cpp, messenger_manager.cpp, questevent.cpp,
    blend_item.cpp, cmd_emotion.cpp, group_text_parse_tree.cpp,
    login_data.cpp, LostCastleDungeon.cpp, new_offlineshop.cpp,
    OXEvent.cpp, refine.cpp, wheel_of_destiny.cpp.

8c tail: CombatSystem.cpp, StatSystem.cpp, event.cpp,
    horsename_manager.cpp, party.h, profiler.h, utils.cpp, ani.cpp,
    belt_inventory_helper.h, buff_on_attributes.cpp, cmd_general2.cpp,
    desc.h, ActivitySystem.cpp, AISystem.cpp, ChatSystem.cpp,
    DragonSoulSystem.cpp, VitalRegenSystem.cpp, entity_view.cpp,
    ip_ban.cpp, item_addon.cpp, login_sim.h, new_offlineshop.h,
    polymorph.cpp, protocol.h, questnpc.h, sectree_manager.h,
    spam.h, trigger.cpp, whisper_admin.cpp.
```

Batch 8 gotchas:
- `battle.cpp`: one legacy `battle_hit` log had more printf placeholders than arguments; the migration corrected the format rather than preserving the bad placeholder.
- `PetSystem.cpp`: pointer-style `0x%x` output was normalized to pointer-safe fmt output using `static_cast<const void*>`; `entt::entity` output was normalized with `entt::to_integral`.
- `input_udp.cpp`: disabled block-comment examples still contained legacy names after the first migration; a follow-up commit converted those to `LOG_ERROR` so grep verification is clean.
- `sectree.cpp`: coordinate fields required local value copies before fmt logging to avoid reference binding issues.
- `fishing.cpp` and `profiler.h`: printf left-alignment (`%-24s`, `%-10s`) must become fmt left-alignment (`{:<24}`, `{:<10}`), not `{:-24}` / `{:-10}`.
- `sectree.h`: adding logging macros through a broad header exposed a `dev_log.h` macro collision; `dev_log.h` now guards its `LOG_WARN` / `LOG_INFO` definitions when the modern logging macros already exist.

Counts after Batch 8:
```text
Before Batch 8:
GameServer sys_log: 177
GameServer sys_err: 425
GameServer _sys_err: 0
Total: 602
Non-questlua remaining: 245

After Batch 8:
GameServer sys_log: 56
GameServer sys_err: 301
GameServer _sys_err: 0
Total: 357
Non-questlua remaining: 0
Questlua deferred group: 353
Legacy logging infrastructure remaining: log.cpp = 4

Batch 8 reduction: 245
Phase 16-2 cumulative reduction: 2010
```

Remaining deferred QuestError group:
```text
questlua.cpp: 33
questlua_affect.cpp: 11
questlua_arena.cpp: 2
questlua_building.cpp: 4
questlua_dragonsoul.cpp: 3
questlua_dungeon.cpp: 80
questlua_game.cpp: 4
questlua_global.cpp: 43
questlua_guild.cpp: 10
questlua_horse.cpp: 4
questlua_item.cpp: 12
questlua_marriage.cpp: 20
questlua_npc.cpp: 9
questlua_party.cpp: 12
questlua_pc.cpp: 87
questlua_pet.cpp: 2
questlua_petnew.cpp: 2
questlua_quest.cpp: 9
questlua_target.cpp: 6
```

Build results:
- Build passed after each Batch 8 migrated file before commit.
- Final full rebuild initially exposed the `profiler.h` fmt alignment issue; fixed in `83954b7 Phase 16-2: Fix profiler fmt alignment`.
- Final successful command:
```powershell
cmake --build build --config RelWithDebInfo --target GameServer --parallel 8
```
- Existing post-build environment warning remains:
```text
'pwsh.exe' is not recognized as an internal or external command
```

Commit status:
- Per-file Batch 8 migration commits from `b32c176 Phase 16-2: Migrate logging format in battle.cpp` through `ddac22d Phase 16-2: Migrate logging format in whisper_admin.cpp`.
- Follow-up fix: `83954b7 Phase 16-2: Fix profiler fmt alignment`.
- WinTest not run for Batch 8 per phase instruction; operator checkpoint remains pending.

## Phase 16-3 - QuestError Modernization

Mode:
- Modernize questlua logging without changing quest behavior.
- Preserve `__FUNCTION__` / `__LINE__` capture.
- Keep questlua `sys_err(...)` call-site name as the local QuestError macro, but convert its format strings to fmt-style.
- `sys_log(...)` call sites in questlua files were migrated to `LOG_INFO(...)`.

Audit:
- `QuestError` implementation was in `SRC/Server/GameServer/questmanager.cpp`.
- Old signature:
```cpp
void CQuestManager::QuestError(const char* func, int line, const char* fmt, ...);
```
- Existing questlua redirect pattern was per-file:
```cpp
#undef sys_err
#ifndef _WIN32
#define sys_err(fmt, args...) quest::CQuestManager::instance().QuestError(__FUNCTION__, __LINE__, fmt, ##args)
#else
#define sys_err(fmt, ...) quest::CQuestManager::instance().QuestError(__FUNCTION__, __LINE__, fmt, __VA_ARGS__)
#endif
```
- Several `questlua_*.cpp` files did not have the redirect before this phase and were still using the Core `_sys_err` bridge. Those files now get the same quest-local redirect.

QuestError changes:
- Added `QuestErrorImpl(const char* func, int line, const std::string& msg)`.
- Added type-safe fmt entry point:
```cpp
template <typename... Args>
void QuestErrorFmt(const char* func, int line, fmt::format_string<Args...> fmt, Args&&... args);
```
- Kept old varargs `QuestError(...)` as an internal transition bridge.
- `QuestErrorImpl` now logs with function and line context:
```text
[QUEST function:line] message
```
- Test-server chat behavior is preserved:
  - `error occurred on [func:line]`
  - formatted quest error message

Macro decision:
- The first attempt used `__VA_OPT__`, but this project is not compiling with MSVC `/Zc:preprocessor`, so MSVC rejected it.
- Final macro keeps the previous platform-compatible variadic style:
```cpp
#undef sys_err
#ifndef _WIN32
#define sys_err(fmt, args...) quest::CQuestManager::instance().QuestErrorFmt(__FUNCTION__, __LINE__, FMT_STRING(fmt), ##args)
#else
#define sys_err(fmt, ...) quest::CQuestManager::instance().QuestErrorFmt(__FUNCTION__, __LINE__, FMT_STRING(fmt), __VA_ARGS__)
#endif
```

Migrated files:
```text
questlua_pc.cpp
questlua_dungeon.cpp
questlua_global.cpp
questlua.cpp
questlua_marriage.cpp
questlua_party.cpp
questlua_item.cpp
questlua_affect.cpp
questlua_guild.cpp
questlua_npc.cpp
questlua_quest.cpp
questlua_target.cpp
questlua_game.cpp
questlua_building.cpp
questlua_horse.cpp
questlua_dragonsoul.cpp
questlua_pet.cpp
questlua_arena.cpp
questlua_petnew.cpp
```

Gotchas:
- `QuestError` template overload plus old varargs overload caused MSVC overload ambiguity. Fixed by naming the type-safe entry point `QuestErrorFmt` and leaving `QuestError` as the explicit legacy bridge.
- `questlua.cpp` had a dynamic printf format built in a local buffer:
```cpp
snprintf(buf, sizeof(buf), "LUA ScriptRunError (code:%%d src:[%%%ds])", size);
sys_err(buf, errcode, code);
```
  This was replaced with a static fmt string and bounded string construction:
```cpp
sys_err("LUA ScriptRunError (code:{} src:[{}])", errcode, std::string(code, size));
```
- The raw grep count for `sys_err(` remains non-zero by design because the questlua local macro name is intentionally preserved. These are no longer printf-style Core `_sys_err` calls; they route to `QuestErrorFmt`.

Counts after Phase 16-3:
```text
GameServer sys_log: 0
GameServer _sys_err: 0
GameServer raw sys_err: 304

Meaning of raw sys_err:
- questlua fmt-style QuestError macro call sites
- questlua per-file macro definitions
```

Core bridge removal decision:
- The Phase 16-3 prompt requested removal of the printf-style `sys_log/sys_err` compatibility bridge after GameServer migration.
- This is not safe yet across the full source tree because non-GameServer modules still have legacy printf-style calls.
- Fresh tree-wide audit after questlua migration:
```text
SRC/Server sys_log: 462
SRC/Server sys_err: 506
SRC/Server _sys_err: 5
Non-GameServer legacy log references: 667
```
- Therefore the Core compatibility bridge remains for now.
- Recommended next phase before bridge removal:
  - Phase 16-4: Core + Database logging modernization.
  - Then remove the varargs bridge once `SRC/Server/Core` and `SRC/Server/Database` are fmt-clean.

Build results:
- Build passed after `QuestError` internals were modernized.
- Build passed after every migrated questlua file.
- Final successful command:
```powershell
cmake --build build --config RelWithDebInfo --target GameServer --parallel 8
```

Commit status:
- `3b0371b Phase 16-3.1: Modernize QuestError internals`
- `43f1f81 Phase 16-3: Migrate quest format in questlua_pc.cpp`
- `5e4971c Phase 16-3: Migrate quest format in questlua_dungeon.cpp`
- `a6a6db7 Phase 16-3: Migrate quest format in questlua_global.cpp`
- `4066b8a Phase 16-3: Migrate quest format in questlua.cpp`
- `4bd5a13 Phase 16-3: Migrate quest format in questlua_marriage.cpp`
- `b18078f Phase 16-3: Migrate quest format in questlua_party.cpp`
- `0f8096d Phase 16-3: Migrate quest format in questlua_item.cpp`
- `28b9e5b Phase 16-3: Migrate quest format in questlua_affect.cpp`
- `1aeb160 Phase 16-3: Migrate quest format in questlua_guild.cpp`
- `1a899d3 Phase 16-3: Migrate quest format in questlua_npc.cpp`
- `0912633 Phase 16-3: Migrate quest format in questlua_quest.cpp`
- `d17f2b5 Phase 16-3: Migrate quest format in questlua_target.cpp`
- `801878d Phase 16-3: Migrate quest format in questlua_game.cpp`
- `6ff5f0f Phase 16-3: Migrate quest format in questlua_building.cpp`
- `f5439c7 Phase 16-3: Migrate quest format in questlua_horse.cpp`
- `f825f5c Phase 16-3: Migrate quest format in questlua_dragonsoul.cpp`
- `8cec65d Phase 16-3: Migrate quest format in questlua_pet.cpp`
- `8a93047 Phase 16-3: Migrate quest format in questlua_arena.cpp`
- `d57b358 Phase 16-3: Migrate quest format in questlua_petnew.cpp`
- `f63e1a9 Phase 16-3: Migrate GameServer log.cpp legacy logging`
- WinTest not run in this environment; operator WinTest remains mandatory before marking COMPLETE.

## Phase 15E-55 - AffectSystem::Add / Remove Replaces CHARACTER Affect Calls

Mode:
- Entity-first affect add/remove caller migration.
- No `LPCHARACTER` overload added.
- Affect storage and duration ticking unchanged.

Audit:
- Initial tree-wide raw `->AddAffect` matches: 95.
- Initial tree-wide raw `->RemoveAffect` matches: 115.
- Top caller files were `questlua_pc.cpp`, `cmd_general2.cpp`, `DragonSoulSystem.cpp`, `SkillSystem.cpp`, `ItemSystem_LegacyBridge.cpp`, `MountSystem.cpp`, `questlua_affect.cpp`, `New_PetSystem.cpp`, guild/PvP paths, and login/GM paths.

Completed batches:
- Verified existing entity-first API in `AffectSystem.hpp`:
  - `AffectSystem::AddAffect(entt::entity, ...)`
  - `AffectSystem::RemoveAffect(entt::entity, uint32_t)`
- Added missing entity-first overload:
  - `AffectSystem::RemoveAffect(entt::entity, CAffect*)`
  - This preserves legacy `RemoveAffect(CAffect*)` call semantics without exposing `LPCHARACTER`.
- Migrated caller-side `ch->AddAffect(...)` and `ch->RemoveAffect(...)` across quest, command, mount/pet, DragonSoul, skill, PvP, guild, login, dungeon, battle-pass, and item bridge paths.
- Commit: `205fa9f Phase 15E-55: Migrate Affect caller accessors`

Counts:
```text
Initial AddAffect raw matches: 95
Initial RemoveAffect raw matches: 115
Final tree-wide raw matches: 9
Final caller-side matches excluding AffectSystem.cpp: 0
```

Remaining intentional sites:
- `ecs/systems/AffectSystem.cpp`: 9 direct calls remain inside the affect bridge and legacy `CHARACTER` affect method bodies.
- These are the internal bridge boundary used by the entity-first API.

Build results:
- First migration build produced fewer than 20 errors; all were `RemoveAffect(CAffect*)` overload mismatches plus one non-simple questlua expression.
- Added the entity-first `CAffect*` overload and fixed the questlua expression.
- Build passed after fixes.
- Final successful command:
```powershell
cmake --build build --config RelWithDebInfo --target GameServer --parallel 8
```

Manual WinTest checklist:
- Potion/buff applies and expires.
- Manual affect removal works.
- DragonSoul activate/deactivate applies/removes affects.
- Mount/dismount affects apply/remove.
- Skill affects apply/remove.
- Death affect cleanup behavior remains correct.
- Logout/login preserves infinite-duration affects.
- `VID_DRIFT` remains zero.

Commit status:
- Code batch committed.
- WinTest not run in this environment.

## Phase 15E-56 - GetDesc Accessor Migration

Mode:
- Entity-first `GetDesc` accessor migration.
- `DESC*` / `LPDESC` remains an infrastructure pointer and was not converted to an entity.
- No `LPCHARACTER` overload added.

Audit:
- Initial raw `->GetDesc` matches: 455.
- Top caller files were `input_main.cpp`, `new_offlineshop_manager.cpp`, `party.cpp`, `guild.cpp`, `cmd_general.cpp`, `questlua_pc.cpp`, `CombatSystem.cpp`, `ItemSystem_LegacyBridge.cpp`, `NetworkSyncSystem.cpp`, and `log.cpp`.
- Common patterns were packet send, buffered packet send, DB packet handle lookup, account/login lookup, disconnect/phase checks, and language lookup.

Completed batches:
- Added `ecs::PlayerRuntime::GetDesc(entt::entity)` returning `LPDESC`.
- Implementation bridges through `ecs::LegacyCharOf(e)` and returns `nullptr` for invalid/null entities.
- Commit: `f19f415 Phase 15E-56.1: Add PlayerRuntime GetDesc API`
- Migrated caller-side `ch->GetDesc()` usage across 71 files.
- Labeled `LPENTITY->GetDesc()` paths as infrastructure and left them unchanged.
- Commit: `3b155f9 Phase 15E-56: Migrate GetDesc caller accessors`

Rollback note:
- A first broad sweep was rolled back because it exceeded the 20-error safety gate.
- Root cause: the naive replacement hit `LPENTITY` variables and member chains such as `it->second.pCharacter->GetDesc()`, and also inserted a wrong include path in `ecs/EntityFactory.cpp`.
- The second pass used narrower standalone identifier matching plus targeted member-chain fixes for `party.cpp`, `guild.cpp`, quest current-character chains, owner chains, and PlayerRuntime helper internals.

Counts:
```text
Initial raw ->GetDesc matches: 455
Final raw ->GetDesc matches: 14
Final caller-side CHARACTER matches excluding PlayerRuntime bridge and LPENTITY infrastructure paths: 0
```

Remaining intentional/non-target sites:
- `ecs/systems/PlayerRuntimeSystem.cpp`: 1 bridge call to legacy `ch->GetDesc()`.
- `ecs/systems/NetworkSyncSystem.cpp`: 9 `LPENTITY->GetDesc()` infrastructure/entity-base calls.
- `building.cpp`: 2 `LPENTITY->GetDesc()` calls.
- `entity.cpp`: 2 `LPENTITY->GetDesc()` calls.
- These are not `CHARACTER::GetDesc` caller accessors and should be handled in a later `LPENTITY`/network infrastructure phase if needed.

Build results:
- Build passed after adding the API.
- Build passed after the narrowed caller migration and targeted fixes.
- Final successful command:
```powershell
cmake --build build --config RelWithDebInfo --target GameServer --parallel 8
```

Manual WinTest checklist:
- Login establishes a valid DESC.
- Chat sends packets correctly.
- Movement/combat packet send paths work.
- Shop/offlineshop windows send packet streams correctly.
- Quest packets and target packets reach the client.
- Logout/disconnect cleanup clears DESC safely.
- Re-login creates a fresh DESC.
- `VID_DRIFT` remains zero.

Commit status:
- API and caller migration committed.
- WinTest not run in this environment.

## Phase 15E-54 - Party/Guild Accessor Migration

Mode:
- Entity-first social accessor migration.
- `LPPARTY` / `CGuild*` remain service-object return types.
- No `LPCHARACTER` overload added.
- No `CParty` / `CGuild` internal signature rewrite.

Completed batches:
- STEP 1 audit:
  - Initial raw caller-side scan found 247 `->GetParty` / `->GetGuild` matches.
  - Split: `GetParty=164`, `GetGuild=83`.
  - Top caller files included `questlua_party.cpp`, `input_main.cpp`, `CombatSystem.cpp`, `cmd_general.cpp`, `war_map.cpp`, and `cmd_gm.cpp`.
- STEP 2 SocialSystem API:
  - Added `ecs::SocialSystem::GetParty(entt::entity)` returning `LPPARTY`.
  - Added `ecs::SocialSystem::GetGuild(entt::entity)` returning `CGuild*`.
  - Implementation bridges through `ecs::LegacyCharOf(e)` and returns `nullptr` for invalid/null entities.
  - Commit: `060dc33 Phase 15E-54.1: Add SocialSystem party guild accessors`
- STEP 3 caller migration:
  - Migrated caller-side `ch->GetParty()` / `ch->GetGuild()` accessors across gameplay, quest, dungeon, combat, guild, PvP, war-map, and command paths.
  - Caller-side residual count excluding bridge/internal deferrals is now zero.
  - Commit: `37d4906 Phase 15E-54: Migrate Party Guild caller accessors`

Counts:
```text
Initial raw ->GetParty / ->GetGuild matches: 247
Initial GetParty matches: 164
Initial GetGuild matches: 83
Final tree-wide raw matches: 14
Final GetParty matches: 12
Final GetGuild matches: 2
Final caller-side matches excluding SocialSystem.cpp + party.h: 0
```

Remaining intentional/deferred sites:
- `ecs/systems/SocialSystem.cpp`: 11 direct calls remain inside the bridge and legacy `CHARACTER` social method bodies.
- `party.h`: 3 direct calls remain inside inline `FPartyDropDiceRoll` helper.
- `party.h` migration was attempted and rolled back because converting the inline header helper caused a broad include cascade with more than 20 build errors. This should be handled in a dedicated header-boundary phase, either by moving the helper body out of the header or adding a narrow non-header bridge.

Build results:
- Build passed after adding the SocialSystem API.
- Build initially failed after the broad caller sweep due `party.h` include cascade and one duplicated `ch` declaration in `questlua_party.cpp`.
- `party.h` was restored, the `questlua_party.cpp` duplicate declaration was fixed, and the build passed.
- Final successful command:
```powershell
cmake --build build --config RelWithDebInfo --target GameServer --parallel 8
```

Manual WinTest checklist:
- Create party, invite member, verify leader/member state.
- Party EXP share on kill.
- Leave/disband party returns null party state.
- Guild member login, guild chat, guild master commands.
- Guild war / cross-guild combat.
- Verify `VID_DRIFT` remains zero.

Commit status:
- API and caller migration committed.
- WinTest not run in this environment.

## Phase 15E-53 - QuestSystem::GetFlag / SetFlag Replaces CHARACTER Quest Calls

Mode:
- Entity-first public quest flag routing.
- Quest flag storage authority and DB save/load paths unchanged.
- No `LPCHARACTER` overload exposed in `QuestSystem`.

Completed batches:
- Added `ecs::QuestSystem::GetFlag(entt::entity, const std::string&)` and `ecs::QuestSystem::SetFlag(entt::entity, const std::string&, int32_t)`.
  - Implementation resolves the legacy character through the ECS bridge and delegates to existing `CHARACTER::GetQuestFlag` / `SetQuestFlag`.
  - Commit: `0a3fc7d Phase 15E-53.1: Add QuestSystem flag API`
- Migrated all caller-side `->GetQuestFlag` / `->SetQuestFlag` call sites.
  - Main affected groups: dungeon entry/rejoin cooldowns, command/GM toggles, questlua helpers, PvP duel flags, login restore flags, refine/costume options, Gaya/skill/player runtime internals, and legacy item bridge flag reads.
  - Commit: `7267ab4 Phase 15E-53: Migrate QuestFlag caller paths`
- Cleaned stale commented `tch->SetQuestFlag` examples in `questlua_dungeon.cpp` so required caller-side grep stays clean.

Counts:
```text
Caller-side quest flag audit before 15E-53: 302
  ->SetQuestFlag: 181
  ->GetQuestFlag: 121
Caller-side quest flag count after 15E-53: 0
Tree-wide ->GetQuestFlag/->SetQuestFlag after 15E-53:
  2, both inside QuestSystem.cpp bridge
```

Declaration status:
- `CHARACTER::GetQuestFlag` and `CHARACTER::SetQuestFlag` declarations were retained.
- Reason: internal legacy member method bodies still call `GetQuestFlag(...)` / `SetQuestFlag(...)` without `->`, and `QuestSystem` intentionally delegates to these methods to preserve existing quest flag semantics and DB behavior.
- Removing or privatizing the declarations needs a dedicated internal member-body migration or private bridge phase.

Build results:
- Initial full caller migration build failed because two LF-only files missed the new include (`LostCastleDungeon.cpp`, `war_map.cpp`).
- Includes were fixed without rollback; the next build passed.
- Final successful command:
```powershell
cmake --build build --config RelWithDebInfo --target GameServer --parallel 8
```

Manual WinTest checklist:
- Login loads quest flags correctly.
- Dungeon rejoin/cooldown flags persist and reset correctly.
- Biologist and questlua flag flows progress correctly.
- PvP duel flags reset after duel.
- Guild/war flags behave correctly.
- Costume/rune/hide option flags persist after relog.
- Quest reward and completion flag updates persist after relog.
- `VID_DRIFT` remains zero.

Commit status:
- API and caller migration committed.
- WinTest not run in this environment.

## Phase 15E-52 - PointSystem::Change Replaces CHARACTER::PointChange

Mode:
- Entity-first public point mutation routing.
- Internal `CHARACTER::PointChange` semantics preserved unchanged.
- No `points[]` storage migration attempted.

Completed batches:
- Added `ecs::PointSystem::Change(entt::entity, uint8_t, int64_t, ...)`.
  - Implementation resolves the legacy character through the ECS bridge and delegates to existing `CHARACTER::PointChange`.
  - Commit: `9997cf3 Phase 15E-52.1: Add PointSystem Change entity API`
- Migrated quest and command paths.
  - `questlua_pc.cpp`, `cmd_general.cpp`, and `cmd_gm.cpp`.
  - Commit: `46b6710 Phase 15E-52: Migrate PointChange in questlua and commands`
- Migrated combat path.
  - `CombatSystem.cpp`.
  - Commit: `5fca36f Phase 15E-52: Migrate PointChange in CombatSystem`
- Migrated party/guild/arena/offlineshop paths.
  - `party.cpp`, `guild.cpp`, `arena.cpp`, and `new_offlineshop_manager.cpp`.
  - Commit: `5efc839 Phase 15E-52: Migrate PointChange in party guild arena shop`
- Migrated input and command paths.
  - `input_login.cpp`, `input_main.cpp`, and `cmd_general2.cpp`.
  - Commit: `7773140 Phase 15E-52: Migrate PointChange in input and command paths`
- Migrated DragonSoul and mount point mutation paths.
  - `DragonSoul.cpp` refine fees and `MountSystem.cpp` stat refresh.
  - Commit: `469936a Phase 15E-52: Migrate PointChange in DragonSoul and mount`
- Migrated ECS stat/movement/gaya point mutation paths.
  - `MovementSystem.cpp`, `GayaSystem.cpp`, and `StatSystem.cpp`.
  - Commit: `37a3c48 Phase 15E-52: Migrate PointChange in ECS stat movement gaya`
- Migrated remaining small gameplay paths.
  - `ItemUse.cpp`, `New_PetSystem.cpp`, `VikingDungeon.cpp`, `cuberenewal.cpp`, `exchange.cpp`, `guild_renewal.cpp`, `pvp.cpp`, `questlua_npc.cpp`, `questlua_party.cpp`, `questpc.cpp`, `shop.cpp`, `shopEx.cpp`, `shop_manager.cpp`, and `wheel_of_destiny.cpp`.
  - Commit: `cbf1043 Phase 15E-52: Migrate PointChange in remaining small gameplay paths`
- Migrated SkillSystem and item bridge caller-side paths.
  - `SkillSystem.cpp` skill absorb/victim point effects.
  - `ItemSystem_LegacyBridge.cpp` party gold distribution and mount stat refresh.
  - Commit: `c9ad952 Phase 15E-52: Migrate PointChange in skill and item bridge paths`

Counts:
```text
Caller-side ->PointChange count at 15E-52 audit: 204
Caller-side ->PointChange count after 15E-52:    0
Tree-wide ->PointChange count after 15E-52:      1
Remaining tree-wide ->PointChange location:      PointSystem.cpp bridge
```

Declaration status:
- `CHARACTER::PointChange` declaration was retained.
- Reason: `PointSystem::Change` intentionally delegates to existing `CHARACTER::PointChange` to preserve POINT_* semantic routing, and many legacy member method bodies still call `PointChange(...)` internally without `->`.
- Removing or privatizing the declaration needs a dedicated private/friend bridge phase, not a caller-side migration pass.

Rollback note:
- One broad automated migration attempt was rolled back before commit because include ordering in ECS files caused a high-error build failure.
- The final implementation used small batches with `stdafx.h` kept first in every ECS `.cpp`.

Build results:
- Build passed after every committed batch.
- Final successful command:
```powershell
cmake --build build --config RelWithDebInfo --target GameServer --parallel 8
```

Manual WinTest checklist:
- Login and stat display.
- Combat HP damage and recovery.
- EXP gain and level-up.
- Gold gain/loss through pickup, shop, exchange, offlineshop, guild, cube, and wheel paths.
- Skill HP/SP absorb and victim point effects.
- DragonSoul refine fee path.
- Mount/pet stat refresh.
- Gaya spend/craft.
- Logout/login persistence.
- `VID_DRIFT` remains zero.

Commit status:
- Code batches committed individually.
- WinTest not run in this environment.

## Phase 15E-51 — ECS Systems Remaining LPITEM Signature Cleanup

Mode:
- ECS-system LPITEM parameter signature cleanup.
- Build gate after each logical subsystem batch.
- No `ItemSystem_LegacyBridge.cpp` internals rewritten.
- No core `item.h` / `item_manager` deletion attempted.

Completed batches:
- PART A Combat arrow/bow signatures:
  - `CHARACTER::GetArrowAndBow` now returns bow/arrow as `entt::entity` out parameters.
  - `CHARACTER::UseArrow` now consumes an `entt::entity` arrow.
  - Skill/combat callers now pass entities into `CalcArrowDamage` and `UseArrow`.
  - Commit: `06dfeba Phase 15E-51 PART A: CombatSystem arrow bow signatures`
- PART B Inventory quickslot signature:
  - `CHARACTER::ChainQuickslotItem` now takes `entt::entity`.
  - Existing legacy quickslot behavior preserved through internal bridge reads where needed.
  - `EquipmentSlots` component storage still uses legacy item pointers and is deferred to a storage-specific phase.
  - Commit: `918f701 Phase 15E-51 PART B: InventorySystem quickslot signature`
- PART C PlayerRuntime item signatures:
  - `SetQuestItemPtr`, `CanTakeInventoryItem`, and `CleanAcceAttr` now take `entt::entity`.
  - Offlineshop, quest manager, and acce call sites updated atomically.
  - Internal legacy resolution remains for quest current item pointer, DS/extra inventory placement helpers, and legacy item logging.
  - Commit: `d5d1c46 Phase 15E-51 PART C: PlayerRuntimeSystem item signatures`
- PART D ECS helper signatures:
  - `SkillSystem` internal `SetPolyVarForAttack` / `FuncSplashDamage` weapon parameters now use `entt::entity`.
  - `MountSummon` / `MountUnsummon` now take `entt::entity`.
  - `CheckMount` now creates a local mount entity before ECS item reads.
  - Commit: `03e2155 Phase 15E-51 PART D: ECS system helper signatures`

Counts:
```text
Scoped LPITEM parameter count before 15E-51: 73
Scoped LPITEM parameter count after 15E-51:  63
Reduction: 10
```

ECS systems residual LPITEM usage:
- `CombatSystem.cpp`: local `for (LPITEM srcItem : s_vec_item)` loops remain in legacy drop/vector cleanup code.
- `InventorySystem.cpp`: private bridge helper `ItemEntityOf(LPITEM)` and local `LPITEM item = LegacyItemOf(e)` remain.
- `ItemSystem.cpp` and `ItemSystem_LegacyBridge.cpp` remain intentional bridge/internal islands.
- No remaining public ECS-system item signatures were found outside bridge/internal boundaries.

Build results:
- Build passed after Combat, Inventory, PlayerRuntime, and Skill/Mount helper batches.
- Final successful command:
```powershell
cmake --build build --config RelWithDebInfo --target GameServer --parallel 8
```

Manual WinTest checklist:
- Bow normal attack consumes arrows correctly.
- Arrow skills consume arrows correctly and calculate damage.
- Quest current item selection still works.
- Offlineshop item return `CanTakeInventoryItem` path still finds a valid slot.
- Acce clean attr consumes cleaner item and clears target attributes.
- Mount summon/unsummon works from costume mount equip/unequip.
- Skill weapon damage variables still calculate correctly.
- `VID_DRIFT` remains zero.

Commit status:
- Code batches committed individually.
- WinTest not run in this environment.

## Phase 15E-50 — DragonSoul LPITEM Signature Migration

Mode:
- DragonSoul-only LPITEM parameter migration.
- Segment-by-segment commits with build gates.
- No DragonSoul DB persistence rewrite.
- No DragonSoul algorithm rewrite beyond signature/caller migration.

Completed segments:
- SEG 1 Initialization/setup:
  - `DragonSoulItemInitialize`, `PutAttributes`, and `RefreshItemAttributes` now take `entt::entity`.
  - Callers in item creation/load and bridge paths now pass ECS item entities.
  - Commit: `03918ad Phase 15E-50 SEG 1: DragonSoul initialization signatures`
- SEG 2 Activation:
  - `ActivateDragonSoul` and `DeactivateDragonSoul` now take `entt::entity`.
  - ECS wrappers and gameplay callers now route through entity signatures.
  - Commit: `498ef38 Phase 15E-50 SEG 2: DragonSoul activation signatures`
- SEG 3 Extract/charge:
  - `ExtractDragonHeart` and `PullOut` now take `entt::entity` item parameters.
  - Existing ECS consume/destroy/socket paths remain in place internally.
  - Commit: `d884df8 Phase 15E-50 SEG 3: DragonSoul extract charge signatures`
- SEG 4 Refine helper:
  - `IsDragonSoulRefineMaterial` now takes `entt::entity`.
  - Refine internals still use legacy CItem locally where the DS transaction engine has not been rewritten.
  - Commit: `61935ab Phase 15E-50 SEG 4: DragonSoul refine signatures`
- SEG 5 Query compatibility:
  - Removed remaining public `const LPITEM` overloads for `GetBasePosition`, `IsValidCellForThisItem`, and `GetDuration`.
  - Callers in `ItemSystem_LegacyBridge.cpp`, `exchange.cpp`, and `input_main.cpp` now pass ECS item entities.
  - Commit: `fafd3ea Phase 15E-50 SEG 5: DragonSoul query signatures`

Counts:
```text
Scoped LPITEM parameter count before 15E-50: 90
Scoped LPITEM parameter count after 15E-50:  73
DragonSoul.cpp/.h LPITEM parameter signatures: 0
Reduction: 17
```

Remaining DragonSoul LPITEM usage:
- Internal local `LPITEM` variables remain inside `DragonSoul.cpp`.
- These are legacy engine internals for refine/grid iteration/item creation side effects and are not public signatures.
- Next cleanup should target DragonSoul internal storage/sets and transaction-local CItem variables only after the DS mutation engine is fully entity-native.

Build results:
- Build passed after every segment.
- Final successful command:
```powershell
cmake --build build --config RelWithDebInfo --target GameServer --parallel 8
```

Manual WinTest checklist:
- DragonSoul inventory opens and displays correctly.
- Activate/deactivate DS item applies/removes affects.
- Deck switch preserves active state.
- DragonSoul refine consumes source/material and creates result.
- DragonHeart extract/charge paths consume items correctly.
- Logout/login preserves DS state.
- `VID_DRIFT` remains zero.

Commit status:
- Code batches committed individually.
- WinTest not run in this environment.
