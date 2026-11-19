# Eredeti születési átlagos- és képességsebzés-bónuszok

A már konvertált **1557 eredeti protó** (1556 fegyverállapot és 1 páncél) most a tényleges tárgygyárban megkapja az eredeti `ApplyAddon` szerinti születési bónuszt. A nulla érték kimarad, a negatív érték megmarad. A már meglévő alap-/finomítási bónusz és az eredeti kivételszabály határozza meg a típusütközés kezelését. A páncél eredeti addonjelölése is működik; nem kap kitalált fegyverkategóriát.

A szerver SHA-256 alapú, külön névtérrel ellátott sorsolása a szerverművelethez és a valós példányazonosítóhoz kötött, torzítatlan egész sorsolással. A kiadási bizonylat ellenőrzése visszaszámítja a kezdeti állapotot. Ismétlés, SQL-betöltés, mozgatás, felszerelés és levétel nem sorsol újra. A szokásos újrabónuszolás ugyanazt a páros szabályt használja, a tényleges finomítási szint alapbónuszaival. Addonprofil megfelelő normál bónuszprofil nélkül már katalóguskiadáskor hibás.

A katalógus táblái bitre azonosak az előző időkorlátos egyedi tárgyas ellenőrzési ponttal. **9763 konvertált, 491 korábban kivezetett és 732 még akadályozott forrássor**, 6845 natív definíció, v42 séma. Ebben a körben új protó nincs; a korábban átvitt protók hiányzó működése készült el.

Ellenőrzések:

- GameServer, Database, a katalóguseszközök és az érintett tesztek lefordultak. A teljes regressziós csomag 63 tesztje elsőre sikerült; a régi kézi fegyverkiadást a valódi tárgygyárra átállító 179-es SQL-teszt javítása után **mind a 64 teszt legutolsó futása sikeres**. A sikertelen köztes futások a buildnaplókban megmaradtak.
- 5481 persistent-item, 145 instance-attribute és 54 attribute-runtime ellenőrzés. Az összes 1557 forrásállapot eredménye egyezik a külön Python-ellenőrzés Gauss-/párosszámításával. A tényleges aszinkron SQL item worker 57 mintán **9106 ellenőrzést** teljesített, mentéssel, ismétléssel, új munkamenettel, felszereléssel és mozgatással.
- Unity **6000.3.27f1**: a teljes általános csomag és 4917 meglévő bónuszműveleti ellenőrzés mellett **11047 új ellenőrzés**, **1785 valódi natív csomagpár**, **114 tényleges felszerelés/levétel gombművelet**, eredeti ikonokkal és pontos előjeles bónuszkijelzéssel.

A teljes TLS/bejelentkezés/ECS világfolyamat és Addressables player build nincs lefuttatva. A játék adatbázisába ez a jelölt nem került telepítésre. A helyi szerver 94 játékosállapot-táblája továbbra is üres, 1574 korábbi definícióval; régi account, karakter és tárgy nem lett visszaállítva. A 542 merge-re félretett hiányos forrássor megmarad. A további 190 viselkedési/politikai sor és az opcionális születési mágikus bónuszok még külön munkát igényelnek.

Jelölt: `D:/AurigaGlobal/LiveWork/AurigaGlobal/build/full-proto-addon-v42-final.json`. SHA-256: `a1ec65d95b722f1acbfe78ca2d56eabc3e4e0a317597b66cbf7f33ba703ad1e7`. Katalógushash: `e0e4a83b013ccd10dc07dfb4d559e9cfa2c1ea89858f9251695032a22b53aef1`. Pontos újragenerálás ellenőrizve.
