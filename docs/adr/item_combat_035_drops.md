# ADR IC-035 — Drop az új tárgyrendszeren

2026-10-31. Fázis: F4 ([IC-034](item_combat_034_runtime_integration_plan.md),
C csomag, „drop”). Állapot: **döntés elfogadva; a C7a (b92fe955), a
C7b-1 (cff05d52), a C7b-2 (e37bb0ec), a C7b-3 (88c3a157) és a C7c kész; a saját
tárgy eldobásának C7d-1 alapja és a C7d-2 futásidejű/Unity-bekötés kész**.

## Régi működés (forrásaudit)

- **Sorsolás** (`ITEM_MANAGER::CreateDropItem`), hat forrásból:
  - közös drop a szörny rangja és a gyilkos szintsávja szerint;
  - fajonkénti dropcsoportok;
  - „kill-per-drop” mobcsoportok (egy nyeremény);
  - szintes csoportok;
  - dupla drop (prémium, kesztyű, `APPLY_DOUBLE_DROP_ITEM`);
  - egyéb: a szörny saját tárgya, metinkő, lókönyv, lottó, eseménydropok.
- **Szorzók** (`GetDropPct`): a szintkülönbség, a `PRIV_ITEM_DROP`
  privilégium, a dupla tárgy egyedi felszerelés és az `ITEM_DROP_EVENT`.
- **Kiosztás:** az `ENABLE_DROP_INSTANT_INVENTORY` be van kapcsolva.
  - A zsákmány a jogosult inventoryjába kerül, a meglévő halmokba egyesítve.
    Partyban a jogosult körforgással változik.
  - Ha nem fér el, a földre esik, és 60 másodpercig csak a jogosult veheti
    fel (`__GiveRewardItemToCharacterOrDrop`).
  - Bizonyos dungeonokban mindenki saját példányt kap (HWID-szűrés).

## Döntések (a felhasználó, 2026-10-31)

1. **A droptáblák a `protos` katalógusba kerülnek**, a recept- és a
   zsákmányprofilok mintájára: benne vannak a katalógus-hashben, a Tárgyak
   oldalon szerkeszthetők, és az F6 konverter ide írja a régi táblákat.
2. **Tele inventorynál a zsákmány a földre esik, 60 másodperces joggal**, ahogy
   ma.

## Döntések a C7c-hez (a felhasználó, 2026-11-01)

3. **A földi zsákmány csak memóriában él**, ahogy ma.
   - A földi tétel tárgyállapotot, darabszámot és jogosultat tart; a
     tárgypéldány a felvételkor jön létre.
   - 60 másodpercig a jogosulté, 5 perc múlva eltűnik.
   - Szerver-újraindításkor a földön lévő zsákmány elvész.
4. **A saját tárgy eldobása külön szelet.** A teljes példányállapot (szint,
   bónuszok) megőrzése külön tervet kíván.
5. **A szerver és a Unity kliens együtt készül.**
   - Új földi csomag megy ki: VID, tárgykulcs és darabszám.
   - A törlés, a jogosult kijelzése és a felvétel a meglévő, VID-alapú
     csomagokon megy.

## Szeletek

- **C7a — futásidejű azonosítók és tárgyadás.** Ez az alap: a drop, a bolt, a
  crafting és a quest-jutalom is új tárgyat hoz létre.
- **C7b — droptáblák a katalógusban és sorsolás a szörny halálakor.** A régi
  szorzók megmaradnak; a jutalom a jogosult inventoryjába kerül.
- **C7c — földi zsákmány tele inventorynál és felvétel.** A döntések fent
  (3–5.); a saját tárgy eldobása külön szelet.

## C7b — droptáblák a katalógusban (C7b-1, C7b-2 és C7b-3 kész)

Három lépésben készül:
- **C7b-1:** katalógusréteg és tiszta sorsoló;
- **C7b-2:** szerkesztőfelület;
- **C7b-3:** bekötés a szörny halálába.

**Modell** (katalógus v25, `031_mob_drops.sql`):
- **`mob_drop_group`:** azonosító, kód, cél és sorsolási mód.
  - A **cél** egy szörny (régi mob-vnum, mert a mobok még nem
    katalógustartalmak) vagy egy szörnyrang (0–5). A rang a régi közös drop,
    és csak „minden bejegyzés” módú lehet.
