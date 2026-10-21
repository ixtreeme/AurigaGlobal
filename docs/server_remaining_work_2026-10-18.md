# Ellenőrzött szerveroldali feladatlista – 2026. október 18.

Az eredeti audit alapállapota: `129a28d7` – a korábbi nyolc javítás commitja. A vizsgálat kezdetén a munkafa tiszta volt; akkor mindhárom szerver normál/ASAN buildje és 43/43 teszt volt sikeres. Az alábbi részletes megállapítások ezt az auditállapotot rögzítik. A végrehajtás friss állapota külön szerepel itt.

Az R1–R5 javításai és regressziói a `25a2b1cf` commitban szerepelnek.

Az R6–R7 javításai és regressziói az `e11abc55` commitban szerepelnek.

## Végrehajtási állapot – R1–R8 megvalósítva és ellenőrizve

| Pont | Megvalósult változtatás |
|---|---|
| R1 | Tartalom szerinti, normalizált névegyezés; login- és klienskulcs-ellenőrzés; lezáratlan mezők elutasítása az account lekérdezése és naplózása előtt. |
| R2 | A karakter INSERT minden értéke típusos prepared paraméter, a skill/quickslot mezők bináris paraméterek. A slot lekérdezés accountazonosítója is paraméterezett; a slotindex és a név ellenőrzött. |
| R3 | Közös klienscsomag-ellenőrzés a feldolgozás előtt, hosszkorlátos normalizálás, külön login/create/delete/version védelem és offline-shop névellenőrzés. |
| R4 | Tényleges SQL-készállapot megvárása, indulás megszakítása SQL/HWID-hibánál, 5 másodperces connector-időkorlátok, 10 másodperces belépési kísérletek és korlátos protokollvárakozás. Weak visszahívási jegyekkel a lejárt/bezárt session azonnal felszabadítható. A naplózó az SQL-objektumok után áll le. |
| R5 | OpenSSL `RAND_bytes` alapú login-kulcs, hibánál kulcskiadás nélküli lezárással. A limiter monoton órát használ; 15 perc inaktivitás után, körönként legfeljebb 256 host- és 256 accountbejegyzést vizsgál. Mindkét állapottér felső korlátja 65 536; a megtelt tér új, nem követhető kéréseket utasít el, az élő sessionök és aktív tiltások megtartásával. |
| R6 | A leváltott `NetworkSyncSystem_Update` ciklus és deklarációja kivezetve. Végleges, eseményenkénti publikálási szerződés készült; a `DirtyTag` nem hálózati sor. A megjelenés UPDATE a mozgással és pontváltozásokkal közös, térbeli verziót ellenőrző `PacketView` útvonalat használja. |
| R7 | Az event flag teljes GameServer/Database lánca `int64_t`, a ló határideje `uint64_t`. Fix 64 bites házassági/privilege/battle-pass belső mezők; 64 bites lejárati sorok és határellenőrzés. Javított privilege-clamp, házassági INSERT-hibakezelés és esküvői kitakarítás; az inaktív battle pass SQL-értéke `NULL`. A megváltozott DG-csomagméretet a feldolgozás előtt ellenőrzi a GameServer. |
| R8 | A quest NPC-kapcsolatok és kategóriák típusos JSON-ból töltődnek; mindkét TXT-parser kivezetve. Szigorú konverter, JSON-sémák, mező-/tartomány-/duplikációellenőrzés és cache-alapú kategórialekérdezés készült. Hibás metadata mellett a reload megtartja a működő állapotot; sikeres reload törli a régi név–index kapcsolatokat. |

Ellenőrzés: GameServer, LoginServer és Database sikeres normál és ASAN RelWithDebInfo build; **46/46 CTest mindkettőben**. Külön MariaDB 13.0.2 tesztpéldányon a repository sémájával a karakter INSERT és minden mező visszaolvasása is sikeres, `uint64_t` maximális HP-val, nagy SP/pénz értékekkel és bináris skill/quickslot adatokkal. A teszt a saját SQL-kapcsolatát megszakítva a cached prepared lekérdezés helyreállását is ellenőrzi; mindkét builddel **136 ellenőrzés sikeres**.

