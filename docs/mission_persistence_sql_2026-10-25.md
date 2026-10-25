# Mission Engine: prepared tranzakciós alap

**Állapot: 2026-10-25, 4/A munkafázis.** A natív mag és a `.aurmission`
csomagolás után elkészült a tartós mentéshez szükséges SQL-végrehajtási alap.
Ez még nem a kész Mission progress store vagy a játékosok jutalmazásának
bekötése. A migráció és a domain adapterek a következő részfeladatok.

**Későbbi bővítés ugyanazon a napon:** a checkpoint/archívum/store és a
fencing-primitívek azóta elkészültek; lásd a
`mission_persistence_store_2026-10-25.md` dokumentumot. A gameplay- és
jutalmazási integráció továbbra is külön feladat.

## Megvalósítás

| Fájl | Feladat |
|---|---|
| `SRC/Server/SQL/SqlParams.h` | A meglévő, bináris protokollon kötött paramétertípusok önálló fejléce |
| `SRC/Server/SQL/PreparedTransaction.h` | Tulajdonolt lépések, eredményállapotok, korlátok |
| `SRC/Server/SQL/AsyncSQLTransaction.cpp` | Egy munkaként végrehajtott tranzakció, CAS-rowcount, session- és hibakezelés |
| `SRC/Server/SQL/AsyncSQL.cpp/.h` | Queue-integráció, teljes prepared result-drain, bináris eredményhosszak |
| `tests/server/AsyncSqlPreparedTests.cpp` | Eredménykezelés és célzott C API hibainjektálás |
| `tests/server/AsyncSqlMariaDbTests.cpp/.py` | Friss MariaDB, valódi SQL-verseny, megszakítás és elveszett COMMIT-válasz |

A `ReturnTransactionPrepared(SqlTransaction, userData)` egyetlen queue-elem.
A worker az összes lépést ugyanazon a kapcsolaton hajtja végre; közéjük más
SQL-munka nem kerül. Minden paraméter és SQL-szöveg a munka tulajdona.
Az eredmény a meglévő `PopResult()` csatornán érkezik vissza.

A `DirectTransactionPrepared()` kizárólag `Setup(..., bNoThread=true)`
kapcsolaton engedélyezett. Az aszinkron worker kapcsolatát ezzel nem lehet a
hívó szálról megnyitni vagy használni. Az aszinkron változat nem inicializált
vagy direkt kapcsolaton `logic_error` kivételt ad.

### Tranzakcióhatár és eredmény

1. A teljes munkát validáljuk a kapcsolat módosítása előtt.
2. Kikapcsoljuk az automatikus reconnectet, majd pinggel ellenőrizzük a
   kapcsolatot. Nyitva hagyott idegen tranzakciót vagy kikapcsolt autocommitet
   nem veszünk át: a kapcsolatot eldobjuk.
3. Kikapcsolt autocommit mellett futnak a prepared lépések. A kapcsolat
   generációját lépésenként és a commit előtt ellenőrizzük.
4. Az opcionális `expectedAffectedRows` ellenőrzi a módosított sorok számát.
   A revisiont növelő `UPDATE ... WHERE revision=?` és `expectedAffectedRows=1`
   használható CAS-műveletként. `SELECT` mellett ez az elvárás tiltott.
5. Lépéshibánál/ütközésnél visszagörgetünk. Vissza nem igazolt rollback után
   eldobjuk a kapcsolatot; ilyenkor az autocommit visszakapcsolása tilos,
   mert az függőben maradt munkát commitolhatna.
6. Egyetlen COMMIT történik. Hibás vagy elveszett válasznál az eredmény
   `CommitUnknown`, a kapcsolatot eldobjuk, a munkát nem ismételjük meg.

| `transactionOutcome` | Jelentés |
|---|---|
| `NotStarted` | Egy új üzenet kezdeti állapota |
| `Committed` | A commit sikerét a szerver visszaigazolta; `uiSQLErrno=0` |
| `RolledBack` | A lépéshiba után a rollback is visszaigazolt |
| `Conflict` | A rowcount eltért az elvárástól, és a rollback visszaigazolt |
| `Failed` | Elutasított munka, indulási hiba vagy vissza nem igazolt rollback; commitot nem küldtünk |
| `CommitUnknown` | Commitot küldtünk, de a végeredményt friss kapcsolaton, tartós műveletazonosítóval kell egyeztetni |

