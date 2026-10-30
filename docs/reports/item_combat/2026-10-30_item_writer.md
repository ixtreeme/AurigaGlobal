# F4.4a — A GameServer tárgyírója

2026-10-30. Ág: `ccr-6afcbd39-c7bgeq`, kiinduló HEAD `c582f98`.
Döntés: [IC-034](../../adr/item_combat_034_gameserver_runtime.md), „F4.4a döntés”.

## Elkészült

- **Tartomány** (`common/item/InventoryOperations.hpp`,
  `ItemDomain/InventoryOperations.cpp`): mozgatás/csere, halmozás, bontás,
  megsemmisítés és fogyasztás írási halmazként.
- **Mentési réteg** (`ItemPersistenceSql`):
  - példányazonosító-blokk foglalása;
  - elveszett commit-visszaigazolás egyeztetése zároló olvasással.
- **Séma:** `db/migrations/item_instance/002_instance_id_allocator.sql`
  (új számlálótábla; a meglévők nem változnak).
- **GameServer:**
  - `ecs::item::Apply`: elmentett írási halmaz alkalmazása az ECS-re, mindent
    vagy semmit;
  - `item/ItemWriter`: tulajdonosonként sorba rendezett aszinkron író;
  - a végrehajtó általános `Pump`-ot kapott.
- **Tesztek:**
  - `InventoryOperationTests` (CTest `inventory_operations`);
  - `ItemWriterTests` (CTest `item_writer`) és MariaDB-próba
    `ItemWriterTests.py` (CTest `item_writer_mariadb`, Windowson);
  - `TypedItemTests` az `Apply`-jal bővült.
  - A munkamenet- és az írótesztek közös segédje:
    `tests/server/ItemSessionTestSupport.hpp`.

## Ellenőrzés

Linuxon (g++ 14, MariaDB 10.11), az F4.1-nél leírt helyi pótlásokkal.

| Ellenőrzés | Eredmény |
|---|---:|
| GameServer, Database build | sikeres |
| `InventoryOperationTests` | 44 ellenőrzés |
| `TypedItemTests` (az `Apply`-jal) | 65 ellenőrzés |
| `ItemWriterTests --completion-only` | 60 ellenőrzés |
| `ItemWriterTests` saját MariaDB-ben | 126 ellenőrzés |
| A fentiek AddressSanitizerrel és UB-sanitizerrel | hiba nélkül |
| Regresszió: `ItemPersistenceSqlTests` MariaDB-n / completion | 947 / 13 |
| Regresszió: `ItemOwnerSessionTests` MariaDB-n / completion | 124 / 66 |
| Regresszió: `ItemCatalogRuntimeTests` MariaDB-n, `PersistentItemTests` | 88, 86 |

A MariaDB-próbák ellenőrzésszáma mostantól futásonként azonos. A várakozó
ciklusok körei már nem számítanak ellenőrzésnek. Ezért mutat a
munkamenet-próba 124-et a korábban jelentett 128/132 helyett.

**Kapu (F4.4): elveszett visszaigazolás és összeomlás után sincs dupla írás
vagy elveszett tárgy.** Valódi MariaDB-n:
- **Valódi műveletek:** mozgatás, csere, bontás, halmozás, felszerelés. Minden
  lépés után az ECS megegyezik egy friss adatbázis-betöltéssel, és a
  konzisztencia-ellenőrzés is átmegy. A szabálytalan felszerelés el sem jut
  SQL-ig.
- **Azonosítók:**
  - az új halom a 2000-es azonosítót kapja a számlálóból;
  - egy 64-es blokk foglalódik, egyszer;
  - egymást követő, át nem fedő tartományok;
  - 64 bites túlcsordulásnál `LimitExceeded`, és a számláló nem mozdul.
- **Elveszett visszaigazolás, valódi commit után:**
  - az írás ténylegesen lefut, az író mégis `CommitUnknown`-t kap;
  - az egyeztetés megtalálja a nyugtát → `Committed`;
  - az ECS egyszer változik; pontosan egy új nyugta keletkezik.
