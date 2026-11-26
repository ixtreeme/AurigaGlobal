# Final legacy item engine retirement

Server Git main; UnityVC /main changesets 224 (combat/equipment) and 225
(client runtime, SÉ shop, loot measurement and validation).

## Runtime changes

- Database no longer allocates old 32-bit item ID ranges or parses item proto
  rows. The obsolete raw refinement manager, attribute maps, extra-proto
  channel and raw-window cache helper are removed.
- Database-to-GameServer boot version is now **8**. Both endpoints use the
  shared DATABASE_BOOT_VERSION. Boot and proto reload no longer transfer
  raw item/shop/refine/attribute tables or item ID ranges. Wrong boot versions
  stop startup; Database and GameServer must be deployed together.
- The native catalog is required. Server configuration and package conversion
  no longer expose the retired ID allocator or item/pricelist cache options.
- Unity no longer contains the raw owned-item DTOs, PlayerState item bags,
  item mutators, obsolete send APIs, receiver events or old item panels.
  Direct window lookup cannot recreate an old raw-cell item prefab.
- Native inventory, equipment, quick slots, tooltip, trade, storage, forge,
  shops and world loot remain. The SÉ shop consumes typed offers and bundle
  members directly. Its history hydration, pagination and accent-free search
  are covered by a new UI integration validator.
- Loot measurement compares committed native snapshots. Moving items inside
  one inventory does not count as loot; consumption and reconnect/map baselines
  do not create false gains. Historical measurement files remain readable.
- Wiki and SÉ 3D previews use server-authored typed preview descriptors; the
  old local proto/vnum preview decision tree is removed.

Numeric artwork/model/NPC addresses, imported serialized asset records,
historical SQL/import sources and the reserved-packet rejection boundary remain
for presentation, recovery and compatibility. They do not provide a second
owned-item backend. No player inventory, catalog content, deferred item set or
unrelated pending mission/Studio work was changed by this retirement.

## Verification (2026-11-26)

- Release builds: Database, LoginServer and GameServer.
- CTest: **46/46 passed**; configuration tools: 8 passed, 1 optional check
  skipped; physical engine boundary: passed.
- Unity **6000.3.27f1**: Editor and player runtime compilation passed.
- Native client validators: **43,850 checks passed**, plus collection bonus,
  chat-link and retired-packet boundary suites. This includes real converted
  costume meshes, owner/observer presentation and native tooltip hover.
- Connected isolated TLS world using the exact final Database and GameServer:
  character login; authoritative native tooltip/count; 64-bit move; partial
  drop, ground publication and pickup; exact quantities after reconnect;
  two-account trade with both confirmations and persisted ownership; rejected
  old item affects; ordinary affect retained; automatic recovery bottle;
  wall-clock, online and worn-time expiry with reconnect.
- Deleted runtime behaviour GUIDs have no serialized references in Metin2
  scenes, prefabs or assets. The six unrelated Unity pending files and sixteen
  unrelated server pending files retained their original hashes.

The first integration attempt requested a move while the login recovery
transaction was busy. The final test retries only an explicit Busy refusal,
with the same instance and destination, under a six-second bound. Setup-only
fixture failures were corrected before the final successful run.

Evidence is kept outside the source repository in
D:/AurigaGlobal/LiveWork/AurigaMergeBuild/final-item-retirement and
D:/AurigaGlobal/LiveWork/AurigaMergeBuild/quest-redesign-release-20261125
(the final connected fixture is game52).
