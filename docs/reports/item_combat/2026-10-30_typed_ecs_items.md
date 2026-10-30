# F4.3 — Típusos tárgypéldány az ECS-ben

2026-10-30. Ág: `ccr-6afcbd39-c7bgeq`, kiinduló HEAD `dfea479`.
Döntés: [IC-034](../../adr/item_combat_034_gameserver_runtime.md), „F4.3 döntés”.

## Elkészült

- `ecs/components/typed_item_components.hpp`: típusos komponensek.
  - Tárgyanként: azonosító + ItemKey, szint, revízió, darabszám, tartós
    tulajdonos, kötés, hely, bónuszok, kövek, ékszerfoglalat.
  - Tulajdonosonként: kerítés, pénztárcák, tárolórácsok, példányindex.
  - Nincs vnum, proto- vagy katalóguspointer.
- `ecs/TypedItems.hpp/.cpp`: `Materialize` (mindent vagy semmit), `Project`,
  `ProjectItem`, `Verify`, `Release`, `FindOwner`, `ItemAt`.
- `TypedItemTests` (CTest `typed_items`). A `ItemOwnerSessionTests`
  MariaDB-próbája az adatbázisból betöltött tulajdonost is átvezeti az ECS-en.

## Ellenőrzés

Linuxon (g++ 14, MariaDB 10.11), az F4.1-nél leírt helyi pótlásokkal.

| Ellenőrzés | Eredmény |
|---|---:|
| GameServer build | sikeres |
| `TypedItemTests` | 56 ellenőrzés |
| `ItemOwnerSessionTests --completion-only` | 66 ellenőrzés |
| `ItemOwnerSessionTests` saját MariaDB-ben (az ECS-átvezetéssel) | 132 ellenőrzés |
| Mindhárom AddressSanitizerrel és UB-sanitizerrel | hiba nélkül |

**Kapu (F4.3): betöltés → ECS → `PersistentItem` azonos.**

Valódi MariaDB-n, két helyen (B core átvétele és a gyors újrabelépés után):
- +9-es kard bónuszokkal és három foglalattal (foglalt, törött, üres);
- 3 darab bőr;
- időzített ékszer a csukló felszerelési helyén.

A kerítés, a tárolók, a pénztárca és mind a három tárgy minden mezőre azonos.

Adatbázis nélkül:
- négy tárgy, köztük kötött tárgy, eltérő revízióval;
- a tárolók szándékosan nem kanonikus sorrendben;
- egy második (fiók-) tulajdonos raktárral és DragonCoin pénztárcával.

**Lábnyom:**
- a 3 cella magas kard a 0., 5. és 10. cellát foglalja;
- a felszerelés a helyének sorszámát használja;
- szabad, rácson kívüli és ismeretlen tárolóbeli cella üres.

**Elutasított bemenetek**, a registry minden esetben változatlan:
- 0-s kerítés;
- idegen, duplikált vagy fióktároló a karakteren;
- fiókpénznem, duplikált vagy idegen pénztárca;
- duplikált példány;
- idegen tárgy, sírkő, ismeretlen prototípus;
- a profilon kívüli szint;
- ismeretlen tároló, rossz felszerelési hely, rácson kívüli horgony, az oldal
  aljáról kilógó tárgy;
- két tárgy egy cellában.

**Sérülések felismerése (`Verify`):**
- a rács frissítése nélkül áthelyezett tárgy;
- a profilon kívüli szint;
- hiányzó darabszám-komponens;
- régi proto-pointer komponens;
- idegen tulajdonos-entitás.

A javítás után mindegyik újra érvényes.

**Életciklus:**
- ugyanaz a tulajdonos másodszor nem tölthető be (a registry nem változik);
- a felszabadítás csak a saját entitásait szünteti meg, a másik tulajdonost
  nem érinti;
- felszabadítás után újra betölthető.
- Ellenőrzött, hogy az entitásszámláló valóban mér (+5 egy sikeres
  betöltésnél).

## Határ

- Az élő GameServer még nem hívja (F4.7). A régi tárgyentitások és az
  inventory-tömb változatlan.
- Nincs írás (F4.4), stat (F4.5) és kliensszinkron (F5).
- MSVC build és a Windowsos MariaDB-tesztmotor nem futott.

## Következő kapu

F4.4: tartós műveletek (mozgatás, halmozás, bontás, megsemmisítés,
fejlesztés, szolgáltatások, fogyasztás). Az ECS csak sikeres commit után
változik.