- **`mob_drop_entry`:** 1…N folytonos sorszám (legfeljebb 256), ItemKey,
  szint, darab, esély vagy súly, ritka bónusz esélye bp-ben, és a gyilkos
  szintablaka (alapból 1–65535).
- **Esélyegység:** milliárdod (ppb) ölésenként, semleges szorzókkal. Ez a régi
  fájlok minden százalékát egészként, veszteség nélkül hordozza.

**Sorsolási módok**, a régi csoportok szerint:

| Mód | Régi forrás | Esély | Metinkőnél |
|---|---|---|---|
| Minden bejegyzés | `drop`, `drop_item_group`, közös drop | `ppb × delta / 25000 ≥ U[1, tartomány]` | rangcsoport nem, szörnycsoport igen |
| Egy a N ölésből | `kill` | `40000 × delta / N ≥ U[1, tartomány]`, majd súlyozott választás: U[1, összsúly] a halmozott súlyokon | igen |
| Szinthez kötött | `limit` | `ppb ≥ U[1, 10⁹]`, szorzók nélkül, ha a gyilkos szintje ≥ a határ | nem |
| Dupla drop | `thiefgloves` | mint a „minden bejegyzés”, de csak prémiummal, kesztyűvel vagy `APPLY_DOUBLE_DROP_ITEM`-mel | nem |

A `delta` és a `tartomány` a régi `GetDropPct` értéke: szintkülönbség,
ritka bónusz, `GetMobItemRate`, `POINT_ITEM_DROP_BONUS`, dupla drop,
prémium, privilégium és esemény. A régi adatból (`dwPct` = fájlszázalék ×
10000) a ppb = `dwPct` × 250, illetve „limit” csoportnál × 1000. Így a régi
egész aritmetika eredménye változatlan marad.

**C7b-1 megvalósítás** (2026-10-31):
- **Tartomány:** `MobDrops.hpp/.cpp`, a validálás és a `PlanMobDrops`. A
  véletlenforrást a hívó adja, ahogy a régi `number(1, határ)`.
- **Katalógus:** `MobDropGroupsFor(mob, rang)`, ranggal és mobbal
  indexelve.
- **Hash:** v25, 54-es tábla.
- **SQL:** táblaleírás, pillanatkép, a szerkesztőeszköz commit-sorrendje és az
  export-staging lefedettsége.
- **Független ellenőrzés:** a Python-orákulumban egy négycsoportos
  droptáblás fixture. Minden korábbi golden újraszámolva (v25).
- **Tesztek:**
  - `MobDropTests` (45 ellenőrzés): validálás, katalógus-keresztellenőrzés,
    lekérdezés, SQL-sorok oda-vissza, és minden mód rögzített dobásokkal a régi
    képletekhez mérve;
  - a sémateszt a 031-es SQL-megkötéseket ellenőrzi.
- A teljes CTest zöld. Egy GUI-teszt egyszer időzítés miatt elbukott, majd
  háromszor egymás után átment.

**C7b-1 határa:**
- A szörny saját dropja, a metinkő, a lókönyv, a lottó és az eseménydropok
  szerverszabályok, nem droptáblák; ezek külön szeletek.
- A ritka bónusz esélye tárolódik, de az új tárgy még nem kap véletlen
  bónuszt.

**C7b-2 — szerkesztőfelület** (2026-10-31):
- **Modell** (`item_catalog.py`): `drop_groups`, `drop_group`, `drop_code`,
  `new_drop_group`, `apply_drops`, `delete_drop_group`.
  - Mentéskor a bejegyzések a dokumentum sorrendjében 1-től számozódnak.
  - A mód által nem használt esély vagy súly, valamint a többi mód paramétere
    nullázódik, ahogy a séma kéri.
  - Foglalt kódnál magyar hibaüzenet jön.
- **Mentés:** `save_drops` és `delete_drops`. A közös `commit_tables` út
  ugyanazt a zárat, nyugtamappát és natív commitot használja, mint a
  tárgymentés.
- **Hibaszöveg:** a natív `InvalidMobDrop` minden oka magyarul jelenik meg, a
  droptábla azonosítójával.
  - A droptáblában szereplő tárgy kivezetése név szerint utasítódik el.
