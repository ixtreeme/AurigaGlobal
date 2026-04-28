# ECS Migration Summary Log

Date: 2026-04-26
Workspace: `E:\AurigaGlobal\LiveWork\AurigaGlobal`

This file summarizes the completed ECS migration work so far. It is intentionally separate from the event-style root `MIGRATION_LOG.md`.

## Current State

High-level status:
- `CHARACTER` storage was reduced from about `38,620` bytes to the low single-digit KiB range during Phase 15C work.
- `CHARACTER_POINT_INSTANT` is mostly migrated out of `CHARACTER`.
- Main inventory, extra inventory, DragonSoul, transient windows, appearance parts, vitals max, and runtime flags are ECS-backed.
- `CHARACTER_POINT` scalar fields with existing ECS authority are routed to ECS.
- `points[]` semantic stat-array migration was attempted, failed runtime validation, and is parked for a dedicated semantic retry.
- Chat send surface is now entity-only through `ecs::ChatSystem`.
- `CharacterAccessors.hpp` no longer exposes LPCHARACTER-taking fake accessor wrappers.

Known parked or deferred work:
- `CHARACTER_POINT_INSTANT.points[]` remains legacy because `POINT_*` indices have mixed semantics.
- Some `CHARACTER_POINT` fields remain legacy or deferred, including unique fields such as job, voice, random HP/SP, skill group, and feature-specific fields.
- Non-ECS code still has many honest `LPCHARACTER` call sites after fake wrapper removal.
- Questlua still contains entity-to-legacy bridge usage that must be addressed separately.
- `ItemSystem.hpp` still had a public LPCHARACTER backdoor in the Phase 15E quality audit: `SyncExtraInventoryAll(LPCHARACTER ch)`.

## Baseline And Early Migration

Completed baseline/audit artifacts:
- `phase0_*` reports documented `CHARACTER`, `DESC`, quest, method, and include fanout.
- `phase9*` reports documented `char.h` split/fanout and strict L3 classification.
- `phase10_event_audit.txt`, `phase11_item_audit.txt`, `phase12_sectree_audit.txt`, `phase13_l3_private_audit.txt`, `phase14c_stat_surface_audit.txt`, and `phase14d_char_manager_audit.txt` mapped later migration risk.

Phase 15 Week 1:
- Created transitional `ecs/CharacterAccessors.hpp`.
- Migrated many low and medium files to ECS accessor usage.
- This created early momentum but later audit classified LPCHARACTER-taking accessors as fake migration, because they hid legacy ownership.

Phase 15 Week 2:
- Expanded accessor usage across more medium-risk files.
- Added wider variable-name replacements and additional accessor helpers.
- Later Phase 15E work removed the fake LPCHARACTER accessor layer and converted those sites back to honest legacy calls or real entity usage.

## Phase 15 VID-A

Status: complete.

Goal:
- Remove legacy `class VID` and unify entity identity around ECS / `entt::entity`, while preserving packet VID compatibility.

Completed work:
- Established VID unification baseline.
- Eliminated creation-order drift.
- Made `CHARACTER_MANAGER::Find` delegate to ECS.
- Added packet-boundary helper `CHARACTER::GetPacketVID`.
- Replaced victim and damage-map storage with entity-backed forms.
- Migrated event payloads and multiple TYPE_P / TYPE_E storage paths to `entt::entity`.
- Removed `CHARACTER::m_vid` and `CHARACTER::GetVID`.
- Deleted stale `vid.h` and `class VID`.
- Kept packet and lifecycle compatibility where required.

Runtime fixes during VID work:
- Kept legacy VID stable through teardown.
- Fixed packet VID compatibility.
- Fixed follower same-sectree respawn behavior.
- Kept mob-party membership on legacy `GetLegacyVID()` where required.

Representative commits:
- `1eac4b6` `Phase 15: VID unification baseline audit`
- `e853f3d` `Phase 15 VID-A1: Eliminate creation-order drift`
- `e153cdd` `Phase 15 VID-A2: CHARACTER_MANAGER::Find delegates to ECS`
- `b194395` `Phase 15 VID-A3: COMPLETE`
- `74b73af` `Phase 15 VID-A4: COMPLETE`
- `afc8467` `Phase 15 VID-A6: Remove CHARACTER::m_vid and GetVID`
- `f0bd339` `Phase 15 VID-A7.4: Delete class VID and vid.h`
- `f224e9d` `Phase 15 VID-A: COMPLETE - class VID eliminated`