A valódi LoginServer mindkét builddel szabályosan 1-es kóddal lépett ki elutasított SQL-portnál, válasz nélküli SQL-végpontnál és elérhető SQL mellett hiányzó HWID-táblánál. Az első két eset reprodukálható a [CheckLoginStartup.ps1](D:/AurigaGlobal/LiveWork/AurigaGlobal/tests/server/CheckLoginStartup.ps1) szkripttel; a részletes tesztleírás a [tests/server/README.md](D:/AurigaGlobal/LiveWork/AurigaGlobal/tests/server/README.md) fájlban van.

Az R5 után a LoginServer és az érintett tesztek normál/ASAN buildje is sikeres; a teljes regresszió ismét **46/46 mindkettőben**. Az új esetek lefedik a hibás vagy csak nulla mintákat adó véletlenforrást és az újrapróbálást, a monoton időablakok és tiltások pontos határait, a takarítási munkakorlátot, az élő sessionök megőrzését, mindkét állapottér telítődését és helyreállását. Telített limiternél a valódi `LoginAuth` az SQL-lekérdezés előtt válaszol `BLOCK` státusszal. A korábbi kapcsolati és credential-küszöbök, valamint a nem nulla, 31 bites kulcstartomány változatlan.

Az R6 után a GameServer és a teljes ServerTests cél normál/ASAN RelWithDebInfo buildje is sikeres; **46/46 CTest mindkettőben**. Az új esetek a valódi packet builderekkel és küldési útvonallal ellenőrzik a mozgás közbeni felszerelés/affect/skill-color UPDATE-et, a saját és közeli/távoli címzetteket, a teljes 255 mezős pontcsomagot, a 64 bites HP/SP-deltákat, a teljes skilltáblát és a két címzett közötti térbeli verzióváltást. Nincs szintetikus WAIT vagy részleges teljespont-csomag. A részletes szerződés: [server_network_publication.md](D:/AurigaGlobal/LiveWork/AurigaGlobal/docs/server_network_publication.md).

Az R7 GameServer/LoginServer/Database és ServerTests buildje normál és ASAN RelWithDebInfo konfigurációban is sikeres; **48/48 CTest mindkettőben**, valamint külön MariaDB 13.0.2 példányon **109 SQL-/állapotellenőrzés mindkettővel** sikeres. Az új `DatabaseTimeTests` és `PrivilegeTimeTests`, valamint a bővített quest/GM/horse/marriage tesztek 2038/2106 utáni időket, signed/unsigned határokat, teljes Lua/GM értékátadást, lejáratot és SQL-hibát is ellenőriznek. Részletek és közös komponensfrissítés: [server_time64_contract.md](D:/AurigaGlobal/LiveWork/AurigaGlobal/docs/server_time64_contract.md).

Az R8 GameServer- és ServerTests-buildje normál és ASAN RelWithDebInfo konfigurációban is sikeres; **49/49 CTest mindkettőben** átment. A `QuestDataTests` mindkét konfigurációban **77 ellenőrzést**, a konverter Python-tesztje mindkét C++ olvasóval **7/7 tesztet** teljesített. A bővített quest runtime összesen **629 ellenőrzése** sikeres. A helyi questadatok 0 NPC- és 1 kategóriarekordja konvertálva, teljes TXT/JSON rekordparitással és változatlan eredeti TXT-hashekkel. Szerződés és telepítés: [server_quest_json_contract.md](D:/AurigaGlobal/LiveWork/AurigaGlobal/docs/server_quest_json_contract.md).

**Nyitott munkacsomagok: R9–R13.** Az R11 továbbra is csak tervezett LostCastle-kiadás esetén szükséges. A teljes klienses belépés/létrehozás/játék, a kétklienses megjelenítés és az éles adatbázis-séma ellenőrzése az R12 része; a helyi regresszió és az izolált SQL-próba ezt nem helyettesíti.

**Klienses tesztfüggőség:** a felhasználó jelzése alapján élő klienspróba csak a Unity kliens csatlakozási átállása után végezhető el. A klienses R12-ellenőrzések addig függőségre várnak; a kliens nélkül automatizálható szerver-/adatbázis-tesztek és a többi szerveroldali feladat továbbhaladhatnak. Az R6 szerveroldali megvalósítása kész, az élő megjelenítés ellenőrzése még nyitott.

## Az R1–R4 eredeti auditmegállapításai – megvalósítva

### R1 – Database kulcsos belépésének névellenőrzése

**Prioritás: P1. Típus: kódból igazolt hiba.**

