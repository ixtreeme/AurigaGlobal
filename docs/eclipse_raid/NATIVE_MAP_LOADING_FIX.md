# Native client map loading repair — 2026-10-27

The black raid map and continually decreasing player/camera Y were client integration
failures. The map builder saved TerrainData but no Terrain objects, and the approved
interior was not registered with the outdoor area loader. LoadAt therefore created
empty areas. GroundHeight fell back to the input Y, and the player's foot offset
was subtracted again each frame. The camera collision message reported that falling
position; it was not the cause.

The source map now contains 20 hidden terrain cells using the existing baked height
data. The connected approved interior is explicitly marked as an authored map layer
and instantiated once for the map lifetime, preserving its hierarchy, mirrored
transform, lights and particles. Terrain cells retain normal streaming. Map unload
releases the interior. Room atmosphere is copied to the runtime map. Other maps
without an authored layer retain their existing loading behavior.

Hungarian raid locale had 57 double-encoded values and English had four. Repaired
JSON uses Unicode escapes to preserve accents independent of the source encoding.

Validation:

- Actual Metin2WorldRuntime.LoadAt, not a whole-prefab preview: 1,249 active mesh
  renderers; loaded terrain and all 20 authored terrain sources.
- 8,456 ground samples across room centers, tile boundaries and 600 repeated
  player-foot-offset updates per room, starting at Y = -9099: no height drift.
- Repeated LoadAt does not duplicate the interior; Unload removes it.
- Exact reported 41-byte loading chat packet, embedded font, repeated HUD state,
  buttons, combat state and cleanup: pass.
- First-room visual capture through native LoadAt: eight main-client player races,
  7.2 m boss, two defending spheres, coldflame/storm/spike checks: pass.

Reports and screenshot are in Library/EclipseNativeMapProof.json,
Library/EclipseUiValidation.json, Library/EclipseFirstRoomClientProof.json and
Library/EclipseFirstRoomOverview.png. These are local client regressions; live
group encounter acceptance is still outstanding. No server rebuild or restart is
required for this repair. No Windows or Android patch was published by this repair.