## Phase 15B

Status: complete.

Goal:
- Clean ECS system internals and document the stable migration patterns.

Completed systems:
- `GayaSystem`
- `AffectSystem`
- `SkillSystem`
- `MountSystem`
- `CombatSystem`

Patterns established:
- Entity-first system APIs.
- Legacy bridge only inside ECS internals when still required.
- Avoid leaking LPCHARACTER into public ECS system headers.
- Keep packet/lifecycle boundaries explicit.

Artifacts:
- `phase15b_ecs_system_patterns.txt`
- `phase15b_final_audit.txt`

Representative commits:
- `703da2e` `Phase 15B-1: Convert GayaSystem internals to entity bridge`
- `970d517` `Phase 15B-1: Document ECS system migration patterns`
- `2fc33d1` `Phase 15B-2: Convert AffectSystem internals to entity bridge`
- `c40189a` `Phase 15B-3: Convert SkillSystem internals to entity bridge`
- `7709a4a` `Phase 15B-4: Convert MountSystem internals to entity bridge`
- `1ba2596` `Phase 15B-5: Convert CombatSystem helpers to entity bridge`
- `f2c07da` `Phase 15B: COMPLETE`
- `3b22c45` `Phase 15B: Final audit - 15C/15D direction assessment`

## Phase 15C Audit Work

Status: complete for audits.

Completed audits:
- `15C-0`: component expansion priority audit.
- `15C-1`: points cluster migration audit.
- `15C-1j pre-analysis`: semantic audit after failed `points[]` migration attempt.

Key findings:
- `CHARACTER_POINT_INSTANT` was the largest removable storage cluster.
- Extra inventory and DragonSoul were the best first high-value migration targets.
- `points[]` cannot be treated as one flat stat authority because some `POINT_*` indices are virtual or already have dedicated authority.

Important artifacts:
- `phase15c_expansion_audit.txt`
- `phase15c_1_points_audit.txt`
- `phase15c_1j_preanalysis.txt`
- `phase15c_1j_allowlist.txt`

Representative commits:
- `da573a2` `Phase 15C-0: Component expansion priority audit`
- `5ad334d` `Phase 15C-1: Points cluster migration audit`
- `cb12530` `Phase 15C-1j: Pre-analysis before retry`

## Phase 15C-1 Storage Migration

Status: mostly complete. `points[]` remains parked.

### 15C-1a Extra Inventory

Status: complete and runtime-confirmed.

Migrated fields:
- `pExtraItems[EXTRA_INVENTORY_MAX_NUM]`
- `wExtraItemGrid[EXTRA_INVENTORY_MAX_NUM]`

Component:
- `ecs::ExtraInventoryRuntimeComponent`

Result:
- Legacy fields removed from `CHARACTER_POINT_INSTANT`.
- Expected memory reduction: about `10,800` bytes per `CHARACTER`.
- Runtime issues found and fixed:
  - component reinitialization overwrote extra inventory slots
  - relog item disappearance caused by stale runtime item / `ITEM_ID_DUP`
- User confirmed extra inventory works and relog-restored items returned.

Representative commits:
- `9161bac` `Phase 15C-1a.1: Add ExtraInventoryRuntimeComponent definition`
- `9118065` `Phase 15C-1a: COMPLETE - Extra inventory migrated to ECS`

### 15C-1b-e Transient Window Components

Status: complete.

Migrated groups:
- Cube window
- Attribute transfer window
- Acce window
- Switchbot runtime slots

Components:
- `ecs::CubeWindowComponent`
- `ecs::AttrTransferWindowComponent`
- `ecs::AcceWindowComponent`
- `ecs::SwitchbotRuntimeComponent`

Result:
- Legacy transient window fields removed from `CHARACTER_POINT_INSTANT`.
- Expected combined memory reduction: about `288` bytes per `CHARACTER`.

Representative commits:
- `5f66b1c` `Phase 15C-1b: COMPLETE - cube window on ECS`
- `1e3cd1b` `Phase 15C-1c: COMPLETE`
- `5ecb4da` `Phase 15C-1d: COMPLETE`
- `315a638` `Phase 15C-1e: COMPLETE`
- `88c85da` `Phase 15C-1b-e: WinTest regression verified`

### 15C-1f DragonSoul

Status: complete and runtime-confirmed.

