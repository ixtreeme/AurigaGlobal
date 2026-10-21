# SQL-séma forrásaudit és ellenőrzés — 2026-10-18

## Kiegészítés — 2026-10-20

A tényleges mentés alapján a mob szöveg/numerikus tárolása, talisman mezők és
előjeles pénznapló-azonosítók szélesebbek; a level log, bónuszdefiníció és
birodalomváltási táblák felesleges egyedisége megszűnt, az affect kulcsába a
`lApplyValue` is bekerült. Így nem szükséges érvényes régi sorokat eldobni.
Az oszlopok sorrendje és a C++ bináris mezők nem változtak. MariaDB 11.4.13:
102 tábla, 365 SQL prepare/végrehajtás, 175 invariáns, 0 feloldatlan hiba.
Részletes adatmegőrzés és ismert korlátok:
[helyreállítási jelentés](../docs/database_restore_2026-10-20.md).

A korábbi következtetett vázlat helyét a [schema.sql](schema.sql) átveszi:
**102 létrehozható tábla, négy adatbázisban**, valódi MariaDB-n ellenőrizve.
Telepítés: [README.md](README.md). A korábbi audit 98 táblára, bizonytalan
routingra és TODO-oszlopokra vonatkozó állításai már nem az aktuális állapotot
írják le; ez a dokumentum felülírja azt az auditot.

## Forrás és módszer

A teljes `SRC/Server` C++ forrásállomány SQL-literáljainak leltára készült,
a `common/service.h`, `common/CommonDefines.h` és a Database saját
`ENABLE_PROTO_FROM_DB` definícióival. Az ellenőrzött profil üres tábla-postfixet
és latin1/UTF-8 kapcsolatot használ, utf8mb4 tárolással. A történeti
`72e9bc79:player.sql` DDL a pozicionális mezőnevek egyeztetéséhez szolgált
kiegészítésként, nem változatlanul átvett telepítési dumpként.

A [source_sql.py](tools/source_sql.py) komment- és stringtudatos lexikai leltár,
korlátozott feltételesfordítás-kezeléssel, **nem teljes C++ fordító vagy
vezérlésifolyam-elemző**. Az SQL-paramétereket reprezentatív értékek helyettesítik.
A több hívásban összeálló queryk áttekintett szerződései a
[sql_assemblies.json](sql_assemblies.json) és
[sql_assemblies.py](tools/sql_assemblies.py) fájlokban vannak. A builderforrások
LF-re normalizált SHA256-ujjlenyomata változáskor felülvizsgálatot kényszerít ki;
az eltűnt szerződések is hibát okoznak. Feature-váltáskor az összeállításokat és
az enumokat újra kell ellenőrizni.

| Forrás | Szerződés |
|---|---|
| `common/tables.h`, `length.h`, `item_length.h` | C++ mezőszélességek, packed BLOB-ok, inventory- és apply-azonosítók |
| `Database/player_create.cpp`, `player_save.cpp`, `ClientManagerPlayer.cpp` | Karakter létrehozása, mentése, betöltése és törlési archívuma |
| `Database/ClientManagerBoot.cpp` | Proto-, skill-, bónusz- és bolttáblák, nyelvi oszlopok, mirror INSERT-ek |
| `Database/Cache.cpp`, `ItemAwardManager.cpp`, `ClientManagerOfflineshop.cpp`, `OfflineshopCache.cpp` | Itemek, jutalmak, nyolc bónusz, offline boltok, aukciók és valuták |
| `GameServer/pet/MountInventory.cpp`, `ecs/systems/MountSystem.cpp` | Fiókszintű mount-raktár player routingja és teljes rekordja |
| `Database/ClientManager.cpp`, `GameServer/core/config.cpp` | Locale és pozíció szerint olvasott itemshop rekordok |
| `GameServer/core/log.cpp`, `LoginServer` | Log-adatbázis pozicionális INSERT-ek, account- és loginmezők |

A táblák eloszlása: `player` **69**, `common` **7**, `account` **3**, `log`
**23**. Mind InnoDB és `utf8mb4_unicode_ci`, dinamikus sorformátummal. Egyes
történeti/opcionális táblák megmaradtak; nincs adatot törlő DDL.

