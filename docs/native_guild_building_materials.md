# Native guild construction recipes

Construction loads category/local/count arrays from object_proto.native_materials.
The Database and GameServer boot version is 9; both endpoints must be updated
together. Invalid or unconverted material data stops prototype loading. The
running server does not read or convert the old materials column.

Migration 058 adds the native recipe column and an import receipt. The explicit
offline migrate_guild_building_materials.py tool preserves reviewed historical
costs and refuses unknown materials. It imports atomically with an advisory
lock; repeats are no-ops after a successful receipt. The current local plan is
60 existing recipes, all previously zero-material recipes, without repricing.

The item writer locks and checks the exact native recipe, price, land revision
and authority before committing item/wallet debits and object creation. Changed
recipes fail closed. Altar upgrades retain their native material-key contracts.

Unity's GuildBuildings JSON contains building presentation and rotation data
only. Server quotes own the price and material quantities; old item-ID material
parsing and the old text resource are removed. Inventory checks use native keys.

Verification: 47 native tests and 6 importer tests passed. Unity 6000.3.27f1
Editor/player compilation and the guild UI validator passed, including desktop
and mobile window wiring and 49 real building models (nine unconverted tree
models remain the pre-existing documented exception).

Connected isolated fixture game64 passed with the old materials column made
unusable before startup: native quote, two visible buildings, exact material and
Yang debits, refusal of outside/duplicate/insufficient-material requests, and
exact persisted state after reconnect. The test runs the preserved Unit 1
Database/GameServer images. The reduced asset fixture still logs its unrelated
missing skill-recipe prefab warning.

No item catalog or unrelated pending work changes. Deployment is coordinated
with the other retirement units so both boot-v9 endpoints update together.
