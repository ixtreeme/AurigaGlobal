# ADR IC-034 — F4 szerverbekötés: ütemterv és 1. szelet (katalógus induláskor)

2026-10-29. Fázis: F4 ([IC-012](item_combat_012_item_runtime_first.md)).
Állapot: **az 1. szelet implementált és tesztelt; a további szeletek terv**.

## Hátralévő munka (Codex listája, 2026-10-29)

| Csomag | Tartalom | Fázis | Állapot |
|---|---|---|---|
| A. Speciális tárgyműveletek | Páros (kapcsolt) bónuszok; jelmez-, stóla- és rúnabónuszok; fejlesztési segédanyagok | F2 maradéka | nyitott ([IC-031](item_combat_031_instance_attributes.md) határa) |
| B. Tartós tárgymentés | Teljes mentés, visszatöltés és tranzakciókezelés a futó szerverben; a még nem támogatott kategóriák példányállapota | F3 maradéka | a modell, az író és a próbapad kész ([IC-032](item_combat_032_item_persistence.md), [IC-033](item_combat_033_item_test_bench.md)) |
| C. Szerver bekötése | Inventory, felszerelés, használat, fejlesztés, drop, bolt, trade, crafting, quest, Mission | F4 | **1. szelet kész** (lent) |
| D. Packetek és Unity | ItemKey, szint és példányadat a protokollban és a kliensben | F5 | nyitott |
| E. Teljes adatkonverzió | Régi protók, példányok, receptek, tartalmi hivatkozások | F6 | az eszközök megvannak; a családtérkép jóváhagyásra vár |
| F. Végső ellenőrzés | Helyi játékpróba, újrabelépés utáni adatmegőrzés, gazdasági műveletek, FreeBSD-build | F7 | nyitott |

## Javasolt sorrend (függőségek szerint)

1. **C1 — Katalógus induláskor.** Kész, lásd lent.
2. **B1 — A mentés helye a futó szerverben.** Döntés kell. A kérdés: a
   karaktert birtokló GameServer saját, dedikált kapcsolaton írjon (ahogy az
   IC-032 író épült), vagy a Database-folyamat gyorsítótára vigye. Ugyanitt
   kell eldönteni a munkamenet-kerítés kiosztását belépéskor.
3. **C2 — Betöltés belépéskor.** A `LoadItemOwner` futtatása, és az új,
   típusos inventory ECS-komponens feltöltése. Ez először csak olvasás, a
   régi út érintetlen marad.
4. **D1 — Minimális protokoll.** Az inventory listázása ItemKey-jel és
   szinttel. Enélkül játékban semmi nem próbálható ki.
5. **C3… — Műveletek egyenként, kapcsoló mögött.** Áthelyezés,
   felszerelés, használat, fejlesztés, drop, bolt, trade, crafting, quest,
   Mission. Minden művelet a már meglévő tiszta tervezőkre épül.
6. **A — Speciális műveletek.** Párhuzamosan haladhat, mert katalógus- és
   szerkesztőmunka, a futó szervert nem érinti.
7. **E — Konverzió.** A felhasználó által jóváhagyott családtérkép után.
8. **F — Végső ellenőrzés.** Helyi átállás a Server2-Local csomagon, és a
   FreeBSD-build.

Futásidejű vnum-adapter és kettős írás továbbra sem lehet (IC-012). Az
átállás végén a régi `item_proto` út kikerül a runtime-ból.

## 1. szelet — katalógus induláskor (C1)

- **Konfiguráció** (`server.json`, game rész), új `items` szakasz:
  - `catalog`: `"off"` (alapérték) vagy `"required"`;
  - `catalog_revision`: legalább 1, alapértéke 1;
  - `catalog_sha256`: üres vagy 64 kisbetűs hexa karakter.

  Hibás érték esetén a konfiguráció nem töltődik be, és az élő beállítás nem
  változik. A séma (`docs/schemas/server.schema.json`) és a szerverkezelő
  magyar feliratai is bővültek.
