# Teljes item protó konverzió: időkorlátos és effektből álló stólák

2026-11-15. **15 további protó**, a maradék **786 → 771**.
Összesen **9 724 konvertált, 491 kivezetett, 771 blokkolt** forrássor a 10 986-ból.
A jelölt **6 806 natív definíció**, változatlan **40-es katalógusformátummal**.
Katalógushash: `1e411ba667a397e5ec3bb1fefade386d009911b2145aaf7f37b0acbf02860516`.

## Konvertált források

- **85088, 85092, 85096, 85100:** Lilith-szárny, az első használattól
  számított 24 óra / 10 nap / 25 nap / 365 nap. Ugyanaz a natív példány
  és ugyanaz a lejárati idő marad felvétel, levétel, csere és újrabelépés után.
- **85128, 85136, 85144, 85152, 85154:** létrehozástól számított
  7 / 7 / 7 / 7 / 10 napos stólák. A szerver a kiadáskor állítja az órát;
  a leltárban és kilépés alatt is telik. Nem változtak első használattól indulóvá.
- **85083, 85104, 85108, 85116, 85140, 85148:** korlátlan stólák,
  amelyek megjelenését az eredeti MSE-effekt adja.

A tizenötből tizenhárom szándékosan `empty.gr2` modellt használ a forrásban.
Ezek eredeti effektdefiníciói és regisztrációi megvannak az aktív Unity projektben.
A konverter a bizonyított effektet és a forrás szerinti `Bip01` / `Bip01 Spine2`
csontot köti be; nem készült helyettesítő modell. A két valódi modelles stóla
a modelljét és a hozzá tartozó effektet együtt kapja meg. A típusos stólák
megjelenése a kosztüm teljes méretét használja.

## Szerver és Unity működés

A hat bónuszos stóla továbbra is külön C19 kategória, saját felszerelési hellyel;
nem az abszorpciós vállszalag. A véges stólák a közös, példányhoz mentett
élettartamot használják. A korlátlan stólák korábbi, óra nélküli állapota megmarad.
A lejárat megsemmisíti a példányt, és eltávolítja a viselt bónuszokat és megjelenést.
A bónuszok újrasorsolása sem tölti újra az órát; lejárt tárgyra elutasított művelet
nem fogyaszt bűvölőanyagot. Korlátlan és véges stóla mindkét irányban cserélhető.

A **030_stole_lifetime.sql** migráció a meglévő 22 bájtos óra SQL-ellenőrzését
kiterjeszti a C19-re. Nem épít újra példányállapotot. A **GC225 v24** külön,
ellenőrzött stólaóra-szakaszt ad; a korábbi v1–v23 csomagok változatlanok.
A Unity a pontos óratípust és hátralévő időt mutatja a tárgyleírásban.

## Lefuttatott ellenőrzések

- GameServer, CatalogEditorTool, ItemBenchTool és a tényleges tárgykezelő fordítása;
  48 natív tesztcél újrafordítása az érintett közös fejléc miatt.
- **71 konverziós teszt**, **64/64 CTest**, beleértve a tényleges SQL-teszteket.
  A natív tesztek a viselt időt mérő stólaórát is ellenőrzik szintetikus tartalommal;
  a mostani kilenc véges forrás a két fent részletezett faliórát használja.
- A teljes konverzió két előállítása pontosan azonos; az eredeti 1 574 definíció
  **29 204** adatsora és az előző jelölt **104 474** adatsora megmaradt.
- Új, saját helyi MariaDB-n a valódi aszinkron tárgykezelő **1 739 ellenőrzése**:
  kiadás, fel-/levétel, ismételt kérés, bűvölés, kétirányú csere, új munkamenetből
  visszatöltés, lejárati határ és leltárban lejárat. A hamis óraállapot és
  hamisított lejárati művelet elutasítása is ellenőrizve.
- Unity **6000.3.27f1**: fordítás, 12 számlált ellenőrzőcsomag és a külön
  stólaasset-ellenőrzés; **141 valódi leltár-/részlet-/megjelenéscsomag-sorozat**,
  a valódi fel-/levételi gombok, ikonok, TCP-darabolás és hibás csomagok elutasítása.
  Mind a 15 megjelenés két tényleges konvertált karakterskeletonon is betöltődött:
  **30 szereplő**, eredeti effekttel, megfelelő csonton, eltávolítási ellenőrzéssel.

## Maradék és telepítés

Még **771** forrássor blokkolt: 256 asset,
276 jutalomadat, 182 működés,
57 forrásadat/szabály. A 85089-es, Valentine Wing nevű
forráshoz továbbra sincs igazolt stólamegjelenés; név alapján nem kapott másik modellt.
A 28 abszorpciós vállszalag külön hiányzó működés maradt.

A teljes auth/TLS/bejelentkezés/ECS világfolyamat és az Addressables játékosbuild
ezzel a jelölttel nem volt tesztelve. Az effekt-/modellpróba az igazi klienskomponenseket
és asseteket használó helyi szerkesztői próba, szerveres szereplőszimuláció nélkül.
Az 50082 láda korábbról ismert véletlen születési bónuszszabálya továbbra sincs kész.

**A jelölt és az új migráció nincs telepítve a futó helyi játékba.** Ott továbbra is
1 574 definíció és 94 üres játékosállapot-tábla van. Régi játékosadat nem lett visszaállítva.

Részletes protójegyzék: [2026-11-15_full_item_proto_stoles.csv](2026-11-15_full_item_proto_stoles.csv).
Gépi ellenőrzési bizonyíték: [2026-11-15_full_item_proto_stoles.evidence.json](2026-11-15_full_item_proto_stoles.evidence.json).
