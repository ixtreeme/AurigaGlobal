# Közös szerverkonfiguráció: server.json

A Database, LoginServer és minden GameServer-példány ugyanazt a **server.json**
fájlt olvassa. A régi, munkakönyvtárankénti `conf.json` automatikus betöltése
megszűnt. A folyamatok külön futnak, külön munkakönyvtárral és naplókkal.

Minta: [config/server.json.example](../config/server.json.example).
Szerkezeti JSON-séma: [server.schema.json](schemas/server.schema.json).
A séma a részleges, öröklésre épülő SQL-/TLS-felülírásokat is megengedi;
a kötelező effektív értékeket és a szekciók közötti összefüggéseket a natív betöltő ellenőrzi.
A minta helyőrző SQL-jelszavakat, közös szerverkulcsot és TLS-fájlneveket tartalmaz;
a letöltött szerveradatokkal ezeket kell véglegesíteni. A minta nem teljes játékadatcsomag.
A tényleges `server.json` és az alapértelmezett futási könyvtárak Gitből kizártak;
a verziózott konfigurációs alap a `.example` fájl.

## Felépítés és öröklés

| Szekció | Tartalom |
|---|---|
| `schema_version` | Jelenleg kötelezően `1`. |
| `common.sql` | `player`, `account`, `common`, `log` SQL-kapcsolatok. |
| `common.tls` | A LoginServer és a GameServerek alapértelmezett tanúsítvány-/kulcsfájlja. |
| `common.interserver_key` | Database, LoginServer és GameServer közös peer-auth kulcsa. |
| `common.locale` | Opcionális közös karakterkódolás. |
| `common.table_postfix` | Database és GameServer közös táblanévutótagja. |
| `common.database_link` | Opcionális DB-szervercím; alapból a Database listeneréből képződik. |
| `database` | A Database saját beállításai; `item_id_range` kötelező. |
| `login` | A LoginServer saját beállításai, például `port`. |
| `game_defaults` | A GameServerek közös működési/játékmeneti beállításai. |
| `game_servers` | Név szerint azonosított core-ok; saját port, csatorna, hostname, maplista stb. |

A közös értékeket először a `game_defaults`, majd az adott példány szekciója
írhatja felül. Az objektumok mezőnként egyesülnek; a tömbök **lecserélődnek**.
Például egy core saját `map_allow` listája nem egészül ki a közös maplistával.
Külön SQL-jogosultságok és külön TLS-tanúsítványok továbbra is megadhatók a
komponens/példány saját szekciójában.

A `common.sql.account` a LoginServer `account_sql` értékébe kerül. A Database
a player/account/common, a GameServer a player/common/log kapcsolatokat kapja.
A közös szerverkulcs a Database `server.interserver_key`, a másik két komponens
`interserver_key` mezőjét tölti ki. Az eltérő effektív kulcs vagy táblanévutótag hibát okoz.

A Database portjának forrása `database.network.bind_port`, alapból 5300.
A többi komponens DB-linkje ezt örökli. A wildcard Database-cím helyett a
kapcsolódási cím alapértelmezetten `127.0.0.1`; távoli DB-hez a
`common.database_link.host` adható meg. Egymástól eltérő DB-link-portokat a betöltő elutasít.

## Indítás

Másold a mintát a telepítés gyökerébe `server.json` néven, és töltsd ki a tényleges
értékeket. Az alapértelmezett fájlnév `server.json` az indításkori könyvtárban;
központi fájlhoz érdemes mindig explicit `--config` útvonalat használni.

```text
Database --config D:/AurigaServer/server.json
LoginServer --config D:/AurigaServer/server.json
GameServer --config D:/AurigaServer/server.json --instance ch1-core1
```

A Windows `.exe` kiterjesztéssel, Linux/FreeBSD a megfelelő futtatható útvonallal
használja ugyanezeket a kapcsolókat. A GameServernél a példánynév kötelező.
Az indítás nem választja ki találomra az első core-t.

A közös indító a build könyvtárából vagy egy közös binárismappából is működik:

```powershell
python SRC/Tools/run_server.py --config server.json --bin-dir build --check-config
python SRC/Tools/run_server.py --config server.json --bin-dir build --instance database
python SRC/Tools/run_server.py --config server.json --bin-dir build --instance login
python SRC/Tools/run_server.py --config server.json --bin-dir build --instance ch1-core1
```

Az indító egyszerre egy kijelölt folyamatot indít, előzetes konfigurációellenőrzéssel.
A `--check-config` példánynév nélkül minden példányt ellenőriz. A `--bin-dir build`
a CMake `SRC/Server/<komponens>/RelWithDebInfo` kimenetét is felismeri.
A `--help` az egyes szerverprogramokon közvetlenül is használható.

## Útvonalak

Minden konfigurált relatív útvonal a **központi JSON fájl könyvtárához** képest értendő:

- `working_directory`: a folyamat saját munkakönyvtára. Alapból `run/database`,
  `run/login`, illetve `run/<példánynév>`; normál indításkor létrejön, ha hiányzik.
- `tls.cert_file` / `tls.key_file`, illetve a GameServer `network.tls` értékei.
- GameServer `server.data_path`, `quest.dir`. A korábbi `quest.object_dir` mező elavult, a közvetlen questbetöltő nem használja.

