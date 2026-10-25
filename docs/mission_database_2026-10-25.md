# Külön Mission-adatbázis

Dátum: 2026-10-25. Ág: `codex/mission-engine`.

## Felosztás

Az új történetalapú Mission Engine nyolc saját táblája a külön **mission**
adatbázisba kerül. A player, account, common és log meglévő sémája változatlan.
A szezonális Mission Pass és a régi Lua quest-adatok a jelenlegi helyükön maradnak.

A Mission-sémában van a csomag-/definícióarchívum, a karakter-checkpoint,
az owner/revízió, a mentési műveletnyugta, az action outbox és az entitlement-ledger.
A tényleges tárgy, raktár és item_award a player adatbázisban marad.

A motornak nem kell új szolgáltatás vagy külön folyamat. A persistence-tervek
SQL-kapcsolatának alapértelmezett adatbázisa az effektív `sql.mission` cél.
A session-/core-koordinátor élő bekötése továbbra is külön munkafázis.

## Konfiguráció

A szerkeszthető minta a `config/server.json.example` fájlban van. Példa:

```json
"mission": {
  "host": "127.0.0.1",
  "port": 3306,
  "user": "auriga",
  "password": "REPLACE_ME",
  "database": "mission"
}
```

Ez a `common.sql` objektum új eleme. A Database és GameServer örökli;
komponensenként/core-onként részlegesen felülírható. A LoginServer nem kapja meg.
Hiányzó Mission-kapcsolat esetén a komponens effektív player-kapcsolatából
képződik, a database mező **mission** értékre cserélésével. A forrás JSON ettől
nem íródik át. Részleges közös beállítás is megengedett:

```json
"mission": {"database": "story_mission"}
```

Az öröklés megőrzi a player-cím/port/hitelesítés mezőit; explicit üres jelszó
felülírásnak számít. A szerverkezelőben **Beállítások → SQL · Küldetések**
néven jelenik meg, a jelszó takarva marad. Az SQL-kapcsolatellenőrzés a Mission
célt is vizsgálja. Régi konfigurációnál is látható a származtatott külön cél.

Indítás előtti ellenőrzések:

- Mission és player azonos host-szöveggel és TCP-porttal legyen beállítva;
  a 0 port 3306-nak számít. DNS-aliasok azonosságát a betöltő nem találgatja.
- A Mission neve eltérjen a komponens meglévő SQL-adatbázisaitól, és ne legyen rendszerséma.
- Az adatbázisnév 1–64 ASCII betű/szám/aláhúzás; SQL-szöveg nem adható meg névként.
- A Database és minden GameServer ugyanazt a Mission-nevet/végpontot használja.

## Telepítés

Az önálló `db/mission_schema.sql` teljesen minősített neveket használ:
`mission.mission_engine_*`. Az adminisztrátori import alapértelmezett
adatbázistól független; nincs USE és nincs régi táblák törlése. A sémaverzió 2.
A DDL forrása továbbra is a két meglévő Mission-migráció; a Python-generátor
és a teszt biztosítja, hogy az önálló SQL ne térjen el tőlük.

Új szervercsomagnál a friss `manage.py init-db` a Mission-sémát is telepíti,
és megadja a generált helyi SQL-felhasználó hozzáférését. A csomagkészítő a
Mission-modult, a migrációkat és az önálló SQL-t is becsomagolja.

Meglévő, friss runtime-ot tartalmazó csomagnál:

```text
python -B manage.py init-mission-db
```

A parancs a konfigurált nevet kezeli, tehát a story_mission célhoz nem kell
kézzel átírni az SQL-fájlt. Ellenőrzi, hogy az általa ismert játékszerverek
leálltak. Saját helyi MariaDB-nél ellenőrzi az adatkönyvtár tulajdonát;
külső SQL-szolgáltatást nem indít és nem inicializál újra. Külső szervernél
a konfigurált Mission SQL-felhasználónak megfelelő telepítési jogosultság kell.
A hitelesítési adatok privát ideiglenes opciós fájlban utaznak, nem argv-ban.

Az installer előbb megvizsgálja a player adatbázist: korábbi Mission-táblák
esetén megtagadja az üres új tár létrehozását. Meglévő kísérleti adatok
mozgatása külön, mentett és ellenőrzött adatátvezetést igényel; ez a parancs
nem másolja vagy törli őket automatikusan. A jelen fejlesztésben korábban
csak izolált tesztadatbázisok kaptak Mission-adatokat, éles engine nem volt bekötve.

A változatlan legacy SQL-mentésimport továbbra is a négy régi adatbázis friss,
üres telepítését tölti. Meglévő Mission-progress esetén ezt is elutasítjuk,
még az import adatváltoztatásai előtt. A Mission-játékállapottal együtt végzett helyreállításhoz
a szerverkezelő teljes SQL-mentési központja használható.

