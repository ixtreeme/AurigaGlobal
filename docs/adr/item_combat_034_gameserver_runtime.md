# ADR IC-034 — F4: a GameServer átállítása (bontás) és az indításkori katalógus

2026-10-30. Fázis: F4 ([IC-012](item_combat_012_item_runtime_first.md)).
Kiinduló HEAD: `235de385`.
Állapot: **F4.1 és F4.2 implementált; F4.3–F4.7 terv.** A tárgyakat a
GameServer írja közvetlenül (a felhasználó döntése, 2026-10-30).

## Miért kell bontani

Az IC-012 F4 kapuja: a GameServer az új katalógusból fut, és a runtime-ban
nincs régi `item_proto`/vnum út. A mai felület, amit ki kell váltani:

| Régi felület | Előfordulás |
|---|---:|
| `ITEM_MANAGER::instance()` hívás | 264, 44 fájlban |
| `CreateItem(` | 131, 27 fájlban |
| `TItemTable` | 125, 34 fájlban |
| `ItemSystem_LegacyBridge.cpp` | kb. 9000 sor |
| `ItemSystem.cpp` + `InventorySystem.cpp` | kb. 7300 sor |

Ez egy lépésben nem ellenőrizhető. Az IC-001/IC-012 tilalmai viszont
érvényesek: nincs runtime vnum-adapter, nincs kettős írás, és nincs a régi
modellre visszakapcsoló flag. Ezért a lépések az új runtime részeit a valódi
GameServerbe építik, saját tesztekkel. A régi tárgyút az F4.7 egyetlen
átállással távolítja el. Addig a régi út változatlanul fut, az új részek
pedig még nem írnak játékállapotot.

Ismert következmény: az F4 és az F5 (protokoll, Unity) között nincs játszható
kliens, mert a régi kliens nem ismeri az ItemKey-t. Ez az IC-001 koherens
kiadási szabályából következik.

## Lépések

| Lépés | Tartalom | Kapu |
|---|---|---|
| **F4.1** | A GameServer induláskor betölti a mentés által rögzített `protos` katalógust, és változtathatatlanul közzéteszi. Hiba esetén nem indul. | Kész (lásd lent). |
| **F4.2** | Tárgytulajdonosi munkamenet: belépéskor/váltáskor tartós `session_generation` foglalás az `item_owner_fence`-ben, `LoadItemOwner`, kilépéskor a folyamatban lévő írások lezárása. Új karakternél tulajdonos, tárolók és pénztárca létrehozása. | Kész a mentési rétegben és a GameServer munkamenet-kezelőjében; az élő belépésbe kötés az F4.7 része (lásd lent). |
| **F4.3** | ECS-példány a `PersistentItem`-ből: ItemInstanceId, ItemKey, szint, revízió, tulajdonos, hely, bónuszok, foglalatok. Nincs `TItemTable` pointer. | Betöltés → ECS → `PersistentItem` oda-vissza azonos. |
| **F4.4** | Tartós műveletek: aszinkron tárgyíró a meglévő `CAsyncSQL` workeren, művelet-ID, `CommitUnknown` egyeztetés. Mozgatás, halmozás, bontás, megsemmisítés, fejlesztés, bónusz- és foglalatszolgáltatás, fogyasztás. Az ECS csak sikeres commit után változik. | Újracsatlakozás és összeomlás után nincs dupla levonás vagy elveszett tárgy. |
| **F4.5** | Statok: a felszerelés a katalógusból (`RefinedBaseStats`, `BonusProfilesAt`) adja a régi harcképletek bemenetét. | Rögzített tárgykészletre a statok egyeznek a várt értékkel. |
| **F4.6** | Gazdaság és tartalom: bolt (katalógusár), trade (két tulajdonos egy commitban), földre dobás és felvétel, mob-drop ItemKey-jel, quest Lua API, Mission `ItemRewardSpec`, raktár és itemshop. | Folyamatonkénti tesztek. |
| **F4.7** | Átállás: a boot `item_proto`, a `TItemTable`, a vnum utak, a Database `player.item` cache és a régi ItemAward kivezetése. | IC-012 F4 kapu: a régi út nincs a runtime-ban, a tesztek zöldek vagy kiváltva. |

## Döntés: ki írja a tárgyakat?

