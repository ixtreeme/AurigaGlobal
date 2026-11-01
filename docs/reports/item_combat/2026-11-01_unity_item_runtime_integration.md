# Új tárgyrendszer: Unity és futó szerver

2026-11-01. Unity `6000.3.27f1`, klienság `/main/codex-mission-engine`,
szerverág `codex/mission-engine`. A kliens mentése: **Unity changeset 40**.

## Elkészült kliensútvonalak

| Funkció | Unity | Szerver / protokoll |
| --- | --- | --- |
| Inventory és pénztárca | Lapozható rács, méretek és teljes állapot | `GC_TYPED_INVENTORY` 225 |
| Katalógusrészlet | Ikonútvonal, névkulcs, követelmények, bónuszok és receptek | Új `GC_TYPED_ITEM_DETAILS` 227 |
| Áthelyezés | Kijelölés, majd szabad célcella; teljes lábnyomellenőrzés | `CG_TYPED_ITEM_MOVE` 225 |
| Felszerelés / csere / levétel | Nyolc hely, automatikus szabad inventorycél levételkor | Ugyanaz a típusos move kérés |
| Gyógyital | Használat gomb az instant profilokra | `CG_TYPED_ITEM_USE` 226 |
| Kovács | Kijelölt NPC, receptválasztás, költség/esély/következmény, megerősítés | `CG_TYPED_ITEM_REFINE` 227 |
| Saját tárgy eldobása | Egész vagy részhalom, megerősítés | Új `CG_TYPED_ITEM_DROP` 228 |
| Felvétel | A meglévő földi tárgy és pickup rendszer | `GC_TYPED_GROUND_ADD` 226, `CG_ITEM_PICKUP` |

A tárgy azonosítója végig 64 bites, típusa `ItemKey`, a fejlesztési szint
külön mező. A kliens nem talál ki régi vnumot. A grafika az explicit ikonút
alapján kapcsolódik a már importált assetekhez.

A részletes nézet megjeleníti az alap- és fejlesztési statokat, a signed
példánybónuszokat, a kötést, a zárolt bónuszhelyet, a köveket és az
ékszerfoglalat kapacitását, fokozatát, időtartamát. A szerveres katalógusrész
és az inventory együtt válik érvényessé, karakterváltáskor mindkettő törlődik.
A kérések nem módosítják előre a kliens tárgyait vagy pénzét.

## Saját tárgy futásidejű életciklusa

A C7d-1 tervezőkre elkészült az SQL után publikáló világfoglalás, a részhalom
azonosítójának kiosztása, az atomi földi nyilvántartás és a lejárat.
A `003_owned_ground.sql` a már kiépített karakterek számára is előkészíti a
fenntartott escrow-t. Az új kiépítések ezt a próbapadtól kapják; a futó író
nem hoz létre tulajdonost vagy tárolót.

A `BeginOwnedGroundRun` a stabil `hostname:port` core-azonosítóhoz új
generációt rendel. A saját korábbi indulásának tételeit igazoltan
megszünteti; más core tételeit nem érinti. Minden földi írás a generációt
is ellenőrzi. A nyilvántartás és a tárgy ugyanabban a tranzakcióban változik.

Felvételkor a forrás friss munkamenete és a felvevő aktuális kerítése vesz
részt a műveletben. Mindkét helyi állapot újratöltődik. Másik core-on lévő
tulajdonoshoz a GG 45 értesítés jut el; a legfrissebb revízióig tartó
újratöltés megelőzi a várakozó tárgyadásokat. Ismeretlen COMMIT esetén az
eredeti nyugta rendezéséig nincs új írás vagy láthatósági döntés.

A láthatósági motor külön ellenőrzi a még nem publikált foglalást, így egy
közben odaérkező játékos sem kap előre látólista-kapcsolatot. A sikeresen
mentett tárgy a már ott álló karaktereknek is azonnal megjelenik.

## Ellenőrzés