Migrated fields:
- `pDSItems[DRAGON_SOUL_INVENTORY_MAX_NUM]`
- `wDSItemGrid[DRAGON_SOUL_INVENTORY_MAX_NUM]`
- `iDragonSoulActiveDeck`
- `m_pDragonSoulRefineWindowOpener`

Components:
- `ecs::DragonSoulInventoryComponent`
- `ecs::DragonSoulRuntimeStateComponent`

Result:
- Legacy DragonSoul fields removed from `CHARACTER_POINT_INSTANT`.
- Expected memory reduction: about `14,532` bytes per `CHARACTER`.
- Manual runtime confirmation completed.

### 15C-1g-h Appearance Parts And Vitals Max

Status: complete and runtime-confirmed.

Migrated appearance fields:
- `parts[PART_MAX_NUM]`
- `bBasePart`

Component:
- `ecs::AppearancePartsComponent`

Migrated vitals max fields:
- `iMaxHP`
- `iMaxSP`
- `iMaxStamina`

Reused components:
- `ecs::Health::max`
- `ecs::Mana::max`
- `ecs::Stamina::max`

Result:
- Legacy fields removed from `CHARACTER_POINT_INSTANT`.
- User-side runtime pass confirmed normal gameplay after follow-up validation.

### 15C-1i Runtime Flags

Status: complete and runtime-confirmed.

Migrated fields:
- `dwAIFlag`
- `instant_flag`
- `position`
- `dwImmuneFlag`
- `dwLastShoutPulse`
- `gm_level`
- `bBlockMode`
- `fRot`

Component:
- `ecs::CharacterRuntimeFlagsComponent`

Result:
- Legacy runtime flag fields removed from `CHARACTER_POINT_INSTANT`.
- Temporary `RFLAG_DRIFT` verification passed before final removal.
- User confirmed the game worked correctly after final cutover.

### 15C-1j points[] Attempt

Status: rolled back and parked.

What happened:
- First full `points[POINT_MAX_NUM]` ECS array attempt caused EXP scaling and combat timing regressions.
- Retry with semantic allowlist also produced incorrect damage timing and stat point behavior.
- The migration was rolled back to stable legacy `points[]` authority.

Current decision:
- `points[]` remains legacy.
- Future retry must classify each `POINT_*` index into:
  - true array-backed stats
  - virtual/dedicated authority values
  - already-migrated ECS values

Important reason:
- `POINT_EXP`, `POINT_GOLD`, `POINT_LEVEL`, `POINT_HP`, `POINT_MAX_HP`, `POINT_SP`, `POINT_MAX_SP`, `POINT_STAMINA`, and `POINT_MAX_STAMINA` are not ordinary stat-array fields.

### 15C-1k Main Inventory

Status: complete and runtime-confirmed.

Migrated fields:
- `pItems[INVENTORY_AND_EQUIP_SLOT_MAX]`
- `bItemGrid[INVENTORY_AND_EQUIP_SLOT_MAX]`

Component:
- `ecs::MainInventoryRuntimeComponent`

Result:
- Legacy main inventory/equipment storage fields removed from `CHARACTER_POINT_INSTANT`.
- Expected memory reduction: about `6,600` bytes per `CHARACTER`.
- User confirmed inventory/equipment behavior works.

Runtime fix after migration:
- Equipment panel packet semantics were corrected so equipment uses the expected client window/cell format while ECS storage remains offset-based.

## Phase 15C-2 CHARACTER_POINT Scalars

Status: partially complete and runtime-confirmed.

Migrated fields from `CHARACTER_POINT`:
- `level` -> `ecs::LevelComponent`
- `exp` -> `ecs::Experience.current`
- `gold` -> `ecs::GoldAmount.amount`
- `hp` -> `ecs::Health.current`
- `sp` -> `ecs::Mana.current`
- `stamina` -> `ecs::Stamina.current`

Result:
- Double-storage for these already-ECS-backed fields was removed.
- Getter/setter surface routes to ECS components.
- Character select, login/logout, loading, enter game, EXP, HP, mana, and attack were user-confirmed working.
- Equipment window regression was fixed in `ItemSystem.cpp`.

Not completed:
- `CHARACTER_POINT` struct was not deleted.
- Remaining fields still require separate handling or deferral.

Commit:
- `dfb9c2e` `Phase 15C-2: Route CHARACTER_POINT scalars to ECS and fix equipment panel semantics`

## Phase 15D LPCHARACTER Reduction

