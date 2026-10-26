# Mission Engine 4/D–4b – SQL erőforrásíró és publikálási protokoll

Dátum: 2026-10-26. Ág: `codex/mission-engine`.

## Elkészült backend

Az `ApplyResourceEffects` a 4/D–4a-ban előkészített és archivált erőforrástervből
valódi, közös MariaDB-tranzakciót készít. Az alábbi változások együtt commitolnak:

1. Mission owner-epoch/token, storage revision, checkpoint és pending action ellenőrzése.
2. A konkrét archivált döntés, definíció, payload és forrásrevision ellenőrzése.
3. A kanonikus karakter és a kezelt tárgyablakok teljes gazdasági pillanatképének zárolása/összevetése.
4. A fogyasztandó tárgyak count UPDATE-je vagy DELETE-je; a maradó stack socket/attribute adatai megmaradnak.
5. A pénz és az XP/szint/negyedszint/stat/skill/random HP/SP progressziós mezők UPDATE-je.
6. A teljes tárgyjutalom `item_award` rekordjai, stabil action → award mappinggel.
7. Az entitlement-ledger, outbox állapot, erőforrás-revision és tényleges hatásnyugta.
8. A teljes művelethez kötött tartós operation receipt.

Bármely ellenőrzési vagy SQL-hiba mindezt visszagörgeti. A jutalomtárgyak itt
award-jogosultságok: a tényleges SAFEBOX-item előállítása továbbra is a korábban
tesztelt közös award-kézbesítő feladata. Tiszta pénzcsomaghoz nem keletkezik üres
vagy mesterséges tárgyjutalom. Az `IssueItemAwards` publikus API továbbra is
elutasítja a kevert csomagot; közös belső draftot csak a teljes erőforrásíró használ.

**Határ:** ez a backend elkészült, nem az élő GameServer/Database integráció.
A meglévő cache- és GD packetkezelők nem kerültek át automatikusan az új
protokollra. Nincs élő Mission dispatcher, nincs bekapcsolt Gameplay binding.
Az adapterek teljes átvezetése nélkül az új írót nem szabad aktiválni.

## Forrászárolás és session/generáció

A `ResourceFence` saját értékeket birtokló, főszálon kezelt protokollállapot.
Nem tart nyers ECS-, karakter-, descriptor- vagy SQL-worker mutatót.
Az identity része a karakter/owner-epoch/token, a core- és session-generáció,
valamint egy író-generációs token. A challenge és mindkét SQL-barrier külön
azonosítóhoz kötött.

Az `ApplyResourceEffects` csak a következők után készít végrehajtható tervet:

- A megfelelő session és challenge visszaigazolta a forrás megállítását.
- A régi fő SQL-sor végéhez tartozó barrier sikeres.
- A külön async SQL-sor végéhez tartozó barrier is sikeres.

Egyetlen sor kiürülése, másik session válasza vagy másik barrier nem elég.
Hibás/bizonytalan drain esetén a fence blokkolva marad. A natív adapternek az
összes korábbi flush eredményét is ellenőriznie kell: egy sikeres sorvégi SELECT
nem igazolja automatikusan, hogy egy korábbi írás sikeres volt.

Elveszett vagy hibás végrehajtási válasz `Reconciling` állapotot eredményez.
Nincs timeout miatti automatikus feloldás. A régi SQL-író karakterzárát megváró
`ResolveResourceEffects` ad hiteles eredményt; újrapróbálás csak a tisztázott
eredményből következhet. Generációváltás/kijelentkezés után a régi callback
nem módosíthatja a fence-et.

## Hatásnyugta és friss állapot

Az immutable hatásnyugta nem azonos az előkészítési archívummal vagy az
entitlementtel: itt már a kanonikus pénz/XP/cost/award változásokat igazolja.
Másik művelettokennel, új ownerrel és DB-restart után is ugyanazok az eredeti
award ID-k kérdezhetők vissza. Ebből nem készül új grant vagy törölt tárgy.

A válasz külön közli az aktuális erőforrás-revisiont és a publikálás állapotát:

