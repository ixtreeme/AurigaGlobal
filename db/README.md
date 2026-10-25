# MariaDB-séma az Auriga szerverhez

A [schema.sql](schema.sql) a jelenlegi szerverforrás új telepítéshez készített
sémája: **4 adatbázis, 102 tábla**, InnoDB, `utf8mb4_unicode_ci`. A Database,
GameServer és LoginServer SQL-használatát, a közös típusokat, az aktív feature
definíciókat és a pozíció alapján olvasott rekordokat is figyelembe veszi.
Valódi, elkülönített **MariaDB 13.0.2** példányon ellenőrizve.

A teljes játék indításához szükséges mob-, tárgy-, skill-, bolt- és bónusztábla
**tartalma nincs meg a forrásban**. Ezeket saját játékadat-mentésből kell betölteni.
A csomag nem hoz létre kitalált protókat vagy adminfiókot. Teljes szerverboot és
Unity klienses belépés ezen tartalom nélkül nem volt ellenőrizhető.

## A csomag

A 2026-10-20-án átadott tényleges SQL-mentés illesztése elkészült:
[adatmegőrzési jelentés, localhost import és nyitott futásidejű feladatok](../docs/database_restore_2026-10-20.md).
A séma a mentés alapján szélesebb tárolást és pontosított kulcsokat kapott;
MariaDB 11.4.13-on 365 SQL-próba és 175 invariáns sikeres. A korábbi
adatbázismentés-hiány a jelenlegi helyi csomagra már nem vonatkozik.

| Fájl | Feladat |
|---|---|
| [schema.sql](schema.sql) | `player` (69), `common` (7), `account` (3), `log` (23) tábláinak létrehozása |
| [seed_core.sql](seed_core.sql) | Két alap konfigurációs sor, meglévő értékek megőrzésével |
| [check_ready.sql](check_ready.sql) | Konfiguráció és indulási referenciaadatok jelenlétének lekérdezése |
| [db_schema_audit.md](db_schema_audit.md) | Forráskapcsolatok, javítások, ellenőrzési eredmények és korlátok |
| [tools/TestSchema.ps1](tools/TestSchema.ps1) | Ideiglenes MariaDB indítása Windows alatt, ellenőrzés és leállítás |
| [tools/validate_schema.py](tools/validate_schema.py) | Forrásból kigyűjtött SQL-ek és adat-visszaolvasási próbák |

## Új telepítés

A cél üres adatbázis-környezet. A fájlok nem állítják át az élő szerver
konfigurációját vagy jogosultságait. A játék SQL-felhasználóját és jelszavát a
telepítés beállításaival kell egyeztetni.

Lépj be a MariaDB kliensbe a négy adatbázis létrehozására jogosult felhasználóval:

```powershell
mariadb --host=127.0.0.1 --user=root -p --default-character-set=utf8mb4
```

A MariaDB parancssorában, az útvonalat szükség szerint módosítva:

```sql
SOURCE D:/AurigaGlobal/LiveWork/AurigaGlobal/db/schema.sql;
SOURCE D:/AurigaGlobal/LiveWork/AurigaGlobal/db/seed_core.sql;
```

Töltsd be a saját, ehhez a szerververzióhoz tartozó referenciaadatokat:
`player.mob_proto`, `item_proto`, `skill_proto`, `shop`, `shop_item`, `item_attr`,
`item_attr_rare`, `item_attr_rare_8`. Régi dumpból az **adatokat** kell a mostani
oszlopokhoz és ENUM-azonosítókhoz illeszteni; a dump régi `CREATE TABLE`/`DROP
TABLE` utasításai visszaállítanák a régi szerkezetet. A boltok hivatkozásainak és
a skillformuláknak is érvényesnek kell lenniük.

```sql
SOURCE D:/AurigaGlobal/LiveWork/AurigaGlobal/db/check_ready.sql;
```

A `READY` jelenlétet, a `MISSING_OR_INVALID` hibás/hiányzó konfigurációt, az
`IMPORT_REQUIRED` hiányzó referenciaadatot jelez. Csak a séma és a mellékelt seed
importja után **2 READY és 8 IMPORT_REQUIRED** az elvárt eredmény. Ez jelenléti
ellenőrzés, nem a teljes játékadat-készlet szemantikai validálása. További
adatokat — refine, extra proto, tiltólisták — a játékmenet igénye szerint kell
betölteni; a fájlos JSON/Lua/mapadatok külön szerveradatok.

A seed alapértékei: `DB_NAME_COLUMN=name`, valamint 41 szám a
`SKILL_POWER_BY_LEVEL` sorban, a 0. szinttel együtt. A görbe a helyi
`share/data/skill_power.txt` ymir sorából származik; a forrás ujjlenyomata a seed
megjegyzésében van. Saját balanszhoz ezt egyeztetni kell. Az `INSERT IGNORE`
megőrzi a meglévő locale sorokat.

Az indulási ellenőrzés legalább 41 egész számot követel meg. A régi, például
45 értékes görbéket is elfogadja: a GameServer csak a 0–40. szinthez tartozó
értékeket használja, a további értékek nem befolyásolják a futását.

