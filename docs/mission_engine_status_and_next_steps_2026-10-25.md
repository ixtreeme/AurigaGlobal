# AurigaGlobal Mission Engine – állapot és folytatási terv

- **Dátum:** 2026-10-26
- **Szerverág:** `codex/mission-engine`
- **Aktuális kódcommit:** `d2155e5c`; előző lezárt állapot: `1cf10ce2`.

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
Elkészült a kizárólag tárgyakat adó Mission ledger → award adapter, tartós mappinggel
és a tényleges itemig végigfutó próbával. A cache a commit-sorrendtől függetlenül találja meg az awardokat.
Elkészült a kevert XP/pénz/tárgyfogyasztás determinisztikus előkészítése és a
bemenet/szabály/sorsolás/eredmény tartós archívuma is (4/D–4a).
Elkészült a teljes erőforráscsomag SQL-írója, tartós hatásnyugtája,
a két SQL-sorhoz kötött fence és a külön publikálási protokoll (4/D–4b).
Ezek a backend API-k izolált adatbázison teszteltek; az élő cache/session
útvonalak még nem használják őket. A jelenlegi Mission-séma 5-ös, 12 táblával.
Az új Mission-adatok külön, ötödik `mission` adatbázisba kerülnek. A
konfiguráció, telepítő, Qt-beállítások, SQL-ellenőrzés és mentési központ ezt
kezeli; külön player/mission sémán közös commit/rollback/crash próba is sikeres.

A teljes, játékosok által használható új küldetésrendszer még nincs bekötve.
A session-/core-életciklus persistence-bekötése, a kevert jutalmazás élő adapterei,
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
A Mission ledger → award összekötés a későbbi 4/D–3-ban készült el. A teljes
item/XP/pénz/cost bundle és az ECS/session protokoll nem következik automatikusan ebből. A meglévő
közvetlen Database ág blokkoló; az új Mission dispatchernek aszinkron működés kell.

### 3.9. Külön Mission-adatbázis

- `common.sql.mission`, öröklés, részleges felülírás és típusos natív konfiguráció.
- A playerrel azonos MariaDB-végpont, külön sémanév; minden core közös Mission-célja.
- Régi konfigurációnál dedikált `mission` cél képzése, player-adatbázisra visszaesés nélkül.
- Önálló `db/mission_schema.sql`, a meglévő migrációkból generálva; a 4/D–4b óta tizenkét tábla.
- Új csomag bootstrap és explicit `init-mission-db`, privát hitelesítési fájlokkal.
- Qt **SQL · Küldetések** oldal, ötödik SQL-szerep ellenőrzése és közös mentési csoport.
- Mission-progress mellett a négyadatbázisos legacy import, illetve a régi négyes SQL-
  mentés részleges visszaállításának tiltása, adatváltoztatás előtt.
- Külön player/mission sémás valódi tranzakciós, jogosultsági és crash/restart próbák.
- Friss Qt6/PyInstaller alkalmazáscsomag és headless frozen worker-próba.

**Fontos határ:** az eredeti adatbázis-szétválasztási teszt csak infrastruktúra-bizonyíték
volt. A 4/D–3 már a tényleges natív item-adaptert, a 4/D–4b a teljes
erőforráscsomag SQL-íróját futtatja. Az élő session/core-dispatcher még hiányzik.
A felhasználó kérésére a külön helyi `mission` nézegethető adatbázis települt
a Server2-Local csomagban, 4-es sémával, játékosprogress nélkül. A mostani
5-ös séma csak izolált tesztadatbázisokra került, nem a helyi/távoli játékadatokra.

### 3.10. Mission tárgyjutalom és commit-sorrendtől független award-cache (4/D–3)

- Véges, legfeljebb 256 soros oldalakból álló cache-körök, rögzített felső ID-val.
- Később commitoló kisebb ID felismerése; egyetlen úton lévő kérés és azonos kurzoros retry.
- Cache-takarítás teljes sikeres kör után; az ajándékikon nyugtázása megmarad.
- `IssueItemAwards`: teljes tárgycsomag entitlement/award/mapping/operation közös tranzakciója.
- SQL-ben ellenőrzött karakter/account/login; hiteles hívótól kapott proto-/stack-korlátok.
- `ResolveItemAwards`/`DecodeItemAwards`: eredeti award ID-k új műveleti token és crash után is.
- A mapping megmarad az item/award törlése után; a receipt nem gyárt új tárgyat.
- Sémaverzió **3**, kilenc saját InnoDB-tábla, nyolc FK; additív harmadik migráció.
- Teljes kernel-esemény → checkpoint/outbox → award → cache → SAFEBOX-item hibapróba.

