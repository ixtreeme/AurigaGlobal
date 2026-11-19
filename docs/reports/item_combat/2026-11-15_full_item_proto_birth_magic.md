# Eredeti létrehozáskori mágikus bónuszok – ellenőrzött részállapot

Az opcionális eredeti mágikus tárgykiadás 6612 forrásállapothoz került bekötésre. Az alapkiadás továbbra is csak az eredeti kötelező páros addont adja; a szándékosan kért mágikus kiadás a protó esélyét vagy a láda saját felülírását használja. A szörnyzsákmány ezt a kért kiadást használja. Az egyedi tárgyazonosító és a műveletazonosító determinisztikus sorsolása megakadályozza, hogy egy mentési újrapróbálás más eredményt adjon.

Az eredeti High/Low súlyok, fegyver/páncél/kosztüm második és harmadik esélyei, a meglévő és statikus bónuszok kizárása, a finomítási szint, valamint a páros addon megőrzése explicit natív szabály. A 43-as katalógusséma négy profilt és 44 esélysort tartalmaz. Nincs futásidejű régi protóazonosítós elágazás.

A két eredeti 65%/70% ládajutalom nem bónuszozható egyedi tárgy, ezért a régi működés szerint ezekre nem kerül mágikus bónusz. Valódi ládanyitás, SQL-mentés, újrapróbálás és visszatöltés igazolja ezt. A Unity már elfogadja a szabályos, nem nulla külön bónuszesélyű ládák előnézetét és eredményét.

Ellenőrzések: a GameServer és a Database fordítása sikeres; 64/64 natív regressziós teszt sikeres. Külön Python SHA-256/esély/súly referencia 19 836 kiadási esetet ellenőrzött. A valódi aszinkron SQL-munkafolyamat 43 017 ellenőrzése, 42 reprezentatív forrás három kiadási módja, mentése, új belépési munkamenete, felszerelése és levétele sikeres. Unity 6.3.27f1 alatt 504 tényleges csomagpár, 252 valódi felszerelési gombnyomás, két ládanyitási megerősítés és 4585 célzott ellenőrzés sikeres; a meglévő 13 262 ellenőrzéses ládavalidátor is sikeres. A konverzió bájtpontosan újra előállítható.

A katalógus darabszáma változatlan: 9763 konvertált, 491 korábban kivezetett és 732 blokkolt forrássor, 6845 natív definíció. Ebből 559 sor a felhasználó kérésére a merge utánra halasztott; 173 további sor és a még hiányzó működések feldolgozása folytatódik. Ez nem a teljes konverzió befejezése.

A helyi játékszerverre ez a jelölt nem került telepítésre. A régi fiókokat, karaktereket és tárgyaikat nem állítottam vissza: a külön, csak olvasó ellenőrzés szerint mind a 94 ellenőrzött játékosállapot-tábla üres, az aktív katalógus továbbra is az 1574 definíciós alap. A teljes bejelentkezéses ECS-játékfolyamot és az Addressables játékosbuildet ebben a részben nem teszteltem.
