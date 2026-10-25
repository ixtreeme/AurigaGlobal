# AurigaGlobal Mission Engine – állapot és folytatási terv

- **Dátum:** 2026-10-25
- **Szerverág:** `codex/mission-engine`
- **Aktuális kódcommit:** `71abb5d7`; előző lezárt állapot: `6475257a`.

**Hatókör:** a régi BattlePass-t kiváltó történetalapú Mission Engine, a hozzá
tartozó csomagformátum és a későbbi Mission Studio/Unity-integráció.

## 1. Jelenlegi állapot

Elkészült és tesztelt az önálló natív küldetésmotor magja, a verziózott
`.aurmission` csomag fordítója/betöltője, az atomi katalógusaktiválás és a
megbízható adatbázismentéshez szükséges prepared tranzakciós infrastruktúra.
Elkészült a saját additív Mission-séma, a tartós checkpoint codec/store és
a pontos régi csomagok/definíciók archívuma is, owner-fencinggel és mentési nyugtákkal.
Elkészült a checkpointtal közösen mentett action outbox és az egyszer rögzíthető,
szimbolikus jutalomjogosultság-ledger is; ez még nem tényleges jutalomkézbesítés.
Az archivált, típusos jutalomcsomag és a pontos reward-payloadhoz kötött entitlement
API is elkészült. A jutalom tartalmának megváltoztatása új mission-verziót követel.
Elkészült a közös item_award → item kézbesítő javítása is: a konkrét tárgy és
az átvételi nyugta együtt commitol, valódi tárgysémás crash/retry próbával.
A Mission ledger és az award létrehozásának összekötése még hátravan.

A teljes, játékosok által használható új küldetésrendszer még nincs bekötve.
A session-/core-életciklus persistence-bekötése, az egyszeri jutalmazás,
a gameplay-események, az NPC-dialógus, az új Unity-felület és a
Mission Studio a következő munkafázisok. A tesztelt rétegek elkészülte nem
jelent kész kampányt vagy éles telepítési engedélyt.

## 2. Rögzített döntések

- Az új motor a **GameServer része**, natív **C++23/EnTT** iránnyal.
  A domain mag önállóan tesztelhető; az élő EnTT/gameplay bekötés még következik.
- Az új rendszerben **nincs Lua vagy AngelScript**. A meglévő Lua 5.5.1 a
  még nem migrált questek és más függőségek miatt megmarad.
- **Csak a régi BattlePass váltódik ki.** A kolléga szezonális Mission Pass
  rendszere külön szolgáltatásként megmarad.
- A JSON szerkesztési forrás; a szerver telepítési egysége **egy verziózott
  `.aurmission` fájl**. Küldetésenként nem generálunk C++-t.
- A későbbi Mission Studio önálló vizuális alkalmazás, a közös típusséma és
  fordító fölött. A Unity a megengedett megjelenítési adatokat és állapotot kapja.
- A `Race` az **első sikerre** zárul. Az első terminális eredményt választó
  viselkedés külön `FirstTerminal` szabály.
- A kapuknál mindkét módszer választható: `ANY_OF` egy teljesített útvonalhoz,
  `AT_LEAST_N(2)` két független előkészülethez. Az M06 végleges
  tartalmi beállítását a kampány összeállításakor rögzítjük.
- A tartós mentés és az idempotens jutalmazási szerződés megelőzi az új
  motor első élő gameplay-hatását.

## 3. Mi készült el?

### 3.1. Audit és a régi BattlePass kivezetése

- Elkészült a szerver-, Unity-, quest-, adatbázis- és API-függőségi audit.
- Kikerült a régi BattlePass szerverkódja és kizárólagos Unity-felülete,
  protokollja, assetjei, valamint a régi launcher kapcsolódó exportjai.
- A szezonális Mission Pass megmaradt. A régi BattlePass-adatbázisadatok
  törlése vagy éles migrációja nem része ennek az elkészült kivezetésnek.
- Az új node-követelményekről **208 neves katalógusbejegyzés**, **712 legacy
  Lua API**, valamint **69 régi questforrás** migrációs leltára készült.
- Megvan a **80 egyedi kampányazonosítót** tartalmazó tervezési manifest:
  10 fő-, 64 mellék- és 6 titkos küldetés, öt felvonásban 16–16 bejegyzéssel.