Forrás: [ClientManagerLogin.cpp:104](D:/AurigaGlobal/LiveWork/AurigaGlobal/SRC/Server/Database/ClientManagerLogin.cpp:104).

A `QUERY_LOGIN_BY_KEY` függvényben `if (r.login == szLogin)` szerepel. Ezek külön karaktertömbök, így ez címeket hasonlít össze. Az elvárt, eltérő név miatti elutasítás nem működik. A kulcs és a klienskulcs ellenőrzése ettől még megtörténik; a hiány a kért login és a regisztrált login egyezésének ellenőrzése.

Teendő: tartalom szerinti, eltérésre elutasító összehasonlítás, a bejövő mező lezárásának ellenőrzésével együtt. Regresszió kell azonos névre, eltérő névre, normalizált névre és rossz kulcsra.

Lezárás: megfelelő kulcs/klienskulcs mellett is elutasítja az eltérő loginnevet; az egyező név továbbjut.

### R2 – Új karakter létrehozásának SQL-formátumhibái

**Prioritás: P1. Típus: forrásból és fordítói diagnosztikából igazolt hiba.**

Forrás: [ClientManagerPlayer.cpp:1228](D:/AurigaGlobal/LiveWork/AurigaGlobal/SRC/Server/Database/ClientManagerPlayer.cpp:1228).

Az INSERT numerikus részét még hosszú `snprintf` formátum építi. A legutóbbi normál és ASAN build is `C4477` figyelmeztetést adott: `%d` helyén `uint64_t`, `%llu` helyén `int` argumentum van. A HP/SP 64 bitesítése után ezen az útvonalon maradt típuseltérés.

Teendő: az oszlopokat, helyőrzőket és argumentumokat egyenként egyeztetni, célszerűen a teljes értéklistát prepared paraméterekre átvinni. A név és a blobok már paraméterezettek, tehát a numerikus rész lezárása a hátralévő munka.

Lezárás: nincs formátumfigyelmeztetés; az új karakter létrehozása után a visszaolvasott HP, SP, pénz, statok és bővített mezők egyeznek a kéréssel, 32 bit feletti értékeknél is.

### R3 – A többi GameServer-csomag szövegmezőinek határellenőrzése

**Prioritás: P1. Típus: megmaradt bemenetkezelési hiány.**

Források: [input_login.cpp](D:/AurigaGlobal/LiveWork/AurigaGlobal/SRC/Server/GameServer/network/input_login.cpp), [input.cpp:148](D:/AurigaGlobal/LiveWork/AurigaGlobal/SRC/Server/GameServer/network/input.cpp:148), [Core/utils.cpp:61](D:/AurigaGlobal/LiveWork/AurigaGlobal/SRC/Server/Core/utils.cpp:61).

A LOGIN3 és az átnevezés már ellenőrzi a lezárást. Más belépési pontokon maradt közvetlen C-string használat: például karakterlétrehozáskor `strlen(pinfo->name)`, kliensverziónál a csomagmezők naplózása és `SetClientVersion(p->timestamp)`. A `trim_and_lower` kezdő whitespace-átugrása forráshossz nélküli.

Teendő: fix méretű csomagmezők ellenőrzése a naplózás és C-string feldolgozás előtt; hosszkorlátos normalizálás. A LOGIN2, karakterlétrehozás/törlés, verzió és további csomagok ellenőrzése ugyanennek a munkának a része.

Lezárás: lezáratlan és csomagvégen álló mezők ASAN alatt sem olvashatók túl; hibás bemenet nem jut SQL-hez vagy állapotmódosításhoz.

### R4 – LoginServer SQL-készállapot és a hiányzó várakozási határidők

**Prioritás: P1. Típus: megmaradt működési hiány.**

Források: [LoginServer/main.cpp:50](D:/AurigaGlobal/LiveWork/AurigaGlobal/SRC/Server/LoginServer/main.cpp:50), [AsyncSQL.cpp:151](D:/AurigaGlobal/LiveWork/AurigaGlobal/SRC/Server/SQL/AsyncSQL.cpp:151), [AsyncSQL.cpp:223](D:/AurigaGlobal/LiveWork/AurigaGlobal/SRC/Server/SQL/AsyncSQL.cpp:223), [login_session.cpp:24](D:/AurigaGlobal/LiveWork/AurigaGlobal/SRC/Server/LoginServer/login_session.cpp:24).

