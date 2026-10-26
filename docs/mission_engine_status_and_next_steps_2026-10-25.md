# AurigaGlobal Mission Engine – állapot és folytatási terv

- **Dátum:** 2026-10-26
- **Szerverág:** `codex/mission-engine`
- **Aktuális kódcommit:** `f9ae65c3`; előző lezárt állapot: `10d4cc43`.

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
útvonalak még nem használják őket. Elkészült a tényleges SQL-workerig érő,
hibás/újrapróbált mentéseket is követő két-lane-es drain adapter (4/D–4c).
Elkészült a tényleges GameServer–Database karakterbetöltés verziózott sessioncsatornája,
a Database aktuális peer/account/slot ellenőrzése és a GameServer descriptor/ECS
generációhoz kötött válaszkezelése (4/D–4d). A source-stop és cache-átadás még hátravan.
Elkészült a Database-oldali, cache-t megtartó player/item flush és két valódi
SQL-lane eredményét ellenőrző natív adapter is (4/D–4e). Saját cache-példány- és
módosításgeneráció védi az A → B → A változástól és a cache újragyártásától.
Ez előkészítési API; nem ad source-stopot vagy Mission-írási jogosultságot.
Elkészült a sessionhöz és EnTT-generációhoz kötött GameServer–Database
cache-drain csatorna is (4/D–4f), külön képességegyeztetéssel. A későbbi GD-írások
– a kettébontott header/payload mentések is – visszavonják a korábbi nyugtát.
Ez a csatorna sem állítja meg az ECS gazdasági termelőit és nem aktivál Mission-jutalmat.
Elkészült a GameServer-oldali player/carried-item pillanatkép összeállítása
és a mentések → Begin kötelező sorrendje is (4/D–4g). A hordott tárgyak kétirányú
ECS-/ID-index ellenőrzése közös legacy szerializálót használ; hibánál nem indul
előkészítés és a delayed-save munkák megmaradnak. Ez sem ad source-stop nyugtát.
Elkészült a játékoscsere és az inventory-mozgatás/split műveleti belépésének
atomikus védelme, valamint e két csoport időkorlátos pause-a a natív
cache-előkészítés alatt (4/D–4h). A nyitott csere és a még futó callbackek
is követettek. A többi gazdasági író miatt ez továbbra sem teljes source-stop.
A jelenlegi Mission-séma 5-ös, 12 táblával.
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

### 3.13. Valódi legacy SQL-sorürítés (4/D–4c)

- Minden queued SQL-munkához worker-lane és monoton sorszám; külön pollolható FIFO-ticket.
- Nem visszatérő, prepared, rollbackelt és bizonytalanul újrapróbált mentés hibanyomának megőrzése.
- Két valódi player-workerhez kapcsolt Database API és session/challenge/barrier szerint ellenőrzött `ResourceDrain`.
- A nem visszatérő szöveges SQL minden eredményét kiolvassuk, a késői multi-result/procedure hibát is.
- Javítva a retry után a másolt FIFO-ban bent maradó munka és annak shutdown-flush útja.
- Tényleges worker-adapteres Mission-próba a demonstrációs SELECT-barrierek helyett;
  hibás régi mentés után az új jutalomterv tiltva marad.

**Határ:** a baseline hibanyoma lane-szintű és folyamaton belüli. A karakterenkénti
flush/mentésattribúció, a source megállítása és a folyamatközi source-stop/drain
GD/DG üzenetprotokoll még hátravan. A betöltés sessionazonosítási protokollja
a következő 4/D–4d részben elkészült. A ticket nem küldhető át folyamatközi mutatóként.
Távoli gépen ebben a munkafázisban semmilyen művelet nem történt; a próbák izolált,
loopbackre kötött helyi MariaDB-példányokat használnak. Az élő Gameplay kapu zárt.

### 3.14. Natív session- és betöltési híd (4/D–4d)