## Közös tranzakciók és jogosultság

A két séma ugyanabban a MariaDB-példányban, InnoDB táblákon működik.
Egy későbbi natív jutalomíró ugyanazon kapcsolat egyetlen tranzakciójába
teheti a Mission-ledgert és a teljes névvel hivatkozott player.item_award INSERT-et.
Két egymástól független kapcsolat COMMIT-ja nem közös atomi művelet.

A konfigurációban alapból közös SQL-felhasználó van. Külön Mission-felhasználónál
a későbbi jutalomírónak megfelelő player-oldali jogosultság is kell; a helyi
telepítő a Mission-séma mellett player.player SELECT és player.item_award
SELECT/INSERT jogot készít elő. Ez nem implementálja a jutalomírót.
Az élő ECS-ben történő XP-/pénz-/inventoryváltozás továbbra sem lesz ettől SQL-tranzakció.

## Mentési és visszaállítási szerződés

A mentési központ a konfigurált öt SQL-adatkört gyűjti össze. A közös
végpont/hitelesítés alatt a player és mission **egy dumpban, közös globális
olvasási zárral** mentődik. Az SQL-szerverenkénti közös mentési hozzáférés
követelménye megmarad; eltérő felhasználók esetén a központ ezt jelzi.

Visszaállításnál az SQL-csoportok neveinek egyezniük kell. Egy korábbi,
csak négy adatbázist tartalmazó SQL-mentést a központ nem állít vissza úgy,
hogy közben az újabb Mission-progress maradjon. Az elutasítás az adatbázisok
megváltoztatása előtt történik. A konfiguráció visszaállításának meglévő
„jelenlegi SQL-kapcsolatok megtartása” viselkedése a Mission-beállításra is érvényes.

## Ellenőrzés és fennmaradó határ

A valódi MariaDB-suite külön, véletlen nevű player és mission sémát hoz létre.
A player oldalon az eredeti item_award DDL-t használja; a Mission store semmilyen
legacy táblát nem kap a saját sémájába. Külön connectionön a rossz player
kontextusba küldött Mission-írás elutasításra kerül.

Teszteltük az önálló minősített telepítő ismételt futását és egy egyedi nevű
Mission-séma használhatóságát. A valódi entitlement-tervhez kizárólag tesztben
hozzáfűzött canonical award INSERT közös rollbackje, commitja, jogosultsághiánya,
újrapróbálása és elveszett COMMIT-válasza is ellenőrzött. Kényszerített MariaDB
leállítás után új C++ folyamat mindkét séma rekordjait ellenőrzi.

Ez infrastruktúra-bizonyíték. Nem regisztrál gameplay bindingot, nem váltja ki
a stabil action/payload → award mappinget, és nem hajtja végre a kevert
item/XP/pénz/cost bundle-t. A következő engine-fázis továbbra is ez a teljes,
idempotens jutalomprotokoll és az award-cache commit-sorrendi helyessége.

## Rögzített ellenőrzések

- Database, GameServer, LoginServer és konfigurációs teszt: normál és ASAN Windows x64 build.
- Célzott normál regressziós kör: 34/34 sikeres; ASAN CTest: 11/11 sikeres.
- Külön Mission/player sémás MariaDB-suite: normál/ASAN 687 + 54 cold-process ellenőrzés.
- SQL-telepítés/config/grant teszt: 11/11, saját ideiglenes helyi MariaDB-bootstrapot is beleértve.
- Qt környezetben a mentési központ 15/15 tesztje és a Mission-beállításcsoport GUI-próbája sikeres.
- A PyInstaller alkalmazáscsomag elkészült; a builder által kért generált quest API-index is frissült.

A polling miatt a C++ ellenőrzésszám változhat. A végső konfigurációs névütközés-
ellenőrzés szigorítása után a normál konfiguráció/tool kör 4/4, az ASAN kör 3/3
eredménnyel külön ismét lefutott. A csomagolt alkalmazás headless worker-próbája
az új beépített Mission-modul elérhetőségét és a hibás rendszerséma SQL előtti
elutasítását is ellenőrizte.
A binárisok és a Qt-csomag a build mappában készültek el; futó helyi/távoli
játékszerver vagy tényleges játékosadatbázis telepítése ebben a fázisban nem történt.
A 34 védett szezonális Mission Pass fájl változatlan maradt.

Naplók: `build/mission-database-regression-tests.log`,
`build/mission-database-asan-unit-tests.log`, `build/mission-database-asan-db-tests.log`,
`build/mission-database-final-bootstrap-tests.log`, `build/mission-database-gui-tests.log`,
`build/mission-database-qt-backup-tests.log`, `build/mission-database-manager-build.log`.
