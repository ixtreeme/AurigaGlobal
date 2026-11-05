# ADR IC-058 — TERA társak és repülés a mission engine ágon

2026-11-05, C26a. A helyi szerver és a Unity 6.3 megkapta a TERA tartalmat. A célzott átvétel az idézést, a mozgást, a bemutatást és a repülést hozza az aktív ágakra. A prototípusok és a Unity Addressables 203 hátast és 69 petet kapcsolnak össze, köztük 84 repülő hátast.

A hátas és a pet meglévő pecsétje a legacy jelmez- és quest-rendszert használja. A típusos társ, a pettojás keltetése és a meglévő karakterek leltárátállítása külön munka.

A repülés mindkét irányban a 221-es fejlécet kapta, mert a típusos tárgyrendszer a 225-öst használja. A kliens támogatást jelez a játékfázis kezdetén. A szerver a ténylegesen felvett pecsétből és az aktuális hátas race-éből ad engedélyt, figyeli a függőleges sebességet, és az érintett megfigyelőknek elküldi az állapotot. A megállás helyén a fel- és leszállás megtartja a talajhorgonyt. A szerver által elfogadott vízszintes mozgás után a következő felszállás új Unity talajmintát használ. A szerver a meglévő kétdimenziós térképadatokat használja.

A belépéskor késleltetve visszatöltött karakterhatások újracsatolhatják a hátast. A szerver a folyamat végén küldi el az aktuális repülési engedélyt, így a karakter az újracsatolás után szállhat fel.

## Ellenőrzés

- A Windows Release szerverbuild, 29 natív regressziós teszt és öt kiadási adatellenőrzés sikeres.
- Unity 6.3 alatt átment 37 repülési ellenőrzés, 272 race és 69 pet betöltése, valamint a C4–C25 típusos regressziós csomag. Az elkülönített tesztprojekt a TERA tartalmat és a típusos paneleket vizsgálta. A teljes világjelenet egyes Crest, SpeedTree és VFX forrásai hiányoztak belőle.
- A három Unity TLS-kapcsolatos helyi próba 27 ellenőrzése lefedte a fel- és leszállást, a magasságugrás elutasítását, a később érkező megfigyelőt, a 225-ös típusos tárgymozgatást, a pet megidézését és visszahívását.
- A helyi SQL-ellenőrzés igazolta mindhárom prototípustábla 272 új sorát, a 84 repülési jelzőt, valamint a korábbi prototípussorok, 165 típusos tárgypéldány és a tesztkarakter tanulási adatainak egyezését.

Nyitott ellenőrzés a teljes világjelenet kézi próbája, a standalone kliensbuild és a FreeBSD build.

Telepítési és ellenőrzési útmutató a [TERA tartalom](../../integration/tera_companions/README.md) és a [repülés](../../integration/mount_flight/README.md) leírásában.