- **`required` mód** (`GameServer/item/ItemCatalogRuntime.*`):
  - A PlayerSQL után a GameServer dedikált, rövid életű kapcsolatot nyit a
    `protos` adatbázishoz, a player fiókkal.
  - Beolvassa és ellenőrzi a katalógust a játék saját katalógusépítőjével
    (`ReadCatalog`, REPEATABLE READ pillanatkép), és csak olvasható
    `shared_ptr`-ként közzéteszi.
  - Ha a katalógus hiányzik, sérült, vagy más a kiadás-hash, a szerver nem
    indul el. A hibaüzenet nem tartalmaz jelszót.
- **`off` mód:** semmi nem változik. Ez az alapérték, így a távoli gépeken
  és a protos nélküli csomagokon minden a régiben marad.
- **A tárgyműködés még nem változik.** A régi `item_proto` út marad. Ez a
  szelet csak bizonyítja induláskor, hogy a szerver futtatható katalógust
  kap.

## Ellenőrzés

- `server_config`: alapértelmezés, `required` beállítás, a négy hibás érték
  elutasítása az élő konfiguráció megváltoztatása nélkül, és visszaállás
  `off`-ra.
- `item_catalog_runtime_mariadb` saját MariaDB-n, csak SELECT jogú
  olvasóval, valódi szerkesztő-commit után (8 ellenőrzés):
  - `off` mód;
  - betöltés és közzététel;
  - eltérő kiadás-hash elutasítása, a korábbi katalógus megtartásával;
  - rossz jelszó (a jelszó nem kerül a hibaüzenetbe);
  - érvénytelen hash-rögzítés.

## B1 döntés — a futó szerver maga ír (2026-10-30)

A felhasználó döntése: a karaktert birtokló GameServer saját, dedikált
SQL-kapcsolaton írja az új tárgyakat. Minden művelet egy tranzakció (IC-032
írója). A Database-folyamat az új tárgyakban nem vesz részt. Ez megegyezik
a küldetésmotor felépítésével (`sql.mission`). A GameServer játékszála nem
vár SQL-re: egy háttérmunkás végzi a betöltést és az írást, az eredményeket
pedig a fő ciklus gyűjti be.

## 2a. szelet — belépési munkamenet és kiépítés (C2a)

`ItemPersistenceSql`:
- **`ClaimOwnerSession`:** a belépés tulajdonosa minden belépéskor új
  munkamenet-generációt kap, egy tranzakcióban. Az előző belépés késve
  érkező írását a kerítése ezután elutasítja.
  - Nem kiépített tulajdonosnál `MissingOwner` a hiba.
  - Ha a generáció kimerült, `LimitExceeded` a hiba; az érték nem fordul át.
- **`ProvisionOwner`:** a tulajdonost az importáló, a karakterlétrehozás vagy
  egy tesztszerszám építi ki, soha nem a tárgyíró.
  - Egy tranzakcióban jön létre a kerítés (1. munkamenet), a tárolók és a
    pénztárcák.
  - Meglévő tulajdonost nem módosít; ilyenkor `false` a válasz.
  - A tárgyíró fióknak nincs jogosultsága a tárolókhoz, így tulajdonost
    sem tud kitalálni.

Saját MariaDB-s teszt (`item_persistence_mariadb`, `Sessions`):
- a hibás kiépítések nem írnak semmit;
- a kiépítés atomi;
- az ismételt kiépítés nem módosít;
- az 1. munkamenet betölthető;
- az igénylés után a régi munkamenet sem betölteni, sem írni nem tud, az új
  pontosan egyszer ír;
- a generáció kimerülése után nincs átfordulás.

## 2b. szelet — tárgytároló-munkás és betöltés belépéskor (C2b)