A leltár és a manifest tervezési anyag. A 80 küldetés még nem játszható
kampány; a 69 régi quest egyike sincs kész natív migrációként elfogadva.
A Lua API-szám feltételes regisztrációkat is tartalmaz, nem egy konkrét
bináris ténylegesen megnyitott exportjainak száma.

### 3.2. Natív Mission Engine mag

- Szigorúan validált, verziózott, futás közben nem módosuló definíciók.
- Objective, condition, branch, sequence, parallel, timer, reward,
  inline sub-mission, success/failure és recovery működés.
- Típusos feltételek, köztük `ALL_OF`, `ANY_OF`, `NOT`, `XOR`,
  `AT_LEAST_N`, `EXACTLY_N`, történeti flag-ek és rögzített választások.
- Karakterhez és sessionhöz kötött, egy tulajdonosszálon futó runtime.
- Esemény-, tény- és határidőindexelés; nincs minden tickben teljes quest-scan.
- Számlálók, különböző célok követése, időzítők és determinisztikus ágsorrend.
- Streamenként 256 sorszámos esemény-deduplikációs ablak.
- Hibás vagy túl nagy munkát végző művelet teljes memóriabeli visszagörgetése.
- Típusos memória-checkpoint és szigorú, pontos definícióverziót igénylő restore.
- Stabil actionkulcs és nyugtázásra váró reward node.

**Fontos határ:** a `Save()` jelenleg memória-checkpointot ad. A reward node
jelenleg kérést készít és nyugtára vár; tárgyat, pénzt vagy XP-t még nem ad.
A jelenlegi végrehajtási modell strukturált fa, nem tetszőleges adatfolyamgráf.

### 3.3. `.aurmission` csomag és fejlesztői eszközök

- Determinisztikus, típusos bináris formátum, verziózott fejléccel és szekciókkal.
- SHA-256 integritásellenőrzés, szigorú méret-/mélység-/típus-/UTF-8-validálás.
- Manifest, gráfok, natív követelmények, megjelenítési és lokalizációs adatok.
- `MissionCompiler`: fordítás, csomagellenőrzés és natív sémaexport.
- `MissionCheck`: önálló definícióellenőrző.
- Stage → validate → activate, teljes katalógusra atomi aktiválás és rollback.
- A futó példányok a saját régi definícióverziójukon maradhatnak.
- Szűrt presentation-adatok előállítása a későbbi kliensintegrációhoz.
- 32 kernel-típus: 11 flow, 12 predikátum és 9 eseménytípus; mellettük 4 új reward-payload séma.

**Fontos határ:** a 36 exportált séma nem 36 kész gameplay-szolgáltatás. A teljes
208-as vizuális katalógus még nincs megvalósítva. A csomaghash nem digitális
aláírás. A csomag-/definíciótörténethez már van tartós archívum és a pontos
régi verziókat az aktív katalógus módosítása nélkül importáló `Retain` API.
A production aktiválás a szükséges
natív bindingok hiányában elutasítja a bekötetlen tartalmat.

### 3.4. SQL-végrehajtási alap – 4/A

- Egy queue-munkaként végrehajtott, tulajdonolt prepared tranzakciók.
- A lépések közé nem kerül más SQL-munka; tranzakción belül nincs automatikus
  reconnect vagy újrajátszás.
- Elvárt módosított sorszám ellenőrzése a későbbi revision/CAS mentéshez.
- Commit, visszagörgetés, konfliktus és bizonytalan commit külön eredményállapot.
- Elveszett COMMIT-válasznál `CommitUnknown`; kötelező későbbi egyeztetés
  tartós műveletazonosítóval, automatikus ismétlés helyett.
- Idegen, nyitva hagyott tranzakciót a kezelő nem vesz át.
- A prepared hívások minden eredményhalmazát kiolvassuk, a késői hibákat is.
- A paraméterszámhiba most valódi hibakódot ad; a prepare hibája megmarad.
- Bináris eredményhosszak kezelése beágyazott NUL-bájtokkal és nagy cellákkal.
- Elkülönített MariaDB-tesztkörnyezet és COMMIT-választ eldobó tesztproxy.

**Fontos határ:** a saját `mission_sql_*` teszttáblák továbbra is a generic
SQL-alap próbái. A tényleges Mission store külön `mission_engine_*` sémát
használ az alábbiak szerint. A mentési receipt nem jutalomledger.

### 3.5. Tartós Mission store és csomagarchívum – 4/B