- **Felület:** „Droptáblák” nézet a Tárgyak oldalon, `DropGroupDialog`
  (`drop_editor.py`).
  - A mentés csak a `mob_drop_*` táblákat érinti, ezért a nyitott
    tárgypiszkozat megmarad.
  - Elutasításkor az ablak a beírt értékekkel újranyílik.
- **Tesztek:**
  - `DropTableModelTests`: oda-vissza út, módváltás, törlés, és minden natív
    elutasítás magyar szövege;
  - `DropPageTests`: lista, kereső, ablak, mentés, újranyitás és törlés;
  - `SqlCommitTests`: commit, szerkesztés, triggeres visszagörgetés, a
    kivezetés tiltása és törlés privát MariaDB-n.

## C7b-3 — sorsolás a szörny halálakor (2026-10-31)

**Kapcsoló:** `server.json` → `items.drops`.
- `"legacy"` (alapérték): semmi nem változik.
- `"typed"`: csak `items.catalog = "required"` mellett érvényes. A szerver
  induláskor naplózza: `ItemDrops: typed (catalog drop tables)`.

**Mi változik `"typed"` módban:**
- A régi öt tábladrop helyett a katalógus droptáblái sorsolnak: közös drop,
  `drop`/`drop_item_group`, `kill`, `limit` és `thiefgloves`.
- Változatlanul a régi úton marad: a szörny saját tárgya, a metinkő, a
  lókönyv, a lottó és az eseménydropok.
- A célpont-információs ablak (target info) is csak a régi úton maradt
  forrásokat listázza. A típusos dropok ebben a régi csomagban nem
  jeleníthetők meg, mert nincs vnumjuk; ez kliensoldali munka (F5).

**Sorsolás:**
- Ölésenként egyszer fut a `GetDropPct`. Ugyanaz a `delta` és tartomány megy
  a régi és a típusos sorsolásnak is.
- A `PlanMobDrops` a régi `number()` véletlenforrással dob.
- A dupla drop feltétele a régi: prémium, dupla tárgy egyedi csoport vagy
  `APPLY_DOUBLE_DROP_ITEM`.

**Kiosztás** (a régi szabály, közös függvényekben):
- Egy drop a gyilkosé.
- Több dropnál a legalább 10%-ot sebzők között körbe megy. Partyban a party
  saját sorrendje (`GetNextOwnership`) dönt; ha senki nem éri el a 10%-ot, a
  gyilkosé.
- A megosztott dungeonokban minden nyertes saját példányt kap.
  - A nyertes HWID|HOST-onként egy karakter; egyezésnél a többet sebző.
- A `SharedDungeonDropWinners` és a `DropContributors` kiemelt függvény, a
  régi drop is ezeket használja. A viselkedése nem változott.

**Átadás** (`GameServer/item/TypedDrops.*`):
- **Várakozási sor:** a karakternek egyszerre egy tárgyművelete lehet a
  tárolóban, ezért a dropok karakterenként sorba állnak
  (`ecs::TypedGrantQueue`).
  - Az azonos tárgyállapotú dropok egy halomnyi mennyiségig összevonódnak
    (`QueueGrant`).
  - Legfeljebb 256 várakozó tétel lehet; a fölötte érkező drop naplózva
    elveszik.
- **Továbbküldés:** minden tárgyművelet (mozgatás, használat, fejlesztés,
  adás) válasza után a következő tétel indul (`PumpTypedItemGrants`).
- **Kiépítetlen karakter:** akinek nincs típusos tárgyállapota, nem kap
  tábladropot. Ezért a `"typed"` mód egyelőre csak próbára való.
- **Tele inventory:** a C7c óta a drop a jogosult lába elé esik, 60
  másodpercig az övé (lásd lent).
- **Kijelentkezés:** a várakozó dropok elvesznek.

**Még nincs benne:**
- a ritka bónusz esélye;
- az `ITEM_POLYMORPH` szűrése, ami a régiben csak a szörny saját gömbjét
  engedi;
- a régi „rare drop” közlemény;
- a tárgynapló (`MONEY_LOG_DROP`).

**Tesztek:**
- `PersistentItemTests`: a várakozási sor összevonása, a halomhatár, a
  szintek külön kezelése, a nulla darab és a 256-os korlát.