| Lehetőség | Leírás | Következmény |
|---|---|---|
| **A) GameServer közvetlenül** (javasolt) | A Mission Engine mintája: saját `CAsyncSQL` worker és az IC-032 tranzakciós író. | A tárgyakat a Database cache nem viszi tovább; a core-váltást a kerítés kezeli (F4.2). |
| B) Database szerver cache-én át | A régi player-cache útvonal bővítése. | Késleltetett flush, két író ugyanarra az állapotra. Ütközik az IC-032-vel („egy művelet = egy tranzakció, nyugtával”) és a terv 13.3 pontjával (egy író). |

**Elfogadva: A** (a felhasználó döntése, 2026-10-30). A Database szerver a
típusos tárgyakat nem cache-eli és nem írja.

A mai Database `player_session::Authority` generációja memóriában él, és
újraindításkor nulláról kezdődik. Az `item_owner_fence` ennél erősebbet kér:
tartósan, monoton növekvő generációt. Az F4.2 foglalása ezért tranzakcióban
növeli a tárolt értéket, és azzal tölt.

## F4.1 döntés: indításkori katalógus

**Konfiguráció.** Új `sql.protos` kapcsolat a GameServernek. Hiányzó beállításnál
a player-kapcsolat címét, portját és hitelesítését örökli, az adatbázis neve
`protos` lesz. Részlegesen felülírható, például külön, csak olvasó fiókkal:
`"protos": {"user": "catalog_reader", "password": "..."}`. Más adatbázisnév vagy
más MariaDB-végpont hibás, mert a betöltő és az író SQL-je teljesen minősített
(`protos.*`, `player.*`). A Database még nem kap ilyen kapcsolatot.

**Kötés.** A GameServer a player-kapcsolaton olvassa a
`player.item_persistence_format` sort: séma 1, revízió > 0, pontosan 32 bájtos
SHA-256. A karakterek tárgyai ehhez a katalógushoz készültek, ezért a GameServer
is ezt tölti be, nem „a legutóbbit”.

**Betöltés.** Külön, közvetlen kapcsolat a `protos`-hoz, `utf8mb4`-gyel.
A munkamenet `REPEATABLE READ` és `READ ONLY`, így tágabb jogú fiókkal sem ír.
A `ReadCatalog` a kötés revíziójával és hash-ével olvas. Bármilyen eltérés
(tartalom, formátum, regiszter) hibát ad, részleges katalógus nincs.

**Közzététel.** Egyszer, a főszálon, a játékciklus és a kliensek előtt.
`shared_ptr<const ItemCatalog>`, a folyamat végéig él. Második telepítés hiba.
Futás közbeni katalógusváltás nincs: új katalógushoz újraindítás kell
(a terv 14.4 pontja külön kapu).

**Hiba.** Hiányzó séma vagy kötés, eltérő tartalom, kapcsolati vagy SQL-hiba
esetén a GameServer naplózza az okot, és kilép. Nincs újrapróbálás és nincs
visszaesés a régi `item_proto`-ra.

**Még nem része.** A betöltött katalógust a játékmenet még nem használja; a régi
boot `item_proto` és a régi tárgyút változatlan (F4.3–F4.7). A Database és a
LoginServer nem változott.

## Üzemeltetési hatás

Ezzel a builddel a GameServer csak olyan környezetben indul, ahol telepítve van:
- a `protos` adatbázis ennek a buildnek megfelelő formátummal
  (`schemaVersion` 24);
- a `player` mentési táblái (`db/migrations/item_instance/001`);
- a katalóguskötés sora.

A helyi `Server2-Local` csomagon a mentési táblák 2026-10-29 óta telepítve
vannak (IC-033). A kötéssort a Tárgyteszt próbapad létrehozása írja be. A többi
környezetet élesítés előtt ugyanígy elő kell készíteni.

**Katalógusszerkesztés után.** A kötés a régi hash-en marad, amíg valaki át nem
állítja. Ezt ma a Tárgyteszt lap teszi meg, de csak akkor, ha minden tárolt
példány érvényes az új katalógussal (IC-033). Addig a GameServer `HashMismatch`
hibával nem indul. Ez szándékos: a tárolt tárgyakat ellenőrizetlen katalógussal
futtatni nem biztonságos. Futó GameServer alatt a kötés átállítása nem cseréli
a betöltött katalógust; újraindítás kell.

## F4.2 döntés: tárgytulajdonosi munkamenet

**Létrehozás (`ProvisionItemOwner`).** Üres tulajdonos egy tranzakcióban:
- kerítés: 1. munkamenet, 1. revízió;
- a megadott tárolók;
- a megadott pénztárcák, kifejezett egyenleggel (revízió 1).

