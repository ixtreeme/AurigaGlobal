# ADR IC-061 — Tartós típusos felülés és leszállás

2026-11-07, C27a. Szerver: `20261103`; Unity:
`/main/unity63-integration-20261103`, Editor `6000.3.27f1`.
Párosított Unity changeset: `cs:87`.

Az állandó típusos hátaspecsét a követő megidézése után a leltár
**Felülés** gombjával használható. A **Leszállás** visszaállítja a követőt,
a **Visszahívás** felülve is megszünteti az aktív társat. A szerver ugyanazt
a közös követőhelyet tartja fenn a pet és a hátas számára. Felülve nincs
külön követőpéldány.

## Tartós szándék és élő megjelenés

A `014_mount_riding.sql` az idézési szándék mellé külön
`companion_riding_requested` mezőt ad, nulla alapértékkel. Csak idézett
hátas kérhet felülést. Az eredeti példányazonosító, darabszám, szint,
kötés és leltárhely megmarad. Az új bit csak igaz értéknél kap `MNT1`
kiterjesztést a kanonikus digestben; a meglévő tárgyak hash-e változatlan.

A felülés és leszállás tulajdonosi munkamenettel és revízióval védett,
nyugtázott SQL-tranzakció. A `CMP` v1 a meglevő idézés/visszahívás,
a v2 a felülés/leszállás pontos átmenetét ellenőrzi. Általános
tárgyművelet nem módosíthatja ezeket a biteket. Ismételt azonos szándék
nem ír új revíziót és nem hoz létre új szereplőt.

Az SQL-ből olvasott fázis továbbra is nyugalmi: az élő követőt vagy
felült karaktert az ECS-vetület bizonyítja. Mentés után a szerver eltünteti
a követőt és a katalógus külön `ridingForm` alakját állítja a karakterre,
a rendes karakterbeszúrási csomaggal a tulajdonosnak és megfigyelőinek.
Leszálláskor a karakterről lekerül az alak, majd a követő visszatér.

Újrabelépéskor az idézési és felülési szándék a karakter, affectek és világ
betöltése után áll vissza. Tiltott világállapot, aréna vagy karavánzár
nem hoz létre fiktív élő hátast. A függő szándék a leltárból törölhető.
A saját vetület csak a maga által felrakott hátasalakot veheti le;
egy közben megjelenő másik mountot nem ír felül.

## Protokoll és Unity

A CG249 marad 10 bájtos törzs és egy szekvenciabájt. A két új akció
`3=Mount`, `4=Dismount`. A Unity a tárgypéldány azonosítóját küldi.
A felülés csak tényleges követőállapotban, aktuális katalógusengedéllyel
elérhető. A leszállás és visszahívás a függő állapotot is feloldja.

A GC225 v11 a felülést kérő hátasnál `5=Riding` és
`6=RidingPending` állapotot visz; a pet nem állíthatja ezeket. A v11 a
v10 petellátmány-oldalmezőjét is megtartja. A korábbi v1–v10
pillanatképek továbbra is olvashatók; felülés nélküli snapshot a korábbi
szükséges verzióját használja.

## Hatókör

Ez a C27a az állandó hátaspecsét felülési útja. Az időkorlátos fix társak,
fix társak statbónuszai és a típusos repülési jogosultság a következő
csomag. A meglévő repülési engedély továbbra is a legacy felvett
pecsétből származik. A normál legacy leltár teljes átállítása külön munka.

Későbbi kiegészítés: az említett következő csomag a
[C27b-ban](item_combat_062_fixed_companion_clocks_bonuses_flight.md)
elkészült, időkorláttal, bónuszokkal és a típusos hátas saját repülési
engedélyével. A fenti hatókör a C27a történeti állapotát írja le.

Az ellenőrzések a [C27a jelentésben](../reports/item_combat/2026-11-07_typed_mount_riding.md),
a tágabb fennmaradó lista a [commit- és kódauditban](../reports/item_combat/2026-11-07_item_remaining_work.md).
