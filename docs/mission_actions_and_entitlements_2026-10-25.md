# Mission Engine – tartós action outbox és jutalomjogosultság

**Dátum:** 2026-10-25. **Részfázis:** 4/C. Előzménye a 4/B checkpoint/store réteg.
**Kódcommit:** `5a8b13b5`.

## Elkészült és még hiányzó képességek

Az action outbox a checkpointtal közös InnoDB-tranzakcióban mentődik. A motor
jutalomkérései stabil azonosítót kapnak; a megszakításuk és a karakterenként
egyszer rögzíthető, szimbolikus jutalomjogosultságuk tartós. A checkpoint nem
nyugtázhat olyan jutalom-node-ot, amelyhez nincs már commitolt jogosultság.

**A ledger jogosultságot igazol, nem kézbesítést.** Nem készül tárgy, nem
változik XP, pénz vagy inventory, és nem fogy erőforrás. A gráfban továbbra is
szimbolikus reward-kulcs szerepel. A tényleges hatáshoz még verziózott natív
jutalomcsomag-kötés, idempotens kézbesítési szolgáltatás és külön hatásnyugta
kell. Az új API nem kérhető a klienstől, és nem helyettesíti ezt a szolgáltatást.

A GameServer session-/core-koordinátora és a gameplay-bekötés még nincs kész.
A csomagok ellenőrzött gameplay-kötéseinek kapuja továbbra is zárt. A
szezonális Mission Pass és a Unity kliens ebben a részfázisban nem változik.

## 1. Miért nem azonnal az item_award útvonal?

A kódaudit konkrét megszakítási rést talált:

- A `Database/ClientManager.cpp` safebox/mall betöltési útja előbb külön
  `DirectQuery` hívással létrehozza az `item` sort.
- Ezután az `ItemAwardManager::Taken` külön aszinkron `UPDATE item_award`
  művelettel jelöli átvettnek a jogosultságot.
- A két írás között bekövetkező leállás után a tárgy már létezhet, miközben az
  award még át nem vettnek látszik. Az in-memory `bTaken` jelző nem tartós nyugta.

Ez kódból azonosított kockázat; az élő szerveren nem idéztünk elő jutalomduplázást.
A régi útvonalat itt nem módosítottuk, és az új Mission Engine nem hívja meg.
Egy outbox/ledger INSERT önmagában nem javítaná meg a kézbesítés ezen rését.
A következő fázisban a tényleges tárgyelőállítást és a claimet egy ellenőrzött
tranzakciós/idempotens szerződéssel kell összekötni, valódi item-sémás próbával.

## 2. Séma és telepítési sorrend

A konfigurált player adatbázist a telepítő választja ki. Sorrend:

1. `db/migrations/2026-10-25_mission_engine_persistence.sql` – a hat alap tábla.
2. `db/migrations/2026-10-25_mission_engine_actions.sql` – két új tábla, az
   operations saját CHECK-jének bővítése és végül a sémaverzió 2-re emelése.
3. Az új program a **2-es** sémaverziót, **8 InnoDB-táblát**, **7 név szerint
   elvárt idegenkulcsot** és a bekapcsolt FK-/CHECK-ellenőrzést követeli meg.

| Tábla | Tartalom |
|---|---|
| `mission_engine_action_outbox` | Action ID, teljes kulcs, pontos gráflenyomat, első tárolási revízió, pending/cancelled/entitled állapot |
| `mission_engine_reward_ledger` | Egyszer megszerzett, karakterhez tartozó szimbolikus jogosultság; kibocsátási owner-epoch és tárolási revízió |
| `mission_engine_operations` | A korábbi claim/save/release mellett a 4-es `Entitle` művelet tartalomhoz kötött nyugtája |

A két migráció ismételhető, meglévő sorokat nem töröl, régi és szezonális
táblát nem módosít. Az első migráció újrafuttatása nem állítja vissza a
2-es verziót 1-re. A második sem csökkenti a verziójelölőt. Egy jövőbeli
sémára régi migrációt ettől még nem szabad automatikusan visszafuttatni:
az operations CHECK-je konkrétan az 1–4 műveleteket írja le.

A MariaDB DDL nem atomi. A frissítés előtt minden Mission-írót le kell állítani;
a verziójelölő utolsó lépésbeli emelése nem helyettesíti ezt. Részlegesen
alkalmazott migrációval az új program megtagadja az írást. A migrációk kizárólag
privát tesztadatbázison futottak; éles alkalmazásuk külön telepítési feladat.

Az 1-es séma korábbi checkpointjait a DDL nem dekódolja vagy jutalmazza újra.
Egy pending régi állapot az exact gráfokkal visszatöltés és új, növekvő runtime-
revíziójú mentés után kaphat outboxot. Egy már nyugtázott, de ledgerrel nem
igazolható korábbi jutalom nem alakítható automatikusan jogosultsággá: ilyenkor
a mentés szándékosan elutasít, külön import/egyeztetés kell. Ez a réteg korábban
sem volt játékosokra bekapcsolva; éles Mission-adatot nem migráltunk.