- `ServerConfigTests`: az alapérték, a `"typed"` elfogadása, a katalógus
  nélküli `"typed"` és a hibás érték elutasítása.
- `ItemManagerLifecycleTests`: `"typed"` módban a célpont-információs ablak
  nem listázza a régi táblákat.
- `CombatStateTests`: a csonkok követik az új `CreateDropItem` és
  `GetDropPct` hívást.
- A szerverkódot (a kiosztást és a várakozási sort) csak a fordítás és a
  kódátnézés ellenőrzi. Élő próbához `"typed"` mód, egy kiépített
  tesztkarakter és egy droptábla kell.

## C7c — földi zsákmány (2026-11-01)

**Tartomány** (`common/item/TypedGround.hpp`, `ItemDomain/TypedGround.cpp`):
- **`GroundLoot`:** tárgyállapot, a jogosult PID-je, a jog vége (60 mp) és az
  eltűnés ideje (5 perc).
- **`DecideGroundPickup`:** a régi felvételi szabály.
  - A jogosult, vagy lejárt jog után bárki, magának veszi fel.
  - A jogosult partytársa a jogosultnak veszi fel, és a tárgy az ő
    inventoryjába kerül.
  - Más nem veheti fel.
- **`GC_TYPED_GROUND_ADD` (226) v1:** explicit little-endian csomag.
  - Mezői: VID, x, y, z, ItemKey, szint, darab és a tartalomkód (1–96
    nyomtatható karakter).
  - A golden fájl (`tests/fixtures/item_combat/typed_ground_v1.hex`) mindkét
    oldalt rögzíti.

**Szerver** (`GameServer/item/TypedLoot.*`):
- **Földi tétel:** egy térbeli entitás (`SpatialKind::TypedLoot`). Csak azt
  hordozza, amit a kliens lát (`ecs::TypedLootView`: a kódolt csomag és a
  jogosult neve); maga a zsákmány VID szerint a `TypedLoot.cpp`-ben van.
- **VID:** a régi tárgy-VID számlálóból jön (`AllocateGroundVID`), így sosem
  ütközik régi földi tárggyal.
- **Látótér:** a láthatóság távolság szerint működik, mint a régi földi
  tárgynál. A hálózati kiosztás a 226-ost küldi, utána élő jognál a régi
  `GC_ITEM_OWNERSHIP`-et; eltávolításkor a régi `GC_ITEM_GROUND_DEL` megy.
- **Tele inventory:** a dropsor `NoRoom` esetén a zsákmányt a jogosult lába
  elé teszi, a régi ±140 egységes szórással. Csatatér-arénán nincs jog.
- **Felvétel:** a régi `CG_ITEM_PICKUP` (VID) előbb a típusos zsákmányt
  nézi.
  - Ellenőrzi: él a karakter, nem megfigyelő, ugyanazon a térképen van, és a
    régi 2400-as távolságon belül.
  - Ezután a tárgyadás (`RequestTypedItemGrant`) indul.
  - A tétel csak akkor tűnik el a földről, amikor a tároló megírta a tárgyat.
    Addig nem vehető fel újra. Sikertelen írásnál újra felvehető.
  - Az elutasító üzenetek karakterenként legfeljebb 3 másodpercenként mennek
    ki, mert a kliens automatikus felvétele másodpercenként négyszer próbál.
- **Időzítés:** a heartbeat másodpercenként futtatja a `TickTypedLoot`-ot.
  - A lejárt jogot törli; a látók üres nevű tulajdonos-csomagot kapnak, mint
    ma.
  - A lejárt vagy elveszett térképrészen maradt zsákmányt eltávolítja (pl.
    egy bezárt dungeon-példányét).
- **`RequestTypedItemGrant`:** kapott egy opcionális visszahívást, amely
  megmondja, hogy a tároló megírta-e a tárgyat. Akkor is meghívódik (hamis
  vagy igaz értékkel, a tényleges mentési eredmény szerint), ha a karakter
  közben kilépett. A belépési token csak a karakter frissítését korlátozza;
  sikeres mentés után a földi tétel kilépés esetén is eltűnik.