## A korábbi vázlat javításai

| Terület | Végleges szerkezet és indok |
|---|---|
| Account jelszó | `VARCHAR(128)`; a korábbi 16 karakter nem tárolta a jelenlegi MariaDB `PASSWORD()` 41 karakteres hashét |
| DB-routing | `account_mount_inventory` és `sms_pool` a **player** adatbázisban; a forrás player kapcsolata éri el őket |
| Extra proto | Hat mező pontos sorrendben: `vnum`, `rarity`, `bonus_type0`, `bonus_value0`, `bonus_type1`, `bonus_value1` |
| Offline valuták | `offlineshop_safebox_valutes(owner_id,yang,cheque)`; egy INSERT kikapcsolt cheque feature mellett is nevesíti a cheque mezőt |
| String tábla | `player.string(name,text)` a forrásbeli lekérdezéshez |
| Nyelvi protók | `locale_name` és a tíz aktív `locale_name_*` oszlop is rendelkezésre áll |
| Itembónuszok | Mindhárom `item_attr*`.`apply` valódi ENUM; az `apply+0` a C++ azonosítóját adja |
| Apply-azonosítók | Az explicit 97-es folytatás előtti 92–96-os helyek fenntartva; a 122–126-os típusok az aktuális forrássorrendet követik |
| Inventory-ablak | Az ENUM első eleme INVENTORY=1. A C++ RESERVED_WINDOW=0 nem kerül elé, mert eltolná a többi azonosítót |
| Guild skillek | `BINARY(12)` teljes, nullázott alapértékkel; a natív másolat mérete és bájtjai kódolástól függetlenek |
| Player BLOB | NULL esetén marad a loader nullázó fallback; nem NULL esetén CHECK rögzíti a 2550 bájtos skill és 108 bájtos quickslot méretet |
| Itemshop dátumok | `offerTime` és `addedTime` nem NULL, a feltétel nélküli natív stringolvasóhoz igazítva |
| 64 bites adatok | Coin, yang, ranglista, HP/SP, eventflag és lóhatáridő a jelenlegi C++ signed/unsigned szélességet követi |
| Itemadatok | Három socket és mind a nyolc bónuszpár a használt item-, award-, mount- és offline tárolókban |
| Indexek | Account/owner/ID keresések; antifarm login/hwid indexek, blocked_hwids kulcs |

A skill BLOB 255 darab packed `TPlayerSkill` (1+1+8 bájt), a quickslot 36 darab
packed `TQuickslot` (1+2 bájt). Ez a jelenlegi build bináris szerződése;
struktúraváltozáskor adatkonverzió is kell. A NULL fallback nem tesz érvényessé
egy régi, rövidebb, nem NULL BLOB-ot.

A régi SQL apply-nevek közül az azonos numerikus értékű neveket megőriztük,
például `MAGIC_ATT_GRADE_BONUS`, `ATT_BONUS_TO_WARRIOR`, `Danni Medi[PVM]`,
`ATTBONUS_RUNE`. A C++/SQL névmegfeleltetés explicit a
[schema_checks.py](tools/schema_checks.py) fájlban. Régi balancing dump vakimportja
a mezőméretek és az utolsó ENUM-azonosítók miatt nem ellenőrzött import.

Nem kerültek be idegen kulcsok vagy egyedi inventory-slot megkötések: a cache-ek
és az aszinkron itemmozgatás több SQL-lépésből állnak. Új ilyen megszorítások
tranzakciós forrásmódosítást és külön tesztet igényelnének.

## Három szükséges forrásjavítás

1. `Database/ClientManagerPlayer.cpp`: a skill-color SELECT-ből hiányzott a
   vessző az `s10_col5` után. MariaDB aliasnak vette a következő mezőt: 54 mező
   jött vissza az olvasott 55 helyett. Az ellenőrző most rögzíti az 55 mezőt.