- Hat saját InnoDB-tábla; régi táblák és játékosadatok módosítása nélkül.
- Determinisztikus, hash-elt, korlátos checkpoint, pontos gráftartalom-lenyomatokkal.
- Teljes progress, flag/choice, eseménydedup, időzítő és pending reward-kulcs visszatöltése.
- Eredeti `.aurmission` csomagok és immutable mission ID/verzió/tartalom archiválása.
- Claim/save/release prepared tervek, növekvő owner-epoch és 128 bites token.
- Külön storage- és runtime-revízió, CAS, tartós műveletazonosító és kéréslenyomat.
- Bizonytalan commit egyeztetése az eredeti írót megváró, aktuális zároló olvasással.
- Valódi adatbázis-crash/restart után új C++ folyamatban ellenőrzött cold restore.

**Fontos határ:** a persistence API kész, az éles session-/core-koordináció
még nem hívja. Nincs automatikus lease/failover. A réteg nem kapcsolja be magát játékosokra.

### 3.6. Action outbox és jutalomjogosultság – 4/C

- Sémaverzió 2, összesen nyolc saját InnoDB-tábla és hét FK.
- A checkpointtal közösen mentett outbox, stabil teljes actionkulccsal és gráflenyomattal.
- Tartós megszakítás; cancelled action ID nem aktiválható újra.
- Owner/revízió/checkpoint-hash ellenőrzése mellett egyszer rögzített entitlement.
- Ledger nélküli reward-nyugtázás elutasítása a teljes mentés visszagörgetésével.
- Külön mentési és entitlement COMMIT-vesztés, verseny, félbeírás és cold restart próbák.
- Teljes 128-as, bound paraméteres SQL-köteg és kapacitástúllépés elutasítása.

**Fontos határ:** a ledger szimbolikus jogosultság, nem inventory/XP/pénz
hatásnyugta. A verziózott reward-payload a következő, 4/D–1 részben elkészült.
A közös item_award átvételi rést a 4/D–2 javítja; az új motor összekötése a
kézbesítővel és a hiteles session-/core-életciklus továbbra is hátravan.

### 3.7. Verziózott jutalompayload és binding-szerződés – 4/D–1

- Definíció-schema 2: név szerinti item/XP/pénz/consume csomagok, teljes típus- és korlátellenőrzéssel.
- A `.aurmission` konténer 1-es marad, a typed reward-gráfok sémája 2; az eredeti 1-es csomag bytepontos kompatibilitása tesztelt.
- A jutalom tartalma része a gráflenyomatnak és az archivált, változtathatatlan verziónak.
- `PinnedReward`: exact mission/verzió/node/reward/gráfhash alapján saját payload-érték.
- Az entitlement API ezt követeli és az SQL-ben az outbox gráflenyomatát is összeveti vele.
- A Gameplay kapu konkrét payloadot, igazolt atomi szerződést, natív effect-képességeket,
  prototípusonként külön grant/consume korlátot és XP/pénz határokat követel.

**Fontos határ:** ez még adatleírás és ellenőrzött tartalomkötés. A Mission
jutalomcsomag teljes végrehajtása, itemfogyasztás, XP-/pénzváltozás, hatásnyugta
és élő inventory-egyeztetés nincs megvalósítva. A tesztbeli trusted binding
nem élő kézbesítő-regisztráció.

### 3.8. Kanonikus item_award → item kézbesítő – 4/D–2

- A tényleges tárgy INSERT és az award taken_time/item_id UPDATE közös tranzakció.
- A meglévő Database item-ID tartományából egyszer lefoglalt ID kerül mindenhová;
  nincs hibás függés a nem AUTO_INCREMENT tábla nulla insert_id értékétől.
- Az eredeti award-tartalom és a raktár teljes elhelyezési snapshotjának összevetése.
- Account-/award-zárolás, teljes footprint-vizsgálat; telt raktárnál az award megmarad.
- Egyeztetés bizonytalan COMMIT-nál, újra-INSERT nélkül, majd friss raktárolvasás.
- Valódi item/item_award/safebox sémán race, rollback, disconnect, lost COMMIT és
  kényszerített MariaDB restart után új C++ folyamatban ellenőrzött tárgy/nyugta.

**Fontos határ:** egy award kézbesítése készült el a meglévő Database útvonalban.
A Mission ledger → award létrehozása még nincs bekötve. A teljes item/XP/pénz/cost
bundle és az ECS/session protokoll nem következik automatikusan ebből. A meglévő
közvetlen Database ág blokkoló; az új Mission dispatchernek aszinkron működés kell.

## 4. Ellenőrzött eredmények