Az általános SQL-hibák eredeti kódjukat megtartják. A kliensoldali
`SqlTransactionConflict=0xffff0001` és `SqlTransactionInvalid=0xffff0002`
nem MariaDB szerverhibakód. A `transactionFailedStep` az elbukott lépés
indexe; indulási/commit-hibánál `SqlTransactionNoStep`.
Az egyes lépések első eredményének helyét a `transactionResultOffsets`
adja meg. Nem sikeres tranzakció részleges eredménye nem jogosít gameplay-hatásra.

Visszaigazolt commit után az autocommit/reconnect visszaállításának hibája
eldobja a kapcsolatot, de az eredmény **Committed marad**. A már elmentett
munkát emiatt sem szabad ismételten végrehajtani.

Az új tranzakciós üzenetet az alapértelmezett worker-retry sem játssza újra.
A meglévő, önálló query-k retry-szabályait ez a fázis nem tervezi át.
A shutdown alatti flush is egyben hajtja végre a már elindított tranzakciót.
A direkt/raw SQL API-k megosztott worker-szálról való szabálytalan használata
továbbra sem megengedett.

### Korlátok és előfeltételek

- A 4/B bővítéssel legfeljebb 512 lépés, összesen 4096 paraméter,
  64 KiB SQL/lépés és 32 MiB összes SQL- és paraméterbájt.
- A `SELECT` előfeltétele `expectedResultRows` lehet; módosító statementnél
  továbbra is az ettől különálló `expectedAffectedRows` használható.
- Csak rögzített, megbízható szerverkód adhat SQL-t. Az értékek `SqlParam`
  paraméterek; mission-csomag, kliens vagy Studio nem adhat tetszőleges SQL-t.
- Engedélyezett kezdő műveletek: `SELECT`, `INSERT`, `UPDATE`, `DELETE`.
  DDL, `CALL`, tranzakcióvezérlés, több statement, komment és NUL a
  statement szövegében tiltott. A szűrő szándékosan konzervatív; nem SQL-sandbox.
- Minden érintett tábla, valamint trigger által elért tábla **InnoDB** kell
  legyen. A szolgáltatásnak ezt a sémaszerződésben is biztosítania kell.
  Külső inventory/ECS-hatás vagy nem tranzakciós tábla nem válik ettől ACID-dá.
- A COMMIT előtti hibánál a munka újraküldéséről a hívó dönt, friss
  revisionnel és owner-ellenőrzéssel. `CommitUnknown` esetén előbb kötelező
  a tartós operation ID/ledger egyeztetése. Ezt a domain store fogja megvalósítani.

## Javított eredménykezelési hibák

A prepared végrehajtás minden eredményhalmazt kiolvas a
`mysql_stmt_next_result()` segítségével, beleértve a `CALL` záró OK eredményét.
Későbbi eredmény hibája nem marad sikernek jelölve; részben kiolvasott,
bizonytalan protokollállapotú kapcsolatot nem használunk újra.
Ez az önálló prepared API-ra is érvényes, bár a tranzakciós API `CALL`-t nem fogad.

Paraméterszám-eltérés most `CR_PARAMS_NOT_BOUND` hibát ad, a korábbi nulla
hibakód helyett. A prepare hiba saját statement-hibakódját megőrizzük a
statement bezárása előtt. A kapcsolódási napló valódi hibaüzenetet ír a
korábbi szó szerinti `%s` helyőrző helyett.

A `SQLResult::FetchLengths()` az utoljára kiolvasott sor bájthosszait adja.
Prepared és text eredménynél is használható. A bináris checkpoint/csomag
`std::string(row[column], lengths[column])` alakban olvasandó; a NUL-ra
végződő szövegként olvasás adatot veszítene. Sor előtt/EOF után nincs hossz;
a NULL és az üres string pointerrel különböztethető meg, hosszuk egyaránt nulla.