Az aszinkron `Setup` a szál indítását jelzi vissza, miközben a szál későbbi sikertelen `Connect` esetén kilép. A LoginServer `auth.Init` hibájánál is csak naplóz és folytatja az indulást. A TLS-kézfogásnak és az új Database-regisztrációnak már van időkorlátja; a TLS utáni alkalmazáskézfogásnak, a LOGIN3 várakozásnak és a függő account-SQL-nek nincs hasonló lezárása a LoginSessionben. A session nem állít be heartbeat/időkorlátot.

Teendő: megkülönböztetett indulási/kész/hibás SQL-állapot; meghatározott leállási vagy újracsatlakozási viselkedés; a várakozások határideje. A késői SQL-válasz kezelését a session élettartamával együtt kell megoldani, szabad pointer visszahívása nélkül.

Lezárás: nem elérhető SQL-lel nincs megtévesztő működő login szolgáltatás; elakadt kliens vagy lekérdezés nem foglal korlátlan ideig sessiont. Újracsatlakozás után új próbálkozás működik.

## További megvalósítandó feladatok

### R5 – Login-kulcs és limiter további megerősítése

**Prioritás: P2. Típus: célzott technikai megerősítés.**

**Állapot: megvalósítva és ellenőrizve.** Az alábbi leírás az eredeti audit megállapítása; az aktuális megoldás és ellenőrzés a dokumentum elején szerepel. A jelszó- és kulcsélettartam-szabályok külön felvetése nem része ennek a célzott javításnak.

Források: [login_auth.cpp:304](D:/AurigaGlobal/LiveWork/AurigaGlobal/SRC/Server/LoginServer/login_auth.cpp:304), [login_limiter.cpp](D:/AurigaGlobal/LiveWork/AurigaGlobal/SRC/Server/LoginServer/login_limiter.cpp), [peer_auth.cpp:43](D:/AurigaGlobal/LiveWork/AurigaGlobal/SRC/Server/Core/peer_auth.cpp:43).

A login-kulcs még `std::mt19937` generátorból származik, miközben a szerver más hitelesítési véletlenjeihez már OpenSSL-t használ. A limiter még fali órát (`std::time`) használ, és nincs rendszeres takarítása a régi host/account bejegyzéseknek. A kapcsolati kivétel és a párhuzamos sessionkorlát már javítva van.

Teendő: a kulcsgenerálás egységesítése a projekt kriptográfiai véletlenforrásával, hibakezeléssel; limiter-időablakok monoton időre vitele; inaktív bejegyzések korlátos takarítása. A jelszó- és kulcsélettartam-szabályok áttekintése külön biztonsági munkacsomag lehet.

Lezárás: óraváltás nem befolyásolja a tiltási időt; sok egyedi IP/login után sem marad korlátlanul növekvő inaktív állapot; véletlenforrás-hibánál nincs kiadott kulcs.

### R6 – Az ECS hálózati állapotszinkron véglegesítése

**Prioritás: P2. Típus: befejezetlen architekturális átállás.**

**Állapot: megvalósítva és ellenőrizve.** Az alábbi leírás az eredeti audit megállapítása; a végleges megoldás a dokumentum elején és a [publikálási szerződésben](D:/AurigaGlobal/LiveWork/AurigaGlobal/docs/server_network_publication.md) szerepel. Az élő kétklienses ellenőrzés az R12 része.

Aktuális források: [main.cpp:745](D:/AurigaGlobal/LiveWork/AurigaGlobal/SRC/Server/GameServer/core/main.cpp:745), [NetworkSyncSystem.cpp:146](D:/AurigaGlobal/LiveWork/AurigaGlobal/SRC/Server/GameServer/ecs/systems/NetworkSyncSystem.cpp:146).

A központi `NetworkSyncSystem_Update` hívás ki van kommentelve. A meglévő ciklus minden dirty játékosnak nulla időtartamú WAIT mozgást és csak néhány vitális pontot kitöltő points csomagot küldene. Emiatt a puszta visszakapcsolás nem megfelelő lezárás. A célzott entity-native csomagküldő függvények közben már működnek.

Teendő: eldönteni és dokumentálni a végleges csomagküldési utat; a központi ciklust teljes mezőparitással befejezni vagy a leváltott részt kivezetni. Az állapotváltozásokhoz pontos dirty- és publikálási szerződés kell.