**Határ:** a tárgycsomag awardjai együtt jönnek létre; a valódi raktári itemek
awardonként atomikusan készülnek. XP/pénz/fogyasztás esetén az egész bundle
elutasításra kerül. Az aszinkron élő dispatcher, session/core-koordinátor és a
Gameplay binding engedélyezése még hátravan. Futó szerver és valódi játékosadat nem változott.

### 3.11. Kevert erőforrások előkészítése és archívuma (4/D–4a)

- Teljes csomagot elfogadó/elutasító, mellékhatásmentes natív erőforrástervező.
- Determinisztikus fogyasztás meglévő, alkalmas stackekből; grant nem fizethet costot.
- XP/szint/negyedszint/stat/skill/random HP/SP számítás, explicit archivált sorsolással.
- XP-blokkolás, maximumszint, pénzplafon és mezőtúlcsordulás teljes csomagos ellenőrzése.
- Kanonikus `AURRES01` formátum, SHA-256, méret-/mélység-/elemszám-korlát és teljes újraszámítás.
- Owner/revision/checkpoint/pending-action ellenőrzött SQL-archiválás és pontos műveletnyugta.
- Sémaverzió **4**, tíz saját InnoDB-tábla, kilenc FK; negyedik additív migráció.
- Az új nyugtákat tartalmazó adatbázison a régi migráció újrafuttatása sem szűkíti vissza a művelettípusokat.
- Lost COMMIT, rollback, cancelled action, tokenütközés, korlátos blobolvasás,
  párhuzamos REPEATABLE READ limitellenőrzés és valódi DB-crash utáni új folyamat próbája.

**Határ:** az archivált terv nem erőforrás-hatásnyugta és nem alkalmazási engedély.
A `Before`/`After` gazdasági/progressziós átmenet; az új jutalomtárgyak elhelyezése,
a teljes vitals/ECS frissítés és a szintlépési értesítések még a későbbi adapter feladatai.
Nincs bekötött tartós resource revision vagy cache-fence az élő karakteríróban.
A tényleges XP/pénz/fogyasztás közös SQL-commitja a következő, 4/D–4b
részfázisban elkészült. A Database/GameServer átadás és az élő koordinátor
továbbra is hátravan. A Gameplay kapu zárt.

### 3.12. Atomi SQL erőforrásíró és publikálás (4/D–4b)

- `ResourceFence`: session/core/writer-generáció, source-stop és két külön SQL-queue barrierje.
- Teljes kanonikus player/item pillanatkép zárolása; korlátos `JSON_TABLE` alapú, 1024 tárgyas ellenőrzés.
- XP/pénz/cost/award/ledger/operation közös tranzakciója, eredeti award ID-kat őrző hatásnyugtával.
- Tartós resource revision és guarded cache-DML; az A → B → A átmenet is revisiont növel.
- `Pending` / `Published` / `Superseded` visszaolvasás; történeti receipt nem játszhat vissza régi postimage-et.
- Külön, generációhoz kötött rebase és publikálás; vitals/pending-feloldás közös commitja.
- Sémaverzió **5**, tizenkét saját InnoDB-tábla, tizenkét FK; ötödik additív migráció.
- Valódi két FIFO worker, párhuzamos grant, MyISAM-elutasítás, részleges cost/vitals rollback,
  hét elveszett COMMIT-válasz, restart és 128 új-tokenes történeti egyeztetés próbája.

**Határ:** a fence-állapotgép és SQL-tervkészítők készek. Az élő karakterforrás
megállítása, korábbi flushok eredménykövetése, a legacy cache-DML átvezetése,
lease-átadás, ECS-rebase, vitals-számítás és értesítések még nincsenek bekötve.
Az egyszeri ASAN-visszaolvasási eltérés oka nyitott; részletek az ellenőrzéseknél.
A Gameplay aktiválása ezért továbbra is zárt.

## 4. Ellenőrzött eredmények

