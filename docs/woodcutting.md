# Woodcutting (initial implementation)

## Isolation

`WoodcuttingSystem.cpp/.hpp` own the tree/session ECS components, configuration,
random spawning, timed harvesting, reward rolls, training and axe refinement.
`WoodcuttingRules.hpp` contains the independent, tested bounds/progression rules.
Mining's sources, ore table, ITEM_PICK classification, MiningState and refine
implementation are unchanged. Existing source files contain only integration
hooks for the game heartbeat, NPC clicks, item delivery, actor insertion, movement
cancellation, equipment presentation and rejection of consumables on tools.

## Data and activation

This feature is **not activated by copying C++ sources alone**. No live migration,
service stop, bin replacement, client publication or deployment is performed by
the implementation/validation tools.

1. Back up the player database. Audit existing player items, shops, quests and
   drops referencing 210..219: these IDs intentionally stop representing swords.
2. On a test deployment, with all services stopped, review/run
   `db/migrations/2026-10-23_woodcutting.sql` against the correct player database.
   Adapt table postfixes first if used. The script checks for conflicting actor
   ID 34090, requires log item 90011, and preserves the original sword prototypes
   in `woodcutting_item_proto_backup_20261023` before changing the prototypes.
   Inventory/quest rows are not deleted or rewritten by the migration.
3. Copy `config/woodcutting.json.example` to `<server.data_path>/woodcutting.json`.
   Missing, malformed or disabled configuration prevents harvesting/spawning.
4. Build on FreeBSD through the existing Server Manager workflow. CMake discovers
   the new source through its CONFIGURE_DEPENDS glob. Deploy the updated server
   together with a matching Unity client; old clients have no tree presentation.
5. Test with `/item 210` (existing GM item command) and `/woodcut spawn` on the
   exact `metin2_map_main_razor93` map (live main map, index 41). The new command validates GM privilege internally.
   `/woodcut reload` reloads the per-core configuration; reload on each hosting
   core when changing server-side settings. `/woodcut refine` shows the localized
   instructions; actual refinement requires dragging the unequipped axe onto NPC
   20016, within three metres.

NPC 34090 is a stationary, noncombat resource actor. NPC 20047 is only the SQL
template used to obtain the existing schema; ore 20047 itself is not changed.
Normal NPC clicks on any other actor take their original quest/trigger path.

## Initial tuning (all configurable except training thresholds)

- Exact allowed map: `metin2_map_main_razor93`; no substring/private-map match.
- Target population: 12 standing trees per hosting game core/channel.
- A completed tree requires 20 valid hits. One hit every two seconds; missed
  ticks never catch up with bursts of rewards.
- Each valid hit independently rolls 30% for one ground log (90011), owned by
  the chopper for 30 seconds, and 20% for one axe training point.
- A fallen tree remains for 180 seconds. Respawn population checks run every
  300 seconds. Total standing + fallen actors are capped at 200 per core.
- Spawn positions reject blocked/object/ban-PK tiles and nearby entities within
  six metres. GM spawning searches a safe ring eight metres from the GM.
- One active chopper per tree. Movement (including moving away and back), map
  changes, weapon replacement, death/stun, disconnection, mounting, polymorph,
  item-handling restrictions and other harvesting activities invalidate work.
- Axe grades 210..218 require 100, 250, 500, 1000, 2000, 4000, 8000, 16000,
  32000 points. 219 is the maximum. Refinement initially succeeds 80% of the
  time. Failure keeps the tool and removes 10% of its points; success grants the
  next grade with zero points. No Yang/material fee in the initial version.
- Training uses socket 0 plus the `0x57434f44` socket-1 format marker. Legacy
  sword/metin sockets and proto -1 sentinels are never treated as training.
  Generic socket/tuning/attribute consumables cannot target the axe.

## Presentation protocol

Uses the existing server-command transport; no existing binary packet layout
or mining packet is changed. Commands are emitted exclusively by the server:

```
woodcut_tree <treeVID> <remaining> <total> <phase:0|1> <elapsedFallMs> <nativeAngle>
woodcut_swing <treeVID> <workerVID> <hitSeconds>
woodcut_hit <treeVID> <workerVID>
woodcut_notice <WOODCUT_LOCALE_KEY>
```

Tree state is sent after every valid hit and on view insertion, so players who
arrive after a fall see its elapsed state rather than an upright tree. Client
snapshot queues are cleared on entity removal/world changes. Clients never grant
logs or training. Duplicate clicks cannot advance the server deadline.

## Unity assets and validation

`Metin2 > Woodcutting > Prepare assets` generates feature-owned wrapper prefabs,
an axe definition, a transparent rendered icon and a short synthesized wooden
impact sound from the imported `Assets/Axe` and `Assets/Hipernt/Pine Pack`
packages. Vendor originals are not edited. The Resources index is
`Assets/Metin2/Resources/Metin2/Woodcutting/Assets.asset`; only the feature's three
tree variants and one axe are referenced, not the entire vendor packages.

Tree models are metre-scaled, grounded at the authored trunk pivot, and have
measured trunk radii. Hits emit bounded chip particles, a growing cut scar and
nonlooping positional audio. The crown falls away from the chopper; picking and
collision are disabled once felled. This is staged visible damage, not dynamic
mesh fracture. Axe model/icon/tooltip apply to all ten grades. Weapon costumes
and standard sword-refinement effects do not replace the tool presentation.

The initial swing uses the existing `dig` animation for all eight player races;
a dedicated `woodcut` event can replace it later. Only woodcut-marked swings are
movement-interruptible; ordinary mining/emotion animation behaviour is preserved.
Blade-to-trunk contact/animation quality still needs visual in-game acceptance.

Every visible caption/notice comes from locale keys. Hungarian and English
feature tables are provided; other supported languages use the English fallback.

Run `Metin2 > Woodcutting > Validate` for model, icon, audio, eight-rig attachment,
animation, snapshot, fall/collision, localization and grade-boundary checks.
On FreeBSD, build/run the standalone `WoodcuttingRulesTests` target, also
registered as CTest `woodcutting_rules`.

Before live release, test two clients (ownership/busy state, late visibility,
movement cancellation, tool swapping, warp/relogin, refine success/failure,
logs and persistent training), then repeat ore mining and pickaxe refinement.
Verify mobile touch interaction/performance on a real device. A syntax check or
Editor asset validation is not a substitute for those gameplay acceptance tests.
