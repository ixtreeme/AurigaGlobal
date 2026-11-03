# Mount flight

Branch: ECS_+_BGFX. Main Unity project and live FreeBSD server integration.

The actually worn costume-mount summon item enables flight when `item_proto.value5 == 1`. All 445 existing mount records had value5=0 before this change. The approved migration enables 84 TERA mounts with an original FlyForward clip, compared without case sensitivity. Other prototype columns and records remain unchanged. `flying-mounts.json` contains the exact IDs; `flight-capability.sql` updates existing records and follows the original TERA prototype insert migration. Updated CSV/TSV exports and the release builder retain the flag for future imports.

Hold either Shift to ascend at 6 m/s and either Ctrl to descend. Release both to hover. WASD remains horizontal, at the existing mount speed. Altitude is an absolute logical world height, with the rider foot offset preserved. Hills and cliffs do not change altitude. Higher terrain blocks forward movement; descending onto the current surface lands. Unmounting and death end flight. Reconnecting or changing map requires a fresh game-phase handshake.

The original movement packet layout is unchanged. CG225 is a packed 10-byte request (header, action, int32 altitude in cm, uint32 client time); actions are hello=0, flight=1, land=2. GC225 is 11 bytes (header, uint32 VID, capability/airborne flags, int32 altitude, int8 vertical axis). The server derives capability from the worn item, checks movement/death/mount state, and limits accepted vertical changes with a 6 m/s token budget capped at 2 m of network jitter. Sender timestamps cannot enlarge that budget. Existing XY movement validation remains active.

Supported clients announce the extension after ENTERGAME. Snapshots go to self and supported visibility viewers, and are included in actor insertion for late arrivals. The client retains snapshots while actor prefabs are loading and interpolates remote altitude. Older clients receive no unknown GC225 packets; all players observing airborne movement need the updated Unity client code.

The initial terrain-height reference is sampled by the client because the existing server maps do not contain Unity terrain heightmaps. Terrain collision remains client-side. Canonical flight altitude is stored in the server flight component, not in persistent character position data. Combat retains the existing XY distance rules.

Original FlyUp, FlyForward, FlyWait and FlyDescent clips are selected with cached animator state IDs; the existing source-linked effect and audio presentation follows those clips. Main Unity Editor testing uses the established Asset Database play mode. No public Windows launcher/player update is published by this integration.

Validation: 21 client checks exercise CG/GC framing, fragmented input, permission, rise/hover/descent, hills/cliffs, terrain obstruction, nonzero rider foot offset, two receiving worlds, late actor creation, revocation, and real Gemfly 630090 flight/hover animations. The native MountLifecycleTests exercise server permission and state validation. The release-data suite verifies exactly the 84 approved mount flags. Live installation preserved other proto fields and returned all five previously running server processes.

Binary backup: /Auriga/Server/run/deployments/8c745cd105689c6e8421191d/backup. SQL backup/audit: /Auriga/Server/run/mount-flight-release/.

Gemfly regression: flight permission packets now resolve the local player as well as remote actors. The client announces flight support after the game phase is ready. The 21-check regression routes permission through the real world packet handler.