| Ellenőrzés | Legutóbbi sikeres eredmény |
|---|---|
| Windows x64 / RelWithDebInfo fordítás | GameServer és Database normál/ASAN; LoginServer a 4/B-ben |
| Natív Mission core | 8723 ellenőrzés |
| Csomagkezelés | 4856 ellenőrzés |
| Checkpoint codec és action identity | 3227 ellenőrzés |
| Verziózott reward-payload suite | 2109 ellenőrzés, normál és ASAN alatt |
| SQL-egységtesztek | 2232 ellenőrzés |
| Valódi SQL-infrastruktúra | 204 ellenőrzés |
| Valódi Mission store/action és új folyamatban cold restore | Normál: 577 + 42; ASAN: 575 + 42 a rögzített körökben |
| Kanonikus item-award terv-/határteszt | 41 ellenőrzés, normál és ASAN |
| Valódi item-award delivery/cold-process | Normál és ASAN: 302 + 111 ellenőrzés |
| Célzott regressziós CTest-kör | 30/30 sikeres |
| Külön AddressSanitizer CTest-kör | 8/8 sikeres |
| Valódi MariaDB-suite-ok ASAN alatt | Az új item-award suite is sikeres, crash/restarttal |

A SQL-polling miatt a diagnosztikai ellenőrzések száma futásonként eltérhet.
A próbák többek között lefedik a hibás csomagokat, verzióütközéseket,
rollbacket, eseményduplázást, két valódi SQL-kapcsolat revisionversenyét,
commit előtti kapcsolatmegszakítást és egy már commitolt tranzakció elveszett
válaszát, az outbox/ledger versenyét és a ledger nélküli nyugtázás tiltását.
Az új próbák elkülönített helyi adatbázison futottak. A 30-as regressziós és a
8-as ASAN kör a 4/D–2 production kódon futott. A hibainjektáló teszt
PROCESSLIST-szinkronizációjának javítása után az új suite normál/ASAN unit és
valódi DB próbái külön ismét sikeresek. A 4/D–1 store-próba az aktív új gráf
mellett a régi konkrét jutalomtartalmat is ellenőrzi.

Ez a célzott regressziós kör eredménye; nem állítja az összes létező
szerverteszt vagy az összes játékbeli quest elfogadását. Ebben a Mission
részfázisban nem történt FreeBSD build, Unity end-to-end elfogadás vagy
az új Mission Engine éles/távoli telepítése.

A jelen fázis hash-ellenőrzése szerint a szezonális Mission Pass 34 védett
fájlja és a nyolc korábbi Unity-módosítás változatlan maradt.

## 5. Commitok és rögzített Unity-munka

| Commit | Elkészült munkafolyamat |
|---|---|
| `e9d1ca90` | Mission Engine audit és migrációs terv |
| `c6e80183` | Régi BattlePass kivezetése, szezonális Mission Pass megtartása |
| `9ab0b12c` | Régi launcher BattlePass-exportjainak kivezetése |
| `18b77435` | Windows állapotfájl-olvasási teszt stabilizálása |
| `7b0c87a5` | Natív gráfmag és verziózott katalógus |
| `89bf2d43` | `.aurmission` compiler/loader és atomi aktiválás |
| `92e43a98` | Node-leltár, kampánymanifest és dokumentáció |
| `17a8314a` | Prepared tranzakciós alap és valódi MariaDB hibapróbák |
| `dc705479` | Az első állapotösszesítő és folytatási terv |
| `b45ed489` | Tartós checkpoint, csomagarchívum és owner-fencing, valódi crash/cold-process próbákkal |
| `9d33889f` | Bináris SQL-eredmények méretkorlátja már a kliensoldali pufferelés előtt |
| `de45f7cd` | A 4/B tartós mentési réteg lezárt állapotösszesítése |
| `5a8b13b5` | Tartós action outbox, fenced szimbolikus jutalomjogosultság és crash/retry/race próbák |
| `8f4b4bbb` | A 4/C lezárt állapotösszesítése és kézbesítési előfeltételek |
| `bd3a2fd1` | Verziózott item/XP/pénz/consume payload, exact jutalompin, binding-kapu és régi bináris kompatibilitása |
| `6475257a` | A 4/D–1 állapot és próbák összesítése |
| `71abb5d7` | Atomi kanonikus item-award kézbesítő, valódi sémás crash/retry/telt raktár próbákkal |