- **`GameServer/item/ItemStore.*`:** egy háttérszál saját, közvetlen
  SQL-kapcsolattal (`sql.player` fiók). Csak `items.catalog = "required"`
  esetén indul, közvetlenül a katalógus után. Ha nem tud csatlakozni, a
  szerver nem indul el.
  - A játékszál csak feladatot ad le (`Submit`), és az eredményeket a fő
    ciklusban, az `idle()` részében alkalmazza (`Drain`). SQL-re soha nem
    vár.
  - Leálláskor (`destroy`) a munkás minden sorban álló feladatot befejez,
    írást sem hagy félbe. Az ezután visszajövő eredményeket eldobja.
- **Belépés** (`CInputDB::PlayerLoad`): a munkás egy feladatban új
  munkamenetet igényel, és betölti a karakter tárgyállapotát.
  - Ha a karakter azóta kilépett vagy újra belépett, a belépési token
    (`ecs::TypedItemLogin`) nem egyezik, és az eredmény elvész.
  - Sikeres betöltésnél az állapot az `ecs::TypedItems` komponensbe kerül,
    csak olvasható árnyékként.
  - A nem kiépített karakternél ez csak egy nyomkövetési bejegyzés; minden
    más hiba naplózódik.
  - A régi tárgyműködés változatlan.
- **Teszt** (`item_catalog_runtime_mariadb`, 20 ellenőrzés):
  - nem indul hibás jelszóval, és kétszer sem indul;
  - belépés, nem kiépített karakter és újrabelépés sorrendben fut, a 2. és
    a 3. munkamenettel;
  - az eredmény csak a hívó (játék) szálon fut;
  - leálláskor a sorban álló munka elkészül, a késői eredmény elvész;
  - leállás után nem fogad új munkát.

A játékfióknak SELECT jog kell a `protos`-ra, és UPDATE jog a
`player.item_owner_fence` táblára. A helyi `auriga_local` ezt már megkapta:
SELECT jogot a `protos`-ra, és ALL jogot a `player.*`-ra.

## D1 — protokoll: az új inventory a kliensnek (2026-10-30)

**Csomag.** `GC_TYPED_INVENTORY` (225). A fejléc 32 bites mérettel kezdődik,
ezt egy 1-es verziójú, explicit little-endian bájtformátum követi. A
formátum nem függ a C++-struktúrák elrendezésétől.
- **Kódoló/dekódoló:** `SRC/Server/common/item/TypedInventoryWire.hpp` és
  `SRC/ItemDomain/TypedInventoryWire.cpp`.
- **Tartalom:**
  - tárolók és pénztárcák;
  - egy név minden különböző ItemKey-hez: a tartalomkód, amíg nincs
    kliensoldali katalógus (F5);
  - tárgyanként: azonosító, ItemKey, szint, darab, tároló és cella, kötés,
    zárolt bónuszhely, bónuszok, a három kőfoglalat és az ékszerfoglalat.
- **Elutasítás:** hibás csomagot egészében elutasít, soha nem dolgozza fel
  részben. Ilyen a csonka, a túl hosszú, a rossz fejlécű, az ismeretlen
  verziójú, a rossz méretű és az ismeretlen jelzőbitet tartalmazó csomag.
- **Rögzített minta:** `tests/fixtures/item_combat/typed_inventory_v1.hex`.
  A szerver (`TypedInventoryWireTests`, 20 ellenőrzés) és a Unity
  (`Metin2ProtocolCodecValidator`) is ehhez a bájtsorhoz vizsgál.

**Küldés.** `GameServer/item/TypedInventorySync.*`. A szerver akkor küldi,
amikor a belépéskori betöltés elkészül, és akkor is, amikor a kliens
játékba lép. Így mindig megérkezik, bármelyik történik később. Játékfázis
előtt, és olyan karakternél, amelynek nincs új tárgya, nem küld semmit.

**Unity** (`D:\Auriga-Unity\SRC\AurigaUnity`):
- `Metin2TypedInventoryCodec.cs` (dekódoló, ugyanazokkal a szabályokkal);
- a csomagkezelő regisztrációja (225, `DynamicAt32`);
- `Metin2GameRuntime.TypedInventoryReceived`;
- `Metin2TypedInventoryPanel`: tesztnézet, Ctrl+Shift+I.

