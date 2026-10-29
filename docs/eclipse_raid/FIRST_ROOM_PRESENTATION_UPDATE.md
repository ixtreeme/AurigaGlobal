# First-room presentation update — 2026-10-27

- First boss uses DemonBoss4 with its own generic skeleton and original Idle1,
  walk1, Run, attack1, gethit1, Death1 and Rage clips. The other three original
  attacks are registered as additional special motions. Earlier procedural
  Tamahakan test gestures are no longer used by this actor.
- Target height: 10.8 metres, six times the 1.8 metre reference. Size is applied
  on an unanimated parent so animation curves cannot reset the scale.
- All 15 source TGA textures were physically reduced to 1024 square, preserving
  GUIDs, alpha channels and material references. Normal maps were renormalized.
  Total source bytes: 805,306,768 -> 36,233,182. Original textures and metadata
  are backed up outside the project under
  `D:/AurigaAssetBackups/DemonBoss4-original-4k-20261027`.
- Coldflame and storm use authored Procedural Fire / Free Quick Effects Vol. 1
  particle prefabs. Imported audio/scripts/lights/colliders are removed from the
  isolated variants. Complex coldflame assemblies have a 48-instance budget.
- Bone Storm damage follows source distance falloff across the arena. Its old
  hard nine-metre boundary was misleading and is hidden; status text explicitly
  states that room-wide damage weakens with distance. Coldflame remains a
  five-metre patch with an explicit thin boundary.
- Pin/unpin presentation is broadcast to participants. Only the visual body is
  raised; actor coordinates remain server controlled. Release/reset restores it.
- GM-only `/eclipseraid test_spike` during first-boss combat explicitly pins the
  issuing GM through the same spike/root/release path as the normal encounter.
  Normal target selection still excludes GMs, the main victim and recent slice
  targets. Use a normal party member for normal spike-targeting acceptance.
- FreeBSD build and all thirteen tests pass. Installed GameServer SHA-256:
  `e5f81ae0612f0ee069a17f00f59633f6f83b21d707a71af3d971ebb61ce3c3f2`.
  Installed binaries and dungeon-core routing were verified after restart.

Client proof files: Library/EclipseDemonBossIntegration.log,
Library/EclipseDemonBossBindings.txt, Library/EclipsePresentationProof.json,
Library/EclipseFirstRoomClientProof.json, Library/EclipseAuthoredEffects.png,
Library/EclipseFirstRoomOverview.png. These local checks do not replace live
party acceptance. No Windows/Android patch was published for this update.
