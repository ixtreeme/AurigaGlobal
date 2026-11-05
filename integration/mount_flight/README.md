# Hátasrepülés

A felvett hátaspecsét engedélyezi a repülést, ha `item_proto.value5 = 1`, és a felszerelt pecsét race-e egyezik a tényleges hátassal. Az [eredeti prototípusok](../tera_companions/prototypes.sql) után futtatandó `flight-capability.sql` kizárólag a `flying-mounts.json` 84, eredeti repülőanimációval rendelkező hátasát jelöli meg.

## Irányítás

A Shift emel, a Ctrl süllyeszt. A két gomb elengedése lebegtet. A WASD a megszokott vízszintes mozgást vezérli. A magasabb terep megállítja az előrehaladást. A talajra süllyedés leszállít, a hátasról leszállás és a halál lezárja a repülést.

## Protokoll

A CG221 10 bájtos törzsét a megszokott szekvenciabájt követi. A mezők a fejléc, művelet, centiméteres magasság és kliensidő. A 11 bájtos GC221 a fejléc, VID, állapotjelzők, centiméteres magasság és függőleges irány mezőkből áll. A típusos tárgymozgatás és leltárfrissítés 225-ös fejléce megmarad.

A kliens a játékfázisban jelzi a 221 támogatását. A szerver a saját karakternek és a támogatást jelző megfigyelőknek továbbítja az állapotot, a később érkezőknek a karakter beszúrásakor is. A kliens eltárolja a még betöltés alatt álló karakter állapotát.

A szerver legfeljebb 6 m/s függőleges változást enged 2 m toleranciával. A kliensidő nem növeli a keretet. Helyben a leszállás és felszállás megtartja a talajhorgonyt. Az első magasságot és a szerver által elfogadott vízszintes mozgás utáni új felszállás talajmagasságát a kliens mintavételezi. A terepütközés a Unityben, a vízszintes mozgás és harci távolság a meglévő szerverkód szerint működik.

A natív `mount_lifecycle`, `entity_network_dispatch` és a Unity `Metin2MountFlightValidator` ellenőrzi az engedélyezést, a csomagokat és a megjelenítést. A `Metin2MountFlightLiveValidator` helyi TLS-kapcsolaton gyakorolja a repülést, a megfigyelést és a típusos leltárműveletet.