- HMAC-hitelesített natív linken egyeztetett, verziózott GD/DG sessioncsatorna; régi peerrel legacy betöltési fallback.
- Külön kapcsolat-, kérés- és session-token, Database-generáció, fiók/karakter/peer/descriptor kötés.
- Valódi warm/cold player/item/quest/affect/skill-color betöltések és owned SQL-callback kontextusok átvezetése.
- Aktuális logon, autentikált peer és pontos négyhelyes fióktulajdonlás ellenőrzése, párhuzamos load tiltásával.
- Kijelentkezés/auth-csere/peer-vesztés utáni régi válaszok és ismételt részek eldobása.
- GameServer oldalon a descriptor mellett a teljes EnTT-entitásgeneráció egyezése is szükséges.
- Hossz-, típus-, verzió-, sor-/owner-/PID-ellenőrzött belső envelope; új Unity-packet nélkül.
- Tényleges késleltetett helyi async SQL-válasz átvezetése logout és azonos PID/handle melletti újrabelépésen.

**Határ:** ez a live koordinátor azonosítási/betöltési része. A hiteles source-stop,
dirty cache-flush, folyamatközi drain-nyugták, tartós writer-lease átadás, guarded legacy
DML, ECS-rebase és reconnect-helyreállítás következik. Nincs élő Mission-dispatcher.
A régi/aszimmetrikus peer- vagy kulcs nélküli fallback nem használható Mission-ownerként.
A Gameplay kapu továbbra is zárt. Távoli kapcsolat vagy telepítés nem történt.

### 3.15. Natív Database cache-flush és két-lane-es nyugta (4/D–4e)

- A teljesen betöltött natív login/peer/account/karakter/descriptor/lease ellenőrzése.
- Saját challenge, egy előkészítés/login, idempotens retry, 256 bejegyzéses korlát és stale takarítás.
- A valódi player-cache és a teljes item-cache map karakterhez tartozó sorainak ellenőrzése, legfeljebb 1024 tárgyig.
- Meglévő termelési player/item DML és ablaknév-leképezés; csak dirty flush, eviction nélkül.
- Folyamaton belül nem újrahasznált cache-példányazonosító, monoton Put/Delete generáció és A → B → A felismerése.
- Mindkét tényleges player-worker owned ticketje; kizárólag az eredeti worker-origóból értékelt SQL-hibanyom.
- Másik/lejárt session, cache-változás, törlés, újragyártás, SQL-hiba és hibás lane elutasítása.
- Két valódi SQL-workerrel és a canonical player/item DDL-lel végzett, advisory lockkal vezérelt próba.

**Határ:** ez kizárólag natív Database cache-előkészítés. A `Flushed` nyugta nem
source-stop, nem canonical resource-revision és nem writer-lease. Nincs új GD/DG
parancs és automatikus gameplay hívó; a ResourceFence nem nyílik meg ettől.
A következő rész a GameServer termelőinek megállítása/final snapshotja és a
folyamatközi protokoll; utána a tartós handoff, guarded DML és ECS-rebase.
A későbbi/direkt/másik SQL-kapcsolatos írásokat külön át kell vezetni.
A helyi játékadatbázis, Unity és távoli gép nem módosult; a Gameplay kapu zárt.

### 3.16. Folyamatközi cache-drain és új mentések felismerése (4/D–4f)

- Külön Hello/ACK képességegyeztetés a meglévő HMAC-hitelesített session-linken; régi Database mellett nincs cache-kérés.
- Fix hosszú, explicit little-endian ACD1 Begin/Poll/Receipt codec, négy új belső GD/DG headerrel.
- Database oldalon a tényleges peer/descriptor és a teljes natív session ellenőrzése, majd a meglévő cache-drain hívása.
- GameServer oldalon teljes betöltés, account/PID és EnTT-generáció ellenőrzése, értéket birtokló descriptorállapottal.
- Idempotens Begin retry, Pending után Poll, 200 ms ütemezés, 256-os korlát és 10 másodperces érvénytelenítés.
- Stale/idegen/duplikált válasz elutasítása; a sikerhez két külön, hibamentes, befejezett SQL-lane szükséges.
- Az első Pending lane- és barriersorszámai rögzülnek; a későbbi pozitív nyugta nem cserélheti le őket.
- A közös DBPacketHeader ponton a későbbi módosítást okozható kérés érvényteleníti a nyugtát; az adatmentés továbbmegy.
- A tényleges két-worker SQL-nyugta ToWire/codec/natív Client lánca is tesztelt.
- Kódszintű forrásleltár a tényleges producer-stophoz: point/item, harc/quest/timer, átadás, külön SQL, raktár/offlineshop.

