# Static artwork and native localization

The running client no longer loads the numeric item-name/range tables and Metin2Language.TryGetItemText has been removed. Canonical item names and descriptions use the existing localization keys. Mob, skill, map and UI localization remain intact.

The player artwork catalogue exposes explicit model/icon path lookups. Numeric source-record lookup and prototype-based portrait/book repair are Editor/import tools only. The old synthetic book range and Dragon God lookup fallbacks are gone from runtime. Existing book and Dragon God artwork was already baked into the source catalogue. The offline bake also fixed 272 existing Tera portrait references. Every model path/reference and custom icon was preserved. SRC/Tools/ItemCatalogBuilder/bake_unity_artwork.py performs the deterministic bake without reserializing unrelated assets. Future catalogue imports already bake the Editor repairs.

Archived source text is available only to the offline asset-import tools. Historical loot measurement conversion and immutable original backups remain unchanged.

Validation: player and Editor compilation; 39 artwork checks including all 15 language loads, native item/NPC text, books, Dragon God icon, explicit money/model paths and 272 portraits; deterministic bake/custom-art preservation test; 13 receipt and 46 measurement checks. Model references were compared byte-for-byte before/after the bake. Connected chat, whisper and tooltip flows use native metadata.