**Natív build:** a GameServer és az érintett eszközök/tesztcélok elkészültek.
**CTest: 10/10 sikeres**, három saját MariaDB-s integrációval:

`mob_drops`, `typed_inventory_wire`, `typed_ground`, `persistent_items`,
`item_persistence_completion`, `item_persistence_mariadb`,
`item_bench_mariadb`, `spatial_lifecycle`, `typed_item_grants`,
`item_catalog_runtime_mariadb`.

Új SQL-esetek: core-onkénti takarítás, elavult core-generáció elutasítása,
atomi földi hivatkozás, pickup-ismétlés és nyugtahash, valamint teljes
példányállapot megőrzése. Új láthatósági eset: publikálás előtti mozgó
néző és publikálás utáni álló néző. A próbapad regressziója katalógusváltás
után a már kiépített tulajdonos újrakiépítés nélküli előkészítését is lefedi.

**Unity: 6/6 sikeres**: protokoll, új typed item integráció, bootstrap,
főjelenet, equipment/player assetszétválasztás, offline shop kivezetés.
Az új integrációs ellenőrzés **4179 ellenőrzést** futtat: szerveres golden,
minden TCP-vágási pont, hibás/trunkált adat, pontos CG-byteok,
inventoryműveletek és megerősítések, kötés, lap-/mérethatár,
karakterváltás, ikonfeloldás és atomi állapotérvényesítés.

Az ablakot grafikus batch módban képként is ellenőriztük:
`D:\Auriga-Unity6\SRC\AurigaUnity\Logs\typed-items-preview.png`.

**Futó helyi szerver + valódi Unity TLS-kliens: sikeres, 32 ellenőrzés.**
Külön helyi tesztfiókokkal, a normál bejelentkezési és csomagútvonalon:

1. Inventory betöltése és egy gyógyitalegység használata.
2. Normál kovácsnál sikeres +0 → +1 fejlesztés, két anyag és 100 Yang.
3. Sikertelen fejlesztés: a +1 és az azonosító megmarad, 50 Yang levonva.
4. Áthelyezés, felszerelés és levétel.
5. Egy anyagegység eldobása és saját felvétele: két külön, egy-egy darabos
   példány, a maradék eredeti azonosítójával.
6. A fegyver eldobása, a forrás kilépése, másik karakteres felvétel.
7. Újrabelépés: ugyanaz az egy fegyverpéldány, +1 szinten.

Külön SQL-ellenőrzés: a `4295004162` fegyver a #1611-es tesztkarakternél
van +1 szinten; a #1610-es forrásnál 2 gyógyital és két külön anyagpéldány
maradt. A pénztárca `999999850` (pontosan 150 levonás), földi hivatkozás: 0.
A live próba hálózati teszt; a megjelenítést és a gombeseményeket a Unity
integrációs/scene ellenőrzések külön vizsgálják. Az online forráskarakterrel,
két külön core között működő GG 45 frissítést ebben a körben nem próbáltuk
élőben; a fenti átadás kilépett forrással, ugyanazon a térkép-core-on futott.

## Helyi használat

- Projekt: `D:\Auriga-Unity6\SRC\AurigaUnity`. Az új inventory az `I` gombon
  érhető el. A régi munkapéldányokat nem módosítottuk.
- A `D:\AurigaGlobal\Server2-Local` csomag GameServere, natív próbapadja,
  futtatási DLL-jei és a 003-as migráció frissült. Az 1-es csatorna és a
  főtérkép core-ja elindult (`STACK_LISTENING`). A főtérkép 41, portja 30078;
  a szokásos kliensindítás továbbra is követi a karakterlistában kapott címet.
- Binárismentés: `run/deployments/typed-items-20261101T113035`.
  A telepítő a `player` adatbázist is mentette:
  `run/item-bench/20261101-113036-d4fb17c2`.
