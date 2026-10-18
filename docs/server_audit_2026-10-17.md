# Szerveroldali állapot és hibaaudit – 2026. október 17.

## Javítások lezárása – 2026. október 18.

Az alábbi, október 17-i kiinduló audit mind a nyolc pontja javítva a munkafában.
Az eredeti megállapítások és sorszámok a vizsgált `175a795f` állapotot írják le.

| Pont | Megvalósított javítás | Tartós regresszió |
| --- | --- | --- |
| F1 | A bemeneti kezelő a mozgásrendszerre bízza a cél commitját; visszautasított parancsot nem publikál | SpatialLifecycleTests: valódi CInputMain::Move és descriptor |
| F2 | Eltelt időből és rögzített indulási koordinátából interpolál; megőrzi az aktív parancs időzítését | SpatialLifecycleTests: azonos időbélyeg, finom/durva frissítés, lag, időszámláló-átfordulás és sebességkeret |
| F3 | A kimenő átnevezési kérés építése ellenőrzi a fiókot, indexet, karakterhelyet és a név lezárását/politikáját | CharacterSelectionTests: mind a 256 index, hibás és lezáratlan nevek |
| F4 | Ready/Pending/Authenticated sessionállapot; a teljes hitelesítés alatt egyetlen kérés; eredmény- és kapcsolattakarítás | LoginAuthTests: párhuzamos kérések, retry és késői eredmény bontott kapcsolatra |
| F5 | Siker kizárólag pozitív Database-ACK után; egyedi meglévő GD/DG handle, egyszeri callback, tíz másodperces határidő | LoginAuthTests és LoginDbLinkTests: offline link, elutasítás, felcserélt/dupla/hibás ACK, timeout és linkvesztés |
| F6 | A betöltéskor futó NormalizeSecondarySkills hozzáadja a régi és új képességek pontköltségét | PlayerLoadTests: régi/új költségek, limitek, alacsony szint és ismételt betöltés |
| F7 | A nyitott session nem kivétel az új kapcsolatok korlátozásából; legfeljebb 128 egyidejű login session címenként | LoginLimiterTests: tiltás alatti új próbák, kapacitás felszabadulása, meglévő hitelesítési limitek |
| F8 | Aktív gyökérszintű enable_testing, ServerTests buildcél, server tesztcímke és futási időkorlát | A CTest mind a 43 aktuális szervertesztet futtatja |

Ellenőrzés Windows/MSVC alatt, mindkét esetben `RelWithDebInfo` konfigurációval:

- Normál build: GameServer, LoginServer, Database és ServerTests sikeres; CTest **43/43**.
- AddressSanitizer build: ugyanazok a célok sikeresek; CTest **43/43**.
- `git diff --check`: sikeres.

A reprodukciók tartós változatai a verziózott tesztekbe kerültek. A korábbi
`build/server-audit` próbák az eredeti audit segédanyagai; a javítások utáni
ellenőrzéshez a verziózott tesztcélokat kell használni. A futtatási parancsok és
a lefedettség a [tesztek leírásában](D:/AurigaGlobal/LiveWork/AurigaGlobal/tests/server/README.md:3) szerepelnek.

A javítások utáni naplók:

- [Normál build](C:/Users/ixtre/AppData/Local/Temp/auriga-eight-fixes-normal-build.log).
- [Normál CTest](C:/Users/ixtre/AppData/Local/Temp/auriga-eight-fixes-normal-tests.log).
- [ASAN build](C:/Users/ixtre/AppData/Local/Temp/auriga-eight-fixes-asan-build.log).
- [ASAN CTest](C:/Users/ixtre/AppData/Local/Temp/auriga-eight-fixes-asan-tests.log).

Élő kliens–adatbázis integráció és telepítés nem történt. A képességpontok
betöltéskori számítása helyes; a korábban esetleg túlköltött képességszintek
visszaállítása külön adatkorrekciót igényelhet, élő karakteradatot nem írtam át.

## Eredeti audit

Vizsgált állapot: `175a795f`, `ECS_+_BGFX` ág. A munkafa az audit kezdetén tiszta volt. A vizsgálat forráskódra, helyi fordításokra, a meglévő regressziós tesztekre és külön célzott reprodukciókra támaszkodik. Éles szerver, élő adatbázis és Unity kliens ellen nem futott integrációs teszt. Alkalmazáskódot nem módosítottam.

## Jelenlegi állapot

