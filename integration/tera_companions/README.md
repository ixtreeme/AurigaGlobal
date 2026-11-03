# TERA companion release

Target source branch: `ECS_+_BGFX`. Client project: `C:/UnityProjects/AurigaUnity`.

This repository contains the server source plus the release records, approved scale snapshot and client integration source. The Unity asset tree is outside this Git repository; generated meshes, clips, textures, prefabs and bundles live in the client project. `release.json` is the stable cross-system mapping.

## Content

- 203 mounts: summon item IDs 630000–630202, race IDs 32000–32202.
- 69 pets: summon item IDs 631000–631068, race IDs 32500–32568.
- 19 supporting models, assembled through 33 original attachment registrations on 22 mounts. Supporting parts do not receive summon items or independent mob records.
- The approved `totalScale` is copied into each production prefab's `Metin2MetricActor`. Thirteen unsaved models use their lab source scale, explicitly approved by the user.
- Original source motion clips and source-linked materials/effects/audio are referenced by each per-race prefab. Spawn/summon effects and recall/extinction audio are excluded as requested.
- All ten client language tables and prototype locale columns receive names. Summon descriptions are client-localized.
- 756 original SoundCue graphs retain weighted choices, modulation, mixers, loops, explicit volume/pitch, distance attenuation and low-pass settings. Six omitted original wave variants are recovered from the original sound export.
- Each new mob folder has a `mob_motion` record using Auriga's existing follower movement convention and the source attack duration when available.
- Mount items use the existing costume-mount subtype `(28,2)` and permanent unique limit `(10,1)`. Pet items use the existing quest-item pet system `(18,0)`, with the race in value1. Template combat/economy bonuses are cleared.

## Publication

`prototypes.sql` inserts new IDs only, in one InnoDB transaction. Do not replace existing prototype tables or use REPLACE/ON DUPLICATE KEY UPDATE. Before publication, the transaction is executed with ROLLBACK against the configured live database and all three complete existing tables are verified unchanged.

`tera_companions.lua` belongs at `data/quest/Quest/system/tera_companions.lua`, registered once in `data/quest/quest_list`. It follows the existing one-active-classic-pet rule and does not consume the seal. The server manager validates a staged immutable quest snapshot, stops the owned stack, installs verified FreeBSD binaries with a backup, explicitly applies the new quest package, publishes prototype additions, and restores the previously running channels.

## Client import and verification

1. `Migration/TeraCompanions/release.json` and `approved-settings.json` are the authoring inputs.
2. `TeraCompanionReleaseBuilder.Run` publishes native source dependencies outside Editor folders, clones production Animator clips, preserves original socket/attachment connections, writes metric prefabs, per-race data/prefab roots and indices, Addressables registrations and summon item catalog entries.
3. Production presentation converts the lab's +Z model frame to the existing +X native actor convention. The established player wrapper provides its normal -90-degree frame. Existing actor/mount handling remains in use.
4. `TeraCompanionReleaseQA.Run` verifies all new race acquisitions, scales, geometry/ground, materials, saddle frames, motion and cosmetic references, item links, and packet-driven pet creation.
5. `TeraCompanionPlayQA.Run` exercises the production riding path, four headings, saddle pivot, preserved warrior size, actual particle emission and motion audio, with render captures.
6. Open `TeraCompanionReleaseContent.OpenMainForTesting` to test the main client scene against the live server using the existing supported Asset Database play mode. Standalone/public Windows releases require a separate matching content, player and patch publication; no public launcher update is activated by this integration.

Server cross-link tests: `python tests/server/TeraCompanionReleaseTests.py`.

The source material/effect conversion has no running original TERA client as a visual reference. This release does not certify 100% original-engine rendering equivalence.

Verification note: `FX_Q_HotFix_181221.PS.Q_Sound_Dummy_PS_01` is the sole source runtime effect with zero emitters. Its original package (`Effects/Definitions/ee54e3e4_a4bd8f66_189fd.json`) contains two objects and no emitter exports. The validator permits this exact original placeholder; all other visual effects must contain particle systems.

Movement audio includes original `onstartmove`, `onmove`, and `onendmove` semantic events. Persistent movement loops survive compatible animation transitions and stop when movement ends; concurrency is shared with attached body parts. Original null random branches retain their silent selection weight.

Six source Kirin particle meshes had two geometry sections, which Unity particle rendering rejects. Their sections are merged without changing index or vertex buffers (230 triangles, 163 vertices each); GUIDs and prefab bindings are preserved. `fix_tera_particle_meshes.py` is the guarded one-time migration and `TeraEffectsBuilder` now merges sections on future export. All 690 metric particle meshes must pass the single-submesh audit.

Live publication completed: 272 additions in each of item_proto, mob_proto, and mob_motion; all prior rows unchanged. The quest is registered once. Database, login, channel 1 cores and channel 99 returned successfully. Binary backup: `/Auriga/Server/run/deployments/32c82d03ad333aa1025eb898/backup`.

All 272 production race acquisitions, 69 packet-based pet spawns, 690 particle meshes and nine production riding/effect/audio Play cases passed. The optional standalone content build was stopped to reopen the main Editor for user testing; no standalone Windows player or public launcher release is claimed.