A korábbi Unity BattlePass-kivezetés rögzített változata: **Unity Version
Control changeset 30**, `/main/codex-mission-engine` ág. Ez a régi felület
kivezetése, nem az új Mission kliens elkészülte. A jelen összesítéshez nem
történt új Unity-módosítás vagy checkin.

A 4/B munkafázis elkészült megvalósítását és próbáit a
`mission_persistence_store_2026-10-25.md` dokumentum részletezi.
A 4/C outbox/entitlement réteg részletei és kézbesítési határai:
`mission_actions_and_entitlements_2026-10-25.md`.

## 6. Folytatás – javasolt, függőségek szerinti sorrend

### 1. Mission-séma, checkpoint codec és csomagarchívum

**A 4/B részfázisban elkészült**, az alábbi elfogadással együtt.

- Külön, additív MariaDB-migráció; meglévő BattlePass-, Mission Pass- és
  játékosadatok törlése nélkül.
- Verziózott, méretkorlátos checkpoint-szerializálás és visszaolvasás.
- `.aurmission` csomagok, hash-ek és pontos definícióverziók tartós archiválása.
- Előkészített statementekre épülő Mission persistence réteg a kész
  tranzakciókezelő fölött.

**Elfogadás:** folyamatleállás után friss runtime ugyanazokat a számlálókat,
flag-eket, választásokat, dedupablakokat és pontos definícióverziókat tölti
vissza; hiányzó vagy sérült verziót nem helyettesít csendben a legújabbal.

### 2. Karaktertulajdonlás, revision és mentési egyeztetés

**A DB-primitívek elkészültek:** owner-epoch/token, revision/CAS és tartós
operation receipt/egyeztetés. A szerver session-/core-életciklusának hiteles
koordinátora és az aszinkron callbackek bekötése még hátravan.

- [x] Tartós DB-owner-fencing: egy korábbi owner késői írásának elutasítása.
- [ ] Hiteles session-/core-koordináció: egy régi core vagy kijelentkezett session
  késői válasza ne írhassa felül az új tulajdonos állapotát.
- [x] Progress, dedup és stabil mentési műveletazonosító közös tranzakciója.
- [x] Revision/CAS alapú mentés és `CommitUnknown` egyeztetése friss kapcsolaton.
- [ ] Aszinkron válasznál karakter/session/generáció újraellenőrzése az ECS elérése előtt.

**Elfogadás:** két core versenyében egy állapot nyer; reconnect, régi callback
vagy elveszett válasz nem okoz állapotvesztést vagy kétszeres eseménybeszámítást.

### 3. Action outbox és egyszeri jutalmazás

**Az outbox és a jogosultság a 4/C-ben, a verziózott payload a 4/D–1-ben,
a közös item-award kézbesítő a 4/D–2-ben elkészült.** Következő a Mission
ledger és az átvehető award közös, idempotens létrehozása; az award-cache
commit-sorrenddel szembeni helyességét is ellenőrizni kell.
Gameplay-hatás csak az igazolt jutalmazás és session-/core-koordináció után kapcsolható be.

- [x] Tartós action outbox a progress mentésével közös tranzakcióban és külön entitlement-ledger.
- [x] A mentett action, karakter, definícióverzió, owner, revision és checkpoint-hash ellenőrzése.
- [x] Verziózott és archivált natív reward-payload és ellenőrzött tartalomkötés.
- [x] Egy meglévő award tényleges item + taken nyugtája közös tranzakcióban,
  normál/ASAN valódi tárgysémás crash/retry/telt raktár próbákkal.
- [ ] Mission ledger → award közös tranzakció és stabil action/payload → award mapping.
- [ ] Az `item_award`/inventory/ECS réteggel összehangolt,
  idempotens kézbesítés; az item, XP, pénz és fogyasztás külön hatásszerződése.
- [ ] Restart/retry során ugyanaz a művelet ne adhasson vagy vehessen el újra tárgyat.

**Elfogadás:** tele inventory, újraküldött kérés, hálózati timeout és a
kézbesítés közbeni crash esetén sincs jutalomduplázás vagy elvesző jogosultság.
Az SQL-tranzakción kívüli ECS-hatásokhoz külön egyeztetési mechanizmus kell.

### 4. A valódi Mission store hibapróbái

A checkpoint-/archívum-/fencing-réteg, az outbox/entitlement és az egyedi
item-award → item réteg crash-, race-, reconnect- és hideg újraindítási próbái
sikeresek. A teljes Mission → award → item út hibapróbái még hátravannak.

