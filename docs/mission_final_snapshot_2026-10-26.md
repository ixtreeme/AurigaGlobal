# Mission Engine – natív karakter- és tárgypillanatkép

Dátum: 2026-10-26. Részfázis: **4/D–4g**. Helyi fejlesztés és tesztelés.

## Az új lépés

A `CLIENT_DESC::RequestPlayerCacheDrain` már maga készíti elő a GameServer
pillanatképét. Nem a hívónak kell külön, helyes sorrendben mentéseket küldenie:

1. Ellenőrzi az egyeztetett Database-linket, az aktuális natív sessiont,
   accountot, karaktert és a teljes EnTT-generációt.
2. Új előkészítés csak GAME fázisban, alkalmazott item-load után indulhat.
   Nyitott csere, raktár/mall, függő raktárbetöltés, mentéstiltás és a meglévő
   `CanHandleItems` által tiltott állapot elutasítja az előkészítést.
3. A valódi `SessionSystem::CreatePlayerProto` készíti a karakter sorát.
   Ez a meglévő mentő, annak játékidő-/alignment-könyvelésével együtt;
   nem egy második, eltérő player-szerializáló.
4. A descriptor élő nyilvántartásában újra ellenőrzi az azonosítót és a
   generációt, majd az ItemManager saját, másolt tárgysorokat készít.
5. Az összes sor ellenőrzése után egy birtokolt bájttömb készül:
   `GD_PLAYER_SAVE → GD_ITEM_SAVE… → GD_PLAYER_CACHE_REQUEST(Begin)`.
   Az egész tömb egy Packet hívással kerül a kimeneti pufferbe, ezért a
   Begin előtt ugyanazon adatfolyamon minden pillanatképsor ott áll.
6. Az új művelet figyelése a korábbi nyugták érvénytelenítése után indul.
   A későbbi legacy GD-írás továbbra is visszavonja a nyugtát.

A GD-packetek meglévő formátuma és a player/item táblák legacy ABI-ja marad.
A keretek fejléce explicit little-endian. Nincs új Unity-packet vagy SQL-séma.
A TCP-fogadás és a cache feldolgozása több részletben is történhet: ez a
csomag **nem** több táblát összefogó Database-tranzakció.

## A hordott tárgyak ellenőrzése

Az új `ITEM_MANAGER::BuildPlayerCacheSnapshot` a tényleges ECS-komponenseket
és az ItemManager ID-indexét olvassa. A fő inventory/felszerelés/öv,
dragon-soul, extra inventory és switchbot tömbjét összeveti az ItemOwner
komponensekkel. Mindkét irány szükséges: sem egy tömbben hivatkozott, de hibás
tulajdonosú tárgy, sem egy tulajdonosra mutató, de a tömbből hiányzó tárgy
nem maradhat észrevétlen.

Elutasítás történik többek között:

- hiányzó vagy újrafelhasznált entitásgeneráció, eltérő owner-entity/PID;
- hiányzó alapkomponens, ID-indexhiba, duplikált ID vagy tömbhivatkozás;
- hibás ablak, pozíció, nullás ID/vnum/count vagy folyamatban lévő fogyasztás;
- skip-save, csere-/zárolás-/megsemmisítés alatt álló tárgy;
- a karaktertől már elkerült, de még függő mentésű tárgy;
- 1024-nél több hordott tárgy.

A sorok ugyanazt a `BuildItemSave` szerializálót használják, amelyet az
eddigi `SaveSingleItem` is. A felszerelés/öv pozíciójának átszámítása, a
socketek, attribútumok és zárolt attribútum így közös kódon haladnak át.
A sorok ID szerint rendezettek; a Batch külön is ellenőrzi a tulajdonost,
az ID-kat és az ablak/pozíció egyediségét.

A capture nem küld tárgypacketet, nem töröl delayed-save bejegyzést, nem
változtat tulajdonost vagy skip-save flaget. Hiba esetén az eredmény üres,
az eredeti függő munkák megmaradnak. A sikeres capture sem nyugtázza őket:
egy később ténylegesen futó mentés visszavonhatja az ürítési bizonyítékot.

## Ismétlés és lezárás

Egy natív loginon belül továbbra is legfeljebb egy cache-előkészítés indul.
Azonos challenge ismétlése a meglévő műveletet használja, nem szerializál
új állapotot a régi Begin mögé. Más challenge vagy egy érvénytelenített
művelet újraindítása nem megengedett. Az új `Client::Started()` ezt már a
capture előtt felismeri. A sikertelen preflight még nem indít műveletet.

A visszatérő `true` a kérés elfogadását jelenti; nem SQL-commitot és nem
Mission-írási jogosultságot. A kész cache-nyugta a meglévő, sessionhöz és
entitásgenerációhoz kötött API-n olvasható vissza.

## Mi nincs még kész?