A szerver technikai alapja jelentősen modernizált, de a következő munkafázisnak a működési útvonalak együttes ellenőrzését és stabilizálását javaslom. A zöld egységtesztek mellett is maradt két alapvető mozgáshiba és több belépési hiba.

- Külön `LoginServer`, `GameServer` és `Database` bináris épül.
- A karakter- és itemállapot ECS-alapú; a régi `CHARACTER` és `CItem` objektummodell már nem az aktuális játékállapot tulajdonosa. A switchbot és a házasság állapota is entitásokon van. Emiatt a szeptemberi `phase31_remaining_inventory.txt` ezekre vonatkozó teendői már elavultak.
- A klienskapcsolatokhoz TLS van; a kontextus minimum TLS 1.2-t állít be, a régi csomagtitkosítás kikerült. A közös Asio réteg külön I/O szálról adja át a befejezéseket a játékszálnak.
- Van prepared SQL támogatás, JSON-alapú adatbetöltés, monoton időkezelés és több időmezőhöz 64 bites adatbázis-migráció.
- A tesztállomány erős az izolált életciklusokban: item, exchange, spatial, event, mount, quest, shop, guild és több más alrendszer valódi implementációját teszteli szolgáltatáshelyettesítőkkel.
- A legsürgetőbb hiány a teljes hívási láncok tesztelése: csomagkezelő → ECS-rendszer → időbeli frissítés, illetve LoginServer → Database → GameServer.

## Ellenőrzések eredménye

| Ellenőrzés | Eredmény |
| --- | --- |
| GameServer, LoginServer, Database fordítása, normál `RelWithDebInfo` | Sikeres |
| Ugyanezek AddressSanitizer konfigurációban, `RelWithDebInfo` | Sikeres |
| A forrásban deklarált 40 szerverteszt, normál build | 40/40 sikeres |
| A forrásban deklarált 40 szerverteszt, ASAN build | 40/40 sikeres |
| A normál build jelenlegi CTest-listája | Csak 31 teszt; mind sikeres |
| Külön login/limiter reprodukciók | 3 hibajelenség igazolva, 137 ellenőrzés |
| Külön mozgási reprodukciók | 2 hibajelenség igazolva, kontrollesettel |

A normál 40-es eredmény a CTest 31 tesztjéből és a hiányzó kilenc bináris külön futtatásából áll. ASAN alatt mind a 40 deklarált szervertesztet közvetlenül indítottam. Ezek az eredmények nem jelentenek élő SQL- vagy kliensintegrációs igazolást.

## Konkrét hibák, prioritással

P1: következő kiadás előtt javítandó működési vagy bemenetkezelési hiba. P2: fontos helyességi vagy tesztelési hiba.

### F1 – P1: a mozgás bemeneti kezelője meghiúsítja a mozgás inicializálását

Forrás: [movement_input.cpp:190](D:/AurigaGlobal/LiveWork/AurigaGlobal/SRC/Server/GameServer/entity/movement_input.cpp:190), [MovementSystem.cpp:1176](D:/AurigaGlobal/LiveWork/AurigaGlobal/SRC/Server/GameServer/ecs/systems/MovementSystem.cpp:1176).

`CInputMain::Move` előbb beírja a csomag célját a `MovementDestination` komponensbe, majd ugyanazzal a céllal meghívja a `GotoWithDuration` függvényt. Utóbbi az egyező cél miatt azonnal `false` értékkel visszatér. A hívó ezt figyelmen kívül hagyja, ezért a parancshoz tartozó időtartam, indulási idő és sebesség nem inicializálódik; a kiküldött mozgáscsomag régi vagy nulla időtartamot kap.

Reprodukció: a spatial teszt szolgáltatáshelyettesítőivel, a valódi `GotoWithDuration` implementációval megismételtem a bemeneti kezelő komponensírását. Az új cél visszautasítást kapott, az időtartam nulla maradt. Kontrollként az előzetes célírás nélkül ugyanaz a hívás sikerült, és 1200 ms-ot tárolt. A teljes `CInputMain::Move` hálózati belépési pontját ez a külön próba nem futtatja.

Javaslat: a cél és az időzítés commitját a mozgásrendszer végezze; a csomagkezelő csak sikeres indítás után publikáljon. Kell olyan regresszió, amely a tényleges bemeneti kezelőből indul.

### F2 – P1: a mozgási ciklus figyelmen kívül hagyja az eltelt időt és a beállított időtartamot

