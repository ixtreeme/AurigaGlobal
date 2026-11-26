# Native mission rewards only

Reward payloads accept schemas 2 and 3 and canonical category/local/refinement keys.
The schema-1 raw prototype grant/consume reader, ItemProtoId graph port and raw
reward binding policy have been removed. IDs 1/4 and port 10 remain reserved.

The unused legacy item-award materializer, resource planner, SQL resource writer,
drain and fence adapters have been removed. Mission runtime delivery continues
through TypedMissionItems and the native ItemStore transaction with its owner
fence, checkpoint, entitlement and idempotent operation receipt.

Historical applied migrations and archived tables are preserved. The active
mission persistence guard checks its eight active tables and seven foreign keys;
it no longer requires the four retired writer tables. Schema version 5 remains
valid, so this cleanup requires no live mission-history conversion.

Validation: server and database builds; 57 distinct native/tool checks passed
(the isolated MariaDB test was rerun after removing its obsolete sentinel).
The SQL test runs without all four retired tables, loses two real COMMIT
acknowledgements and checks durable recovery after an actual MariaDB restart.
Native reward tests retain bounded decoding, canonical hashes, pinning,
capability/quantity checks and malformed-input rejection. The current installed
typed_missions.aurmission loads with the new compiler.
Unity gameplay packets and its native mission tooltip contract are unchanged.

The four locally edited MissionPackage files retain the unrelated pending
localization/Studio work. Only the scoped retirement changes are committed.
