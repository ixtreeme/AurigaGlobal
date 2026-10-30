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

A helyi bekapcsoláshoz a játékfióknak (`auriga_local`) UPDATE jog kell a
`player.item_owner_fence` táblára.

## Határ

- **Helyi csomag:** még nincs bekapcsolva. A bekapcsoláshoz telepíteni kell
  az új GameServert, és a `game_defaults.items.catalog` értékét
  `"required"`-re kell állítani.
- **FreeBSD:** a GameServer mostantól linkeli az `AurigaItemCatalogSql`
  könyvtárat. A FreeBSD-build ezzel még nincs kipróbálva (F).