**Határ:** a cache-nyugta nem source-stop és nem writer-lease. Nincs automatikus
RequestPlayerCacheDrain gameplay-hívó, ResourceFence-ready vagy jutalmazás.
A GD-figyelő core-szintű, konzervatív; a még nem mentett ECS-változás és a
külön GameServer DBManager/AccountDB írásai külön rendezendők. A következő
rész a safe ponton működő karakter-koordinátor és az eseményeket megőrző
producer-stop, majd a tartós writer-handoff/guarded DML/ECS-rebase.
A tényleges socketes két teljes szerverfolyamatos és Unity-elfogadás még hátravan.
Nem történt távoli művelet vagy helyi játékadatbázis-módosítás; Gameplay zárt.

### 3.17. Natív player/item pillanatkép és mentési sorrend (4/D–4g)

- A RequestPlayerCacheDrain maga készíti a player és a hordott tárgyak birtokolt pillanatképét; Begin csak a sorok után kerülhet a kimeneti adatfolyamba.
- Csak teljesen betöltött, aktuális GAME/session/EnTT scope; descriptor-nyilvántartás újraellenőrzése a capture után.
- Csere, raktár/mall, függő betöltés és tiltott inventory-állapot esetén nincs új előkészítés.
- A tényleges inventory-tömbök és az ItemOwner/ID-index kétirányú ellenőrzése; duplikált, stale, hiányos, lezárt vagy még átadás alatt álló tárgy elutasítása.
- A SaveSingleItem és a snapshot ugyanazt az item-szerializálót használja; felszerelés/öv pozíció, socket és attribútum együtt ellenőrzött.
- Legfeljebb 1024 tárgy, ID és ablak/pozíció egyediség; sem részleges capture-output, sem eldobott delayed-save munka.
- Ismételt challenge nem küld új pillanatképet; érvénytelenített előkészítés nem éleszthető újra.
- A snapshot keretei → termelési cache/DML → két valódi SQL-worker → wire/client út külön MariaDB-próbája elkészült.

**Határ:** ez a pillanatkép nem állítja meg az ECS-termelőket, nem igazol teljes
DB/live inventory-halmazegyezést és nem ad tartós writer-lease-t. A fiókszintű
mount-raktár külön GameServer SQL-író, nem a player cache-drain része. Nincs
új automatikus gameplay-hívó vagy jutalmazás. A tényleges producer-stop,
cross-character/külön SQL-írók, writer-handoff és rebase továbbra is hátravan.
Részletek: `mission_final_snapshot_2026-10-26.md`.

### 3.18. Csere és inventory-mozgatás műveleti engedélyei (4/D–4h)

- Önálló Core admission réteg: több szereplő egyszerre lép be, vagy egyik sem; birtokolt RAII engedélyekkel, ECS-pointer tárolása nélkül.
- Session nélküli legacy aktivitás is követett; egy futó művelet callbackje közben nem köthető köré új natív session és pause.
- A csere nyitott élettartama és az egyes API-hívások/publikálások külön engedélyt tartanak; a tárgy- és pénzátadás előtt új ellenőrzés történik.
- A közös MoveItemImpl lefedi a drag/drop, split és az ebből delegált merge/equip/unequip útvonalat; zárlat alatt nincs részleges debit vagy elvesző split-tárgy.
- A RequestPlayerCacheDrain a pillanatkép előtt pause-olja a két csoportot; pozitív nyugta csak pontos session/entity/PID/actor-példány és még tartott pause mellett használható.
- Negatív nyugta, új GD-írás, életciklusváltás vagy az eredeti tíz másodperces előkészítési határ lejárta visszavonja a bizonyítékot és feloldja az érintett pause-t.
- A félbeszakított, még SQL-submit nélküli előkészítés után a legacy aktivitás helyreállhat; ettől régi natív kötés vagy írási jogosultság nem éled újra.

