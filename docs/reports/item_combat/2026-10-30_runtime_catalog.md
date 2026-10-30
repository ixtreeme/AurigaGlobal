# F4.1 — A GameServer induláskor a protos katalógust tölti be

2026-10-30. Ág: `ccr-6afcbd39-c7bgeq`, kiinduló HEAD `235de385`.
Döntés és az F4 bontása: [IC-034](../../adr/item_combat_034_gameserver_runtime.md).

## Elkészült

- `GameServer/item/ItemCatalogRuntime.hpp/.cpp`:
  - a katalóguskötés (`player.item_persistence_format`) szigorú dekódolása;
  - a kötött katalógus betöltése külön, `REPEATABLE READ`, `READ ONLY`
    munkamenetben;
  - egyszeri, változtathatatlan közzététel;
  - olvasható hibaszövegek.
- `core/config.cpp`: a GameServer a player-kapcsolat után betölti a
  katalógust. Bármilyen hibánál naplóz és kilép; nincs visszaesés az
  `item_proto`-ra.
- Konfiguráció (`Core/server_config.cpp`, `GameConfig`): új `sql.protos`.
  Alapból a player-kapcsolatot örökli `protos` adatbázisnévvel, részlegesen
  felülírható. Más név vagy más végpont hibás. A Database nem kapja meg.
- A GameServer most először linkeli a típusos katalógust
  (`AurigaItemCatalogSql`).
- Tesztek:
  - `ItemCatalogRuntimeTests` (CTest `item_catalog_runtime`);
  - MariaDB-próba `ItemCatalogRuntimeTests.py` (CTest
    `item_catalog_runtime_mariadb`, Windowson `AURIGA_TEST_MARIADB_BIN_DIR`-rel);
  - bővített `ServerConfigTests`.
- Dokumentáció: `server_configuration.md`, IC-034.

## Ellenőrzés

Ebben a körben Linuxon (g++ 14, MariaDB 10.11) futott minden. Részletek
lent, a „Környezet” részben.

| Ellenőrzés | Eredmény |
|---|---:|
| GameServer, Database build az új katalógusmodullal | sikeres |
| `ItemCatalogRuntimeTests --completion-only` | 48 ellenőrzés |
| `ItemCatalogRuntimeTests` saját MariaDB-ben | 217 ellenőrzés |
| Ugyanez AddressSanitizerrel és UB-sanitizerrel (szivárgásfigyeléssel) | 217, hiba nélkül |
| `ServerConfigTests` | 111 ellenőrzés |
| `PersistentItemTests`, `ItemPersistenceSqlTests --completion-only` | 86 + 13 |
| Meglévő `ItemPersistenceSqlTests` a helyi MariaDB-homokozóban (a homokozó hitelesítésére) | 947, mint a Windowsos jelentésben |
| Valódi GameServer-indítás a homokozó adatbázisán, 3 eset | mind a várt módon |

**A MariaDB-próba lefedi:**
- **Kötés nélkül:** a telepített, de kötés nélküli séma elutasítva; a GameServer
  nem választ „aktuális” katalógust.
- **Sikeres indulás:** a kötés a player-kapcsolatról, a katalógus a csak
  `protos`-t olvasó fiókkal töltődik. Revízió, hash és tárgyszám egyezik.
- **Munkamenet:**
  - `READ COMMITTED` alapállapotból is `REPEATABLE READ` lesz;
  - a munkamenet csak olvasó, és nem marad nyitott tranzakció;
  - írási kísérlet még teljes jogú fiókkal is elutasítva (MariaDB 1792).
- **Közzététel:** egyszer sikerül; a második kísérlet hiba, és a közzétett
  katalógus nem változik.
- **Eltérések, mindegyik elutasítva:**
  - a kötés után módosított tárgy (`HashMismatch`);
  - másik katalógusra mutató kötés;
  - más formátumverziójú `protos` (`FormatMismatch`);
  - hiányzó mentési tábla (SQL 1146 → külön üzenet).
- **Visszaállítás:** minden eset után az eredeti állapot újra betölthető.

**Dekóder-határesetek** (adatbázis nélkül):
- beágyazott NUL bájtot tartalmazó hash;
- `uint64` maximum;
- rossz sémaverzió;
- 0, előjeles, szóközös, túl nagy vagy nem szám revízió;
- 31 és 33 bájtos hash, NULL mezők;
- két sor, rossz oszlopszám, szöveges protokoll, hiányzó eredményhalmaz.

**Valódi GameServer-indítás** (a teszt után megmaradt, kötött adatbázison):

| Eset | Napló | Kilépés |
|---|---|---|
| Kötött katalógus | `ItemCatalog: revision 31, hash d06b2c17…, 17 items`, utána `LogSQL connected` | Később, a homokozóból hiányzó `common.locale` tábla miatt; ez a katalógus után van |
| Katalógus a kötés után módosítva | `protos catalog rejected: catalog content differs from the pinned hash` | 1, a LogSQL előtt |
| Kötéssor törölve | `player.item_persistence_format has no catalog pin row` | 1, a LogSQL előtt |

## Környezet és ami nem futott

- A repó Windows (MSVC) és FreeBSD célú. Linuxon csak helyi, a repóba nem
  került pótlásokkal fordult:
  - `srandomdev` és `optreset` (BSD-specifikus);
  - hiányzó `<cstring>` include a `tables.h`-ban;
  - a `Core/signal.cpp` Linux-ága.

  Ezek a meglévő kódra vonatkoznak, nem az F4.1-re.
- A LoginServer Linuxon a meglévő `login_hwid.h` miatt nem fordul (`uint32_t`
  include hiányzik). Az F4.1 nem érinti.
- A Windowsos MariaDB-tesztmotor (`db/tools/backup_sandbox.py`) csak Windowson
  fut. Itt egy ugyanazt a sémát és felhasználókat létrehozó helyi homokozó
  futott. Hitelesítésként a meglévő mentési teszt pontosan a jelentett
  947 ellenőrzést adta. Az új `ItemCatalogRuntimeTests.py` Windowson még nem
  futott.
- MSVC build és a teljes CTest nem futott ebben a körben.

## Határ

- A betöltött katalógust a játékmenet még nem használja. A régi boot
  `item_proto` és a régi tárgyút változatlan (F4.3–F4.7).
- Futás közbeni katalógusváltás nincs, újraindítás kell.
- A GameServer ettől a buildtől csak olyan környezetben indul, ahol megvan:
  - a v24-es `protos`;
  - a `player` mentési táblái;
  - a kötéssor.

  Részletek: IC-034, „Üzemeltetési hatás”.

## Következő kapu

F4.2: tárgytulajdonosi munkamenet. Előtte a felhasználó döntése kell arról,
ki írja a tárgyakat (IC-034, javaslat: a GameServer közvetlenül).
