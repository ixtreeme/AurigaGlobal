# AurigaGlobal Mission Engine – állapot és folytatási terv

- **Dátum:** 2026-10-25
- **Szerverág:** `codex/mission-engine`
- **Az összesítő alapjául szolgáló kódcommit:** `17a8314a`

**Hatókör:** a régi BattlePass-t kiváltó történetalapú Mission Engine, a hozzá
tartozó csomagformátum és a későbbi Mission Studio/Unity-integráció.

## 1. Jelenlegi állapot

Elkészült és tesztelt az önálló natív küldetésmotor magja, a verziózott
`.aurmission` csomag fordítója/betöltője, az atomi katalógusaktiválás és a
megbízható adatbázismentéshez szükséges prepared tranzakciós infrastruktúra.

A teljes, játékosok által használható új küldetésrendszer még nincs bekötve.
A Mission-specifikus adatbázisséma és tartós progress store, az egyszeri
jutalmazás, a gameplay-események, az NPC-dialógus, az új Unity-felület és a
Mission Studio a következő munkafázisok. A tesztelt alapok elkészülte nem
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
- 32 stabil natív típus: 11 flow, 12 predikátum és 9 eseménytípus.

**Fontos határ:** a 32 típus nem 32 kész gameplay-szolgáltatás. A teljes
208-as vizuális katalógus még nincs megvalósítva. A csomaghash nem digitális
aláírás. A csomag-/definíciótörténet jelenleg memóriában marad meg; hideg
újraindításhoz tartós archiválás kell. A production aktiválás a szükséges
natív bindingok hiányában elutasítja a bekötetlen tartalmat.

### 3.4. SQL-végrehajtási alap – legutóbb elkészült rész

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

**Fontos határ:** ez a mentés infrastruktúrája. A saját `mission_sql_*`
teszttáblák nem a végleges Mission-séma. A Mission-állapot, dedup és jutalom
még nincs ezen keresztül tartósan elmentve. Az SQL-teszt receiptje nem a kész
Mission-jutalomledger vagy inventory-kézbesítés.

## 4. Ellenőrzött eredmények

| Ellenőrzés | Legutóbbi sikeres eredmény |
|---|---|
| Windows x64 / RelWithDebInfo fordítás | GameServer, Database, LoginServer |
| Natív Mission core | 8723 ellenőrzés |
| Csomagkezelés | 4842 ellenőrzés |
| SQL-egységtesztek | 2220 ellenőrzés |
| Valódi, elkülönített MariaDB | 204 ellenőrzés |
| Célzott regressziós CTest-kör | 25/25 sikeres |
| Külön AddressSanitizer CTest-kör | 5/5 sikeres |
| SQL-egységteszt és valódi MariaDB ASAN alatt | Sikeres |

A próbák többek között lefedik a hibás csomagokat, verzióütközéseket,
rollbacket, eseményduplázást, két valódi SQL-kapcsolat revisionversenyét,
commit előtti kapcsolatmegszakítást és egy már commitolt tranzakció elveszett
válaszát. Az új próbák elkülönített helyi adatbázison futottak.

Ez a célzott regressziós kör eredménye; nem állítja az összes létező
szerverteszt vagy az összes játékbeli quest elfogadását. Ebben a Mission
részfázisban nem történt FreeBSD build, Unity end-to-end elfogadás vagy
az új Mission Engine éles/távoli telepítése.

Az előző fázis hash-ellenőrzése szerint a szezonális Mission Pass 34 védett
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

A korábbi Unity BattlePass-kivezetés rögzített változata: **Unity Version
Control changeset 30**, `/main/codex-mission-engine` ág. Ez a régi felület
kivezetése, nem az új Mission kliens elkészülte. A jelen összesítéshez nem
történt új Unity-módosítás vagy checkin.

A kód az összesítés megkezdésekor commitolva volt, a Git-munkafa tiszta.
Ez az összesítő és a kapcsolódó dokumentációs pontosítás külön commitba kerül.

## 6. Folytatás – javasolt, függőségek szerinti sorrend

### 1. Mission-séma, checkpoint codec és csomagarchívum

**Ezzel folytatom a fejlesztést.**

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

- Tartós owner/session-fencing: egy régi core vagy kijelentkezett session
  késői válasza ne írhassa felül az új tulajdonos állapotát.
- Progress, dedup és stabil mentési műveletazonosító közös tranzakciója.
- Revision/CAS alapú mentés és `CommitUnknown` egyeztetése friss kapcsolaton.
- Aszinkron válasznál karakter/session/generáció újraellenőrzése az ECS elérése előtt.

**Elfogadás:** két core versenyében egy állapot nyer; reconnect, régi callback
vagy elveszett válasz nem okoz állapotvesztést vagy kétszeres eseménybeszámítást.

### 3. Action outbox és egyszeri jutalmazás

- Tartós action outbox és jutalomledger a progress mentésével közös tranzakcióban.
- A még érvényes action, karakter, definícióverzió és revision ellenőrzése.
- Az `item_award` és a meglévő inventory/ECS szolgáltatásokkal összehangolt,
  idempotens kézbesítés; az item, XP, pénz és fogyasztás külön hatásszerződése.
- Restart/retry során ugyanaz a művelet ne adhasson vagy vehessen el újra tárgyat.

**Elfogadás:** tele inventory, újraküldött kérés, hálózati timeout és a
kézbesítés közbeni crash esetén sincs jutalomduplázás vagy elvesző jogosultság.
Az SQL-tranzakción kívüli ECS-hatásokhoz külön egyeztetési mechanizmus kell.

### 4. A valódi Mission store hibapróbái

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
- [Gépi node-/API-leltár](D:/AurigaGlobal/LiveWork/AurigaGlobal/docs/mission_node_library_coverage_2026-10-25.json)
- [80-as kampány tervezési manifestje](D:/AurigaGlobal/LiveWork/AurigaGlobal/docs/mission_campaign_manifest_2026-10-25.json)