Status: multiple batches complete.

### 15D-1 Questlua Mass Migration

Status: complete.

Result:
- Removed the dominant `LPCHARACTER ch = GetCurrentCharacterPtr()` / NPC variant boilerplate.
- Questlua LPCHARACTER declarations reduced from `418` to `73`.
- Whole-tree LPCHARACTER local declarations reduced from `940` to `596`.
- Total dominant pattern instances migrated: `345`.

Commit:
- `2f83f7b` `Phase 15D-1: Questlua mass migration COMPLETE`

### 15D-2 cmd_*.cpp Trivial Getter Migration

Status: complete.

Touched:
- `cmd_general.cpp`
- `cmd_gm.cpp`
- `cmd_general2.cpp`

Result:
- ACMD macro surface stayed unchanged.
- Trivial direct getter surface on `ch` reduced from `141` to `0` in the targeted command files.
- Later Phase 15E eliminated the fake accessor layer introduced by this transitional pattern.

Representative commits:
- `c474dfd` `Phase 15D-2: cmd_general2.cpp trivial getter migration`
- `cd355bf` `Phase 15D-2: cmd_gm.cpp trivial getter migration`
- `1ff22ce` `Phase 15D-2: cmd_general.cpp trivial getter migration`
- `17e906c` `Phase 15D-2: Document cmd accessor migration checkpoint`

### 15D Continuation Batch

Status: complete.

Touched:
- `party.cpp`
- `guild.cpp`
- `input_db.cpp`
- `battle.cpp`
- `char_manager.cpp`

Deferred:
- `pvp.cpp` was initially deferred because of `DynamicCharacterPtr` event paths.

Result:
- LPCHARACTER local declarations: `599 -> 540`
- LPCHARACTER params: `1246 -> 1244`
- direct method-call heuristic: `6144 -> 6110`

Representative commits:
- `285ab03` `Phase 15D: party.cpp LPCHARACTER reduction`
- `3a66c01` `Phase 15D: guild.cpp LPCHARACTER reduction`
- `0d08e46` `Phase 15D: input_db.cpp LPCHARACTER reduction`
- `cddf914` `Phase 15D: battle.cpp trivial getter cleanup`
- `d2cf0a7` `Phase 15D: char_manager.cpp trivial getter cleanup`
- `21ef4ff` `Phase 15D continuation: batch checkpoint`

### 15D-3

Status: complete and runtime-confirmed.

Touched:
- `cmd_general.cpp`
- `cmd_gm.cpp`
- `new_offlineshop_manager.cpp`
- `pvp.cpp`

Result:
- LPCHARACTER local declarations: `540 -> 532`
- LPCHARACTER params stayed `1244`
- direct method-call heuristic: `6110 -> 6029`
- `pvp.cpp` received safe getter cleanup while leaving `DynamicCharacterPtr` storage/event paths intact.

Commits:
- `799c87f` `Phase 15D-3: cmd_general.cpp trivial getter cleanup`
- `05cfb71` `Phase 15D-3: cmd_gm.cpp trivial getter cleanup`
- `2d0dd1c` `Phase 15D-3: new_offlineshop_manager.cpp getter cleanup`
- `9912f9f` `Phase 15D-3: pvp.cpp safe getter cleanup`
- `825124a` `Phase 15D-3: batch complete`

## Phase 15E Chat And Quality Work

### 15E-1a ChatSystem

Status: complete and runtime-confirmed.

Result:
- Created `ecs::ChatSystem`.
- Replaced `CHARACTER::ChatPacket` and `CHARACTER::ChatPacketNew` call sites.
- Removed `CHARACTER::ChatPacket` / `ChatPacketNew` declarations from `char.h`.
- Follow-up fixed the initial mistake where LPCHARACTER overloads let callers avoid entity conversion.
- Final `ChatSystem` API is entity-only.

Important counts:
- Exact `CHARACTER`-style `->ChatPacket(` call sites: `0`
- Exact `CHARACTER`-style `->ChatPacketNew(` call sites: `0`
- `LPCHARACTER` in `ChatSystem.hpp/.cpp`: `0`

Representative commits:
- `f3c13fb` `Phase 15E-1a.1: Create ChatSystem with entity-first API`
- `2e4b315` `Phase 15E-1a: Complete ChatPacket call site migration`
- `2b9109f` `Phase 15E-1a fix: COMPLETE - ChatSystem entity-only`

Runtime result:
- User confirmed chat works.