Lezárás: felszerelés, affect, HP/SP, skillek és mozgás helyesen jelennek meg saját és másik kliensen; nincs duplán küldött vagy nullázó frissítés.

### R7 – A 64 bites időátállás maradékának lezárása

**Prioritás: P2. Típus: igazolt típuskülönbségek, részben hosszú távú munka.**

**Állapot: megvalósítva és ellenőrizve normál/ASAN konfigurációban.** Az alábbi szöveg az eredeti audit megállapítása. A végleges adat- és protokollszerződés a [server_time64_contract.md](D:/AurigaGlobal/LiveWork/AurigaGlobal/docs/server_time64_contract.md) fájlban szerepel.

Források: [tables.h:438](D:/AurigaGlobal/LiveWork/AurigaGlobal/SRC/Server/common/tables.h:438), [tables.h:1258](D:/AurigaGlobal/LiveWork/AurigaGlobal/SRC/Server/common/tables.h:1258), [ClientManagerEventFlag.cpp](D:/AurigaGlobal/LiveWork/AurigaGlobal/SRC/Server/Database/ClientManagerEventFlag.cpp).

A sima quest flag már 64 bites, de az event flag csomag/map/API még 32 bites. A ló health-drop ideje C++ oldalon `uint32_t`, miközben az SQL-oszlop már BIGINT. A házasság egyes mezői `time_t` típusúak; ezeknél az explicit, platformfüggetlen wire-típus egységesítése maradt. A régi `TMarriage` objektum migrációja viszont már nem nyitott feladat.

Teendő: az event flag teljes láncának és a ló idejének egységesítése, a platformfüggő wire-mezők ellenőrzése. A Database/GameServer/LoginServer együtt telepíthetőségét és a klienshatárt külön kell kezelni.

Lezárás: 2038/2106 utáni releváns időértékek mentése és visszatöltése nem csonkul; a protokollméretek ellenőrzöttek.

### R8 – Questadatok JSON-átállásának utolsó fázisa

**Prioritás: P2. Típus: dokumentált és kódból ellenőrzött nyitott migráció.**

**Állapot: megvalósítva és ellenőrizve; normál és ASAN konfigurációban is 49/49 teszt sikeres.** Az alábbi szöveg az eredeti audit megállapítása. A séma, konverter és betöltési/reload szerződés a [server_quest_json_contract.md](D:/AurigaGlobal/LiveWork/AurigaGlobal/docs/server_quest_json_contract.md) fájlban szerepel.

Forrás: [questmanager.cpp:131](D:/AurigaGlobal/LiveWork/AurigaGlobal/SRC/Server/GameServer/quest/questmanager.cpp:131), valamint a 271. és 307. sor körüli kategóriabetöltés.

A `questnpc.txt` és `questcategory.txt` betöltése még szöveges; a JSON-audit Phase 4 része ténylegesen hátravan. A Lua/quest programok nem tartoznak ebbe az adatkonverzióba.

Teendő: konverter, egyértelmű séma, típusos olvasó, hibajelzések és rekord-/viselkedésparitás. A Database proto-TXT importjának jövője külön adatpipeline-döntés, a Unity kliens exportjával együtt.

Lezárás: ugyanazok az NPC-quest kapcsolatok és kategóriák tölthetők be, a hibás/dupla bejegyzések determinisztikus diagnosztikát kapnak.

### R9 – Offline shop automatikus ajánlatértesítés

**Prioritás: P2. Típus: megmaradt funkcionális hiány.**

Forrás: [new_offlineshop.cpp:570](D:/AurigaGlobal/LiveWork/AurigaGlobal/SRC/Server/GameServer/social/new_offlineshop.cpp:570).

A `NotifyOffers`/`NotifyAcceptedOffers` ág a kliensértesítő hívást kommentben hagyja, a `__SendOfferNotify` üres. Közben az ajánlatot értesítettként jelöli és elküldi ezt a DB-nek. Az ajánlatlista/boltpanel ettől külön működhet; a hiány a külön értesítési útvonal. Az offline shop be van kapcsolva a közös definíciókban.

Teendő: a Unity klienssel egyeztetett értesítő csomag/üzenet, a megtekintett/értesített állapot pontos jelentése és megfelelő mentési sorrend.

Lezárás: új és elfogadott ajánlat értesítése megérkezik, újrabelépéskor sem vész el vagy ismétlődik hibásan.

### R10 – ItemAward blend tárgyak teljes validálása

