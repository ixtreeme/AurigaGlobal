# Herbalism (Lua-free)

Status: implementation and isolated validation; **not activated in production**.
Mining and woodcutting remain separate. No Lua scripts implement progression,
spawning, drops or upgrades. Existing native NPC-click dispatch calls the new C++
system before falling through to the legacy quest handlers.

## Identifiers

- Pruning shears +0…+9: **230–239**, repurposing the existing sword prototypes.
- Plant resource NPCs: **34800–34811**; species index maps directly to herb
  **50701–50712**. All twelve reward items already exist.
- Separate training marker `HERB` (`0x48455242`), socket 0 points/socket 1 marker.
- Refinement NPC **20016**: unequip the trained shears and drag them onto the
  blacksmith. 80% default success; failure preserves the tool and loses 10% of
  training. +9 is terminal. Mining/axe points cannot qualify shears for upgrades.

Read-only audit on 2026-10-23: all 10 original sword prototypes exist, no player
items in that range, all 12 herbs exist, NPC range unoccupied. Repeat the audit
before activation; shop/drop references must also be reviewed.

## Configuration

Copy `config/herbalism.json.example` to server data `herbalism.json`. The example
is disabled. Missing/invalid configuration is fail-closed.

Initial scope: `metin2_map_main_razor93`, main map index 41. Defaults: 3 nodes per
species, 8 attempts/node, 3 seconds/attempt, 40% item chance, 20% training chance,
300 seconds depleted-node cooldown. Depleted nodes count against population
until cooldown expiration. No immediate full replacement on harvest. Per-core
hard cap 160 nodes; reject blocked/object/BANPK positions and nearby entities.

GM commands (HIGH_WIZARD enforced server-side):

```
/herbalism spawn 50701
/herbalism spawn 50706
/herbalism reload
```

`/herbalism refine` tells ordinary players to use the blacksmith; it cannot
perform a remote upgrade. Spawn requires enabled configuration and valid map.

## Authority and cancellation

The server owns timing, tool identity, range, exclusive worker, drop chance,
ownership and training. Repeated clicks cannot accelerate a scheduled attempt.
One attempt maximum per tick, no catch-up reward burst. Loot is ground-dropped
at the worker with a 30-second ownership claim; failed placement/claim rolls the
item back. Every attempt reports success/failure by locale key.

Movement, map change, death, stun, mounting, polymorph, disconnect, tool swap or
another incompatible activity interrupts pruning. Worker/plant references are
generation-checked ECS entities, not raw pointers. Woodcutting refuses concurrent
pruning and pruning refuses concurrent chopping. Mining code is untouched.

## Client

Unity resources `Metin2/Herbalism/Assets`: twelve plant prefabs with three LODs,
pruning-shears FBX/weapon definition, rendered icon, pruning feedback audio and
leaf particles. Native `pick_up` provides the bending motion. Movement interrupts
only the herbalism marker; cosmetic root sway cannot move the logical player.
Approach targets 0.81 m and starts at 0.85 m, not the generic five-metre NPC range.
The server allows 2 m centre distance to absorb network skew, not client rewards.
Local and remote workers use the same command route, including late snapshots.
Empty nodes hide graphics/click collider and reject targeting until removed.
All UI/notice text uses locale keys, with EN/HU resources and English fallback.

Asset tools (rerunnable): `Tools/DownloadHerbalismAssets.py`,
`PrepareHerbalismSources.py`, `ConvertPruningShears.py`,
`OptimizeHerbalismModels.py`, Unity menu **Metin2 > Herbalism > Prepare assets**.
Vendor source collections are not instantiated wholesale: exactly one selected
variant/species, capped around 10K/2.5K/0.6K triangles (topology constraints may
leave the lowest LOD slightly above budget). Attribution is preserved in client
Resources `Metin2/Herbalism/Credits.txt`; confirm shears redistribution rights.

## Safe activation (not performed automatically)

1. Repeat range/player/shop/drop audit and back up database/prototypes/config/bin.
2. Stop database/login/game processes. Run the reviewed migration on the correct
   `player` database (adapt table postfix only if configured).
3. Upload reviewed source, build on FreeBSD, test and install matching binaries
   into `bin` with recoverable backups. Never compile C++ on Windows.
4. Enable `herbalism.json`, restart services and verify configuration/spawns.
5. Test all twelve species and two clients: movement, ownership, busy state,
   repeated clicks, disconnect, full harvest/cooldown, tool swap and upgrade.
6. Publish matching Windows client/patch only after license and in-game checks.
   Android and Mission Pass are outside this activation step.

Tests: `HerbalismRulesTests` / CTest `herbalism_rules`; isolated FreeBSD syntax
checks and migration replay on uniquely named copied tables (live data unchanged).

Validation evidence (2026-10-23): all changed server translation units pass
FreeBSD syntax checks; both axe and shears rules tests pass; migration replayed
twice on isolated table copies. Unity asset generation and command/approach
regressions pass: 12 plants, 36 LOD meshes, 10 item presentations, queued depleted
snapshot, own-player pruning, malformed command rejection, direct/reserved NPC
click with final STOP first, physical approach clearance, all 8 character rigs.
This is not a claim of a production build or an end-to-end live gathering test.