A megadott relatív útvonalakból abszolút útvonal képződik **a munkakönyvtárváltás
előtt**, így más könyvtárból indítva is ugyanazok a fájlok töltődnek. A
`server.data_path` alapértéke a központi fájl melletti `data` könyvtár.
Az üres, opcionális questútvonal továbbra is a meglévő adatkönyvtári alapértéket használja.

A Database néhány régi, relatív protofájlja és egyéb munkafájl a folyamat saját
munkakönyvtárához kötött marad. A konverter ezért a régi folyamatkönyvtárakat
megőrzi `working_directory` értékként. A játékadatok, mapok, questek és dropadatok
továbbra is külön adatfájlok.

A dungeon-kódban és questekben használt relatív `data/...` JSON-nevek is a
beállított `server.data_path` gyökerére oldódnak fel, ezért külön core-
munkakönyvtárból is megtalálhatók. A TXT útvonalakat a csomagkészítő az ismert
JSON-megfelelőkre vezeti át; TXT parser nem kerül vissza a szerverbe.

A régi teljes szervermentés automatikus átalakítása és a hozzá tartozó
helyi indító: [Server2 csomag](server2_package.md).

## Meglévő JSON-konfigurációk összevonása

A konverter az eredeti fájlokat megőrzi, és létező kimeneti fájlt nem ír felül:

```powershell
python SRC/Tools/merge_server_config.py --database old/db/conf.json --login old/auth/conf.json --game ch1-core1=old/channel1/core1/conf.json --game ch1-core2=old/channel1/core2/conf.json --output server.json
```

A relatív adat-/quest-/TLS-útvonalakat úgy számolja át, hogy a korábbi
konfiguráció mappájából használt fájlokra mutassanak. Az egyező SQL-, TLS- és
közös értékeket egy helyre emeli; az eltérő hitelesítési adatokat megtartja a
komponensek saját szekcióiban. Az eredményben SQL-jelszavak lehetnek: a fájl
szerveroldali konfiguráció, a Unity kliensbe és annak assetcsomagjaiba nem kerül.
A Unity külön végpontprofilja a nyilvános címeket/portokat tartalmazza.

A konverter régi **JSON** konfigurációkat vár. A letöltött TXT/CONFIG adatok
forrás szerinti konverziója külön adatimportlépés. A konverter kimenetét a
natív `--check-config` móddal kell ellenőrizni indítás előtt.

## Ellenőrzés és korlátok

Mindhárom program `--check-config` kapcsolója a kiválasztott konfigurációt beolvassa,
majd `CONFIG_OK` jelölővel kilép. Nem kapcsolódik SQL-hez, nem indít listenert,
nem nyit TLS-kapcsolatot, és nem hozza létre a futási/naplókönyvtárakat.
A topológia egészét is ellenőrzi: hallgatóport-ütközés, DB-linkek, közös kulcsok,
táblanévutótag, core-azonosítók és alapvető számtartományok.

A duplikált JSON-kulcs, hibás típus, ismeretlen felső szintű/közös kulcs,
ismeretlen core és érvénytelen séma verzió hibával leállítja a betöltést.
Hibánál nincs részlegesen felülírt típusos konfiguráció; újratöltéskor a tömbök
nem őrzik meg a korábbi értékeket. A hibák nem nyomtatják ki a JSON értékeit
vagy a feldolgozó kivételének esetleg jelszót tartalmazó szövegrészletét.

A `CONFIG_OK` a konfiguráció ellenőrzése. A tanúsítványok/adatfájlok meglétét,
az SQL-jogosultságokat és a tényleges játékindítást a normál indítás és a
localhostos integrációs próba ellenőrzi majd. A központi fájl nem rendelkezik
include- vagy külső felülíró konfigurációs lánccal.

Regressziók a projekt gyökeréből:

```powershell
cmake --build build --config RelWithDebInfo --target ServerTests Database LoginServer GameServer
ctest --test-dir build -C RelWithDebInfo --output-on-failure
python -B tests/server/ServerConfigToolsTests.py --bin-dir build -v
pwsh -NoProfile -File tests/server/CheckLoginStartup.ps1 -Binary build/SRC/Server/LoginServer/RelWithDebInfo/LoginServer.exe
```

Az új C++ teszt az éles közös és típusos betöltőket használja. A Python-próba az
összevonás adatmegőrzését, a külön hitelesítési adatokat, az útvonalakat, az
eredeti/kimeneti fájl védelmét és a valódi három bináris ellenőrző módját vizsgálja.

Ellenőrzött eredmény (Windows, 2026-10-19): mindhárom szerver normál és
AddressSanitizer-változata lefordult; a teljes CTest-csomag mindkét buildben
51/51 sikeres. A konfigurációs C++ teszt 60 ellenőrzése és a natív binárisokat
is használó Python-próba 9/9 esete mindkét buildben átment. A minta és az
öröklött részleges felülírások JSON-séma-ellenőrzése is sikeres. A LoginServer
visszautasított/néma SQL-indítási próbája rendre 2,28/5,15 másodperc alatt,
1-es hibakóddal, TLS/listener indítása előtt kilépett.
