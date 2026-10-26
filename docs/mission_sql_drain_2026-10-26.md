# Mission Engine 4/D–4c – ellenőrzött legacy SQL-sorürítés

Dátum: 2026-10-26. Ág: `codex/mission-engine`. Kizárólag helyi fejlesztés és teszt.

## Miért szükséges?

A Database két külön workerre küldi a player-oldali mentéseket:
`m_mainSQL[SQL_PLAYER]` és `m_asyncSQL[SQL_PLAYER]`. A cache-flush visszatérő
QID-jeinek egy részét a főszál további feldolgozás nélkül elengedi; a nem
visszatérő SQL-hívások eredménye eleve nem jut el a karakterhez.

Egy utánuk beállított `SELECT 1` csak a sorrendet igazolná. Akkor is sikeres
lehetne, ha korábban egy item/player mentés hibás volt, vagy egy elveszett
visszaigazolás miatt újrapróbált írás eredménye bizonytalan. Ebből nem szabad
a Mission-forrás feloldására következtetni.

## Elkészült kapcsolat a tényleges workerhez

- Minden queued SQL-munka a producer queue mutex alatt kap monoton sorszámot.
- Minden `CAsyncSQL` példánynak külön, a folyamaton belül nem újrahasznált lane-azonosítója van.
- `DrainQueue()` tényleges FIFO-munkát tesz a sorba. A completion a workerben,
  minden előtte álló munka végrehajtása és teljes eredménykiolvasása után készül.
- A ticket saját, szálbiztos állapotot birtokol; nincs benne ECS/session mutató.
  Nem kerül a legacy `PopResult()` sorba és nem értelmezhető `CQueryInfo`-ként.
- A receipt közli a határsorszámot, az eddig nem igazolt munkák számát és a
  legutóbbi ilyen munka sorszámát/SQL-hibakódját. Nem tárol SQL-szöveget vagy jelszót.
- A nem visszatérő, prepared és tranzakciós munkák is beleszámítanak.
  Rollback vagy CAS-konfliktus sem jelenti a kért mentés sikerét.
- A sikertelen próbálkozás akkor sem tűnik el, ha a legacy automatikus retry
  később sikeres. A jelen változás nem teszi idempotenssé a régi retry-politikát.
- Indulási hiba, eldobott queue vagy leállt/nem worker kapcsolat nem adhat
  sikeres nyugtát. A függő ticket törlése megszakított állapotot eredményez.

A Database `CDBManager::PlayerQueueOrigins()` és `DrainPlayerWrites()` API-ja
a valódi két player-workert adja vissza, külön tartva őket a direkt kapcsolattól.
A `ResourceDrain` ezeket a worker-ticketeket a `ResourceFence` konkrét
owner/core/session/writer-generációjához, challenge-éhez és barriersorszámaihoz köti.
Másik session, challenge, barrier vagy kétszer ugyanaz a lane nem fogadható el.
Mindkét lane és a hiteles source-stop kell a `Ready` állapothoz; bármelyik hiba blokkol.

## Baseline és hibák egyeztetése

Az első baseline a worker origója. Egy későbbi határ csak akkor lehet új baseline,
ha a natív koordinátor az addigi kanonikus állapotot és minden bizonytalan
mentést külön egyeztette. Az új barrier önmagában nem nyugtázza a régi hibát.
`ConfirmsSince()` ugyanazt a lane-t, későbbi sorszámot és változatlan hibanyomot követel.
Az összehasonlítás csak `Complete` ticket-eredménnyel használható.
Lane-azonosító és baseline nem menthető át új Database-folyamatba. Restart
után új, hitelesített kanonikus betöltés és új origó szükséges.

A nyom jelenleg lane-szintű: másik karakter hibás munkája is blokkolhatja a
Mission-egyeztetést. Ez konzervatív viselkedés; még nincs karakterenkénti
mentésattribúció vagy hibát automatikusan törlő eljárás.

