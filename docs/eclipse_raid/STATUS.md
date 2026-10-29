# Eclipse raid — implementation status

## Active delivery order (2026-10-27)

Proceed one encounter room at a time, covering server mechanics, main-client
presentation, interaction, all four difficulty modes, reset and group testing
before advancing to the next room. The first room (Marrowgar equivalent) is the
current acceptance target. Passing isolated domain tests is not room completion.

Current source adapters: encounters 1, 5 and 6 run their source-backed controllers
through the ECS adapter. Ten domain/runtime/data suites pass on FreeBSD. Numeric
catalogues now contain 1555 spells and 435 creature templates; the original eight
missing DBC records were resolved from the supplied server SQL. Two additional
transitive numeric candidates remain explicitly unresolved.

First-room work includes original spike creature statistics, a dedicated native
bone-spike race (62019), source-timed moving storm markers, coldflame particles,
localized rescue/status text and clearing local immobilization after map exit.
The shared ice-prison visual is not reused for the first-room bone spike.
Main-client rendering/spawn-pose validation and a full linked FreeBSD server
build have passed. The first-room GM-led playtest has been deployed and restarted.
Live player combat and multiplayer acceptance remain pending. See FIRST_ROOM_PLAYTEST.md. The older progress sections below are historical groundwork, not a
claim that the raid is ready for release. Public raid admission stays disabled; GM-led access permits encounter 1 only.

Target: adapt the locally supplied AzerothCore ICC C++ implementation, including
10/25-player Normal/Heroic branches, into the Auriga ECS server. The upstream
snapshot is in `upstream/`, with content hashes in `upstream_inventory.json`.
These files are reference sources, outside the GameServer compilation tree.
The AzerothCore copyright/license notices remain with the source snapshot.

## Completed integration work

- Approved architecture imported into the main Unity project as an on-demand map.
- No research Warrior or research player controller is used by the game.
- Map template reserved as 430, global base 4608000/4608000, size 1024×1280 metres.
- Main Unity integration completed with fourteen boss actors and four supporting
  interaction/add visuals. The existing main gameplay scene was not replaced.
- Native race wrappers and motion states registered only for the new raid assets.
  A global race-wrapper rescan crashed Unity, so the integration tool now performs
  bounded registration instead of loading every existing race.
- Client/server blocked grids are identical. The entry and all twelve encounter
  centres are connected; 142318 grid cells are walkable.
- Private-instance admission, bounded scheduling, state/telegraph messages and
  normal combat hooks exist as infrastructure. The initial generic encounter
  scaffold is not an ICC fidelity implementation and is disabled.
- The new server adapter passes an isolated FreeBSD compiler syntax check.
  This was not a full server build or deployment; the running service was not
  stopped, replaced or restarted.
- Source-backed Marrowgar and Festergut domain controllers/helpers have tests
  covering all four difficulty modes. They are not yet ECS-connected fights.

## Release gate

`config/eclipse_raid.json` stays disabled. Passing infrastructure tests or having
a spell data record does not establish behavioral parity. `coverage.json`
records source script units individually and keeps domain, ECS and multiplayer
status separate. No near-100% claim is made while these entries are incomplete.

Remaining: connect the source-backed controllers and spell/aura/targeting
adapters, port the other encounter/NPC/spell units and instance events, implement
source lockouts/attempt limits and required interactions, verify every mode,
then build and test on FreeBSD before enabling live admission.

The 911 DBC mechanical records are data, not an implementation. Eight unresolved
records are retained explicitly in `eclipse_raid_spells.json`; their behavior
must be resolved from the server source/DB rather than guessed.

## Source difficulty and spell groundwork update

- `/eclipse enter 10_normal|25_normal|10_heroic|25_heroic` stores an immutable
  per-instance mode. Default is 10 normal; actual source raid-size limits are
  10/25, although the map has physical room for thirty players. Unknown modes
  are rejected. This admission plumbing does not complete the encounter ports.
- A reproducible numeric catalog contains 911 source spell records. Mode
  selection follows the original SpellMgr, including heroic fallback to the
  same-size normal mode and missing SpellInfo fallback. Eight records remain
  unresolved; they are not filled with guessed effects. Signed trigger sentinels
  are preserved. The generator validates conflicting difficulty groups.
- Marrowgar coldflame helpers cover non-tank/combat-reach selection, impaled
  exclusion, fallback target selection, half-model summon offset, blocked-LOS
  retry and the four independent diagonal storm trails. UnitAI.h is retained as
  an engine reference. The geometry uses Auriga's planar encounter coordinates.
- The encounter-owned multi-aura ledger preserves simultaneous effects, caster
  ownership, periodic phases, stack caps and explicit removal reasons. It has
  bounded catchup and reentrant callback tests. It is shared lifecycle machinery,
  not a completed interpretation of every aura ID and not ECS-connected yet.
- All five ICC domain suites pass with the native FreeBSD C++ compiler. All
  eight changed server translation units pass isolated syntax checks against
  the server's existing build settings. This is not a full linked server build.
- Main Unity script compilation exits successfully. No playmode/multiplayer
  proof or production deployment is inferred from a compilation pass.