Forrás: [MovementSystem.cpp:1286](D:/AurigaGlobal/LiveWork/AurigaGlobal/SRC/Server/GameServer/ecs/systems/MovementSystem.cpp:1286), [MovementSystem.cpp:1335](D:/AurigaGlobal/LiveWork/AurigaGlobal/SRC/Server/GameServer/ecs/systems/MovementSystem.cpp:1335), [main.cpp:742](D:/AurigaGlobal/LiveWork/AurigaGlobal/SRC/Server/GameServer/core/main.cpp:742).

`MovementSystem_Update` hívásonként a `MovementSpeed.run` értékével lép. A távolságszámításban nem használja sem az eltelt időt, sem a `GotoWithDuration` által beállított `moveSpeed`/`moveDuration` értéket. Ráadásul minden nem végső lépésnél 1-re írja át az időtartamot. A karakter sebessége így a frissítések gyakoriságától függ, és a csomagban közölt időzítés eltérhet a szerver koordinátáinak haladásától.

Reprodukció: a valódi mozgásrendszeren egy 1000 egységes, 10 másodperces mozgás két, teljesen azonos időbélyeggel futtatott frissítése `x=100 → 300 → 500` pozíciókat adott. Az első frissítés után az időtartam 1 ms lett. Ez F1-től függetlenül, helyesen inicializált mozgáson is jelentkezik.

Javaslat: az előrehaladás alapja eltelt monoton idő és az érvényes mozgási sebesség/időzítés legyen. Tesztelendő azonos időbélyeg, különböző tickgyakoriság, szerverakadás utáni felzárkózás, sebességbónusz, séta és mountváltás.

### F3 – P1: ellenőrizetlen karakterindex az átnevezésnél

Forrás: [input_login.cpp:218](D:/AurigaGlobal/LiveWork/AurigaGlobal/SRC/Server/GameServer/network/input_login.cpp:218), [CG_Packet.h:588](D:/AurigaGlobal/LiveWork/AurigaGlobal/SRC/Server/common/Headers/CG_Packet.h:588).

A kliens által küldött `uint8_t index` közvetlenül indexeli a `c_r.players` tömböt, először a `bChangeName`, majd a `dwID` olvasásakor. A tömb öt elemű, de a csomag 0–255 közötti értéket hordozhat. A karakterválasztó és törlő útvonallal ellentétben itt hiányzik az `index < PLAYER_PER_ACCOUNT` ellenőrzés.

Kódból igazolt tömbhatáron túli olvasás. Az adott futás memóriatartalmától függően hibás PID kerülhet további feldolgozásba vagy a folyamat hibázhat; konkrét összeomlást vagy másik fiók átnevezését nem reprodukáltam.

Javaslat: határellenőrzés minden tömbelérés előtt, valamint az átnevezési névmező hosszának/lezárásának ellenőrzése. Határesetek: 4, 5 és 255.

### F4 – P1: egy login kapcsolat több fiók állapotát keverheti össze

Forrás: [login_auth.cpp:101](D:/AurigaGlobal/LiveWork/AurigaGlobal/SRC/Server/LoginServer/login_auth.cpp:101), [login_auth.cpp:117](D:/AurigaGlobal/LiveWork/AurigaGlobal/SRC/Server/LoginServer/login_auth.cpp:117), [login_auth.cpp:145](D:/AurigaGlobal/LiveWork/AurigaGlobal/SRC/Server/LoginServer/login_auth.cpp:145).

A függő kérések ellenőrzése fióknévre történik, kapcsolatonként nincs kizárás. Ugyanaz a session ezért A és B fiókra is indíthat aszinkron lekérdezést. A második kérés felülírja a session nevét, miközben az első sikeres válasz A néven bekerülhet a `m_logins` mapbe. A kapcsolat lezárásakor csak a session utolsó neve törlődik.

Reprodukció a valódi `LoginAuth` implementációval: A sikeres, B sikertelen, majd sessiontakarítás után A új belépése `ALREADY` választ kapott, új SQL-lekérdezés nélkül. Az A bejegyzés a sessionre mutató pointert is megtartja, miközben a listener a sessiont felszabadíthatja; a reprodukció az elakadt belépést igazolta.

Javaslat: explicit sessionállapot (`Auth`, `Pending`, `Authenticated`), egyszerre egy kérés, kérésazonosítóhoz kötött eredmény és teljes kapcsolattakarítás. A későn érkező eredmény ne írhasson újabb hitelesítés állapotára.

### F5 – P1: a kliens a Database-regisztráció sikere előtt kap belépési sikert