## 3. Stabil action-azonosító

A `MissionActions` modul a teljes tuple-ből számol SHA-256 azonosítót:

`character / mission / definition-version / completion-instance / node / reward`

Az encoding `MEACT` domainjelölőt, 1-es formátumot, explicit little-endian
32/64 bites számokat és hosszprefixes karakterláncokat használ. Nem C++ ABI-t,
nem sessionazonosítót és nem az aktuális katalógus verzióját szerializálja.
Az outbox külön tárolja és minden mentéskor ellenőrzi a teljes tuple-t és a
pontos gráflenyomatot is. Azonos reward-név külön node-on vagy instance-ben
külön jogosultság; reconnect ugyanahhoz a kéréshez nem hoz létre új kulcsot.

A `CheckpointActionBatch` kizárólag validált checkpointból emeli ki az aktív
jutalomkéréseket és a már nyugtázott reward-node-okat. Egy checkpointban
összesen legfeljebb **128** ilyen bejegyzés lehet, a megtartott régi nyugtákkal
együtt. Túllépéskor a teljes mentési terv elutasításra kerül; nincs csonkítás.
Ez külön SQL-kötegkorlát a domain mag általános gráf-/instance-korlátai mellett.
A későbbi állapotretenció/pruning tervezésekor ezt is figyelembe kell venni;
a tartós ledger nem törölhető az instance memóriából való eltávolításával.

## 4. Három tartós lépés

### A. Save: progress és outbox

A meglévő `persistence::Save` most ugyanabban a tranzakcióban:

- owner/token/revízió CAS-sel menti a checkpointot és pontos archívumhivatkozásait;
- létrehozza a még hiányzó aktív outbox-kéréseket;
- ellenőrzi a teljes actionkulcsot és gráflenyomatot;
- minden nyugtázott node-hoz már commitolt jogosultságot követel;
- cancelled állapotba teszi a már nem aktív, még ki nem bocsátott kéréseket;
- rögzíti a mentési operation receiptet.

Az outbox INSERT nem írja át az eredeti kulcsot vagy a kezdeti revíziót.
A korábban cancelled action ID nem éleszthető újra. Egy hibás nyugtázás,
hiányzó definíció, kulcsütközés vagy stale owner az egész mentést visszagörgeti.
A nagyobb listák csoportosított, bound paraméteres statementekbe kerülnek;
a 128-as köteg a prepared végrehajtó korlátain belül marad.

### B. RecordEntitlement: szimbolikus jogosultság

A megbízható owner-koordinátornak előbb az aktuális runtime-időt, deadline-okat
és érvényes actiont kell rendeznie, majd ezt a döntési checkpointot mentenie.
A `RecordEntitlement(owner, action, storageRevision, checkpointDigest, operation)`:

1. zárolja a karaktert, és ellenőrzi az exact epoch/token/revízió/checkpoint-hasht;
2. ellenőrzi a mentett, még pending action teljes kulcsát;
3. egyedi action ID-val beírja a jogosultságot a ledgerbe;
4. entitled állapotba állítja az outbox-sort;
5. ugyanabban a tranzakcióban rögzíti a 4-es operation nyugtát.

Ez a lépés nem változtatja a checkpointot, ezért nem emeli annak tárolási
revízióját. Az API nem jogosultsági határ: kizárólag hiteles szerveroldali
koordinátor hívhatja. A DB-fence nem ellenőrzi helyette a sessiont, az aktuális
játékidőt vagy a jövőbeli natív reward-payload engedélyezését.

Minden karaktert módosító action-tranzakció először ugyanazt a karaktersort
zárolja. Ha a megszakítás commitol előbb, nincs jogosultság. Ha a kibocsátás
commitol előbb, a már megszerzett jogosultság egy későbbi abandon után is megmarad.
Ezt a sorrendet a tényleges kézbesítőnek is követnie kell majd.

### C. Acknowledge és következő Save

`ReadAction` csak a kért aktuális owner számára olvas, teljes kulcsot, állapotot,
revíziót és entitled esetén ledger-metaadatot ellenőriz. A későbbi koordinátor
csak igazolt, még érvényes runtime-actiont nyugtázhat. A következő `Save` ismét
ellenőrzi a ledger hátterét a checkpoint nyugtázott node-jaihoz.

Ha a B lépés commitol, majd a folyamat a C előtt leáll, reconnectkor a régi
checkpointban még pending action marad. Az entitled sorból ugyanaz a jogosultság
olvasható vissza; nem kell másodszor kibocsátani. A már létező entitled sor mellett
egy köztes pending checkpoint is menthető. A runtime `Acknowledge` továbbra is
memória-művelet: önmagában nem bizonyít DB-commitot és nem valósít meg tárgyhatást.

## 5. Bizonytalan commit, callback és helyreállítás

