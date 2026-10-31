# Eclipse raid: twelve-encounter implementation

Server controllers are instance-owned C++ (no Lua), with real main-client actor wrappers and original decoded WoW spell models/audio. Upstream source attribution is preserved in `raid/icc` and `docs/eclipse_raid/upstream`.

| Room | Fight | Main mechanics |
| --- | --- | --- |
| 1 | Bone guardian | Coldflame, breakable impaling spikes, Bone Storm, berserk |
| 2 | Veiled regent | Mana barrier, cultists/reanimation, interrupts, mind control, phase two |
| 3 | Skybridge guardian statue | Three aiming cannons on carriages, muzzle-origin projectiles/recoil, heat/charged shots, freezing mage, boarders, repair and statue health |
| 4 | Blood champion | Blood power, permanent marks, marked death healing, blood beasts, rune, boiling blood, frenzy |
| 5 | Plague breather | Gas intensity/inhale, spores and inoculation, Pungent Blight, tank bloat |
| 6 | Ooze guardian | Infection/dispels, merging oozes, growing ooze danger, spray, explosion |
| 7 | Alchemist | 80/35% transitions, growing puddles, mutation energy, green/gas experiments, heroic plague, final tank plague |
| 8 | Three princes | Shared health/active-prince immunity, nuclei ownership/resistance, kinetic bomb altitude, seeking flames, vortices |
| 9 | Blood Queen | Essence/thirst/player bite, control on failure, tank mirror, blood pact, shadow trails, airborne volleys |
| 10 | Dreamwalker | Friendly dragon healing, five add types, suppression, dream portals/clouds, delayed healing victory, death failure |
| 11 | Frost dragon | Air phase/four bombs, real ice tomb roots and geometric cover, grip/25m Cold escape, skill instability, 35% Mystic Buffet |
| 12 | Final king | 70/40% winters, jumping plague, Infest healing threshold, carriers/rescue, hit-growing Defile, spirits, soul realm, 10% scripted ending |

All four difficulty modes are supported. Server prerequisite masks enforce the branching progression, including the three-wing requirement before room12. Wipes release owned roots/auras, temporary actors and client effect/audio leases. Instance retirement returns players to their entry location.

## Intentional Metin adaptations

This is not an exact reproduction of every WoW engine subsystem. Room3 is a ground-level skybridge siege rather than two moving vehicle ships. Room10 uses a 100-SP healing action for all Metin classes and a local dream alcove. Room7 mutation is an empowered, green-tinted player role, not a full abomination model swap. The final king uses a local soul encounter and a shortened scripted rescue, without the original movie. These adaptations preserve combat decisions within the existing Metin movement, skill and map systems.

## Validation and live state (2026-10-29)

-26 deterministic server tests pass, including all encounter controllers and their actual world adapters across four modes.
-Unity isolated Play Mode loads the three new animated bosses,28 further native actor assets, original spell visuals/audio, cleanup and the final four HUD protocols. This is not a connected30-player playthrough or a mobile performance certification.
-Approved map430 remains owned by ch1-core1. All12 encounters are enabled in the live configuration. Five managed server processes were verified alive after installing the tested FreeBSD binary.
-Temporary completion reward:500000Yang per participating character, once per run. Configure `data/eclipse_raid.json` to replace the amount.
-Windows/Android player builds and patch publication are separate; this change updates the Unity project and live server.

## Test entry

On CH1: `/eclipseraid enter 10_normal` (also `25_normal`, `10_heroic`, `25_heroic`). Use Checkpoint, Ready, then Start Fight. Every participating player must be nearby and ready. After a kill, Checkpoint advances to the next unlocked encounter. E performs the room-specific interaction; the HUD exposes targeting, cannon charge and mutation actions where relevant.

### Statue siege presentation correction
The room3 main target is now an Auriga guardian statue, not a second cannon. Cannons use the posed original barrel mesh, detailed wagon wheels, separate aiming/recoil transforms and a real muzzle anchor. `er_siege_target` supplies the authoritative statue VID to every client; each shot names the firing cannon as its source. Native projectile/impact timing is750ms.
