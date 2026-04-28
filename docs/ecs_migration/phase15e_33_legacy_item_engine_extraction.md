# Phase 15E-33 - Legacy Item Engine Extraction Plan

Date: 2026-04-27

Mode: audit + design. No gameplay code changes in this slice.

## Objective

Move item mutation authority away from `LPITEM` / `CItem` method bodies into ECS-first item systems, subsystem by subsystem.

This phase is the final blocker before any realistic global `LPITEM` delete attempt, but deletion is not safe yet.

## Mutation Audit Summary

### `ItemSystem_LegacyBridge.cpp`

| Mutation class | Count | Current authority | Risk |
|---|---:|---|---|
| `SetCount` | 133 | `CItem` / legacy CHARACTER item flows | Critical |
| `SetSocket` | 83 | legacy item use, timers, pet/DS/refine | Critical |
| `RemoveItem` | 37 | `ITEM_MANAGER` deletion/lifecycle | Critical |
| `AddToCharacter` | 35 | inventory placement | Critical |
| `SetForceAttribute` | 24 | attribute/refine/pet/costume | Critical |
| `RemoveFromCharacter` | 15 | inventory/equip/DS/refine removal | Critical |
| `CreateItem` | 14 | item creation/refine/reward | Critical |
| refine entry points | 11 | refine engine | Critical |
| `SetAttribute` | 6 | attribute mutation | High |
| `EquipTo` | 3 | equip engine | Critical |

### `DragonSoul.cpp`

| Mutation class | Count | Current authority | Risk |
|---|---:|---|---|
| refine entry points | 12 | DS refine algorithms | Critical |
| `SetCount` | 11 | DS consume/refine/extract | Critical |
| `RemoveFromCharacter` | 8 | DS refine/extract | Critical |
| `CreateItem` | 7 | DS result/byproduct creation | Critical |
| `SetForceAttribute` | 4 | DS attribute generation | Critical |
| `SetSocket` | 4 | DS charge/active state | Critical |
| `AddToCharacter` | 2 | DS result placement | Critical |

### `ItemSystem.cpp`

| Mutation class | Count | Current authority | Risk |
|---|---:|---|---|
| refine wrappers | 10 | wrapper to legacy engine | High |
| `SetSocket` | 3 | ECS write API mirrors legacy | Medium |
| `RemoveItem` | 2 | bridge destruction | High |
| `RemoveFromCharacter` | 1 | ECS remove wrapper | High |
| `SetCount` | 1 | ECS write API mirror | Medium |
| `CreateItem` | 1 | ECS AutoGive path | High |
| `AddToCharacter` | 1 | ECS placement wrapper | High |

## Existing ECS Mutation APIs

Already available:

```cpp
void SetItemCount(entt::entity item, uint32_t count);
bool ConsumeItem(entt::entity item, uint32_t amount = 1);
bool SetItemSocket(entt::entity item, int index, uint32_t value);
bool SetItemAttribute(entt::entity item, int index, int type, int value);
bool ClearItemAttribute(entt::entity item, int index);
bool PlaceItemEcs(entt::entity owner, entt::entity item, uint8_t window, uint16_t cell);
bool RemoveItemEcs(entt::entity item);
bool EquipItemEcs(entt::entity owner, entt::entity item, int candidateCell = -1);
bool UnequipItemEcs(entt::entity owner, entt::entity item);
bool UseItemEcs(entt::entity owner, entt::entity item, TItemPos destCell = NPOS);
bool ReceiveItemEcs(entt::entity receiver, entt::entity from, entt::entity item);
RefineResult RefineItemEcs(entt::entity e, const RefineInput& input, entt::entity target);
```

Gap: several APIs still use legacy as the side-effect engine internally. The next step is to move individual call paths so ECS writes first and legacy only syncs or observes.

## ECS Equivalent Design

### Count / consume

Target:

```cpp
bool AddItemCountEcs(entt::entity item, int delta);
bool SetItemCountEcs(entt::entity item, uint32_t count);
bool ConsumeItemEcs(entt::entity item, uint32_t amount);
```

Rules:
- ECS `ItemCount` is primary.
- Legacy item count is mirrored only if legacy item still exists.
- Deletion at zero must go through one destruction helper.
- Do not touch stack merge/refine/DS count paths until each subsystem is isolated.

First migration candidates:
- simple consumable decrement paths already identified in item-use code.
- non-refine, non-DS `SetCount(item->GetCount() - 1)` paths.

### Socket mutation

Target:

```cpp
bool SetItemSocketEcs(entt::entity item, int index, uint32_t value);
bool AddItemSocketEcs(entt::entity item, int index, int delta);
```

