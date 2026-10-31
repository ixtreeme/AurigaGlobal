# Second encounter: the Veiled Regent

Server adapter: `raid/EclipseDeathwhisperRuntime.hpp`; source-derived phase scheduler: `raid/icc/Deathwhisper.hpp` (AzerothCore attribution retained).

## Live test

Use CH1 and `/eclipseraid enter 10_normal` (also 25_normal, 10_heroic, 25_heroic). The existing entry routing transfers players to the dungeon core. Clear encounter 1 normally. Its cleared bit unlocks room 2 and its client door; room 2 cannot be started before this. Walk to the next room, or use Checkpoint after the first kill. All members must be alive, ready and within 55 metres before the leader starts encounter 2. The countdown is revalidated.

Phase one: stationary regent, mana barrier, Shadow Bolt, eight-metre Death and Decay, cultist waves (3/7), empowerment, martyrdom and reanimation. Barrier damage consumes mana and only overflow reaches HP. Phase two: threat reset, pursuit/melee, Frostbolt, Volley, Touch threat stacks, pursuing explosive shades. Heroic modes retain smaller add waves. Mind control excludes the tank (0/1/1/3 targets by mode); the Auriga adaptation immobilizes the controlled player and directs hostile shadow attacks at nearby companions. Interrupt Frostbolt using the panel or E action: 20m, 50 MP, 12-second cooldown. Cultist spells include healing absorption, casting slow and a timed shield. Reanimated adds have broad damage resistance as an Auriga simplification. Ten-minute berserk remains.

Wipes remove minions, missile leases, pools, roots and transient native affects. Rejoin republishes current phase/barrier. Missing/dead shade targets are reassigned; dead aura owners stop rendering.

Client: native animated ice witch, skeleton melee/mage and chaos-ghost models; casting reuses real native attack motions. Original 12340 effect data is rendered by the dedicated `Metin2.Runtime.RaidEffects` renderer. Ground effect animation is constrained to the exact server radius; this is not a claim of original-client visual equivalence. All UI text uses locale keys (HU/EN additions, existing fallback for other locales).

## Verification

`ICCDeathwhisperTests`: four difficulty modes, barrier/overflow/one-time transition, waves, phase casts, shades, berserk, prerequisites, casting guard.
`ICCDeathwhisperRuntimeTests`: actual adapter against a deterministic world fixture; four modes, damage/cast packets, adds/shades, shield transition, wipe cleanup and state publication.
Existing first-encounter tests remain green. Native FreeBSD GameServer compilation and live binary hash/process checks are separate deployment gates.
`Metin2DeathwhisperValidation.Run`: Unity Play Mode actor motion binding/hitboxes, effect protocol, animated radius agreement and cleanup. Does not claim a completed connected multiplayer acceptance test.

Only encounters 1 and 2 are enabled; full-raid completion rewards remain disabled during GM-led testing. No Windows/Android patch is implicitly published by these changes. Editor uses the imported assets directly.