- **Bizonytalan COMMIT:** a tárgytároló megőrzi az eredeti kérést és
  műveletazonosítót. A nyugta zároló olvasása megvárja a folyamatban lévő
  tranzakció végét. Olvasási hiba esetén másodpercenként újrapróbálja az
  ellenőrzést; addig nincs végleges válasz, a karakter művelete és a földi
  tétel foglalt marad. A műveletet nem írja újra. Csak igazolt nyugta vagy
  igazolt hiány után oldja fel a várakozást.

**Unity kliens** (Plastic, `D:\Auriga-Unity6`):
- **Kodek:** `Metin2GamePacketCodec.ReadTypedGroundItem`, a szerverrel
  azonos szigorú ellenőrzéssel.
- **Csomagkeretező:** a 226 uint32 méretmezős dinamikus csomag.
- **Földi világ:** a típusos tételt a régi ismeretlen vnumú tárgyhoz hasonlóan
  rakja ki: kattintható dobozzal és felirattal, pl. „material.leather +3
  x200”. A felvétel a meglévő VID-es úton megy.
- **Validátor:** `Metin2ProtocolCodecValidator` a szerver golden fájljával.

**Tesztek:**
- `TypedGroundTests`: 31 ellenőrzés.
  - a felvételi szabály, a jog és a lejárat határai;
  - a csomag oda-vissza útja;
  - minden hibás csomag elutasítása;
  - a golden fájl.
- `EntityNetworkDispatchTests`: a 226-os bájtok változatlanul mennek ki, a
  jogosult felirata, a VID szerinti törlés, és nézet nélkül nem megy semmi.
- A földi tétel élettartamát és a felvételt csak a fordítás és a kódátnézés
  ellenőrzi. Élő próbához kell a kliens, a `"typed"` drop mód és egy tele
  inventoryjú, kiépített tesztkarakter.

**Még nincs benne:** a saját tárgy eldobása (külön szelet); a zsákmány 3D
modellje (a kliensnek még nincs típusos katalógusa, F5); a felvett tárgy
„102”-es kliensoldali üzenete (a típusos tárgy üzenete a szerver
chatszövege).

## C7d — saját tárgy eldobása (2026-11-01)

A felhasználó ezt választotta a következő fejlesztési iránynak. A C7c
`ItemGrant`-alapú földi zsákmánya új, állapot nélküli egységeket hoz létre;
a saját tárgyat ezen az úton nem szabad újragenerálni.

### C7d-1 — teljes példányállapotú atomi műveletek (kész)

`common/item/OwnedGround.hpp`, `ItemDomain/OwnedGround.cpp`:

- `MakeGroundDropCommit`: inventoryból előre kiépített `TradeEscrow`
  tárolóba helyez. Az egész halom azonosítója megmarad. Részleges eldobásnál
  a maradék az eredeti példányon marad, a leválasztott rész külön lefoglalt
  azonosítót kap; a két módosítás egy tranzakció.
- A kulcs, fejlesztési szint, példánybónuszok, zárolt bónuszhely, kövek és
  ékszerfoglalat-állapot másolással marad meg; nincs alapértékekre állítás.
- Kötött és felszerelt tárgy nem dobható el. A régi DROP/GIVE tiltásnak
  megfelelően az `allowDrop` és az `allowTrade` is szükséges. A túl nagy
  darabszám és a már használt split-azonosító elutasított.
- `MakeGroundPickupCommit`: a konkrét escrow-példány kerül a felvevő szabad
  inventoryhelyére. Két karakter esetén mindkét tulajdonos kerítése ugyanabban
  a tranzakcióban vesz részt. Saját visszavételnél egyetlen kerítés szerepel.
  Ebben a szeletben nincs halomösszevonás; az azonosság megőrzése explicit.
- `MakeGroundExpireCommit`: csak escrow-példányt szüntethet meg; az utolsó
  teljes állapot sírkőként megmarad. Inventoryban vagy felszerelve lévő
  tárgyhoz ezen a belépési ponton nem nyúlhat.
- Minden érintett rácsot a teljes előtte/utána állapot alapján ellenőriz;
  az érintetlen tárgyak többcellás lábnyomát is figyelembe veszi.
- A meglévő `Move`, `Trade`, `Consume` tranzakciókat használja. Nincs
  sémamódosítás és nincs új adatbázis-író.

**Ellenőrzés:**
- `PersistentItemTests`: teljes és részleges eldobás, saját és idegen
  felvétel, azonosság, bónuszok és foglalatok, lejárat, tilalmak,
  hiányzó tároló/azonosító, hibás kerítés és teljes lábnyomos ütközés.
