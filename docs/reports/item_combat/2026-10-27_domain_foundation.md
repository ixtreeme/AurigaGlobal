# Item/combat migráció — első implementációs lépés

Dátum: 2026-10-27. Kiinduló HEAD: `ac4a7cf3aa9820745c3901039d7cee16ac0b1ec5`.
Ág: `codex/mission-engine`.

**Készültség: az izolált domainkulcs/codec és a kategóriaséma alapja elkészült.
A teljes P0/P1, az új katalógus és a harcmotor még nincs kész.** A felhasználó
kérése szerint ez a munka a Mission Engine véglegesítése előtt történik.

## Ténylegesen elkészült

| Elem | Megvalósítás |
|---|---|
| Kategóriás prototípuskulcs | `common/item/ItemKey.hpp`: validált uint16 kategória + uint32 local ID. Nullás/ismeretlen kategória és nullás local ID elutasítva. Különböző kategóriák azonos local ID-ja külön kulcs. |
| 64 bites példányazonosító | `ItemInstanceId.hpp`: nullát tiltó, külön erős típus; nem konvertálható ItemKey-vé. A 32 bites és signed64 határon túli értékek is megmaradnak. |
| Packed64 | Veszteségmentes ItemKey-reprezentáció, felső 16 bit ellenőrzésével. Nincs régi vnumra visszafordítás. |
| Bináris codec | `ItemWireCodec.hpp`, `SRC/ItemDomain/ItemWireCodec.cpp`: explicit little-endian, ItemKey 6 bájt, instance ID 8 bájt. Pontos hosszellenőrzés, unaligned span kezelése, nincs struct-layout/sizeof/reinterpret_cast wire szerződés. |
| Közös kategóriaregiszter | `config/item_combat/category_registry.json`: a specifikáció 18 kategóriája, draft státuszban. Egyetlen inputból képzett C++ enum/descriptortábla és SQL seed. A kiadott végleges ID-k freeze-je első release előtt kötelező. |
| Generátor | `SRC/Tools/ItemCatalogBuilder/generate_item_categories.py`: duplikált JSON tag/ID/kód/név, hibás szélesség és azonosító elutasítva; determinisztikus sorrend. A `--check` írás nélkül észleli a header/SQL driftet. |
| `protos` induló DDL | `000_protos_database.sql`, `001_category_registry.sql`, generált `002_category_seed.sql`: külön adatbázis, kategóriatábla, kategóriánkénti sorzárolásra alkalmas számláló. A 4294967296 exhaustion sentinel, nem kiadható uint32 ID. |
| Build | Önálló `AurigaItemDomain`, `ItemKeyTests`, `ItemCategoryRegistryCheck`; fő repositoryból és külön CMake-projektként is fordítható. `ServerTests` építi az új C++ tesztet is. |

A számláló DDL és a referenciatranzakció tesztelt; a végleges CatalogBuilder
prepared tranzakciós ID-foglalója még nincs implementálva. A SQL-rész **nem a
teljes prototípus-/instance-séma**, és nincs bekötve a csomag `init-db` műveletébe.

## Ellenőrzés és eredmény

- A repository buildjében és ASAN buildjében az új `ItemKeyTests` lefordult.
- A C++ suite az összes uint16 kategóriaértéket vizsgálja, továbbá endian goldeneket,
  felső biteket, határértékeket, hibás méreteket és 64 bites instance ID-kat;
  összesen **197442 ellenőrzés**, nem ennyi önálló CTest-eset.
- A generátor Python suite-ja **5/5 sikeres**; a header és SQL egyaránt része
  a változatlansági/driftellenőrzésnek. Hiányos/hibás bemenet nem írja felül
  a korábbi generált fájlokat.
- Az új SQL suite **5/5 sikeres** saját, ellenőrzött tulajdonú MariaDB **11.4.13**
  példányon. Minden kategória számlálója külön 1-ről indult. Nyolc párhuzamos,
  sorzárolt foglalási/publikálási tranzakció 1–8-at adott ütközés nélkül.
- A CHECK, egyediség, FK és exhaustion-határok ténylegesen kikényszerültek.
  Újraalkalmazott seed hibával megállt, és nem nullázta a számlálókat.
- `player` → `protos` összetett FK, két adatbázist érintő prepared SQL commit,
  rollback és observer connection ellenőrzés sikeres. Az `ic_audit_*` táblák
  kizárólag fixture-ek, nem a végleges itemmodell.
