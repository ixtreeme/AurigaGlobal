# Itemkatalógus — fogyasztható tárgyak gyógyítási profiljai

2026-10-27. Ág: `codex/mission-engine`; HEAD: `ac4a7cf3`.
**Az izolált új katalógus öt kategóriát kezel. A fogyasztható kategóriában
egyelőre a gyógyító italok profiljai és az azonnali használat tiszta terve kész.**

Elkészült a külön `ItemConsumableRow`, stack- és None-refinement szabályokkal.
Nem jön létre szintenként új proto; a meglévő fegyver/páncél/ékszer fejlesztési
maximumai és +9 feletti próbái változatlanul sikeresek.

Három explicit gyógyítási profil van: azonnali, tartalékból adagolt és
időszakos. Külön cooldown, anyagdarabszám és környezeti/PvP-használati szabály
tartozik hozzájuk. A hat új SQL-táblával a friss fejlesztési séma 27 tényleges
`protos` tábladefiníciót tartalmaz. A saját audit fixture-táblák ebbe nem számítanak.

Az azonnali használati terv a szerveroldali állapotból számol, ellenőrzi a
szint/osztály/nem/combat/kontextus/PvP/cooldown/darabszám feltételeket és a
halott vagy hibás karakterállapotot. A flat potion bonus és max-pool százalék
külön számolódik, egész aritmetikával. A gyógyítás nem lépi túl a hiányzó
HP/SP-t; hatástalan használat nem ad fogyasztási tervet. A terv nem ír valódi
HP-t, inventoryt vagy adatbázist, és nem indít éles timert.

A teljes forrásaudit, új képlet, tartománybizonyítás és nyitott határok:
[IC-009](../../adr/item_combat_009_recovery_consumables.md).
A régi reserve ágban a SP-módosítás egy későbbi HP-ellenőrzés előtt történik;
ez részleges módosítás kockázata. Ezt forrásból rögzítettük, élő reprodukció
és régi runtime-fix ebben a körben nem történt. Az új végrehajtó közös commitot
kell használjon, minden ellenőrzés után.

## Ellenőrzés

| Helyi célzott ellenőrzés | Eredmény |
|---|---:|
| Normál CTest | 10/10 |
| ASAN build CTest | 9/9 |
| Külön standalone build CTest | 9/9 |
| Saját MariaDB 11.4.13 tesztesetei a normál körben | 18/18 |
| Fogyasztható profil/használati terv C++ ellenőrzések | 118 |
| Katalógus / refinement / bonus / foglalat C++ ellenőrzések | 179 / 457 / 229 / 208 |
| ItemKey C++ ellenőrzések | 197442 |

A próbák lefedik a hiányzó/dupla/árva típusos rekordot, rossz profilt, korábbi
séma elutasítását, minden új határt, a gyógyítás túlcsordulásmentes maximumát,
negatív potion bonust, külön kerekítést és közös cooldown referenciaadatát.
Reserve és periodic nem futtatható azonnali italként. A schema/hash v5 minden
új profilmezőt tartalmaz; független Python encoder és rendezési/módosítási
próbák ellenőrzik. SQL oldalon típusos child FK, numerikus CHECK-ek, read-only
jogosultság és tranzakciós rollback is futott.

Naplók: `build/item-consumables-20261027/`. A jelentés melletti
`2026-10-27_recovery_consumables.evidence.json` tartalmazza a forráshasheket,
naplókat, SQL-környezetet és az izolált buildfüggőségek ellenőrzését. A natív
C++ célok ASAN-instrumentáltak; Pythonra és MariaDB-re ez nem állítás.
A korábbi leletek történeti pillanatképek maradnak.

## Folytatás

Hátravan a tartalékos és időszakos gyógyítás futási állapota/időzítése,
a tartós használati tranzakció, valamint az egyéb fogyasztható tárgyak
(buff, blend, tisztítás, automata palack stb.) tényleges típusos hatása.
Ezután a többi kategória, SQL loader, példánymentés/fejlesztés, offline
összevonás és runtime/Unity bekötés követi az
[elfogadott prioritást](../../adr/item_combat_006_category_refinement_priority.md).
Teljes migrációt vagy jóváhagyott balance-t ez a részszállítás nem jelent.

Csak helyi forrás/build és saját ideiglenes SQL-példány változott. Meglévő
játékadatbázis, futó szerver, Unity és távoli gép módosítása nem történt.