Rules:
- ECS `ItemSockets` is primary.
- Legacy mirror only through sync helper.
- Do not migrate timed unique sockets, DS sockets, pet sockets, or refine sockets until their subsystem wrapper owns the behavior.

First migration candidates:
- simple box/pet bookkeeping sockets only after quest/pet wrappers own their lifecycle.

### Attribute mutation

Target:

```cpp
bool SetItemAttributeEcs(entt::entity item, int index, int type, int value);
bool SetItemForceAttributeEcs(entt::entity item, int index, int type, int value);
bool ClearItemAttributesEcs(entt::entity item);
```

Rules:
- ECS `ItemAttributes` is primary.
- Need exact parity for forced attributes vs normal attributes.
- Attribute generation/randomization remains legacy until RNG and logging behavior is copied exactly.

First migration candidates:
- quest current item table writes already partially routed.
- `SetForceAttribute` in pet/new-pet remains blocked by pet DB semantics.

### Location / placement

Target:

```cpp
bool PlaceItemEcs(entt::entity owner, entt::entity item, uint8_t window, uint16_t cell);
bool RemoveItemEcs(entt::entity item);
bool MoveItemEcs(entt::entity owner, entt::entity item, uint8_t window, uint16_t cell);
```

Rules:
- ECS `ItemOwner`, `ItemLocation`, inventory runtime components and grid state must stay consistent.
- Legacy `AddToCharacter` / `RemoveFromCharacter` remains side-effect sync only until packet and DB save are fully ECS-owned.
- Do not migrate refine/DS/result item placement before DS/refine wrappers are fully authoritative.

First migration candidates:
- normal inventory insert paths created by `AutoGiveItemEcs`.
- no shop/exchange/safebox/offlineshop yet.

### Equip

Target:

```cpp
bool EquipItemEcs(entt::entity owner, entt::entity item);
bool UnequipItemEcs(entt::entity owner, entt::entity item);
```

Rules:
- ECS updates equipped state and location first.
- Legacy currently still needed for affect/stat recalculation.
- Full extraction requires item affect/stat application model.

Blocked by:
- affect application
- stat recalculation
- equipment packets
- special items and costume/mount exceptions

### Refine

Target:

```cpp
RefineResult RefineItemEcs(entt::entity owner, const RefineInput& input, entt::entity target);
RefineResult RefineDragonSoulEcs(...);
```

Rules:
- Keep legacy refine engine until exact creation/destruction/material consumption behavior is reproduced.
- ECS wrapper should own entry point and post-sync first.
- Next safe step is to convert all refine entry calls to wrappers, not internal algorithm rewrite.

Blocked by:
- material consumption
- success/fail RNG/logs
- result item creation
- socket/attribute transfer
- item destruction ordering

### DragonSoul

Target:

```cpp
bool ActivateDragonSoulEcs(entt::entity owner, entt::entity item);
bool DeactivateDragonSoulEcs(entt::entity owner, entt::entity item);
RefineResult RefineDragonSoulGradeEcs(...);
RefineResult RefineDragonSoulStepEcs(...);
RefineResult RefineDragonSoulStrengthEcs(...);
```

Rules:
- ECS controls entry points.
- DS internals remain legacy until sockets/count/result placement are independently ECS-owned.

Blocked by:
- DS active socket
- DS inventory routing
- extraction/pull-out semantics
- refine byproduct/result behavior

## Recommended Migration Order

1. Freeze all new direct `LPITEM` mutation additions outside core/bridge.
2. Move remaining safe `SetCount` decrement flows to `ConsumeItemEcs`.
3. Extract non-DS, non-refine socket writes to `SetItemSocketEcs`.
4. Add ECS force-attribute API, then migrate quest current item force-attribute paths.
5. Extract item placement for AutoGive and normal inventory insert.
6. Extract equip state only after stat/affect recalculation API exists.
7. Convert all refine/DS callers to ECS wrappers, then rewrite internals one algorithm at a time.
8. Only after all above: attempt `LPITEM` typedef deletion compile pass.

## Explicit Non-Goals For This Slice

- No blind rewrite of `ItemSystem_LegacyBridge.cpp`.
- No DragonSoul/refine algorithm rewrite.
- No packet format change.
- No inventory layout change.
- No global `LPITEM` typedef delete.
- No `points[]` touch.

## Global Delete Readiness

Not safe.

Deletion blockers still include:
- `ItemSystem_LegacyBridge.cpp`
- `DragonSoul.cpp`
- refine internals
- exchange / safebox / offlineshop / shopEx transaction engines
- switchbot
- quest current item legacy bridge
- item manager and CItem core
- packet-sensitive inventory mutation paths