- A C client `mysql_stmt_prepare` alakellenőrzése és a szerveroldali
  `PREPARE`/`EXECUTE ... USING` is lefutott. Az új CatalogBuilder saját
  `S_SQL` bound-param integrációját ez nem helyettesíti; az még későbbi munka.
- A teszt runtime-felhasználó a katalógust olvasta, módosítást/törlést és
  ID-foglalást nem végezhetett.
- Integrált célzott regresszió: **8/8 normál**, **7/7 ASAN buildből futtatott**
  CTest sikeres. Az ASAN buildben nincs SQL sandbox regisztrálva; a DB-engine
  nem ASAN-instrumentált, a SQL proof a normál izolált suite-ból származik.
  A meglévő item/combat/Mission core/reward/resource tesztek is ebben a körben futottak.

Az SQL teszt session-mode-ja:
`STRICT_ALL_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION`.
Az aktuális globális mode és környezetlelet a privát sandbox
`item-combat-environment.json` fájljában maradt, credential nélkül.

## Reprodukálás

```powershell
python -B SRC/Tools/ItemCatalogBuilder/generate_item_categories.py --sql-output db/migrations/item_combat/002_category_seed.sql --check
cmake --build build --config RelWithDebInfo --target ItemKeyTests --parallel 2
cmake --build build-asan --config RelWithDebInfo --target ItemKeyTests --parallel 2
ctest --test-dir build -C RelWithDebInfo -R "^(item_key|item_category_generator|item_combat_schema_foundation|item_runtime|combat_state|mission_core|mission_rewards|mission_resources)$" --output-on-failure
ctest --test-dir build-asan -C RelWithDebInfo -R "^(item_key|item_category_generator|item_runtime|combat_state|mission_core|mission_rewards|mission_resources)$" --output-on-failure
```

Külön domain build:

```powershell
cmake -S SRC/ItemDomain -B build/item-domain-standalone -G "Visual Studio 18 2026" -A x64
cmake --build build/item-domain-standalone --config RelWithDebInfo --target ItemKeyTests
ctest --test-dir build/item-domain-standalone -C RelWithDebInfo --output-on-failure
```

Az izolált SQL CTest csak Windows alatt, az `AURIGA_TEST_MARIADB_BIN_DIR` cache
érték megadásakor regisztrálódik. Közvetlenül is futtatható:

```powershell
python -B tests/server/ItemCombatSchemaTests.py --mariadb-bin D:/AurigaGlobal/Server2-Local/tools/mariadb/bin --work-dir build/item-domain-20261027/sql -v
```

Ez a parancs mindig új sandboxot indít, nem fogad meglévő SQL endpointot/datadirt.
A részletes naplók a `build/item-domain-20261027` és a CTest által jelzett
privát sandboxok alatt vannak; a követhető hash-lelet:
`2026-10-27_domain_foundation.evidence.json`.

## Nem változott és még nem igazolt

A GameServer/Database jelenlegi item- és harci útvonala, a Unity-kliens és a
meglévő játékadatbázis még nem használja az új modult. Nincs kompatibilitási
adapter, régi motorra visszakapcsoló flag vagy félkész új runtime release.
Távoli kapcsolat, feltöltés, telepítés és éles adatimport nem történt.

Nem készült új harcmotor, teljes `protos` tartalom, offline konverzió, worker
commit, tartós DeathId/event journal vagy új kliensprotokoll. Nem futott új teljes
GameServer/Database build, FreeBSD cross-build, teljes end-to-end klienspróba,
benchmark, restore vagy 100%-os tartalom/referenciakonverzió. A checked-int128
Boost-függőség hiánya változatlan P3-előfeltétel.

A régi C++ Launcher és Mysql2Proto a `SRC/CMakeLists.txt` alapján már kivezetett;
nem állítjuk vissza őket. A megmaradó kliensintegráció a Unityre vonatkozik.

## Következő munkacsomag

1. Folytatódik a megmaradó item store, apply/formula, timer/socket/soul és
   közvetlen writer szemantikai auditja. A tartalmi leképezés hiánya az adott
   tartalom publikálását blokkolja.
2. Következik a típusos `protos.item_base`, kategória- és policy/profiltáblák,
   refinement profile, valamint immutable catalog validátor. A base→child
   teljességet és az összes profillookupot a builder is ellenőrzi.
3. Ezután az új instance perzisztencia/owner/container/revision és CatalogBuilder
   közvetlen prepared ID-foglalása, az elkülönített MariaDB integritástesztekkel.
4. Az új fixpontos CombatKernel a specifikáció szerinti későbbi fázis; a régi
   motor cseréje, Mission/Dungeon és Unity bekötése koherens release része lesz.
