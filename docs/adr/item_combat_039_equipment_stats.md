# ADR IC-039 — Új felszerelésből karakterstat és harci bemenet (C11)

2026-11-01. Az [IC-012](item_combat_012_item_runtime_first.md) szerint a
meglévő harci képletek kapják az új katalógus statjait. Ez a szelet nem az
új determinisztikus harci kernel kiadása.

## Forrás és élettartam

A tiszta `BuildEquipmentStats` a karakter tartós `Equipment` elhelyezésű
példányait ellenőrzi és slot szerint rendezi. Összegzi a kategória alapértékeit,
a finomítás halmozódó nyers növekményeit, a tárgy alap- és aktuális finomítási
bónuszprofilját, példánybónuszait, foglalt köveit és ellenállásprofilját.
Üres/törött kő és inventoryban hordott tárgy nem ad felszerelésstatot.
A flat bónuszok egyszer kerülnek az összesített nyers alapértékekbe.

A GameServer `TypedEquipmentProjection` komponense származtatott cache.
Forrása a commitált `TypedItems` snapshot és tulajdonosi kerítése; nem második
mentett inventory. Valódi felszerelésváltozás új statrevíziót és teljes
karakter-újraszámolást okoz. Puszta táskamozgatás csak a kerítést frissíti.
A késleltetett tulajdonosfrissítés is a következő tickben érvényesül.

A kiépített karakter felszerelésstatja és fegyvertípusa kizárólag ezen az
úton érkezik. Üres typed felszerelésnél sem tér vissza a régi viselt tárgyakhoz.
A régi inventory, öv és mount-inventory tárgybónuszai nem adódnak hozzá.
Új legacy felszerelés felvétele ennél a karakternél elutasított.
Belépéskor a tulajdonosi mód tisztázásáig a támadás tiltott; betöltési hiba
lezárja a munkamenetet. A még ki nem épített helyi karakterek régi működése
megmarad. Ez fejlesztési bekötés, nem éles, vegyes kiadás engedélyezése.

## Stat és jelenlegi harc

A `GetReal` és a mentett alappontok változatlan források. A publikus statolvasás
adja hozzá az új gear értékeit. A buffmódosítás a szorzó előtti forrást változtatja,
így ismételt újraszámolás és bufflevétel nem halmozza a felszerelés szorzóit.
A CON/INT a HP/SP alapképletében, a többi primer stat a jelenlegi csatastatban
is megjelenik. A későn betöltött védelmi bónusz a képlet mágikus részét is
azonnal frissíti.

A fizikai/mágikus támadás és védelem, sebességek, fegyvertípus, skillpolinom
fegyverbemenete, penetráció, normál/skill és PvE/PvP módosítók, csatorna- és
fegyverellenállás, kritikus esély/ellenállás/többlet, fizikai block/evasion
csökkentés, globális sebzés és tapasztalatszorzó a jelenlegi képletekhez kapcsolódik.
A kritikus alapdobás és a többi régi harci szakasz megmarad; ez nem a későbbi
crit-before-defense pipeline megvalósítása. A mágikus sebzésből az új gear
mágikus védelme levonódik; a többi régi mágikus formula nem cserélődik le.

A domain előjeles egész aritmetikát és ellenőrzött, legközelebbi egészre kerekítést
használ, félértéknél nullától kifelé. A nyers skála 1000, a százalék 10000 bp.
A jelenlegi whole-point/int32 interfész határán explicit számtartomány-korlát van;
ez nem új balance cap. A régi sebzésképlet lebegőpontos támadási ratingje és
csonkolása megmarad, regressziós teszt ellenőrzi. A támadás időzítése továbbra is
az animációt és támadási sebességet használja; az interval/windup mezők nem élednek fel.

## Unity és packet

Owner-only **GC_TYPED_EQUIPMENT = 231, v1**. Nem új klienskérés; a C10
CG231 craftingkérésével eltérő irányban használja ugyanazt a számot.
Little-endian mezők, padding nélkül:

| Mező | Típus |
| --- | --- |
| header, teljes méret, verzió, státusz | u8, u32, u8, u8 |
| VID, statrevízió, tulajdonosi revízió | u32, u64, u64 |
| fegyvertípus; 0 üres, 1–7 typed subtype | u8 |
| fizikai/mágikus támadás és védelem nyers gearösszeg | 4 × i64 |
| STR, DEX, CON, INT, fizikai/mágikus ATT/DEF, attack/move/cast speed, max HP/SP | 13 × i64 |
| nem nulla bónuszok száma, majd ID/érték | u8, N × (u32, i64) |
| ellenállásprofil a `Values()` sorrendjében | 15 × i64 |

Méret: 285–693 bájt. Státusz 0 kész, 1 nem támogatott; utóbbinál minden
gear-hozzájárulás és fegyvertípus nulla. A sebességek a jelenlegi engine
`GetLimitPoint` eredményei. A dekóder szigorúan ellenőrzi a méretet, verziót,
azonosítókat, enumot, bónuszokat és ponttartományt.

**I → Harci statok** mutatja a szerver tényleges karakterértékeit és külön a
gear összegét, bónuszait, ellenállásait. A karakterablak is teljes pontfrissítést
kap. A helyi fegyveralkalmasság és mozgásmód a typed subtype-ot követi.
Az explicit `model_asset` egy `Metin2EquipmentDefinitionAsset` Addressables-címe;
nem ItemKey → vnum leképezés. Hiányzó grafika mellett sem kerül vissza a régi
fegyvermodell vagy fegyvertípus. A karakterváltás törli a snapshotot és az ablakot.

## Maradó határok

- Aktív ékszerfokozat és fogyó lőszert igénylő fegyver felvétele elutasított,
  amíg az időzítő, illetve typed lőszer-életciklus nincs bekötve. Már betöltött
  nem támogatott felszerelés levehető; addig nem ad részleges, félrevezető statot.
- A régi hatótáv és animációs ütközés marad. Teljes elemi skillmetadata,
  wind/earth/dark csatornák, új determinisztikus pipeline és procok külön harci munka.
- A távoli avatarok typed modellpublikálása és az összes osztály teljes
  grafikai tartalomkonverziója még nyitott. A helyi modellcímekhez az adott
  karakterrel kompatibilis assetet kell szerkeszteni; a demovért férfi harcosra szűkített.
- Nincs új SQL-séma, runtime vnumadapter, kompatibilitási item-proto vagy dual write.
  A teljes legacy tartalom és játékosállomány konverziója továbbra is külön kapu.

Ellenőrzések és helyi próba:
[C11 jelentés](../reports/item_combat/2026-11-01_equipment_stats_runtime.md).