**Határ:** csak két konkrét műveletcsoport védett. A külön equip/use/pickup/drop,
bolt, harc/quest/XP/pénz, timer, raktár, pet és külön SQL-író még rendezendő.
Nincs teljes SourceStopped nyugta, tartós writer-handoff vagy Mission SQL-submit.
A tíz másodperces feloldás nem használható későbbi CommitUnknown recoveryre.
Nincs automatikus gameplay-hívó vagy új Mission-jutalmazás. A teljes DB/live
inventory-egyeztetés és ECS/cache rebase továbbra is hiányzik.
Részletek: `mission_operation_admission_2026-10-26.md`.

## 4. Ellenőrzött eredmények

| Ellenőrzés | Legutóbbi sikeres eredmény |
|---|---|
| Windows x64 / RelWithDebInfo fordítás | GameServer, Database és LoginServer normál/ASAN |
| Natív Mission core | 8723 ellenőrzés |
| Csomagkezelés | 4856 ellenőrzés |
| Checkpoint codec és action identity | 3227 ellenőrzés |
| Verziózott reward-payload suite | 2109 ellenőrzés, normál és ASAN alatt |
| Kevert erőforrástervező / codec / fence | 1606 ellenőrzés, normál és ASAN |
| Natív player/session protokoll és életciklus | 818 ellenőrzés; izolált async SQL-lel 826, normál/ASAN |
| Natív player/item cache-flush és generáció | 38 natív; izolált snapshot/két-worker SQL-/wire-úttal 455, normál/ASAN |
| Cache GD/DG codec és natív descriptorállapot | 1507 ellenőrzés, normál/ASAN |
| Natív snapshot/framing és kapacitás | 4426 ellenőrzés, normál/ASAN |
| ItemManager lifecycle és kétirányú snapshot capture | 8947 ellenőrzés, normál/ASAN |
| Core/ECS műveleti admission és pause | 1081 ellenőrzés, normál/ASAN |
| Tényleges játékoscsere és callback-élettartam | 14736 ellenőrzés, normál/ASAN |
| Tényleges inventory/quickslot és split | 821002 ellenőrzés, normál/ASAN |
| SQL-egységtesztek | 2256 ellenőrzés; az új queue/retry/lifecycle próbákkal |
| Valódi SQL-infrastruktúra | 234 ellenőrzés; a valódi drain és késői szöveges SQL-hibák próbáival |
| Valódi Mission store/action/item/resource effects, három séma és cold restore | Normál és ASAN: 2959 + 155 a legutóbbi drain-adapteres körökben |
| Kanonikus item-award terv-/határteszt | 41 ellenőrzés, normál és ASAN |
| Valódi item-award delivery/cache/cold-process | Normál: 2823 + 111; ASAN: 2827 + 111 ellenőrzés |
| Célzott regressziós CTest-kör | 44/44 a 4/D–4h részfázisban; öt izolált MariaDB-integrációval |
| Külön AddressSanitizer CTest-kör | 32/32 natív próba a 4/D–4h részfázisban; új ASAN MariaDB-kör nélkül |
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

A 4/D–4c végső kódján a normál regressziós kör **22/22**, az ASAN natív kör
**12/12** sikeres. A normál és ASAN GameServer, Database és LoginServer build is
elkészült. A külön ASAN SQL-infrastruktúra 234 ellenőrzéssel, a Mission store
2959 + 155 ellenőrzéssel sikeres; az utóbbi hét elveszett COMMIT-választ és
kényszerített DB-restartot tartalmaz. Az új drain-teszt közben előkerült retry-FIFO
hiba külön, determinisztikus regressziós próbát és javítást kapott; ez nem bizonyítja
a korábbi, különálló hatásnyugta-visszaolvasási észlelés okát.

A 4/D–4d végső kódján a normál regresszió **27/27**, a natív ASAN-regresszió
**16/16** sikeres. A session-suite **810**, a valóban késleltetett helyi SQL-válaszos
próba **818** ellenőrzéssel sikeres mindkét konfigurációban. A három szerverbináris
normál és ASAN buildje is elkészült; a végső duplikált-callback sorrendjavítás után
a Database mindkét változata ismét lefordult. Ez nem két folyamat közötti
Unity/gameplay elfogadási próba. A naplókat és a pontos határt a
`mission_session_bridge_2026-10-26.md` dokumentum sorolja fel.