2. `Database/ClientManagerBoot.cpp`: az ötbónuszos item-proto mirror INSERT két
   névoszlopága öt apply-párt sorolt fel, de háromhoz adott értéket.
   A formátum és az argumentumlista kiegészült a negyedik/ötödik párral.
3. `GameServer/ecs/systems/MountSystem.cpp`: a mount-raktár SELECT hat
   bónuszpárt kért a mentett/feldolgozott nyolcból. Most mind a nyolcat lekéri;
   az ellenőrző rögzíti a 23 eredménymezőt.

Ezeket a futó szerverbinárisokban is alkalmazni kell: a DDL önmagában nem javítja
a régi binárisok lekérdezéseit.

## Ellenőrzési eredmény

Elkülönített MariaDB **13.0.2**, Windows/MSVC, 2026-10-18:

| Ellenőrzés | Eredmény |
|---|---|
| Friss DDL import, majd ismételt import | 102 tábla, mindkét import sikeres |
| Aktív C++ SQL-leltár | 367 egyedi sablon |
| Latin1/UTF-8: prepare és végrehajtás | **365/365**, 0 feloldatlan hiba |
| SQL-ek komponensenként | Database 184, GameServer 176, LoginServer 5 |
| Külön locale-ágak | 2 SJIS-specifikus sablon indokoltan kizárva |
| Szerkezeti/adat-visszaolvasási invariánsok | **168/168** |
| Normál és ASAN szerver CTest | **49/49**, mindkét buildben |
| Natív PlayerCreateTests valódi SQL-lel | **136 ellenőrzés**, normál és ASAN |
| Natív DatabaseTimeTests valódi SQL-lel | **109 ellenőrzés**, normál és ASAN |
| Helyi aktív quest Lua-k statikus guild SQL-jei | **4/4** külön prepare és végrehajtás |

A DDL és az SQL-próbák strict módban futottak:
`STRICT_ALL_TABLES,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION,NO_ZERO_DATE,NO_ZERO_IN_DATE`.
A DML-próbák véletlen nevű saját adatbázisokban, visszagörgetéssel futnak.
Az invariánsok a signed/unsigned 64 bites szélsőértékeket, a hash hosszát, a
BLOB méretellenőrzést, a guild bináris bájtjait, minden aktív apply/window
azonosítót és a pozicionális struktúrákat is mérik.

A natív karakterteszt szándékosan hibás prepared statementet idéz elő az
újracsatlakozás vizsgálatához; az ekkori 1243-as SQL-napló elvárt hibainjektálás,
a teszt sikeresen zárult. A két natív teszt DDL-kiemelője kezeli az új
táblabeállításokat; módosításuk után CTest-eseteik mindkét buildben újra lefutottak.
A Python ellenőrző és az ideiglenes MariaDB teljes indítás/leállítás folyamata
a mellékelt PowerShell scriptből is sikeresen futott.

## Korlátok és a még szükséges adatok

- A prepare az SQL-szintaxist és oszlopokat, a reprezentatív végrehajtás az adott
  próbát igazolja. Sok SELECT/UPDATE üres táblán fut; ez nem teljes játékmenet-
  vagy minden lehetséges paraméterértéket lefedő teszt.
- A hiányzó referenciaadatok nem állíthatók elő C++ típusokból. A seed két
  konfigurációs sort ad; a readiness lekérdezés nyolc üres referenciatáblát jelez.
  A szükséges saját játékadat-dump nincs a repóban.
- Lua futás közben tetszőleges SQL-t építhet; erre a C++ leltár nem ad általános
  garanciát. A helyi aktív Quest könyvtár négy statikus guild SQL-je külön volt
  ellenőrizve. Külső web/admin rendszerek sémája nincs ebben a forrásban.
- Nincs élő DB-migráció, FreeBSD/Linux futás, teljes szerverboot, újraindításos
  játékmenet vagy Unity klienses integrációs próba. Az élő DB-t nem módosítottuk.

Ez ellenőrzött **friss telepítési séma**. A `CREATE IF NOT EXISTS` nem javít
meglévő táblát; élő adatbázishoz állapotfelmérés és külön migráció kell.
Az R12 kiadási/integrációs feladat továbbra is részben nyitott.