- Hideg újraindítás, régi csomagverzió, hot reload és rollback együtt.
- Crash a mentés előtt/után és a jutalomkézbesítés különböző pontjain.
- Két core, ismételt esemény, kijelentkezés, reconnect és késői SQL-válasz.
- Teljes checkpoint–outbox–ledger kapcsolat ellenőrzése.

**Elfogadás:** a próbák a tényleges Mission persistence/jutalom kódot futtatják,
nem csak az SQL-infrastruktúra demonstrációs tábláit. Ez a gameplay bekötés kapuja.

### 5. Natív gameplay adapterek és az első teljes küldetés

- Szerveroldali kill, pickup, NPC-interakció, régió és választás események.
- Valódi mob-/item-/NPC-/map-azonosítók és tulajdonlási/credit szabályok.
- Natív szint-, tárgy-, történeti és mission-feltételek, ellenőrzött actionök.
- Első végigjárható szelet: **NPC → ölés/gyűjtés → választás → jutalom → lezárás**.

**Elfogadás:** a játékban végrehajtott lépésekből halad a küldetés, session- és
jogosultságellenőrzéssel, már a tartós store és jutalomledger használatával.

### 6. Natív dialógus és Unity Mission kliens

- Szerveroldali dialógussession, NPC-távolság és választási opciók validálása.
- Verziózott snapshot/delta protokoll és szűrt presentation-cache.
- Unity napló, tracker, NPC-dialógus, jelölők és jutalom-előnézet.
- Reconnect és a megjelenítési adatok verzióváltásának kezelése.

**Elfogadás:** az első küldetésszelet Unityből végigjátszható. A kliens nem
kap titkos feltételeket, és nem küldhet hiteles progress- vagy jutalomeseményt.

### 7. Mission Studio, további node-ok és tartalommigráció

- Önálló vizuális editor a közös típus-/portséma és compiler fölött.
- Szerkesztés, validálás, szimuláció, diff/debug és jogosított publish.
- A teljes katalógushoz szükséges EntityRef/scope, változó-, loop/retry-,
  signal-, party-, dungeon-, boss- és egyéb képességek fokozatos bővítése.
- A 80-as kampány tényleges tartalomkötése és végigjátszhatósági vizsgálata.
- A 69 régi quest és mentéseik fokozatos, igazolt átvezetése.

**Elfogadás:** csak valóban implementált és tesztelt natív képesség jelenik
meg kész node-ként. A régi Lua-rendszer csak az utolsó tényleges függőség és
tartalom igazolt migrációja után vezethető ki. Az éles átállás külön
mentési, migrációs, visszaállítási és telepítési munkafázis.

## 7. Részletes háttérdokumentumok

- [Eredeti audit és migrációs terv](D:/AurigaGlobal/LiveWork/AurigaGlobal/docs/mission_engine_audit_2026-10-24.md)
- [Natív mag és futási szerződések](D:/AurigaGlobal/LiveWork/AurigaGlobal/docs/mission_core_2026-10-25.md)
- [Node Library átvezetés és csomagformátum](D:/AurigaGlobal/LiveWork/AurigaGlobal/docs/mission_node_library_delta_2026-10-25.md)
- [Prepared tranzakciós alap és hibapróbák](D:/AurigaGlobal/LiveWork/AurigaGlobal/docs/mission_persistence_sql_2026-10-25.md)
- [Tartós checkpoint/store, csomagarchívum és fencing](D:/AurigaGlobal/LiveWork/AurigaGlobal/docs/mission_persistence_store_2026-10-25.md)
- [Action outbox és szimbolikus jutalomjogosultság](D:/AurigaGlobal/LiveWork/AurigaGlobal/docs/mission_actions_and_entitlements_2026-10-25.md)
- [Verziózott natív jutalomcsomagok és binding-szerződés](D:/AurigaGlobal/LiveWork/AurigaGlobal/docs/mission_reward_payloads_2026-10-25.md)
- [Kanonikus item-award kézbesítő és valódi tárgysémás hibapróbák](D:/AurigaGlobal/LiveWork/AurigaGlobal/docs/mission_item_award_delivery_2026-10-25.md)
- [Gépi node-/API-leltár](D:/AurigaGlobal/LiveWork/AurigaGlobal/docs/mission_node_library_coverage_2026-10-25.json)
- [80-as kampány tervezési manifestje](D:/AurigaGlobal/LiveWork/AurigaGlobal/docs/mission_campaign_manifest_2026-10-25.json)
