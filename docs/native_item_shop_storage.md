# Native SÉ shop storage

The running shop reads ishop_offer, its native-key members, immutable
ishop_purchase_archive and native purchase receipts. It no longer joins
ishop_data, ishop_log or item-ID conversion tables. Unity's panel no
longer takes an artwork/proto catalog: offer and history details, tooltips
and previews use the existing native metadata packets.

Apply native foundation migrations, then 056 and 057. Fresh installations
can author offers directly without any legacy shop table. Existing servers
run SRC/Tools/ItemCatalogBuilder/migrate_item_shop_storage.py explicitly
after review, with the game services stopped. Preview is the default; --apply
performs an atomic import into empty native tables. A named lock and immutable
source-hash receipt prevent duplicate imports. Re-running needs no source
tables once that receipt exists. Never commit the private connection file.

The import preserves offer IDs, prices, discounts, dates, sales baseline and
rolling weekly/monthly limits. Native history cohorts preserve the old shared
historical limit arithmetic without retaining prototype IDs. Duplicate history
rows survive; ambiguous or unmapped historical items retain payment/date/count
but have no invented item identity. Existing native receipts are untouched.
The old merchandising tables can remain archival; runtime never consults them.

Local reviewed import: 485 offers and 485 members, no old history rows.
31 already inactive/unmapped offers remain inactive. Catalog content, inventory
and unrelated pending drafts are outside this change.

Validation: Release Database/GameServer build; 46 native tests; 10 import tests
and 5 historical converter tests; Unity 6000.3.27f1 Editor/player compilation.
Unity validators passed: shop 21, tooltip 68, typed item integration 14090,
observer/cache 13, native stole 14, actual costume meshes 403.

Connected isolated fixture game58 imported duplicate and unmapped history,
then made every old shop table unavailable before starting the server. The
Unity client bought native items using full 32-bit offer IDs; verified wallet,
receipts, old-history purchase limits and refusal; read both archived and new
history; and confirmed the balance across reconnect. Observer equipment and
real stole equip/unequip passed in both clients. The ordinary native move,
partial drop/pickup, trade, recovery clocks, live hover and expiry flows passed.

The broader pre-existing ServerManagerEditorTests suite is blocked by missing
ServerPackageCheck and stale quest-API expectations; ItemBenchToolTests assumes
exactly migrations 1..44. Neither suite was changed to hide those failures.
The reduced asset fixture also logs an unrelated missing skill-recipe prefab.