- **Elveszett visszaigazolás, ha az írás el sem ért a szerverhez:**
  - nincs nyugta → `NotCommitted`;
  - az ECS és az adatbázis változatlan; az írás nem megy ki újra;
  - a művelet utána kérésre normálisan lefut.
- **Összeomlás:** az A core lezárás nélkül eltűnik. A B core átveszi, és
  pontosan A utolsó állapotát tölti be; utána ír. A sírkövek megmaradnak.

**Állapotgép (adatbázis nélkül):**
- **Sorrend:**
  - tulajdonosonként egyszerre egy SQL-írás;
  - a második szándék az első elmentett állapotából épül;
  - az ECS a commit előtt nem változik.
- **Elutasítás SQL nélkül:** ismeretlen tárgy, nem halmozható pár.
- **Visszagörgetett írás:** semmi nem változik, az írási jog felszabadul.
- **Bontás:** előbb azonosítóblokkot foglal; a második bontás ugyanabból a
  blokkból vesz.
- **Elveszett visszaigazolás:**
  - a nyugta dönt;
  - az egyeztetés pontosan ennek a commitnak a zároló olvasása;
  - nyugta nélkül nincs újraküldés;
  - eltérő nyugtánál `Diverged`;
  - háromszori SQL-hibánál `Unresolved`.

  Az utóbbi kettőnél a munkamenet újratöltést kér.
- **Kerítésütközés:** `Superseded`, a sor többi eleme is; új kérés nem
  fogadható.
- **Zárás:** a várakozók `Cancelled`; az SQL-ben lévő írás befejeződik; a
  munkamenet csak utána zárul.
- **Az író háta mögött módosított ECS:** `Diverged`, újratöltés.
- **Sikertelen azonosítófoglalás:** csak a bontás bukik el.

**Tiszta műveletek (`InventoryOperationTests`):**
- **Mozgatás:** szabad cellára; helyben maradó mozgatás; ismeretlen tárgy és
  tároló; rácson kívül; az oldal aljáról kilóg.
- **Csere:** két halom; magas tárgy egy cellással; nem horgonyra ejtés; két
  tárgyat fedő cél.
- **Felszerelés:** fegyver a fegyverhelyre; rossz hely; levétel ékszerállapottal.
- **OnEquip kötés:** felszereléskor köt, levételkor megmarad.
- **Halmozás:** rész- és teljes halmozás sírkővel; túl sok vagy nulla darab;
  önmagába; kötött és kötetlen halom; eltérő típus; halommaximum (199+1
  elfogadva, 199+2 elutasítva).
- **Bontás:** az egészet nem bontja; nullát nem bont; foglalt cellára,
  meglévő azonosítóval és ismeretlen tárolóba nem.
- **Megsemmisítés:** a teljes utolsó állapot megmarad.
- **Fogyasztás:** egy egység; az utolsó egységek sírkövet hagynak; rossz
  darabszám elutasítva.
- **A nézet határai:** idegen és sírkő tárgy nincs a nézetben; nulla
  műveletazonosító elutasítva.

## Ami nem futott

- MSVC build és a Windowsos MariaDB-tesztmotor (`ItemWriterTests.py`).
- Valódi hálózati kiesés a COMMIT közben. Szimulált, de valódi SQL-lel: az
  írás tényleg lefut vagy tényleg elmarad, csak a visszaigazolás vész el.
- Az egyeztetés várakozása egy még futó eredeti tranzakcióra külön nem mért.
  A zárolási mintát az F4.2 foglalási próbája már igazolta.

## Határ

- Az élő GameServer még nem hívja; a régi tárgyút változatlan (F4.7).
- A szerverkezelő telepítője még csak a 001-es sémát telepíti; a 002 az F4.7
  aktiválás része.
- Fejlesztés, bónusz- és foglalatszolgáltatás: F4.4b.
- Több tulajdonost érintő írás (trade): F4.6.

## Következő kapu

F4.4b: fejlesztés, bónusz- és foglalatszolgáltatás ugyanezen az írón, a
játékszabály szerinti dobással és költséggel.