### 15E Quality Audit

Status: complete.

Key finding:
- Recent ECS public headers were mostly clean, but the call-site layer still had a large fake migration surface.

Important audit counts:
- One fake ECS public API remained in audit:
  - `ItemSystem.hpp :: SyncExtraInventoryAll(LPCHARACTER ch)`
- Fake accessor surface in `CharacterAccessors.hpp`: about `700` non-ECS call sites.
- Non-ECS `EcsOf(...)` conversions: about `1332`.
- `LegacyCharOf(...)` outside allowed ECS areas: about `345`, concentrated in questlua.
- Heuristic score:
  - real surface: `1325`
  - fake surface: `2377`
  - fake share: `64.2%`

Artifact:
- `phase15e_quality_audit.txt`

Commit:
- `4fe6113` `Phase 15E: Migration quality audit`

### CharacterAccessors.hpp Elimination

Status: complete.

Result:
- Removed LPCHARACTER-taking wrappers from `ecs/CharacterAccessors.hpp`.
- Removed fake `ecs::GetXxx(ch)` call sites tree-wide.
- Left only valid bridge/entity helpers.
- Files that still use legacy pointers were documented honestly instead of hidden behind wrappers.

Verification:
- `LPCHARACTER|CHARACTER *` in `CharacterAccessors.hpp`: `0`
- Tree-wide fake accessor calls for the removed names: `0`

Artifacts:
- `phase15e_honest_legacy_list.txt`

Commits:
- `d79cf72` `Phase 15E: CharacterAccessors.hpp LPCHARACTER surface eliminated`
- `499346f` `Phase 15E: Document honest legacy file list`

## Runtime Incidents Fixed During ECS Work

These are not all ECS migrations themselves, but they were found during ECS runtime validation and are part of the completed stabilization history.

Fixed incidents:
- Login DB packet flush issue in `desc_client.cpp`.
- Channel deploy reparse-point / stale binary issue.
- Extra inventory overwrite caused by `emplace_or_replace` reinitializing runtime components.
- Extra inventory relog disappearance caused by stale runtime items / `ITEM_ID_DUP`.
- Metin reward tuning after failed `points[]` migration attempt and rollback.
- Logout item persistence hardening.
- Equipment panel display semantics after main inventory ECS migration.
- Movement rubberband causing delayed metin damage/spawn/collapse feedback.
- Inconsistent WinTest binary deployment causing fixed behavior to appear regressed.

Current validated runtime facts:
- Extra inventory works and persists after relog.
- DragonSoul migration works.
- Runtime flags migration works.
- Main inventory and equipment migration works.
- `CHARACTER_POINT` scalar routing works for login, character select, EXP, HP, mana, and attack.
- Chat works through `ecs::ChatSystem`.
- Metin damage delay was resolved after movement fix and consistent binary deployment.

## Current ECS Work Queue

Highest-priority remaining ECS work:
- Design a safer `points[]` semantic migration using an allowlist and dedicated authority routing.
- Continue reducing honest LPCHARACTER holders from the Phase 15E legacy list.
- Remove the remaining public LPCHARACTER backdoor in `ItemSystem.hpp`.
- Reduce non-ECS `EcsOf(ch)` hotspots by passing entities deeper into call chains.
- Clean questlua reverse `LegacyCharOf(...)` bridges.
- Re-clean ECS system internals with high LPCHARACTER counts, especially:
  - `CombatSystem.cpp`
  - `PlayerRuntimeSystem.cpp`
  - `ItemSystem.cpp`
  - `NetworkSyncSystem.cpp`
  - `MovementSystem.cpp`

## Build And Validation Standard Used

Standard build gate:
- `cmake --build build --config RelWithDebInfo --target GameServer --parallel 8`

Standard WinTest validation pattern:
- deploy fresh `GameServer.exe`
- verify binary hash where relevant
- restart:
  - `Database`
  - `auth/GameServer`
  - `ch1/core1`
  - `ch1/core2`
  - `ch99/core99`
- inspect logs for:
  - `VID_DRIFT`
  - `RFLAG_DRIFT`
  - `STATS_DRIFT`
  - `heart_idle`
  - `DestroyCharacter ... not found`
  - `SpawnMob: cannot create`
  - feature-specific null deref or persistence errors

Known recurring log noise:
- motion/content warnings
- auth-side `Cube_Init failed`
- auth-side `Blend_Item_init fail`