| Ellenőrzés | Legutóbbi sikeres eredmény |
|---|---|
| Windows x64 / RelWithDebInfo fordítás | GameServer és Database normál/ASAN; LoginServer az előző körben |
| Natív Mission core | 8723 ellenőrzés |
| Csomagkezelés | 4856 ellenőrzés |
| Checkpoint codec és action identity | 3227 ellenőrzés |
| Verziózott reward-payload suite | 2109 ellenőrzés, normál és ASAN alatt |
| Kevert erőforrástervező / codec / fence | 1606 ellenőrzés, normál és ASAN |
| SQL-egységtesztek | 2232 ellenőrzés |
| Valódi SQL-infrastruktúra | 204 ellenőrzés |
| Valódi Mission store/action/item/resource effects, három séma és cold restore | Normál és ASAN: 2952 + 155 a legutóbbi stresszkörökben |
| Kanonikus item-award terv-/határteszt | 41 ellenőrzés, normál és ASAN |
| Valódi item-award delivery/cache/cold-process | Normál: 2823 + 111; ASAN: 2827 + 111 ellenőrzés |
| Célzott regressziós CTest-kör | 21/21 sikeres a jelen részfázisban |
| Külön AddressSanitizer CTest-kör | 12/12 sikeres |
| Mission SQL-eszközök és saját helyi bootstrap/upgrade | 13/13 sikeres |
| Qt mentési központ / Mission GUI-csoport | 15/15 + 1/1 sikeres |
| Valódi MariaDB-suite-ok ASAN alatt | Az erőforrásíró is sikeres crash/restarttal; egy korábbi, egyszeri visszaolvasási eltérés nyitott |

A 4/D–4b végső kódján az érintett natív erőforrás- és valódi DB-suite külön
ismét sikeres: normál 2/2; a külön ASAN MariaDB crash/restart próbák is sikeresek.
A telepítő újrafuttatása meglévő kind-5 és kind-8 műveletnyugtákkal is sikeres.

**Nyitott ellenőrzési észlelés:** az első ASAN DB-kör egyszer elbukott egy már
commitolt, 1024 tárgyas jutalom új-tokenes történeti visszaolvasásánál. A hiba
akkor még nem közölte, hogy SQL- vagy dekódolási eltérés történt; memóriahibát
jelző ASAN-riport nem volt. A megőrzött adatbázisban az eredeti jutalom, head és
award ID-k megvoltak, és friss folyamatból visszaolvashatók. Bővített diagnosztika,
128 ismételt egyeztetés és több új ASAN-kör után nem ismétlődött. Az ok nincs
bizonyítva, ezért ezt nem tekintjük javítottnak; az élő aktiválás előtti nyitott
vizsgálati tétel. A részletes dokumentum a megőrzött naplókat is felsorolja.

A SQL-polling miatt a diagnosztikai ellenőrzések száma futásonként eltérhet.
A próbák többek között lefedik a hibás csomagokat, verzióütközéseket,
rollbacket, eseményduplázást, két valódi SQL-kapcsolat revisionversenyét,
commit előtti kapcsolatmegszakítást és egy már commitolt tranzakció elveszett
válaszát, az outbox/ledger versenyét és a ledger nélküli nyugtázás tiltását.
Az új próbák elkülönített helyi adatbázison futottak. A korábbi 34-es regressziós és
11-es ASAN kör után a Mission/legacy sémanévütközés-ellenőrzés szigorításának
konfigurációs próbái külön ismét sikeresek: normál 4/4, ASAN 3/3.
A valódi DB-próbák az aktív új gráf mellett a régi konkrét jutalomtartalmat és
a külön Mission/player/account adatbázisok közös, hitelesített tartós írását is ellenőrzik.

Ez a célzott regressziós kör eredménye; nem állítja az összes létező
szerverteszt vagy az összes játékbeli quest elfogadását. Ebben a Mission
részfázisban nem történt FreeBSD build, Unity end-to-end elfogadás vagy
az új Mission Engine éles/távoli telepítése.

A jelen fázis hash-ellenőrzése szerint a szezonális Mission Pass 34 védett
fájlja és a nyolc korábbi Unity-módosítás változatlan maradt.

## 5. Commitok és rögzített Unity-munka

| Commit | Elkészült munkafolyamat |
|---|---|
| `d2155e5c` | Atomi erőforrás-hatások, két SQL-soros fence, publikálás és sémaverzió 5 |
| `20e1dd77` | Determinisztikus kevert erőforrástervek, codec, tartós archívum és sémaverzió 4 |
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
| `0cea085d` | A 4/D–2 lezárt állapotösszesítése |
| `fb8b4b6e` | A csomagolás által ellenőrzött generált quest API-index frissítése |
| `508e2842` | Külön Mission-adatbázis, konfiguráció/telepítés/Qt/mentés és két sémás hibapróbák |
| `6ac2819a` | A külön Mission-adatbázis lezárt állapotösszesítése |
| `5f539899` | Commit-sorrendtől független, lapozott award-cache és nyugtázott ikonok megőrzése |
| `ec084d4e` | Mission tárgyjutalom közös tranzakciója, tartós award-mapping és teljes út hibapróbái |

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
a közös item-award kézbesítő a 4/D–2-ben, a Mission → award mapping és a
commit-sorrenddel szemben helyes cache a 4/D–3-ban elkészült.**
Az előkészítés és tervarchívum a 4/D–4a-ban, a tényleges XP/pénz/cost SQL-írója
és publikálási protokollja a 4/D–4b-ben elkészült. Következő az élő koordinátor
és az összes érintett cache/session író átvezetése.
Gameplay-hatás csak az igazolt jutalmazás és session-/core-koordináció után kapcsolható be.