- `item_persistence_mariadb`: valódi SQL-írás és visszatöltés; két tulajdonos
  közötti fegyver- és ékszerátadás; részhalom és lejárat; idempotens nyugta;
  elavult felvétel nem módosít semmit.

### C7d-2 — futó szerver és Unity együtt (kész)

A szerver- és kliensbekötés együtt készült, Unity `6000.3.27f1` alatt,
a `/main/codex-mission-engine` ágon. A teljes helyi próba és a natív/SQL
regressziók eredménye a [jelentésben](../reports/item_combat/2026-11-01_unity_item_runtime_integration.md).

- **Előzetes kiépítés:** a `003_owned_ground.sql` és a próbapad kiépítője
  külön, fenntartott `TradeEscrow` tárolót hoz létre. A futó tárgyíró továbbra
  sem kap tárolólétrehozási jogosultságot. Ez a tároló nem inventoryoldal.
- **Foglalás és publikálás:** az elhelyezhető földi entitás a mentés előtt
  elkészül, de `TypedLootView` és látólista-kapcsolatok nélkül. Csak igazolt
  írás után válik láthatóvá. Elutasításkor a foglalás megszűnik; ismeretlen
  COMMIT-nál a nyugta rendezéséig megmarad.
- **Atomi nyilvántartás:** a példány és az `item_ground_entry` hivatkozás
  ugyanabban az SQL-tranzakcióban változik. A core és indulási generáció
  az ellenőrzés és a nyugtahash része.
- **Felvétel:** a forrás aktuális munkamenetével és a felvevő kerítésével
  készül. Saját felvételnél egy kerítés van. A helyi online állapotok frissülnek;
  másik core-on lévő tulajdonost generáció-/revízióellenőrzött
  `GG_TYPED_ITEM_REFRESH` (45) értesítés frissít. Későbbi értesítést egy
  folyamatban lévő művelet sem veszít el. Offline forrás is támogatott.
- **Élettartam:** öt perc, saját eldobásnál 60 másodperces jog nélkül.
  Lejáratkor és bezárt térképnél igazolt SQL-megszüntetés után tűnik el.
  Induláskor a core a saját korábbi indulásainak foglalásait takarítja,
  visszaspawnolás és más core érintése nélkül.
- **Kliens:** `CG_TYPED_ITEM_DROP` (228), 64 bites példányazonosító és
  32 bites darabszám; megerősítő ablak és részhalom-választás. A meglévő
  `GC_TYPED_GROUND_ADD` és pickup út használható. A teljes példányállapot
  a szerveren marad; nincs `ItemGrant`-ból újragenerálás vagy összevonás.

## C7a — futásidejű azonosítók és tárgyadás

**Azonosító-kiosztás.**
- **Tábla:** `player.item_instance_id_allocator` (migráció:
  `db/migrations/item_instance/002_instance_id_allocator.sql`), egyetlen
  számlálóval.
  - Az alsó határ 2^32, így a futás közben kiosztott azonosítók nem
    ütközhetnek a konverzióval áthozott régi 32 bites tárgyazonosítókkal.
  - A migráció a már kiadott legnagyobb azonosító fölé állítja.
- **`ReserveItemInstanceIds`:** egy tranzakcióban (`FOR UPDATE`) foglal le
  egy blokkot. Az el nem használt azonosítókat senki más nem kapja meg.
  Elutasítja az üres, a túl nagy (1 000 000 feletti) és a túlcsorduló kérést.
- **Ki használja:** minden író, amely tárgyat hoz létre. A `MAX(id)+1` egy futó
  játékszerver mellett soha nem helyes. A próbapad is átállt, a régi
  `0x00BE…` saját tartománya megszűnt.

**Tárgytároló** (`GameServer/item/ItemStore.*`).
- **Első blokk:** indításkor 4096 azonosítót foglal le.
  - Ha a tábla hiányzik, a szerver ettől még elindul: a meglévő tárgyakkal
    minden működik, csak új tárgy nem jön létre. Ezt naplózza.
- **Kiadás:** a játékszál várakozás nélkül veszi az azonosítókat
  (`PeekInstanceIds`, `SpendInstanceIds`). Az elköltöttek a commit
  sorba állítása előtt kikerülnek a készletből, így egy másik adás sem
  kaphatja meg őket.
