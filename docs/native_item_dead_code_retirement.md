# Retired item surfaces removed

Removed unused numeric potion/PvP predicates from the arena, the old guild creation item constant, five orphaned item ECS events, the unused database IProto column enum, the Editor-only CubeRenewal payload decoder and the unused numeric tooltip price helper. Native item admission, arena policy and typed tooltips remain active.

Quest package preparation removes the retired item namespace and obsolete item grant/count/removal whitelist entries. The reviewed local whitelist loses 62 unavailable function names. Native pc.typed_item_* operations, select_item and mob/NPC identifiers remain. Nine commented raw reward calls were removed from the Meley source; quest behavior is unchanged.

Validation: GameServer and Database compiled; 9 native test suites passed (ground receipts, typed grants, equipment admission, construction, quests, trade, affects, daily rewards and native guild materials). Quest source migration passed 5 tests; focused whitelist filtering and offline measurement manifest tests also passed. The actual local quest package validates as 76 quests / 591 handlers with the cleaned whitelist. Unity Editor and non-Editor player compilation passed. Connected chat/whisper, linked tooltips, committed loot receipts and measurement/reconnect flow passed against the resulting binaries.

Historical SQL/import tools, reserved packet rejection and read-only artwork identities remain intentional. They do not provide an old gameplay item backend. Version 1/2 measurement files have an isolated one-way importer so historical data is not lost.