A Unity-projekt batch módban lefordul, és a `Metin2ProtocolCodecValidator`
sikeresen lefut. Ikon és honosított név a kliensoldali katalógussal jön
(F5).

**Próbapad valódi karakterrel (D1c).** Az `ItemBenchTool` kérésenként
választható karakteren dolgozik: alapértelmezésben a #990001-es
tesztkarakteren, vagy egy valódi karakter PID-jén.
- **Kiépítés:** ugyanaz a `ProvisionOwner`, amelyet az importáló és a
  karakterlétrehozás is használ.
- **Munkamenet:** minden munkaművelet új munkamenetet igényel, mint egy
  belépés. Az állapotlekérdezés csak olvas.
- **Katalóguskövetés:** a kötés csak akkor áll át, ha minden kiépített
  tulajdonos tárgyai érvényesek az új katalógussal. Visszaállításkor a
  karakter régi állapota előbb törlődik, így egy érvénytelenné vált karakter
  is újraindítható.
- **Felület:** a Tárgyteszt lapon karakterválasztó van (a ✓ jelzi a
  kiépített karaktert), és a valódi karakter kiépíthető.

## C3 — áthelyezés az új rendszeren (2026-10-30)

- **Tiszta építő:** `MakeMoveCommit` (`PersistentItems`).
  - Egy elhelyezett tárgyat visz a tulajdonos ugyanazon vagy másik
    inventory-tárolójának egy cellájára.
  - Minden célcellának szabadnak kell lennie: nincs csere, halom-összevonás
    vagy felszerelés; ezek külön szeletek.
  - **Elutasítja:** a foglalt célt, a rácson kívüli cellát, a változás nélküli
    áthelyezést, a felszerelést, az ismeretlen tárgyat vagy tárolót, az idegen
    tulajdonost és a nulla műveletazonosítót.
- **Tárgytároló:** `RequestCommit`. Egy feladatban fut a commit, majd a
  tulajdonos friss újratöltése. A játékszál így mindig a mentett, irányadó
  állapotot kapja vissza, elutasítás vagy ütközés után is.
- **Játékszál** (`GameServer/item/TypedItemOps.*`):
  - a kérést a karakter aktuális állapotából építi;
  - karakterenként egyszerre egy művelet futhat (`ecs::TypedItemBusy`);
  - az eredmény mindig egy friss `GC_TYPED_INVENTORY` pillanatkép, hiba esetén
    magyar chatüzenet is.
  - A belépési token itt is véd: egy kilépett vagy újra belépett
    karakternél az eredmény elvész.
- **Protokoll:** `CG_TYPED_ITEM_MOVE` (225), 22 bájt (méret
  `static_assert`-tel rögzítve). Mezői: tárgyazonosító, tárolófajta,
  tárolóazonosító, cella. A Unity oldalon
  `Metin2TypedInventoryCodec.WriteMove` + validátor.
- **Tesztparancs** a kliensfelület elkészültéig (F5):
  `/typed_move <honnan cella> <hová cella>` a fő inventoryban.
- **Ellenőrzés:**
  - `PersistentItemTests`: 97 ellenőrzés, ebből 11 új áthelyezéses eset.
  - `item_catalog_runtime_mariadb`: 30 ellenőrzés, ebből 10 új:
    - a commit és az újratöltés lefut;
    - a régi állapotból épített áthelyezést elutasítja, és az újratöltött
      állapot az irányadó;
    - egy korábbi belépés munkamenete sem írni, sem tölteni nem tud.

## C4 — felszerelés és levétel (2026-10-31)

**`MakeEquipCommit`**
- **Cél:** a tárgy a saját helyére kerül, a tulajdonos 1-es felszerelési
  tárolójába (8 hely: fegyver, test, fej, pajzs, láb, csukló, nyak, fül, a
  0–7. horgonyon). A helyet a fegyver-, páncél- vagy ékszersor adja.