Az `account.account.password` 128 karakteres, a kód jelenlegi MariaDB
`PASSWORD()` útvonalához is elegendő. A teszt 41 karakteres natív hash tárolását
ellenőrizte. [MariaDB PASSWORD dokumentáció](https://mariadb.com/docs/server/reference/sql-functions/secondary-functions/encryption-hashing-and-compression-functions/password).

## Profil és meglévő adatbázis

- A `common/service.h` és `CommonDefines.h` jelenlegi definíciói, a Database-ben
  `ENABLE_PROTO_FROM_DB`; üres `table_postfix`; a négy fenti adatbázisnév.
- A helyi `proto.from_db=true` konfigurációhoz a protók DB-adatként szükségesek.
  A proto-mirror INSERT-ek is szerepelnek az SQL-ellenőrzésben.
- A táblák UTF-8 tárolást használnak. A jelenlegi `latin1` kapcsolati beállítás
  mellett MariaDB karakterkonverziót végez; az import és a kliens szövegeinek
  valódi kódolását ehhez kell egyeztetni. A két `sjis_japanese_ci` SQL-ág külön
  locale-profil, az átadott UTF-8 sémával nem használható.
- Más MariaDB-verziót, feature készletet vagy MySQL-t ez a helyi futás nem igazol.

A `CREATE ... IF NOT EXISTS` **nem migráció**: a meglévő tábla hibás típusát,
oszlopsorrendjét vagy ENUM-listáját nem javítja. Élő átállításhoz a tényleges
`SHOW CREATE TABLE` kimenetekből külön migráció kell. Különösen fontosak a
skill/quickslot BLOB-ok, az ENUM-azonosítók, a 64 bites mezők, és az
`account_mount_inventory`/`sms_pool` **player** routingja. A régi bónuszmigrációt
nem kell ráfuttatni a friss sémára. A DDL nem ad új idegen kulcsokat vagy egyedi
inventory-slot megszorítást az aszinkron mentési sorrend felülvizsgálata nélkül.

## Ismételhető ellenőrzés

Windows, MariaDB binárisok, Python 3.9+ és a Python architektúrájával egyező
Connector/C könyvtár szükséges. Helyi útvonalpéldák:

```powershell
& ./db/tools/TestSchema.ps1 `
  -MariaDBBin 'C:/Program Files/MariaDB 13.0/bin' `
  -ClientLibrary 'C:/vcpkg/installed/x64-windows/bin/libmariadb.dll' `
  -Python 'C:/path/to/python.exe'
```

A script új TEMP-könyvtárban inicializál saját DB-folyamatot, véletlen loopback
porton, ellenőrzi az adatkönyvtárát, lefuttatja a vizsgálatot és leállítja a
saját példányát. Nem használ Windows-szolgáltatást. A naplókat és a `report.json`
jelentést a kiírt könyvtárban megőrzi.

Már előkészített, elkülönített, üres root jelszavas helyi **tesztpéldányon**:

```powershell
python db/tools/validate_schema.py --port 51885 `
  --library 'C:/vcpkg/installed/x64-windows/bin/libmariadb.dll' `
  --report schema-report.json
```

Az ellenőrző véletlen `auriga_schema_<azonosító>_*` adatbázisokat hoz létre,
ezekre írja át a lekérdezések adatbázisneveit és visszagörgeti a DML-próbákat.
A tesztadatbázisok megmaradnak a jelentés vizsgálatához. Az üres root jelszó a
helyi tesztindító sajátossága, nem telepítési követelmény.

Legutóbbi eredmény: **102 tábla, 365 előkészített/végrehajtott SQL-sablon,
168 invariáns, 0 feloldatlan hiba**. A szerver **49/49 CTest** esete normál és
ASAN buildben is sikeres; a natív valódi SQL-es karakterteszt 136, az
időbélyeg/mentési teszt 109 ellenőrzést teljesített mindkét buildben. A
[sémaaudit](db_schema_audit.md) rögzíti az eredmények pontos jelentését.

## Négy karakterhelyre átállás

A friss séma `player_index` táblája már csak `pid1`–`pid4` mezőket tartalmaz.
Meglévő adatbázishoz a [külön migráció](migrations/2026-10-19_four_character_slots.sql)
és a [telepítési/adatmegőrzési leírás](../docs/four_character_slots.md) tartozik.
A szerverek és a Unity kliens együtt frissítendők; éles adatbázis-migráció nem történt.

Az átállás utáni izolált MariaDB 12.2.2 ellenőrzés: **102 tábla, 365 előkészített és
végrehajtott SQL, 169 sémainvariáns, 6 migrációs ellenőrzés, 0 feloldatlan hiba**.

## Mission Engine persistence réteg

Az új Mission Engine külön, additív migrációja:
`migrations/2026-10-25_mission_engine_persistence.sql`. A konfigurált player
adatbázist a hívónak kell kiválasztania; a fájl nem vált adatbázist és nem
töröl vagy módosít régi BattlePass-/Mission Pass-/questtáblát. Hat saját
`mission_engine_*` InnoDB-táblát hoz létre. A base schema alkalmazása önmagában
ezt az opcionális modult még nem telepíti.

A persistence réteg jelenleg elkülönített MariaDB-példányon tesztelt;
a GameServer session-/gameplay-bekötése és a jutalomledger még következik.
A migráció éles alkalmazása külön telepítési munkafázis. Részletek:
`docs/mission_persistence_store_2026-10-25.md` a forrásgyökér alatt.