**Ez a rész nem állítja meg a gameplay erőforrás-termelőit.** A nyugta nem
jelenti az ECS-változások leállítását, a Database teljes itemhalmazának és
a live inventorynak az egyezőségét, kanonikus resource revisiont vagy
tartós writer-lease átadását. A korábban cache-be került, a live inventoryból
hiányzó sorok egyeztetése a későbbi rebase/handoff szerződés része.

A fiókszintű mount-raktár `account_mount_inventory` táblába, a GameServer
saját SQL-kapcsolatán ment (`pet/MountInventory.cpp::SaveItem`). Nem sorolható
be a player itemcache két workerének bizonyítékába. A safebox/mall és más
külső írók is külön rendezendők. A Batch ezeket nem fogadja karakterhez
tartozó itemként.

Nincs automatikus gameplay-hívó, `ResourceFence::SourceStopped` nyugtázás,
reward-dispatch vagy Mission-aktiválás. A következő feladat a tényleges
producer-stop és a cross-character/külön SQL-írók kezelése, majd a tartós
writer-handoff, ECS/cache-rebase és publikálás. Az egyszer észlelt korábbi
ASAN történeti hatásnyugta-visszaolvasási eltérés oka külön nyitott tétel.

## Ellenőrzési határ

A tesztek a termelési ItemManagert, a közös tárgysoros szerializálót, a Batch
keretezőt, a Database Cache/Save DML-t, a két valódi SQL-workert és a natív
cache-clientet ellenőrzik. A snapshot → cache → SQL út izolált MariaDB-n fut.
A teljes GameServer adapter fordítását a szerverbuild ellenőrzi; ez nem a
két teljes szerverfolyamat közötti socketes vagy Unity gameplay-próba.

A bővítés során előkerült egy korábbi tesztinfrastruktúra-hiány is: tizenkét
legacy suite SQL-helyettesítőjének konstruktora nem inicializálta az új,
const FIFO-lane azonosítót. Ezek lane-je most szándékosan nulla: nincs valódi
workerük, ezért nem állíthatnak elő hiteles drain-nyugtát. A `SQLMsg` lezárása
viszont a valódi `QueueDrain.cpp` cancellation segédjét igényli, amelyet ezek
a tesztcélok külön megkapnak. Az SQL-kapcsolat továbbra is a kontrollált
teszthelyettesítő, nem kapcsoltunk be mögöttük élő SQL-workert.

A teljes fordítás első, korlátlan `/MP` körében a két párhuzamos build túllépte
a fordítók memóriaigényét (C1060). A végső kör `_CL_=/MP2` korláttal futott;
ez helyi build-beállítás, nem forrás- vagy éles konfigurációváltoztatás.

Végleges helyi eredmények:

- Snapshot/framing és kapacitás: **4426** ellenőrzés, normál/ASAN.
- Tényleges ItemManager lifecycle/capture: **8947**, normál/ASAN.
- Cache codec/client, indult művelet és új login: **1507**, normál/ASAN.
- Cache/DML → két SQL-worker → wire/client: **455**, normál/ASAN,
  az új snapshot-soros MariaDB-scenarióval együtt.
- Natív session: **818**, izolált késleltetett SQL-lel **826**;
  normál és külön ASAN MariaDB-ellenőrzéssel.
- Normál CTest: **31/31** Mission/session/cache/SQL és **12/12** újrafordított
  legacy suite, összesen **43/43**.
- Natív ASAN CTest: **19/19 + 12/12**, összesen **31/31**. A két külön ASAN
  MariaDB-suite nincs beleszámítva ebbe a 31-es számba.
- GameServer, Database, LoginServer és az érintett natív testcélok:
  Windows x64 RelWithDebInfo normál/ASAN build.
- A 34 védett szezonális Mission Pass-fájl és nyolc korábbi Unity-fájl hash-e
  változatlan; eltérés nincs.

Helyi, nem verziózott ellenőrzési naplók a `build/` alatt:

- `mission-final-snapshot-native-verified-build.log`, `mission-final-snapshot-asan-verified-build.log`.
- `mission-final-snapshot-native-unit-build.log`, `mission-final-snapshot-asan-unit-build.log`.
- `mission-final-snapshot-native-legacy-tests-build.log`, `mission-final-snapshot-asan-legacy-tests-build.log`.
- `mission-final-snapshot-native-regression.log`, `mission-final-snapshot-asan-regression.log`.
- `mission-final-snapshot-native-legacy-regression.log`, `mission-final-snapshot-asan-legacy-regression.log`.
- `mission-final-snapshot-native-mariadb.log`, `mission-final-snapshot-asan-mariadb.log`.
- `mission-final-snapshot-asan-session-mariadb.log`, `mission-final-snapshot-protected-hashes.json`.

A tesztadatbázisok privát, helyi MariaDB-példányokon futottak.
A helyi játékadatbázis, Unity és távoli gép nem módosult. FreeBSD-build,
két teljes szerverfolyamat socketes elfogadása és Unity gameplay-próba
ebben a fázisban nem történt.