A 4/D–4e végső kódján a normál regresszió **29/29**, a natív ASAN-regresszió
**17/17** sikeres. A tényleges cache/player-save adapter **38** natív, illetve
két valódi SQL-workerrel **339** ellenőrzéssel sikeres mindkét konfigurációban.
Az ASAN MariaDB-próba külön futott; az ASAN buildben nincs konfigurálva az
integrációs MariaDB-bináriskönyvtár, ezért nem része a 17-es CTest-körnek.
A GameServer, Database és LoginServer normál/ASAN buildje és az érintett
DatabaseTimeTests újrafordítása sikeres. A pontos scope és a naplók:
`mission_cache_drain_2026-10-26.md`. A cache-drain nem igazolja a még hiányzó
GameServer source-stopot vagy a tartós writer-lease átadását.

A 4/D–4f végső kódján a normál regresszió **30/30**, a natív ASAN-regresszió
**18/18** sikeres. Az új codec/client suite **1506** ellenőrzéssel sikeres.
A cache/DML → két worker → ToWire/codec → natív Client út **404**, a teljesebb
session-betöltés/életciklus **818**, izolált késleltetett SQL-lel **826**
ellenőrzéssel sikeres normál és ASAN alatt. A két ASAN MariaDB-próba külön
futott, nem része a 18-as CTest-körnek. A végső lane/barrier-pinning után a
GameServer, Database, LoginServer és DatabaseTimeTests mindkét konfigurációban
újra elkészült. A nyugta nem source-stop; a natív packetkezelő teljes buildje
és a codec/SQL út próbája nem helyettesíti a két teljes folyamat socketes vagy
Unity-elfogadását. Részletek: `mission_cache_protocol_2026-10-26.md`.

A 4/D–4g végső kódján a normál CTest-körök **31/31 + 12/12**, összesen
**43/43**; a natív ASAN-körök **19/19 + 12/12**, összesen **31/31** sikeresek.
A snapshot/framing **4426**, a tényleges ItemManager **8947**, a cache-client
**1507** ellenőrzéssel sikeres. Az új snapshot → cache → valódi SQL → wire
próba **455**, a késleltetett session-válasz **826** ellenőrzéssel sikeres
normál/ASAN MariaDB-n. A két ASAN MariaDB-suite külön futott, nem része a
31-es CTest-számnak. A korábbi FIFO-fejlesztésből kimaradt tizenkét legacy
SQL-teszthelyettesítő lane-inicializálása és a valódi cancellation helper
bekötése is elkészült; ezek frissen fordítva futottak. A GameServer,
Database és LoginServer normál/ASAN buildje sikeres. A source-stop, teljes
DB/live inventory-egyeztetés és writer-handoff továbbra sincs aktiválva.
Részletek és naplók: `mission_final_snapshot_2026-10-26.md`.