- **Feltételek:** minimális szint, a használati szabály kaszt- és nembitjei.
  Ha nem teljesülnek: `RequirementNotMet` (új hibakód).
- **Kötés:** egy `OnEquip` tárgy ekkor kötődik a tulajdonoshoz.
- **Csere:** a helyen lévő tárgy a felszabaduló inventoryhelyre kerül, ha
  elfér ott. Ha nem fér el, az egész kérés elutasított.
- **Jelmez és más kategória:** későbbi szelet (`UnsupportedState`).

**`MakeUnequipCommit`:** a levett tárgy szabad inventory-cellákba kerül.

**Közös ellenőrzés.** Az áthelyezés, a felszerelés, a csere és a levétel
egy közös segéden (`Relocate`) megy át. Ez minden érintett tárolót az összes
változás után egészében ellenőriz, így a csere előbb felszabadítja a
cellákat, és két tárgy sosem fedi egymást.

**Szerver.**
- Ugyanaz a `CG_TYPED_ITEM_MOVE` csomag a célhely szerint választ:
  - felszerelési cél: felszerelés;
  - felszerelt forrás: levétel;
  - különben: áthelyezés.
- A karakter szintje, kasztja és neme a futó ECS-ből jön.
- **Tesztparancsok:** `/typed_equip <inventory cella>`,
  `/typed_unequip <hely 1-8> <inventory cella>`.

**Próbapad.**
- Kiépítéskor a felszerelési tárolót is létrehozza.
- A korábban kiépített karakterek a következő „létrehozás” lépésnél
  (visszaállítás nélkül) kapják meg.
- A felszerelt tárgy nem jelenik meg az inventoryrácson; helyette
  `equipped_slot` mezőt kap.

**Ellenőrzés.**
- `PersistentItemTests`: 112 ellenőrzés (+15).
- `item_catalog_runtime_mariadb`: 39 ellenőrzés (+9: felszerelés, majd
  levétel a tárolón át, újratöltéssel).

## Határ

- **Helyi csomag (2026-10-30): bekapcsolva.**
  - **Telepítés:** a GameServer, a Database, a LoginServer és a
    ServerPackageCheck a `63bb29e1` buildből került a csomagba. Mentés:
    `run/deployments/items-f4-20261030T180450-82b0395f`.
  - **Konfiguráció:** `game_defaults.items.catalog = "required"`.
  - **Adattisztítás:** az első indítás leállt, mert a csomag régi adatai a
    9dfb1def óta kivezetett DragonSoul-tárgyakra hivatkoztak. Lásd
    [IC-010](item_combat_010_dragonsoul_retirement.md) auditját.
    - Kivett hivatkozások: `data/special_item_group.json` 22 bejegyzés és
      1 csak DS-tartalmú csoport (`cor_draconis_antico`),
      `data/mob_drop_item.json` 11 bejegyzés.
    - Mentés: `run/deployments/retired-cleanup-20261030T180438-406e9b9e`.
    - A `baule_start.lua` (kezdőláda: 88961) a közzétett quest-pillanatképből
      fut, ezért változatlan maradt. Ez nem akadályozza az indítást.
  - **Próbaindítás** az 1-es csatornával, `STACK_LISTENING`-ig: mindhárom
    core betöltötte a katalógust, és elindította a tárgytárolót.
  - **A játékbeli próba lépései:**
    1. a Tárgyteszt lapon ki kell építeni egy karaktert, és tárgyat adni neki;
    2. el kell indítani a szervert;
    3. a módosított Unity-klienssel be kell lépni, majd megnyomni a
       Ctrl+Shift+I-t.
- **A műveletek még nem az új rendszeren futnak.** Az áthelyezés,
  felszerelés stb. a C3-tól kezdve kerül át.
- **FreeBSD:** a GameServer mostantól linkeli az `AurigaItemCatalogSql`
  könyvtárat. A FreeBSD-build ezzel még nincs kipróbálva (F).