Az új művelet a korábbi `Plan`/`Resolve` protokollt használja. A kérés digestje
az action ID-t és a pontos döntési checkpoint digestjét is tartalmazza. Azonos
operation token más kéréshez nem használható fel. Minden nem igazolt írás után
új kapcsolaton, a karaktersort zároló `Resolve` dönti el a commitot vagy a hiányt.
Egy történeti operation receipt nem bizonyítja az owner jelenlegi jogosultságát.

Az SQL-tervek tulajdonolt értékek, a worker csak SQL-t hajt végre. A későbbi
ECS callbacknek újra kell keresnie a karaktert és ellenőriznie kell a sessiont,
owner-epochot és a kérésgenerációt; a `ReadAction` eredményének dekódolása nem
váltja ki ezt. A `Missing` eredmény stale ownert is jelenthet, nem alap új állapot
vagy új jutalom automatikus létrehozására.

## 6. Ellenőrzések

A célzott próbák a tényleges store-/action-kódot és a két valódi migrációt
használják, friss, véletlen sémájú és loopback-portú MariaDB 11.4.13-példányon.

| Ellenőrzés | Rögzített sikeres eredmény |
|---|---|
| Windows x64 GameServer, normál és ASAN build | Lefordult |
| Checkpoint és action identity egységteszt | 3227 ellenőrzés |
| Célzott normál regressziós CTest | 27/27 sikeres |
| Külön ASAN CTest | 6/6 sikeres |
| Valódi Mission store/action, végső normál kör | 529 ellenőrzés, majd 39 az új folyamatban |
| Valódi Mission store/action ASAN alatt | Sikeres; két elveszett COMMIT-válasz és DB crash/cold restart is |
| Korábbi valódi SQL-infrastruktúra normál és ASAN alatt | 204 ellenőrzés, sikeres |
| Védett szezonális / meglévő Unity-fájlok | 34 / 8 változatlan |

A SQL-polling miatt az ellenőrzések száma futásonként eltérhet. A 27-es kör
után még egy késői mentési tokenütközés próbája került a tesztbe; a bővített
Mission-tesztek normál és ASAN körét külön újrafuttattuk. A production kód
eközben nem változott. FreeBSD build és Unity end-to-end próba nem történt.

Helyi naplók az ignorált buildmappában: `mission-actions-full-build.log`,
`mission-actions-asan-build.log`, `mission-actions-regression-tests.log`,
`mission-actions-asan-unit-tests.log`, `mission-actions-final-tests.log`,
`mission-actions-final-asan-db-tests.log`, `mission-actions-base-sql-asan.log`.
A privát MariaDB server.log-ja tényleges InnoDB crash recoveryt rögzített.

Lefedett esetek:

- stabil/golden action encoding, minden kulcsmező, sessionváltás és kapacitáshatár;
- checkpoint és outbox közös commitja; hiányzó, hamis és stale action elutasítása;
- ledger nélküli runtime-nyugta elutasítása a progress visszagörgetésével;
- késői operation-token ütközés: nincs részleges ledger vagy outbox-állapot;
- ismételt és két kapcsolaton versenyző kibocsátás: egyetlen jogosultság;
- abandon–kibocsátás verseny, megszakított kulcs újraaktiválásának tiltása;
- félbeírt ledger után `KILL CONNECTION`, majd hiányegyeztetés és biztonságos újrapróba;
- valódi sikeres COMMIT-válasz eldobása külön a mentésnél és a kibocsátásnál;
- privát MariaDB kényszerített leállítása, InnoDB recovery, új C++ folyamat,
  ugyanannak az operationnek az egyeztetése és a checkpoint nyugtázása;
- teljes 128-as SQL-köteg, vegyes pending/nyugtázott node-ok és túllépés elutasítása;
- régi/szezonális táblák sentineljei változatlanok; az item_award sentinel sem változik.

A valódi inventory-teltség és tárgyátvétel crash-próbái még nincsenek lefedve,
mert nincs kézbesítési adapter. Ez nem teljes, egyszeri jutalomkézbesítési elfogadás.

## 7. Következő sorrend

1. Verziózott, archivált natív jutalompayload és binding: item/XP/pénz/fogyasztás
   képességek explicit támogatása és elutasítása.
2. A meglévő tárgykézbesítés auditált claim-/előállítási résének rendezése vagy
   saját, ugyanahhoz a kanonikus item-tárolóhoz kapcsolódó, tartós kézbesítő.
3. Valódi item-sémás full-inventory, crash, retry és két-core-os kézbesítési próbák.
4. Hiteles session-/core-koordináció és aszinkron persistence/action lifecycle.
5. Natív gameplay-események és az első végigjátszható küldetésszelet.

Az outbox és a szimbolikus ledger elkészülte nem oldja meg automatikusan a
2–4. pontokat. Az új Mission Engine ezek igazolása előtt nem kapcsolható
be játékosokra, és az SQL-en kívüli ECS-hatás nem tekinthető közös ACID-írásnak.