A 4/D–4h végső kódján a normál CTest-kör **44/44**, a natív ASAN-kör
**32/32** sikeres. A Core/ECS admission **1081**, a termelési csere **14736**,
az inventory/quickslot **821002** ellenőrzéssel sikeres normál és ASAN alatt.
A normál kör öt izolált MariaDB-integrációt is tartalmaz; ebben a részfázisban
külön ASAN MariaDB-kör nem futott. A GameServer, Database és LoginServer
mindkét buildje elkészült. A védett 34 szezonális és nyolc Unity-fájl hash-e
változatlan. Ez a két ismert műveletcsoport pause-át igazolja; teljes source-stopot,
writer-handoffot vagy élő Mission-jutalmazást nem. Naplók és pontos scope:
`mission_operation_admission_2026-10-26.md`.

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
| `f9ae65c3` | Csere és inventory-mozgatás atomikus admission rétege, cache-előkészítési pause és tényleges műveleti hibapróbák |
| `3829456d` | Natív player/item pillanatkép, kétirányú ECS-ellenőrzés és kötelező mentések → Begin sorrend |
| `d27406a7` | Tizenkét legacy SQL-teszthelyettesítő lane-inicializálása és a valódi drain cancellation helper bekötése |
| `9b8d6371` | Hiteles GD/DG cache-drain, descriptoros poller és későbbi mentések felismerése |
| `f3a1f6d1` | Natív Database cache-drain, cache-generáció és tényleges player/item SQL-próbák |
| `45473766` | Natív GameServer–Database sessioncsatorna, owned load-callbackok és EnTT-generáció ellenőrzése |
| `8044061f` | Valódi SQL-worker drain, hibás mentések nyoma, eredménykiolvasás és retry-FIFO javítása |
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
- [x] Hiteles player-load sessioncsatorna és késői betöltési válaszok generációs ellenőrzése.
- [ ] Teljes source-stop/core-koordináció és tartós writer-lease átadás az élő erőforrásíróban.
- [x] Progress, dedup és stabil mentési műveletazonosító közös tranzakciója.
- [x] Revision/CAS alapú mentés és `CommitUnknown` egyeztetése friss kapcsolaton.
- [x] Betöltési válasznál karakter/session/EnTT-generáció újraellenőrzése az ECS elérése előtt.
- [ ] Mission save/reward/rebase callbackek bekötése ugyanehhez a hiteles native scope-hoz.

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
- [x] Valódi két-lane-es FIFO-drain, hibanyom és session/challenge szerint ellenőrzött adapter.
- [x] Teljes XP/pénz/cost/award csomag közös SQL-commitja és tartós resource-effect nyugtája.
- [x] Natív Database oldali, cache-t megtartó dirty player/item flush és két-worker nyugta, session/cache-generáció ellenőrzéssel.
- [x] Egyeztetett GD/DG cache-drain, scope-hoz kötött nyugta és későbbi GD-mentések felismerése.
- [x] Natív GameServer player/carried-item pillanatkép és ellenőrzött mentések → Begin sorrend.
- [x] Játékoscsere és inventory-mozgatás/split atomikus műveleti belépése és időkorlátos cache-előkészítési pause-a.
- [ ] Élő GameServer producer-stop, teljes DB/live inventory-egyeztetés és minden legacy/külön SQL-író átvezetése.
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
Az egyszeri ASAN-eltérés oka nyitott; a sessionazonosítás próbái elkészültek,
a teljes source-stop/writer-átadás és élő Mission/core-protokoll próbái még hátravannak. A backend stresszpróbája nem helyettesíti ezeket.

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
- [Valódi legacy SQL-sorürítés és retry-hibák](D:/AurigaGlobal/LiveWork/AurigaGlobal/docs/mission_sql_drain_2026-10-26.md)
- [Natív session- és betöltési híd](D:/AurigaGlobal/LiveWork/AurigaGlobal/docs/mission_session_bridge_2026-10-26.md)
- [Natív Database cache-flush és két-lane-es nyugta](D:/AurigaGlobal/LiveWork/AurigaGlobal/docs/mission_cache_drain_2026-10-26.md)
- [Folyamatközi cache-drain, GD-írófigyelő és source-stop leltár](D:/AurigaGlobal/LiveWork/AurigaGlobal/docs/mission_cache_protocol_2026-10-26.md)
- [Natív player/item pillanatkép és mentési sorrend](D:/AurigaGlobal/LiveWork/AurigaGlobal/docs/mission_final_snapshot_2026-10-26.md)
- [Csere és inventory-mozgatás műveleti engedélyei](D:/AurigaGlobal/LiveWork/AurigaGlobal/docs/mission_operation_admission_2026-10-26.md)
- [Commit-sorrendtől független award-cache](D:/AurigaGlobal/LiveWork/AurigaGlobal/docs/mission_award_discovery_2026-10-25.md)
- [Külön Mission-adatbázis, telepítés és mentési szerződés](D:/AurigaGlobal/LiveWork/AurigaGlobal/docs/mission_database_2026-10-25.md)
- [Gépi node-/API-leltár](D:/AurigaGlobal/LiveWork/AurigaGlobal/docs/mission_node_library_coverage_2026-10-25.json)
- [80-as kampány tervezési manifestje](D:/AurigaGlobal/LiveWork/AurigaGlobal/docs/mission_campaign_manifest_2026-10-25.json)