API-hivatkozások: [több prepared eredmény](https://mariadb.com/docs/connectors/mariadb-connector-c/api-prepared-statement-functions/mysql_stmt_next_result),
[autocommit](https://mariadb.com/docs/connectors/mariadb-connector-c/api-functions/mysql_autocommit),
[commit](https://mariadb.com/docs/connectors/mariadb-connector-c/api-functions/mysql_commit).

## Valódi MariaDB-teszt

A Python runner friss, egyedi datadirt és szabad loopback-portot hoz létre.
Nem használ meglévő Windows-szolgáltatást, konfigurációt, mentést vagy a
felhasználó adatbázisát. A leállításhoz kizárólag a saját folyamatát kezeli;
a datadir és a naplók diagnosztikához megmaradnak az ignorált buildmappában.

A külön tesztpéldányban üres jelszavas rootot használunk, kizárólag loopbacken.
A protokollhiba-próba plaintextet igényel: csak ez a tesztszerver indul
`--skip-ssl` kapcsolóval, és kizárólag a tesztgyermek környezete kapja a
Connector/C `MARIADB_TLS_DISABLE_PEER_VERIFICATION=1` beállítását.
A normál szerverprogramok TLS-beállításai nem változnak.
Lásd a [Connector/C kapcsolódó hibajegyét](https://jira.mariadb.org/browse/CONC-773).

```powershell
cmake --build build --config RelWithDebInfo --target AsyncSqlPreparedTests AsyncSqlMariaDbTests --parallel 4
& build/SRC/Server/GameServer/RelWithDebInfo/AsyncSqlPreparedTests.exe
& 'C:/Users/ixtre/AppData/Local/Programs/Python/Python314/python.exe' -B tests/server/AsyncSqlMariaDbTests.py `
    --server-bin-dir build/database-tools/mariadb-11.4.13-winx64/bin `
    --test-executable build/SRC/Server/GameServer/RelWithDebInfo/AsyncSqlMariaDbTests.exe `
    --work-dir build/mission-sql-isolated
```

Opcionális CTest-bekötéshez Windows alatt az `AURIGA_TEST_MARIADB_BIN_DIR`
CMake-cache értéket kell megadni. Az `async_sql_mariadb_isolated` teszt csak
ekkor regisztrálódik, 180 másodperces limittel. Az integrációs C++ programot
közvetlenül valódi adatbázisra irányítani nem szabad; a runner hozza létre
a hozzá szükséges elkülönített környezetet és tesztproxyt.

Ellenőrzött esetek:

- Bináris paraméter/eredmény, beágyazott NUL és 70 KiB-os cella.
- Ismételt, cache-elt stored procedure három eredményhalmazzal, köztük üressel;
  záró OK és késői `SIGNAL` hiba.
- Progress UPDATE + egyedi receipt INSERT közös commitja; az INSERT
  duplikált kulcsa esetén a progress UPDATE visszagörgetése.
- Két valódi kapcsolat ugyanarra a revisionre: pontosan egy commit és egy
  CAS-conflict, egyetlen receipt.
- Queue-sorrend és idegen nyitott tranzakció elutasítása.
- `KILL CONNECTION` az UPDATE után, COMMIT előtt: nincs progress/receipt,
  a soron következő munka új kapcsolaton lefut.
- A proxy ténylegesen továbbítja a COMMIT-ot MariaDB-nek, elolvassa a sikeres
  OK választ, de nem adja át a kliensnek. Az eredmény `CommitUnknown`,
  miközben friss kapcsolaton a mentett revision és receipt is megvan.
- Hibainjektálással rollbackhiba, sessionváltás, sikertelen ping és
  commit utáni kapcsolat-visszaállítási hiba is ellenőrzött.

A végső Windows x64 / RelWithDebInfo fordításban a **GameServer, Database és
LoginServer** is elkészült. **2220** SQL-egységellenőrzés és **204** valódi
MariaDB-ellenőrzés sikeres; mindkét SQL-teszt AddressSanitizer alatt is lefutott.
A célzott regressziós kör **25/25**, a külön ASAN CTest-kör **5/5** sikeres.
A kész kódhoz tartozó naplók a helyi `build` könyvtárban:
`mission-sql-final-build.log`, `mission-sql-final-tests.log`,
`mission-sql-asan-unit.log`, `mission-sql-final-asan-tests.log`,
`mission-sql-last-real-tests.log`, `mission-sql-asan-real-tests.log`.
A késői syntax/prepare/paraméterszám-próbákkal bővített MariaDB-teszt külön is
újrafordult és ismét sikeres. FreeBSD build és Unity end-to-end teszt ebben
a részfázisban nem történt.

## Következő munkarész

1. Additív Mission-séma és verziózott checkpoint codec; hideg újraindításhoz
   archivált `.aurmission` csomagok és pontos definícióverziók.
2. Karakterenkénti authoritative owner/fencing, progress revision/CAS,
   teljes dedupállapot és stabil műveletazonosító egy tranzakcióban.
3. Ugyanabba a tranzakcióba kötött action outbox / jutalomledger, majd az
   `item_award` és inventory/ECS szolgáltatásokkal igazolt kézbesítési szerződés.
4. Reconnect/crash/full-inventory/retry próbák a valódi Mission store-ral.
   Csak ezek után kapcsolható be az első live gameplay vertikális szelet.

A jelenlegi SQL-integrációs teszt saját `mission_sql_*` táblái kizárólag
az infrastruktúra próbái, nem telepíthető Mission-séma. Ebben a fázisban
nem történik éles vagy távoli telepítés, játékosadat-módosítás vagy jutalomkiadás.