- Külön tesztkatalógus-bejegyzések: fegyver **1/2**, instant ital **5/1**,
  alapanyag **6/1** (`*.local_probe_*` kódok). A korábbi katalógusról a
  helyi `run/catalog-editor/typed-items-demo-*` mappában van pillanatkép.
- Dedikált tesztkarakterek: **#1610** és **#1611**. A privát belépési adatok
  az ignorált szerveres `build/typed-item-live-46e5b423.private.json` fájlban
  vannak; a verziókezelt kód nem tartalmaz jelszót.
- Naplók: `build/typed-items-ctest.log`, `typed-items-unity-suite.log`,
  `typed-items-live-unity.log`. A `Metin2TypedItemLiveValidator` opt-in
  localhost ellenőrzés, kizárólag saját tesztfiókokkal/provisionáló harness-szel.

## Megismételhető helyi próba

A verziókezelt Python-eszközök nem tartalmaznak géphez kötött útvonalat
vagy belépési adatot. A fixture privát JSON: az ignorált `build` mappába
vagy a checkouton kívülre kerüljön.

1. Állítsd le a saját helyi játékfolyamatokat. A csomag kezelt MariaDB-je
   maradjon elérhető, a 001–003 item migráció legyen telepítve.
2. `SeedTypedItemUnityLive.py`: menti a katalógust, idempotensen kiépíti a
   dedikált demo típusokat és a két receptet, előkészíti a próbapadot,
   és létrehozza/frissíti a privát fixture-t.
3. Indítsd frissen a helyi stack-et. Ez szükséges a régi karaktercache miatt.
4. `TypedItemUnityLiveSmoke.py`: a fixture két ellenőrzött saját fiókját
   létrehozza vagy újrahasználja, és elindítja a Unity live validátort.
   **Csak e dedikált karakterek typed inventoryját állítja alaphelyzetbe.**
   A karaktereket és a privát belépési adatokat megőrzi kézi próbához.

Példa a szerver checkoutjából, a jelenlegi Windows-környezettel:

```powershell
py -3 tests/server/SeedTypedItemUnityLive.py `
  --package D:/AurigaGlobal/Server2-Local `
  --catalog-tool build/codex-item-fixes/SRC/ItemDomain/Release/CatalogEditorTool.exe `
  --bench-tool build/codex-item-fixes/SRC/ItemDomain/Release/ItemBenchTool.exe `
  --fixture build/typed-item-live.private.json

# A helyi stack friss indítása után:
py -3 tests/server/TypedItemUnityLiveSmoke.py `
  --package D:/AurigaGlobal/Server2-Local `
  --unity-project D:/Auriga-Unity6/SRC/AurigaUnity `
  --unity-editor 'C:/Program Files/Unity/Hub/Editor/6000.3.27f1/Editor/Unity.exe' `
  --bench-tool build/codex-item-fixes/SRC/ItemDomain/Release/ItemBenchTool.exe `
  --fixture build/typed-item-live.private.json
```

A seed alapértelmezése a jelenlegi főtérkép: 41, `(987300, 268800)`, 30078.
Más csomaghoz a `--map-index`, `--x`, `--y`, `--game-port` kapcsolókkal
állíts be egy normál kovács közeli helyet. A Python-burkolók szintaxisát és
CLI-jét külön ellenőriztük; a fent rögzített 32-es élő kör ugyanezen
provisionálás korábbi, helyi útvonalas változatával futott.

## Megmaradó szerveres határ

Ez a munka az elkészült C1–C7d futásidejű műveleteket köti be. A további
használati típusok, a bónusz-/kőszolgáltatások, a bolt, trade, crafting,
quest/Mission tárgyútjai és a nem támogatott kategóriák futásidejű
bekötése külön F4-feladat. Ezekhez nincs megtévesztően aktív kliensművelet.
Az eszköz- és megjelenésprofilok teljes tartalmi konverziója, valamint a
FreeBSD-build és az összes gazdasági út végső játékpróbája továbbra is nyitott.
