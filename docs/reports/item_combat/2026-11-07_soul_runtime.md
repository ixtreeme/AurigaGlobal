# C30 — Önálló lélektárgyak

Szerver `20261103`; Unity `/main/unity63-integration-20261103`, Editor
`6000.3.27f1`, párosított Unity-changeset **cs:91**. Szerződés: [IC-065](../../adr/item_combat_065_soul_runtime.md).

A külön típusos leltár vörös/kék core-jának automatikus online töltése,
aktiválása, kikapcsolása, tekercses fejlesztése és elfogadott találatonkénti
fogyása együtt működik szerveren és Unityben. Szint, kötés, töltés,
részintervallum, megmaradt ütés és aktív szándék tartós.

## Valódi helyi játékpróba

Két elkülönített TLS-karakter, valódi Unity-leltárgombok és az új
lélekablak preview/confirm gombjai: **7 fázis, 343 ellenőrzés és 6
újrabelépés sikeres**. Szerverújraindítás után további **91 ellenőrzés
és 2 újrabelépés sikeres**, a korábbi elfogyott/fejlesztett/spent állapottal.

| Eset | Igazolt eredmény |
|---|---|
| Töltés | Nulláról pontos kapacitásra töltődés; külön lassú core részintervalluma is mentődik, nincs ingyenes ütéskeret |
| Fejlesztési kudarc | 0%-os tekercs egyszer fogy, a teljes core állapota változatlan; nonce-replay és újrabelépés után is |
| Sikeres fejlesztés | Vörös és kék ugyanazon ID/ItemKey alatt +1 lesz, saját kezdő töltéssel; tekercs pontosan egyszer fogy |
| Aktiválás | A két csatorna együtt használható; második vörös elutasított; aktív újrabelépés/offline várakozás nem tölt vissza ütést |
| Kizárt cél | Másik saját tesztkarakter és nem létező cél nem fogyaszt egyik csatornából sem |
| Hat normál találat | A vörös egyesével fogy, a kék változatlan. Két találatonként 2 töltés vonódik le; a hatodiknál inaktív, üres, de meglévő core marad |
| Tényleges szorzó | A három fokozat damage-packetjei rendre **1042, 1042; 750, 750; 625, 625**. Kikapcsolva a bonusz megszűnik |
| Képességtalálat | Valódi warrior skill **10440** sebzéssel csak a kék ütéskeretét 2→1 módosítja; újrabelépés után is 1 marad |
| Kikapcsolás és szerverrestart | Vörös +1/0 töltés/2 ütés/inaktív, kék +1/6 töltés/1 ütés/inaktív pontosan visszatér; új aktiválás/kikapcsolás sem állítja vissza az elhasznált ütést |

Az SQL **7 egyedi combat eventet** talált. Minden kimeneti sebzés a
receipt bemenete és szorzója alapján egzakt egész szorzással egyezik,
és a Unityben kapott hét damage-packet értékeivel is azonos.
Sikeres tekercsből 2, sikertelenből 3 maradt a kezdeti 4–4 darabból.
A végső offline SQL-snapshot várakozás közben pontosan változatlan.
**960 másik typed példány**, a védett pénztárcák és tanulási adatok,
valamint a korábbi katalógussorok megmaradtak. Legacy tárgy nem került
a tesztkarakterekre. A 65076-os ideiglenes szörny és map-regen visszaállítva.

## Natív, SQL és Unity

- **17 natív/SQL tesztcsoport sikeres**, az előző C29 regressziókkal
  együtt. Az új `soul_runtime` 672 ellenőrzést tartalmaz, az IC-025
  domainpróbái is sikeresek.
- A valódi MariaDB-writer próbája új sessionből tölti vissza az aktív és
  fejlesztett állapotot. Azonos operation ID replaye nem ír újra; azonos
  damage-event új operation ID-val is elutasított. Hiányos owner-viewból
  tervezett második aktív vöröset az SQL egyedi indexe is megfogja.
- A Windows GameServer és a két natív helper végleges fordítása sikeres.
- Unity 6.3 fordítás és **1252 új ellenőrzés sikeres**; a korábbi 700
  companion-, 4179 leltár-, 2447 stat-, 397 jelmez- és 8003 C29-ellenőrzés
  is lefutott. Native golden, minden csonkolás, kevert TCP-folyam minden
  darabolása, tényleges leltár-/lélekablak-gomb és runtime dispatch szerepel.
- A CG213/GC219 azonosítók a hexadecimális legacy értékekkel együtt
  ütközésmentesek. A v28 katalógus változatlan.

## Telepítés és korlátok

A `018_soul_runtime.sql` kétszer lefutott a leállított saját stacken,
teljes player/protos mentéssel. Mind a **956 megelőző typed rekord**,
tároló és katalógussor változatlan maradt. Új állapotmezők és két
adatbázis-védelem került be: owner/csatorna egyediség és damage-event
egyediség. A későbbi részpróba állapotát nem állítottuk vissza; a sikeres
kör saját friss, tíz tárgyas készletet használt.

A friss binárisok, Qt-kezelő és migráció a `Server2-Local` csomagba
kerültek mentéssel és manifesttel. Mindhárom bináris, az új migráció és
a 293 kezelőfájl hash-e ellenőrzött; 34 Unity forrás/meta egyezik az aktív
workspace-szel. Az öt saját szerverfolyamat fut. Az aktív Unity
forrásokkal azonos ellenőrzőmásolatban futott a próba. A normál legacy leltár átállítása és
a teljes tartalomkonverzió külön feladat.

A halálkori cleanup natív modellben ellenőrzött; az élő kör nem ölte meg
a tesztkaraktert. Nem futott teljes világjelenetes/animációs, standalone,
FreeBSD-, terheléses vagy SQL-kiesés/crash-injection játékpróba.
Az NPC-HP továbbra is ECS-világállapot; a tartós Soul-debit és a végső
harci folytatás sorrendje elkészült, a teljes legacy harci procrendszer
nem lett tranzakciós outboxszá alakítva.

Bizonyítékok az ignorált `build` alatt: `c30-game-build4.log`,
`c30-native-build8.log`, `c30-native-build9.log`, `c30-native-tests3.log`,
`c30-native-tests4.log`,
`c30-unity-offline3.log`, `c30-soul-live2.log`, `c30-soul-restart1.log`,
`c30-soul-sql1.log`, `c30-soul-sql2.log`, `c30-migration-evidence.json`,
`c30-manager-build3.log`, `c30-manager-deployment.json` és
`c30-deployment-evidence.json`. A fixture hitelesítési adatai ignoráltak.
