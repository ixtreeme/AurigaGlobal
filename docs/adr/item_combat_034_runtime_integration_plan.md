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

## Határ

- **Helyi csomag:** még nincs bekapcsolva. A bekapcsoláshoz telepíteni kell
  az új GameServert, és a `game_defaults.items.catalog` értékét
  `"required"`-re kell állítani.
- **FreeBSD:** a GameServer mostantól linkeli az `AurigaItemCatalogSql`
  könyvtárat. A FreeBSD-build ezzel még nincs kipróbálva (F).