- **Utántöltés:** a negyed alá csökkent készletet a tárolószál a háttérben
  egy új blokkal tölti fel.

**`MakeGrantCommit`** (`PersistentItems`). Egy tárgyállapot (kulcs, szint,
darab) új egységei a tulajdonos inventoryjába kerülnek.
1. A halmozható egységek előbb a tulajdonos egyező halmait töltik fel. Egyező
   az a halom, amelynek azonos a kulcsa, a szintje és a kötése, és nincs
   példányállapota. A sorrend tároló, majd cella szerint halad.
   A feltöltés helyet hagy a maradék új halmainak is a 32 tételes
   tranzakcióban. Ha így nem fér el egyben, helyhiánnyal tér vissza, ezért a
   drop a földre kerül, nem vész el érvénytelen műveletként.
2. A maradék új tárgyként az első szabad cellákba kerül. Halmonként egy-egy
   azonosítót kap, a kapott lista elejéről.
3. **Mindent vagy semmit:**
   - `InvalidPlacement`, ha nem fér el;
   - `LimitExceeded`, ha kevés az azonosító;
   - a már használt azonosító `InvalidChange`.
4. Az `OnAcquire` kötésű tárgy a tulajdonoshoz kötődik.
5. A foglalatprofilos ékszer üres `AccessorySocketState` állapotot kap.

**C7 regressziós ellenőrzések (2026-11-01):**
- `TypedItemGrantTests`: a tényleges szerveroldali visszahívás sikeres és
  sikertelen mentése kilépés után, illetve sikeres mentés új belépés mellett.
- `PersistentItemTests`: foglalatos ékszer létrehozása; 33 részleges halom
  feltöltése a 32 tételes korláton belül; tele inventorynál földi visszaesés.
- `item_persistence_mariadb`: folyamatban lévő nyugtát nem jelent hiányzónak;
  visszagörgetés után a hiány igazolható.
- `item_catalog_runtime_mariadb`: a proxy elveszíti egy valódi tárgyadás
  COMMIT-válaszát és az első nyugtaellenőrzés válaszát. Az eredmény késleltetett
  ellenőrzéssel, egyszer érkezik meg, egyetlen tárggyal és revíziónöveléssel.

**Szerver.**
- **`RequestTypedItemGrant`:** a jövőbeli drop, bolt és jutalom útja. A
  következő okokkal utasít el:
  - nincs bekapcsolva;
  - foglalt;
  - érvénytelen;
  - nincs hely;
  - nincs azonosító.

  Elutasításkor semmi nem kerül sorba.
- **GM-tesztparancs:**
  `/typed_give <kategória> <helyi azonosító> [darab] [szint]`
  (`GM_HIGH_WIZARD`, mint a régi `/item`).

**Próbapad.**
- **Telepítés:** a 001 után a 002 is lefut.
- **Frissítés:** egy korábban telepített csomagban (a 9 régi tábla megvan, a
  kiosztó nincs) az „előkészítés” csak a 002-t futtatja, előtte a `player`
  adatbázisról mentés készül.
- **Csomagolás:** a `build_manager` mindkét fájlt becsomagolja.

**Ellenőrzés.**
- `PersistentItemTests`: 152 ellenőrzés (+14). Lefedi:
  - a halom feltöltését és túlcsordulását;
  - az új halmokat az első szabad cellákban;
  - a szintet;
  - a kevés azonosítót, a helyhiányt, a hiányzó inventoryt, a nulla darabot,
    az ismeretlen tárgyat és a foglalt azonosítót;
  - az `OnAcquire` kötést.
- `item_persistence_mariadb`: diszjunkt blokkok egy és két kapcsolaton, 2^32
  feletti kezdet, üres és túl nagy blokk, nincs átfordulás.
- `item_catalog_runtime_mariadb`:
  - teljes blokk induláskor;
  - adás a tárolón át a kiosztott azonosítókkal;
  - háttérbeli utántöltés új blokkal.
- `ItemBenchToolTests`:
  - a próbapad azonosítói a kiosztóból jönnek;
  - egy 002 nélküli adatbázis frissítése a már kiadott legnagyobb azonosító
    fölé állítja a számlálót.
