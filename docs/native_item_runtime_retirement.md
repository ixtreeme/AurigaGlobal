# Final native item runtime boundary

The gameplay runtime now has no raw prototype grant/consume reward format,
item-award informer, old resource writer, raw item logger or unique-item
constant catalog. Retired quest event and wire numbers remain reserved so
persisted indices are never shifted or reused. Receiving retired item packets
cannot create or change native inventory.

Unity daily gifts accept only the native reward command. Character equipment
accepts model assets, not imported item entries. Costume effects accept only
the server-authored native profile. Raw equipment catalog lookups and artwork
tooltip previews remain available to Editor import/asset tools only; they are
absent from the standalone player assembly. The unused accessory-material
calculator and old sound-rule helper are removed.

The native inventory tooltip remains active and shows authoritative description,
counts and specifications on hover. Character race/hair appearances, mob IDs,
skill IDs and stable artwork/model addresses are presentation or world identities,
not old item identities. Historical SQL migrations, archived source tables and
explicit offline import tools are preserved.

Validation:
- Database and GameServer build together with boot protocol 9.
- 57 native/tool checks pass, including isolated MariaDB crash recovery and
  genuinely lost COMMIT acknowledgements without the retired mission tables.
- Unity 6000.3.27f1 compiles both Editor and standalone runtime sources.
- The standalone assembly metadata gate rejects 12 retired API entries and
  retired helper types while keeping the native tooltip/equipment code.
- Shared UI checks cover daily gifts, guild construction, native Mission Pass,
  68 tooltip checks, 3236 mission checks and 3871 item-mission UI checks.
- Real-model checks cover 403 costume assertions, 13 observer appearance
  assertions and native body/weapon effects through mount, unequip and stale loads.
- The connected TLS client/server proof covers the canonical shop,
  native hover, 64-bit identities, move, partial drop and pickup, two-account
  partial-stack trade, persisted reconnect, ordinary versus retired effects,
  automatic recovery, online/wall/worn-time clocks, costume expiry, the native
  mission journal, all 80 Mission Pass reward definitions, premium refusal,
  one durable claim, duplicate refusal, native Mall withdrawal and reconnect.

The reduced fixture reports the pre-existing missing skill-3 recipe prefab;
it does not affect these item flows. The proof records retain the initial read-only quote timeout and its successful
reconnect/withdrawal continuation. The final full-flow run permits only explicit
transient owner-write guards to defer read-only quotes. The release proof identifies the
exact tested binaries and scenario. The production Mission Pass stays disabled;
the local publication adds its native schema without enabling a season.
Unrelated pending Studio/localization work and deferred item decisions are preserved.
