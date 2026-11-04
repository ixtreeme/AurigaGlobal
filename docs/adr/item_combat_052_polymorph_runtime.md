# ADR IC-052 — Átváltoztató tárgyak a szerveren és Unityben

2026-11-04, C24. Alapmodell: [IC-024](item_combat_024_polymorph_items.md).
Előzmény: [IC-051](item_combat_051_book_aids_runtime.md).

## Használat és példányállapot

A 13-as kategória gömbje és gyakorlókönyve saját szerveres előnézetet és
külön megerősítést kapott. Az alakot a katalógus rögzíti, vagy a példány
`boundForm` mezője hordozza. A kliens nem választhat tetszőleges mobot.
Csak létező, valódi szörny, a megjelenítési protokollban elférő race engedett;
NPC, kő és ismeretlen mob nem használható. Az előnézet mobkatalógus-revízióhoz,
forrásállapothoz, karakter-, skill- és masteryértékhez kötött.

A gömb sikeres használata egy darabot fogyaszt. A könyv sikertelen használata
nem fogyaszt és nem csökkenti a gyakorlatot; siker esetén egy gyakorlás teljesül.
A kész könyv külön előnézettel fejleszthető. A fejlesztés megtartja a példányt,
emeli a fokozatot, és a profil szerint új gyakorlási követelményt állít be.
A már aktív átváltozás, tiltott élet-/harci állapot, lovaglás és az aktuális
szerveres szabályok elutasíthatják a műveletet.

A `008_polymorph.sql` három nullable mezőt ad az itempéldányhoz: forma,
könyvfokozat, hátralévő gyakorlat. Az új payload része a validációnak,
hashnek, teljes előállapot-CAS-nak, grantnak és halomegyesítésnek.
A payload nélküli régi példányok hash-e változatlan. A katalógus v26 marad.

## Tartós hatás és valódi harci állapot

Egy ItemCommit menti a forrás fogyását vagy könyvállapotát, az owner-fence-et,
a recovery-sidecart és a receiptet. Az ismételt megerősítés nem fogyaszt újra.
Az aktív átváltozás opcionális RCS3 kiterjesztés; átváltozás nélkül az RCS1/RCS2
formátum byteazonos marad. Az alak és a külön bónusz abszolút UTC-határidőt kap.
Újrabelépés nem indítja újra az időt; az offline lejárt alak nem tér vissza.
Halál, szabályváltozás, megerősített megszüntetés és kényszerített affecttörlés
a közös időzített állapot útján törli a hatást.

A szerver tényleges race-et és affectflaget állít, újraküldi az entitást és
a pontokat. A mob ereje pontos bázisponttal számolódik az alapstatokban és
a közelharci sebzésben. Az attack/defense/movement bónuszok a típusos
felszerelés módosításaival együtt érvényesülnek, felhalmozódás nélkül.
A könyvbónusz lejárhat úgy, hogy az alak még aktív. Visszaváltozáskor az
eredeti race, felszerelési értékek és fegyverkorlátozások állnak vissza.

A típusos owner legacy átváltoztató tárgyhasználata és legacy affectbetöltése
nem kerülheti meg az új állapotot. A régi, még át nem állított karakterek
legacy útja megmarad.

## Protokoll és Unity

CG/GC247 v1 külön munkamenet: használat, fejlesztés, megszüntetés előnézete és
megerősítése, aktuális állapot, lezárás. GC225 v5 csak polymorph-payloadnál
szükséges; a korábbi v1–v4 továbbra is olvasható. GC227 v2 új Polymorph actiont
ad a 13-as kategóriának. A natív golden fájlok a Unity-validátorok bemenetei.

Unity: **külön típusos leltár → tárgy → Átváltoztat → előnézet →
Művelet megerősítése**. A felső **Átváltozás** gomb forrás nélkül is megnyitja
az aktív állapotot és a megszüntetést. A könyv gyakorlata/fokozata, a szerveres
esély, az alak, bónusz és határidő megjelenik. Nincs kliensoldali levonás;
az utolsó gömb után az eredmény olvasható marad. A normál legacy leltár
átállítása a felhasználó döntése szerint továbbra is vár.

Ellenőrzések és korlátok:
[C24 jelentés](../reports/item_combat/2026-11-04_polymorph_runtime.md).