| Állapot | Teendő |
|---|---|
| Pending | Ez a döntés a legutóbbi, még publikálásra váró postimage. A sessiont fagyasztva kell tartani. |
| Published | Ennek a döntésnek a publikálása már commitolt. A jelenlegi kanonikus állapotot kell betölteni. |
| Superseded | Azóta újabb erőforrás-revision létezik. A régi postimage nem írható vissza; a receipt csak történeti igazolás. |

A régi jutalomnyugta önmagában nem jogosít régi karakterállapot visszaírására.
Az ugyanazon revision mellett ellentmondó head/receipt elutasításra kerül.

## Publikálás és vitals

Az első commit `publication_pending=1` állapotot hagy maga után. A régi
cache-írás az új guardon keresztül ekkor nem futhat le, és a source nem oldható fel.

A következő natív lépés a postimage alkalmazása és a cache újraalapozása. A
`Rebased` visszajelzés is session/generáció/döntés/revision szerint ellenőrzött.
Csak ezután készíthető `PublishResourceEffects` terv. A trusted natív adapter
adja az autoritatív HP/SP/stamina értékeket; ezek nem kliensbemenetek.

Refillt kérő negyedszint/szintlépésnél a vitals és a pending flag feloldása
közös SQL-tranzakció. Refill nélküli csomag a meglévő vitals egyezését követeli,
nem gyógyít a pénzjóváírás mellékhatásaként. Sikertelen publikálásnál a HP/SP és
a pending állapot is visszaáll. Elveszett publikálási COMMIT-választ külön
operation receipt tisztáz; a source addig nem folytatható.

**A valódi ECS-rebase, max-vitals számítás és a szintlépési quest/guild/party
értesítések még a következő natív adapter feladatai.** A tesztek ezeknek az
adapter-visszajelzéseit ellenőrzött értékekkel adják; nem Unity-játéktesztet futtatnak.

## A cache-írások új szerződése

A `GuardResourceWrites` a trusted, teljesen minősített natív player/item DML
batch-et egy tranzakcióba zárja a Mission owner, writer/core/session-generáció,
erőforrás-revision és publication állapot ellenőrzésével. Minden sikeres batch
revisiont növel, az A → B → A állapotváltozás is. Későn érkező régi snapshot
nem válhat újra érvényessé attól, hogy az összegek később ismét egyeznek.

A műveletnyugta a teljes SQL-szöveget, paramétertípust/értéket és eredményelvárást
is köti. Megváltozott batch azonos tokennel nem kaphat hamis siker-visszajelzést.
A karakter batch-eit sorban kell küldeni; nem szabad két workerre ugyanazzal
az előre feltételezett revisionnel szabadon kiosztani őket.

Ez nem SQL-sandbox: csak a saját, rögzített cache-DML tartozik ide. A
végrehajtó SQL-kapcsolat alapértelmezett adatbázisa Mission, ezért a canonical
player/item nevek teljesen minősítettek. A legacy nyers ReturnQuery/AsyncQuery
útvonalakat a Missiont használó karaktereknél át kell vezetni; megkerülés esetén
nem állítható cache-biztonság. InnoDB- és trigger-ellenőrzés előzi meg az írásokat.

## Séma, platform és jogosultság

Sémaverzió **5**, **12 saját InnoDB-tábla, 12 FK**. Új táblák:
`mission_engine_resource_heads` és `mission_engine_resource_effects`.
A head tartós állapot, nem üríthető a memória-cache takarításakor.
Az 5. migráció additív; nem módosítja a legacy táblák oszlopait vagy adatait.
A teljes installer régebbi migrációinak ismétlése az új művelettípusokkal is működik.