**Prioritás: P2. Típus: befejezetlen, aktívan hívott validáció.**

Forrás: [ItemAwardManager.cpp:425](D:/AurigaGlobal/LiveWork/AurigaGlobal/SRC/Server/Database/ItemAwardManager.cpp:425); hívás: [ClientManager.cpp:1017](D:/AurigaGlobal/LiveWork/AurigaGlobal/SRC/Server/Database/ClientManager.cpp:1017).

A függvény csak a nulla socketértékeket naplózza. A megjelölt hiány – engedélyezett apply típus, érték és időtartam egyeztetése a blend-konfigurációval – nincs implementálva. Az itemaward bővítés aktív.

Teendő: a tényleges blend-konfiguráció szerinti ellenőrzés; hibás jutalom elutasítása vagy egyértelműen meghatározott javítása; a kiadási folyamat ezt vegye figyelembe.

Lezárás: érvénytelen vagy konfiguráción kívüli bónusz/időtartam nem kerül kiosztásra, a szabályos jutalom változatlanul működik.

### R11 – LostCastle végleges bekötése, ha kiadandó funkció

**Prioritás: funkciófüggő. Típus: befejezetlen integráció.**

Források: [LostCastleDungeon.cpp:213](D:/AurigaGlobal/LiveWork/AurigaGlobal/SRC/Server/GameServer/dungeon/LostCastleDungeon.cpp:213), [CombatSystem.cpp:3711](D:/AurigaGlobal/LiveWork/AurigaGlobal/SRC/Server/GameServer/ecs/systems/CombatSystem.cpp:3711), [input_login.cpp:583](D:/AurigaGlobal/LiveWork/AurigaGlobal/SRC/Server/GameServer/network/input_login.cpp:583).

A dungeon implementáció létezik, de a fontos külső login/kill bekötések kommentben vannak; a negyedik szint blokkoló téglalapjaihoz valós mapadat-egyeztetést kér a kód. Nem tekinthető bekapcsolt, végig integrált játékelemnek.

Teendő: kiadási döntés után NPC-, kill-, itemátadás-, clone-, login/logout- és timerútvonalak bekötése; a mapkoordináták ellenőrzése. Nem tervezett funkció esetén a státuszát és az inaktív kód kezelését kell lezárni.

Lezárás: a dungeon teljesen végigjátszható újrabelépéssel, sikertelen próbával és kitakarítással együtt, vagy egyértelműen ki van véve a kiadási tervből.

## Kiadási és karbantartási feladatok

### R12 – Automatizált integrációs és kiadási ellenőrzés

**Prioritás: kiadás előtt szükséges. Típus: tesztelési/üzemeltetési megvalósítás.**

A 43 teszt erős helyi regressziós alap, de a tesztek leírása is külön jelzi a valódi SQL, kliens és egyes szolgáltatások hiányát. A repositoryban nem találtam verziózott GitHub/GitLab/Jenkins CI-konfigurációt vagy CMake presetet. A mostani commit ellenőrzése Windows/MSVC alatt történt.

Az eredeti audit óta a helyi regresszió 49 tesztre bővült; az izolált karakter-INSERT, SQL-újracsatlakozás és az R7 64 bites adatmentési útvonalainak valódi SQL-ellenőrzése, valamint az R8 JSON-konverter/olvasó paritástesztje is elkészült. A teljes R12 továbbra is nyitott, az alábbi függőségek szerint.

Elkészült a forrás alapján összeállított MariaDB friss telepítési séma és az ismételhető izolált tesztindító: 102 tábla, 365 előkészített/végrehajtott SQL-sablon, 168 szerkezeti/adat-visszaolvasási ellenőrzés. A natív valódi SQL-es tesztek 136, illetve 109 ellenőrzést teljesítenek normál és ASAN buildben. Telepítés és korlátok: [db/README.md](../db/README.md). A séma nem helyettesíti a hiányzó játékadatokat, a meglévő adatbázis migrációját vagy a teljes szerverindítást; ezek még ellenőrizendők.

Kliens nélkül megvalósítható feladatok:

- Mentési hibainjektálás: megszakadó SQL/DB-link, újraindítás, item/exchange/offline shop/safebox/daily reward tartósság és ismételt kérések.
- Tiszta buildből normál/ASAN tesztek automatizálása; FreeBSD/Linux fordítás és célplatformos regresszió.
- Az élő séma és a verziózott SQL-migrációk gépi egyeztetése; a változó belső packet-layoutokhoz verzió-/kompatibilitási ellenőrzés és összehangolt telepítési lépés.
- A korábbi skillpont-hiba miatt érintett karakterek felmérése, majd csak meghatározott szabályokkal adatkorrekció.