Meglévő tulajdonost soha nem ír felül: `OwnerExists`, és semmi nem változik.
Új karakter szabványos tulajdonosa (`CharacterOwnerProvision`): inventory a
játék rácsállandóiból (`INVENTORY_PAGE_COLUMN` × `INVENTORY_PAGE_ROW` ×
`INVENTORY_PAGE_COUNT`), felszerelés 8 hellyel, arany pénztárca. A mount- és az
extra inventory a hozzájuk tartozó kategóriákkal érkezik (F4.6).

**Foglalás (`ClaimItemOwner`).** Egy tranzakció:
1. a szokásos őrök (izoláció, katalóguskötés, táblák);
2. `session_generation + 1`;
3. a kapott generáció és a revízió kiolvasása.

A növelés zároló írás, ezért megvárja ugyanannak a tulajdonosnak a folyamatban
lévő íróját; a később induló betöltés már annak commitját látja. Ezután a
régebbi munkamenet minden írása és betöltése hibát ad. Nem létező tulajdonosnál
`MissingOwner`: a foglalás nem hoz létre tulajdonost. `CommitUnknown` után a
foglalás nyugodtan ismételhető, mert mindig a legújabb nyer.

**Betöltés.** `LoadItemOwner` aszinkron változatot is kapott
(`PrepareLoadItemOwner` + `LoadItemOwnerCompleted`). A kerítés hiányát és az
elavult munkamenetet külön kód jelzi:
- `MissingOwner`: a tulajdonos nem létezik;
- `StaleSession`: egy újabb munkamenet átvette;
- `InvalidFence`: soha ki nem adott generáció.

Korábban mindhárom „sérült adat” volt.

**GameServer: `ItemOwnerSessions`.** Főszálon élő állapotgép tulajdonosonként.
Állapotok: Claiming → Loading → Active → Closing → Closed, illetve Superseded
vagy Failed.
- A munkamenet egy bérlethez kötött: a Databasezel egyeztetett betöltési kulcs
  és a teljes EnTT-entitás. Eredményt csak ugyanarra a bérletre alkalmaz.
- Minden SQL-munka jegyet kap. A befejezésnek pontosan az előkészített munkának
  kell lennie: ugyanazok az utasítások, értékek és sorelvárások. Az ismeretlen,
  ismételt vagy hamisított befejezés nem változtat állapotot.
- Egy core-on egy tulajdonosnak egyszerre egy élő munkamenete lehet. A második
  bérlet `Busy`; zárás alatt az új belépés sorba áll (`Queued`), és a
  folyamatban lévő írások lezárása után foglal.
- Írás csak Active állapotban indulhat (`BeginWrite`/`EndWrite`). A kerítést
  csak ugyanannak a munkamenetnek az újabb revíziója viszi előre.
- Zárás közben megérkező foglalási/betöltési eredményt eldob.
- `AsyncItemSqlExecutor`: dedikált, aszinkron `CAsyncSQL` kapcsolat; a
  befejezéseket a főszál `Pump` hívása adja át. ECS-pointer, descriptor vagy
  callback nem kerül az SQL-szálra.

**Miért nincs még az élő belépésben.** Az F4.7 előtt az új részek nem írnak
játékállapotot. Ha a belépés most foglalna:
- a meglévő, még nem konvertált karakterekhez üres típusos tulajdonos kellene,
  ami ütközne az F6 offline importtal;
- az új karakterek kezdőtárgyai még a régi úton jönnek, így a két modell
  eltérne.

Ezért a bekötés az F4.7 átállás része, az F6 importtal együtt:
1. `HEADER_DG_PLAYER_LOAD_SUCCESS` és `BindEntity` után `Open` a
   `{kulcs, entitás}` bérlettel;
2. a világba lépés az Active eseményre vár;
3. Superseded vagy Failed esemény: a kapcsolat bontása;
4. kilépés és core-váltás: `Close`;
5. a főciklus minden tickben `Pump`-ol;
6. karakterlétrehozás után a GameServer létrehozza a tulajdonost. Ha ez
   elmaradt (például összeomlás miatt), a belépésnél kapott `MissingOwner`
   után létrehozza, majd foglal. Átállás után ez biztonságos, mert addigra az F6
   minden korábbi karaktert importált, így hiányzó tulajdonos csak frissen
   létrehozott karakternél fordulhat elő.

**Jogosultság.** A GameServer tárgyírója SELECT-et kap a `protos`-ra és a
`player`-re; INSERT/UPDATE/DELETE-et a mentési táblákra, a létrehozáshoz az
`item_owner_fence`, `item_storage` és `item_wallet` táblákon is. A katalógust
nem módosíthatja.