A tömeges snapshot/cost ellenőrzés `JSON_TABLE` segítségével korlátos számú
statementet és paramétert használ 1024 tárgynál is. A cél MariaDB 10.6 vagy újabb;
a tényleges próba 11.4.13-on fut. A funkció a többtáblás UPDATE/DELETE műveletekben
is használható. [MariaDB hivatalos JSON_TABLE dokumentáció](https://mariadb.com/docs/server/reference/sql-functions/special-functions/json-functions/json_table).

A snapshot hatóköre Inventory, ExtraInventory, Equipment, DragonSoul, Belt és
Ground. Account SAFEBOX/MALL nem kerül karakterköltségként elfogadásra.
A SQL-adapter lapos pozíciót használ (`category=0`); a natív extra-inventory
adapternek ugyanilyen kanonikus cellaszámot kell előállítania.

A helyi telepítő Mission-jogosultságai mellett player SELECT/UPDATE, item
SELECT/INSERT/UPDATE/DELETE, item_award SELECT/INSERT és azonos végponton account
SELECT jogot készít elő. A player táblákra nem ad általános DDL-jogot.

## Ellenőrzés és következő lépés

A részletes darabszámokat a központi állapotdokumentum rögzíti. A valódi
MariaDB-próbák kiterjednek két tényleges FIFO SQL-worker drainjére, a teljes
kevert csomagra és tiszta pénzjutalomra, 1024 tárgyra, stale snapshotra,
MyISAM-ra váltott cost tároló elutasítására, partial cost/publikálás rollbackre,
két párhuzamos jutalomkérésre, késői cache-save-re és A → B → A revisionre.
Összesen hét elveszett COMMIT-válasz szerepel a store-suite-ban; a végén
kényszerített MariaDB-restart és külön új C++ folyamat ellenőriz.

### Nyitott ASAN-visszaolvasási észlelés

Az első ASAN MariaDB-futás egyszer megállt a 1024 tárgyas eset új-tokenes
hatásnyugta-visszaolvasásánál (`new retry token lost original effect/award IDs`).
A kezdeti ellenőrzés nem különítette el a SQL-hibát, a dekódolási eltérést és az
azonosítók eltérését; memóriahibát jelző ASAN-riport nem keletkezett.
A tényleges adatokat ellenőrizve a commitolt hatás, publikált head és eredeti
award ID-k megmaradtak. A megőrzött eredeti adatkönyvtáron új folyamatból a
155 cold-process ellenőrzés sikeres volt.

Bővült a SQL-errno/outcome és a visszaolvasási ok diagnosztikája. A 1024 tárgyas
eset 128 külön új művelettokennel is ellenőrzi az eredeti nyugtát/award ID-kat.
A végső normál és több új ASAN-kör 2952 + 155 ellenőrzéssel sikeres, köztük
két párhuzamos izolált ASAN-futás is. Az eltérés oka ettől még **nincs bizonyítva**;
nem tekintjük javítottnak. Az élő aktiválás előtti nyitott vizsgálati tétel marad.

Megőrzött helyi bizonyítékok (a build mappa nem verziózott):

- Eredeti hibás kör: `build/mission-effects-asan-mariadb.log`.
- Eredeti adatkönyvtár: `build/mission-effects-isolated-asan/2b9a500f9c5f424e9b15bc0834eafac0/data`.
- Bővített diagnosztika/stressz: `build/mission-effects-asan-mariadb-diagnostics.log`,
  `build/mission-effects-asan-mariadb-stress.log`.
- Két új ASAN-kör: `build/mission-effects-asan-reproduce-0.log`,
  `build/mission-effects-asan-reproduce-1.log`.
- Végső normál 2/2: `build/mission-effects-release-tests.log`.

Következő: hiteles élő session/core koordinátor, source/konfiguráció megállítás,
cache-flush eredménykövetés, mindkét SQL-queue barrierje, a megfelelő karakterhez
kötött guarded batch-ek, új login/core számára lease-átadás, ECS/cache rebase,
vitals és értesítések, valamint reconnect/cold-start helyreállítás.
Aktív fence mellett a releváns szabály-/prototípusváltást is rendezni kell.
A Published/Superseded receipt út nem játszhatja vissza a régi postimage-et.

Ebben a részfázisban a tesztek saját ideiglenes MariaDB-példányokat használnak.
A korábban megmutatott helyi csomag `mission` adatbázisa nem frissült automatikusan
5-ösre; a futó helyi/távoli szerver, Unity-forrás és szezonális Mission Pass nem változott.
FreeBSD build és játékbeli elfogadás még nincs.