Forrás: [login_auth.cpp:259](D:/AurigaGlobal/LiveWork/AurigaGlobal/SRC/Server/LoginServer/login_auth.cpp:259), [login_db_link.cpp:236](D:/AurigaGlobal/LiveWork/AurigaGlobal/SRC/Server/LoginServer/login_db_link.cpp:236), [ClientManager.cpp:2081](D:/AurigaGlobal/LiveWork/AurigaGlobal/SRC/Server/Database/ClientManager.cpp:2081).

A `SendAuthSuccess` megelőzi a kulcs regisztrálását. Ha a DB-link nincs kész, a `SendAuthLogin` csak naplóz és eldobja a kérést. A Database egyébként küld regisztrációs eredményt, de a LoginServer ezt nem köti a kliens sikerüzenetéhez: a link a normál állapotában csak a pinget dolgozza fel. A kliens így olyan kulcsot kaphat, amelyet a GameServer mögötti Database nem ismer.

Reprodukció: sikeres hitelesítési SQL-válasz után, a valódi `LoginDBLink` lekapcsolt állapotában a session sikerüzenetet kapott, miközben a regisztráció eldobása is megtörtént. Élő adatbázist nem használtam.

Javaslat: a Database pozitív válasza legyen a kliensnek küldött siker feltétele. Kell kéréskorreláció, időkorlát, linkvesztés-kezelés, és sikertelen regisztráció esetén az ideiglenes loginállapot visszavonása. A kulcsot ne lehessen a regisztráció előtt felhasználni.

### F6 – P2: belépéskor hibásan újraszámolt másodlagos képességpontok

Forrás: [ClientManagerPlayer.cpp:720](D:/AurigaGlobal/LiveWork/AurigaGlobal/SRC/Server/Database/ClientManagerPlayer.cpp:720). Az `ENABLE_NEW_SECONDARY_SKILLS` aktív a `service.h` fájlban.

A `skill_point =+ ...` hozzárendelés pozitív előjelű új értéket állít be. Emiatt a korábban összesített régi másodlagos skillek pontköltsége elvész, és a `sub_skill_point = max_point - skill_point` túl sok szabad pontot ad vissza betöltéskor.

Példa: 50-es szintnél 41 a keret. Ha egy régi skillre 20 pont ment, az új skillekre pedig semmi, a jelenlegi számítás 41 szabad pontot ad a várt 21 helyett. Ezt a számtani hibát a forrás igazolja; élő karakterrel nem futott próba.

Javaslat: `+=`, célzott adatbetöltési regresszió és a már érintett karakterek pontjainak ellenőrzése. A meglévő skillpontok javítása külön adatkorrekciós döntés.

### F7 – P2: egyetlen nyitott session megkerüli az új kapcsolatok tiltását

Forrás: [login_limiter.cpp:11](D:/AurigaGlobal/LiveWork/AurigaGlobal/SRC/Server/LoginServer/login_limiter.cpp:11), [login_listener.cpp:119](D:/AurigaGlobal/LiveWork/AurigaGlobal/SRC/Server/LoginServer/login_listener.cpp:119).

Blokkolt IP esetén az `AllowConnect` minden új kapcsolatot átenged, ha az IP-hez már tartozik legalább egy aktív session. A NAT mögötti játékosok megóvása érthető cél, de ez az új kapcsolatok korlátozását is kikapcsolja. Az aktív számláló már az elfogadott kapcsolatot számolja, nem csak a sikeresen bejelentkezett játékost.

Reprodukció a valódi limiterrel: egy nyitott session mellett a 31. kapcsolat tiltást kapott, az ezt követő 100 új kísérletet viszont mind engedélyezte. Ehhez nem kellett kivárni a tiltási időt.

Javaslat: a már nyitott kapcsolatok megtartása mellett az új kapcsolatoknak külön sebesség- és darabszámkorlát kell. A protokollkézfogás és az autentikáció várakozására is érdemes határidőt tenni; a TLS-kézfogási időkorlát önmagában ezt nem fedi le.

### F8 – P2: a CTest nem futtatja az aktuális teljes szerverteszt-készletet

Forrás: [CMakeLists.txt:61](D:/AurigaGlobal/LiveWork/AurigaGlobal/CMakeLists.txt:61), [GameServer/CMakeLists.txt:101](D:/AurigaGlobal/LiveWork/AurigaGlobal/SRC/Server/GameServer/CMakeLists.txt:101).

Az egyetlen `enable_testing()` ki van kommentelve. Az aktuális GameServer CMake 40 tesztet deklarál, de a build könyvtárban megmaradt régi `CTestTestfile.cmake` csak 31-et tartalmaz. A build után sem került be a kilenc új teszt a CTest-listába. A szokásos `ctest` ezért zöld, miközben éppen több friss regresszió kimarad.