Unity kliens csatlakozási átállására váró ellenőrzések:

- Elkülönített valódi adatbázissal, az átállított Unity klienssel login → karakterlétrehozás/választás → játék → logout/relog integrációs folyamat.
- Két csatlakozott klienssel mozgás közbeni felszerelésváltás, affectek hozzáadása/lejárata, skillpontok és skill-color, HP/SP és target/party UI, látótérbe belépés/kilépés és mentett állapot visszatöltése.

Ezeket a klienses eseteket az átállás elkészülte után kell végrehajtani; addig nem tekinthetők sikeresen teszteltnek. A szerveroldali automatizálásuk és a protokolltesztek előkészítése ettől függetlenül haladhat. Az élő adatbázis és a célplatform ellenőrzéséhez a megfelelő környezet is szükséges.

Lezárás: a kiadás ellenőrizhetően reprodukálható; migrációhiány vagy hibás komponensverzió felismerhető; az újraindítási és mentési esetek ténylegesen teszteltek. Az élő állapot és korábbi túlköltések ténye jelenleg nincs megállapítva.

### R13 – Aktuális dokumentáció és a maradék átmeneti szerkezetek rendezése

**Prioritás: P3. Típus: karbantartás, részben választható refaktor.**

Források: [char_interface.hpp](D:/AurigaGlobal/LiveWork/AurigaGlobal/SRC/Server/GameServer/entity/char_interface.hpp), [vital_components.hpp:19](D:/AurigaGlobal/LiveWork/AurigaGlobal/SRC/Server/GameServer/ecs/components/vital_components.hpp:19), [item_proto_components.hpp](D:/AurigaGlobal/LiveWork/AurigaGlobal/SRC/Server/GameServer/ecs/components/item_proto_components.hpp).

Teendő: az elavult migrációs TODO-k és auditok megjelölése; a `char_interface.hpp` teljes `char.h` beemelésének rendezése; az entity-native `ItemSystem_LegacyBridge.cpp` felelősségek szerinti szétválasztásának mérlegelése. A proto-pointerek és offline-shop pointerkapcsolatok további kiváltása/ellenőrzése választható megerősítés, nem ebben a vizsgálatban igazolt új memóriabiztonsági hiba.

Fontos pontosítás: a `CHARACTER_POINT` a régi inventory-audit állításával ellentétben jelenleg **használt** típus, az `ecs::CharacterPoints::base` mező része. Törlését előbb az aktív felhasználás szétbontásával kell előkészíteni.

Lezárás: a teendőlista a tényleges kódállapotot követi; megszűnnek a félrevezető átmeneti címkék; a választott szerkezeti változtatások megtartják a regressziós szerződéseket.

## Ami már elkészült, ezért nem kerül vissza a listára

- Az október 17-i audit F1–F8 pontjai: javítva és commitolva `129a28d7` alatt.
- Switchbot entitásalapú állapota és PID-indexe.
- A házaspár entitásalapú `CoupleState` tárolása és a régi `TMarriage` objektum kiváltása.
- Affectek értékként tárolása; a régi `shared_ptr<CAffect>` lista már nem az aktuális megoldás.
- A régi `CHARACTER`, `CItem`, `CParty` objektummodell kiváltása a játékállapotban.
- A normál quest flag, valamint az R7-ben az event flag teljes láncának 64 bitesítése.
- Kliens-TLS, a régi packet cipher kivezetése, valamint a teljes szerverteszt-regisztráció.
- A `PrivManager` „TODO send packet” megjegyzései melletti küldések már megvannak; a cube betöltő a „TODO check cube data” mellett ténylegesen meghívja az ellenőrzőt. Ezeket nem soroltam hiányzó implementációként.

Az **R1–R8 szerveroldali megvalósítása és normál/ASAN ellenőrzése elkészült**. Következőként folytatható az **R9**, illetve az **R12 kliens nélkül automatizálható része**. Az élő klienspróbák a Unity kliens csatlakozási átállására várnak. Az R11 kiadási szándékhoz kötött, az R13 nem előzi meg a működési javításokat.
