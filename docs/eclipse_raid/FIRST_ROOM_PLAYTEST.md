# First room — deployed GM-led playtest

Use the updated main Unity project in the Editor. An older Windows/Android
installation does not contain these new client resources.

FreeBSD GameServer was built and copied to `/Auriga/Server/bin/GameServer`.
Installed SHA-256: `67a14fbafd380ca652adfa6f0d99af23032dc05614182037d7610c023d850bd2`.
The service was restarted. Database, login, CH1 core1/core2 and the configured
general core reported listening. Other numbered channels were not enabled.
Map template 430 is assigned to CH1 core1. Its 2048×2560 attribute grid loaded
successfully (`ECLIPSE_ATTRIBUTE_OK`). The encounter configuration logged enabled,
GM-led access and encounter mask 1. There are no completion rewards in this test.

## Enter and start

Log in on **CH1** with a GM character. If grouped, be the group leader and gather
all group members alive, on the same map and within 30 metres.

1. `/eclipseraid enter 10_normal`
2. If transferred to the public raid entrance, repeat the same command after
   everyone arrives. This creates the group's private instance on the hosting core.
3. `/eclipseraid checkpoint` moves to the first room.
4. Every participant uses `/eclipseraid ready`.
5. The leader uses `/eclipseraid engage 1`.

Other source modes: `25_normal`, `10_heroic`, `25_heroic`. The group-size limits
are 10 or 25 respectively. Later rooms cannot be engaged in this deployment.
`/eclipseraid reset` is the GM retry command, including after a kill.
`/eclipseraid leave` returns from the instance. The old `/eclipse` GM event
command was preserved and does not control the raid.

## Ingame acceptance checks

- Boss, bone spike and ground effects appear at the intended world scale.
- Normal melee and targeted skills damage the boss.
- Coldflame propagates and damages players standing in it; leaving it stops damage.
- Bone Storm starts around 45–50 seconds, visibly follows the moving boss and
  hurts more at close range. It lasts 20 seconds in 10-player, 30 in 25-player modes.
- Bone spikes immobilize eligible non-tanks. Other players can destroy the spike
  and release the passenger. Source target selection excludes GM characters:
  include a normal, non-GM second character for this check.
- A group wipe clears effects and immobilization and allows another attempt.
- Reset/leave clears client presentation and restores ordinary control.
- Two groups receive different private instances and cannot affect one another.

## Verified before handoff

Full linked FreeBSD GameServer build; thirteen passing configuration/routing/raid data/domain/runtime
tests; native compiler checks for affected hooks; Unity compilation; production
spawn-pose path and hit-profile validation; rendered main-map screenshot with
all eight production player prefabs; source/built/installed binary hash agreement.
No research Warrior is used. Metric-authored raid actors bypass the normal
centimetre conversion; existing Metin2 actors keep their original conversion.

This is a playable first-room test, not a claim of complete ICC parity. Live
player combat, group wipe and cross-core admission still require the ingame
acceptance run. The current boss movement/death clips are inherited test clips
and still need a visual animation pass. Other encounters, lockouts, progression
and final rewards are not released by this test.

Runtime backup:
`/Auriga/Server/run/eclipse-first-room-20261027_181852/backup`.
Source backups are under `/Auriga/Source/.eclipse_backups/`.

Routing update: all four server programs were installed together. Template 430
was confirmed in the map-location registry on CH1 core1, CH1 core2 and the general
core. The dungeon core remains CH1 core1. See MAP_ROUTING_FIX.md.

Client map-loading repair: the black map and falling player/camera were fixed in
the native loader, and corrupted raid accents were repaired. Native LoadAt,
8,456 ground queries, the reported loading packet/HUD and first-room visual
regressions pass. See NATIVE_MAP_LOADING_FIX.md. Restart Play before retesting;
this client repair does not require another FreeBSD build.