Kimarad: `login_db_link`, `login_auth`, `login_session`, `tls_session`, `peer_auth`, `core_buffer`, `singleton`, `daily_reward`, `async_sql_prepared`.

Javaslat: `include(CTest)` vagy aktív `enable_testing()`, szükség esetén `BUILD_TESTING` kapcsolóval. Tiszta buildből is ellenőrizni kell a regisztrált darabszámot, majd mind a 40 teszt futását. A tesztek `EXCLUDE_FROM_ALL` célpontok, ezért a buildlépésnek ezeket is ki kell választania.

## Javasolt következő munkasorrend

1. **Mozgás javítása egy összefüggő feladatként:** F1 és F2, valódi bemeneti kezelővel és időalapú integrációs regresszióval. Ellenőrizendő séta/futás, mount, sebességbónusz, célváltás, szektorszél, távoli megfigyelő és lag utáni viselkedés.
2. **Kliensbemenetek határellenőrzése:** először F3, majd a hasonló indexek, csomagban érkező fix méretű karakterláncok és változó hosszúságú csomagok egységes átnézése. A LOGIN3 lezárásvédelméből nem következik a többi GameServer-csomag védettsége.
3. **Belépési folyamat lezárása:** F4/F5/F7, kapcsolatonkénti állapotgéppel és Database-visszaigazolással. Külön teszt induló szerverre, megszakadó DB-linkre, késői eredményre, párhuzamos kérésekre, gyors újracsatlakozásra és login → karakterválasztás átmenetre.
4. **Képességpontok javítása:** F6, majd annak megállapítása, érintett-e már létező karakteradatot.
5. **Megbízható automatikus ellenőrzés:** F8 javítása, normál és ASAN buildből a teljes készlet, továbbá az öt most reprodukált hibajelenségből tartós regresszió. A jelenlegi mozgástesztek több helyen hívásonkénti lépéshosszra ellenőriznek; ezeket az időalapú szerződéshez is hozzá kell igazítani.
6. **Valós SQL-integráció és hibainjektálás:** loginregisztráció, logout/relog itemmentés, exchange, offline shop/safebox és daily reward. Kapcsolatvesztésnél és folyamatleállításnál is ellenőrizni kell a mentést, az ismételt kérés hatását és az újraindulás utáni állapotot. Ezek szükséges ellenőrzési területek, az audit nem igazolt bennük új duplikálási hibát.
7. **FreeBSD/Linux és tesztkörnyezet:** a célplatformon is fordítás és teszt, majd a szükséges SQL-migrációk tényleges alkalmazottságának ellenőrzése. A mostani fordítások Windows/MSVC alatt futottak.
8. **Dokumentáció és üzemeltetés:** egyetlen friss állapotlap, a régi ECS-teendők archiválása; mérhető loginhibák, DB-várakozók, mentési hibák és tickidők. A következő nagy refaktor előtt ezekből legyen ellenőrizhető kiadási alap.

## Reprodukciós anyagok és naplók

A külön próbák a git által figyelmen kívül hagyott build könyvtárban maradtak, az alkalmazáskód és a meglévő tesztforrások módosítása nélkül:

- [Login/limiter próba](D:/AurigaGlobal/LiveWork/AurigaGlobal/build/server-audit/AuditLoginProbe.cpp), a meglévő `LoginAuthTests` szolgáltatáshelyettesítőiből, a valódi `login_auth.cpp`, `login_limiter.cpp` és `login_db_link.cpp` felhasználásával.
- [Mozgási próba](D:/AurigaGlobal/LiveWork/AurigaGlobal/build/server-audit/AuditMovementProbe.cpp), a meglévő spatial tesztkörnyezetből, a valódi mozgásrendszerrel.
- Binárisok: `build/SRC/Server/GameServer/RelWithDebInfo/AuditLoginProbe.exe` és `AuditMovementProbe.exe`.
- Normál buildnapló: `C:/Users/ixtre/AppData/Local/Temp/auriga-server-audit-build-20261017.log`.
- CTest-napló: `C:/Users/ixtre/AppData/Local/Temp/auriga-server-audit-ctest-20261017.log`.
- ASAN buildnapló: `C:/Users/ixtre/AppData/Local/Temp/auriga-server-audit-asan-build-20261017.log`.
- ASAN teljes tesztfutás: `C:/Users/ixtre/AppData/Local/Temp/auriga-server-audit-asan-tests-20261017.log`.
