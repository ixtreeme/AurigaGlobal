# Eredeti egyedi tárgyak időkezelése

A korábban átvitt **93 időkorlátos egyedi tárgy** teljes példányszintű órája bekötve: 26 létrehozástól, 14 első felvételtől és 53 viselés közben fogyó idővel. Új protó nincs ebben a körben; a katalógus minden táblája és a 9763 konvertált / 491 korábban kivezetett / 732 még akadályozott forrássor változatlan.

A szerver kezeli a két egyedi tárgyhelyet, a végleges tárgyakkal történő cserét, a viselési tartozás elszámolását, az új SQL munkamenetet és a lejáratot. Az óra nem tölthető vissza újrabelépéssel vagy ismételt művelettel. A lejárt példány és bónusza együtt tűnik el. A végleges tárgyak nem kapnak rejtett órát.

A 39002, 70004, 70005 és 70043 eredeti levételi tilalma megmarad; a kliens letiltja náluk a levételt, és megmagyarázza a korlátozást. Az idő és a teljes eredeti időkeret a GC225 v25 csomagban érkezik, a régi csomagváltozatok támogatása megmarad. Az új `031_unique_lifetime.sql` migráció a meglévő 22 bájtos óra tárolását engedi meg C3 példányokon; meglévő állapotot nem ír át.

Ellenőrzések: GameServer, Database, a katalóguseszközök és a tesztek lefordultak; **64/64 natív/SQL regressziós teszt sikeres**. A külön helyi SQL szerveren futó tényleges aszinkron item worker **6068 ellenőrzést** teljesített az összes 93 protóval. Unity **6000.3.27f1** alatt az általános item/felszerelés/jelmez/tárolás ellenőrzések mellett **5277 új ellenőrzés** sikerült **651 valódi csomagpáron**, a tényleges felszerelés/levétel gombokkal, eredeti ikonokkal, darabolt és hibás csomagokkal.

A teljes TLS/bejelentkezés/ECS világfolyamat és Addressables player build ebben a körben nincs lefuttatva. A konverziós jelölt nem került telepítésre a játék adatbázisába: annak 94 játékosállapot-táblája továbbra is üres, a korábbi 1574 definíció megmaradt. A felhasználói kérés szerint 542 hiányos forrásadat merge-re vár; további 190 viselkedési/politikai sor feldolgozása és a még hiányzó általános születési bónuszkezelés folytatódik.

Jelölt: `D:/AurigaGlobal/LiveWork/AurigaGlobal/build/full-proto-unique-clocks-v42-final.json`. SHA-256: `1119447f2dfffaf188d15be135b72050be172ac43abe0459b198b217377ded5b`. Katalógushash: `e0e4a83b013ccd10dc07dfb4d559e9cfa2c1ea89858f9251695032a22b53aef1`; a katalógus tartalma bitre azonos az előző socketes ellenőrzési ponttal. Az új eszközujjlenyomattal a jelölt pontosan újra előállítható.
