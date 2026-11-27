# Canonical ground-item presentation

The GameServer now sends version 3 of GC_TYPED_GROUND_ADD (226) with the canonical key, quantity, refinement, localization key, authored model path and optional direct Yang value. Unity stores that native identity and resolves the model by explicit asset path. Ground-item language changes no longer call a prototype-name resolver. Yang retains its amount through language changes. Ownership, pickup distances, persistent item identity, atomic drop/pickup and existing VID pickup/delete packets are unchanged.

Both decoders still read the historical typed v1/v2 formats. The old raw ground-add frame stays rejected. Name/model lengths and character sets are bounded, malformed or truncated payloads are rejected. Native/Unity decoding is pinned by a shared v3 golden fixture.

Validation: GameServer/Database compilation; 42 native ground assertions; 123 Unity ground assertions including real model instantiation and pose, ownership, click pickup, HU/EN name changes, Yang amounts, retired-frame rejection and truncation. Connected local two-account flow verified server-authored v3 presentation, actual model renderer, HU/EN label and exact stack count, native pickup receipt, and preserved linked tooltips. The reduced test project has a previously known skill-3 prefab exception unrelated to item presentation.