- [x] Tartós action outbox a progress mentésével közös tranzakcióban és külön entitlement-ledger.
- [x] A mentett action, karakter, definícióverzió, owner, revision és checkpoint-hash ellenőrzése.
- [x] Verziózott és archivált natív reward-payload és ellenőrzött tartalomkötés.
- [x] Külön Mission-adatbázis; a player-oldali jutalomírással közös tranzakció infrastruktúrájának próbája.
- [x] Egy meglévő award tényleges item + taken nyugtája közös tranzakcióban,
  normál/ASAN valódi tárgysémás crash/retry/telt raktár próbákkal.
- [x] Mission ledger → award közös tranzakció és stabil action/payload → award mapping tárgycsomagokra.
- [x] Commit-sorrendtől független, korlátos oldalakból álló award-cache és helyes ikonnyugtázás.
- [x] Tárgycsomag teljes útjának valódi kernel/SQL/cache/item crash/retry/telt raktár próbája.
- [x] Kevert erőforrás-átmenet determinisztikus előkészítése és tartós, korlátos tervarchívuma.
- [x] Tartós resource revision, két SQL-soros fence-protokoll és guarded DML backend.
- [x] Teljes XP/pénz/cost/award csomag közös SQL-commitja és tartós resource-effect nyugtája.
- [ ] Élő source-stop, régi cache és függő mentések ellenőrzött kiürítése; minden legacy író átvezetése.
- [ ] Új login/core writer-lease átadása, hiteles ECS/cache rebase és publikálás.
- [ ] Az egyszeri ASAN történeti visszaolvasási eltérés okának tisztázása, aktiválás előtt.
- [ ] Az `item_award`/inventory/ECS réteggel összehangolt,
  idempotens kézbesítés; az item, XP, pénz és fogyasztás külön hatásszerződése.
- [ ] Restart/retry során ugyanaz a művelet ne adhasson vagy vehessen el újra tárgyat.

**Elfogadás:** tele inventory, újraküldött kérés, hálózati timeout és a
kézbesítés közbeni crash esetén sincs jutalomduplázás vagy elvesző jogosultság.
Az SQL-tranzakción kívüli ECS-hatásokhoz külön egyeztetési mechanizmus kell.

### 4. A valódi Mission store hibapróbái

A checkpoint-/archívum-/fencing-réteg, az outbox/entitlement és az egyedi
item-award → item réteg crash-, race-, reconnect- és hideg újraindítási próbái
sikeresek. A teljes Mission → award → item út próbái is sikeresek; a kevert
csomagok előkészítési/archiválási és tényleges SQL-hatáspróbái is lefutottak.
Az egyszeri ASAN-eltérés oka nyitott; az élő session/core-protokoll próbái is
hátravannak. A backend stresszpróbája nem helyettesíti ezeket.

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
- [Mission tárgyjutalom, stabil award-mapping és teljes út hibapróbái](D:/AurigaGlobal/LiveWork/AurigaGlobal/docs/mission_item_awards_2026-10-25.md)
- [Kevert erőforrások előkészítése és tartós archívuma](D:/AurigaGlobal/LiveWork/AurigaGlobal/docs/mission_resource_preparation_2026-10-25.md)
- [Atomi SQL erőforrásíró, fence és publikálás](D:/AurigaGlobal/LiveWork/AurigaGlobal/docs/mission_resource_writer_2026-10-26.md)
- [Commit-sorrendtől független award-cache](D:/AurigaGlobal/LiveWork/AurigaGlobal/docs/mission_award_discovery_2026-10-25.md)
- [Külön Mission-adatbázis, telepítés és mentési szerződés](D:/AurigaGlobal/LiveWork/AurigaGlobal/docs/mission_database_2026-10-25.md)
- [Gépi node-/API-leltár](D:/AurigaGlobal/LiveWork/AurigaGlobal/docs/mission_node_library_coverage_2026-10-25.json)
- [80-as kampány tervezési manifestje](D:/AurigaGlobal/LiveWork/AurigaGlobal/docs/mission_campaign_manifest_2026-10-25.json)