A drain nem zárja le a forrást, nem flusholja önállóan a dirty cache-t, és
nem követi a direkt vagy másik SQL-kapcsolaton kiadott munkát. A native adapternek
ezeket rendeznie kell a ticketek sorba állítása előtt. A később érkező munka már
a következő határhoz tartozik. A drain nem egyetlen DB-tranzakció a két lane között.

## Kapcsolódó eredménykiolvasási javítás

A régi szöveges SQL-hívás minden eredményét kiolvassuk, a nem visszatérő hívásokét is.
A több statement/procedure későbbi `mysql_next_result` hibája megmarad a
`SQLMsg::uiSQLErrno` mezőben és a lane hibanyomában. Így egy első sikeres
eredményhalmaz nem fedhet el egy későbbi hibát, és a következő munka nem kapja
meg a korábbról olvasatlanul maradt eredménypaketteket. Az újrapróbált queue-munka
korábbi részleges eredménypufferei is felszabadulnak az új próbálkozás előtt.

Az új retry-próba egy meglévő sorrendi hibát is előhozott: a worker másolt
FIFO-jában maradhatott munka, miközben a producer queue már üres volt.
A `CopyQuery()` ezt üres állapotnak vette, így új producerkérésig nem folytatta
a bent maradt munkát. Most mindkét queue-t figyelembe veszi; a graceful shutdown
is a régebbi másolt FIFO-t dolgozza fel először. A regressziós próba ezt a
konkrét queue-állapotot időzítéstől függetlenül is előállítja.

## Ellenőrzés és határ

Az automatizált próbák scripted klienssel vizsgálják a sikertelen indulást,
shutdown/discard esetet, lane-azonosságot, bizonytalan retryt és azt, hogy a
drain nem kerül a legacy callbackek közé. A valódi helyi MariaDB-próba nem
visszatérő szöveges/prepared hibát, késői multi-result/procedure hibát, rollbacket,
sikeres következő lekérdezést és változatlan hibanyomot ellenőriz.

A Mission tényleges store-suite-ja már az új két-lane-es adaptert használja
a korábbi demonstrációs `SELECT 1` barrier helyett. Vizsgálja a stale sessiont,
challenge-et, invalidálást és a hibás régi mentés után tiltott jutalomtervet is.
A végleges normál regressziós kör **22/22**, az ASAN natív kör **12/12** sikeres.
A scripted SQL-suite 2256 ellenőrzést futtat; a külön ASAN MariaDB-infrastruktúra
234, a Mission store 2959 + 155 ellenőrzéssel sikeres, utóbbi kényszerített
DB-restarttal és hét elveszett COMMIT-válasszal. A GameServer, Database és LoginServer
normál/ASAN Windows x64 RelWithDebInfo buildje elkészült. FreeBSD-build vagy
Unity-játékteszt ebben a részfázisban nem történt.

Helyi naplók:

- `build/mission-drain-final-build.log`, `build/mission-drain-final-asan-build.log`.
- `build/mission-drain-regression.log`, `build/mission-drain-asan-regression.log`.
- `build/mission-drain-asan-sql-mariadb.log`, `build/mission-drain-asan-mission-mariadb.log`.

A részletes állapotot a `mission_engine_status_and_next_steps_2026-10-25.md` rögzíti.

Ez az élő koordinátor egyik szükséges adaptere. Az élő GameServer source-stop,
karakterenkénti cache-flush, writer-lease átadás, guarded legacy DML, ECS-rebase
és reconnect-helyreállítás még nincs végig bekötve. A Gameplay kapu zárt.
A worker-ticketek folyamaton belüli objektumok: a külön GameServer és Database
között nem küldhetők át mutatóként. A két folyamat közötti hiteles GD/DG
source-stop/drain/rebase üzenetek és az őket birtokló koordinátor még következnek.
A 4/D–4b egyszeri ASAN-visszaolvasási észlelésének oka továbbra is nyitott;
ez a módosítás nem bizonyítja annak javítását.

Távoli kapcsolat, feltöltés, build, telepítés vagy adatbázis-módosítás nem történt.
Az adatbázispróbák saját, ideiglenes, loopbackre kötött helyi MariaDB-példányon futnak.
