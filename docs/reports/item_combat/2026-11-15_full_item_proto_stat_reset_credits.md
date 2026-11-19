# Vértabletta: natív statpont-visszaosztási keret

A 70014 eredeti tárgy új kulcsa C5:792. Pontosan egy statpont-visszaosztási lehetőséget ad; a karakterablak meglévő mínusz gombja választja ki a visszaosztandó pontot. Nem állítja vissza az összes tulajdonságot.

- Összesen 9777 konvertált, 491 korábban kivezetett és 718 blokkolt forrássor; 6859 natív definíció.
- Katalógusséma 44, migráció 052; katalógus SHA-256 `7e2195767b7c2e55b40cf9759bfe988481ba2926e31dd433d1729b1c07fa58fc`.
- Reprodukálható jelölt `build\full-proto-stat-credit-v44-final.json`: `ec1a836d9f0e35e5190dd8d08b7c1ac862e83b361e819ba7fdf20ee1fffd252f`.

## Megvalósítás

A fogyasztás és a player.stat_reset_count jóváírása közös, revízióval védett SQL-tranzakció. A BOK4 állapot külön opcionális keretet tárol; BOK1–3 változatlanul olvasható. A tárolási határ 32767, a meglévő előjeles SMALLINT tartománya. A felső határ nem fogyaszt tárgyat. A régi teljes karaktermentés és a Database gyorsítótára védett a régebbi revíziójú felülírástól. A függő művelet alatt a statmínusz-parancs teljes egésze várakozik.

A közös mentés javítása a változatlan rangpontnál vagy keretnél megengedi a nulla módosított sort. A végső revízióváltó UPDATE továbbra is kötelező egyetlen soros CAS; a konfliktus a teljes tranzakciót visszagörgeti.

A GC244 v4 külön szolgáltatást ad át. Unity előnézet, megerősítés, hibajelzés és a karakterablak statmínusz-gombjai ellenőrizve; a rangpontok GC244 v3 formátuma megmaradt.

## Futtatott ellenőrzések

- GameServer és Database teljes fordítása sikeres; a javítás után 64/64 CTest sikeres.
- 7852 BookRuntime és 280 BookAid ellenőrzés; 45 SQL-séma teszt.
- Valódi aszinkron SQL item worker: 67 ellenőrzés, 6 csomagpár; egyszeri fogyasztás, ismétlés, új SQL-munkamenet, tárolási határ, késői mentés és revízióütközés.
- Unity 6000.3.27f1: 12849 ellenőrzés, valódi inventory/megerősítő/mínusz gombok; a korábbi hat rangponttárgy regressziója további 54628 ellenőrzés.
- Mind a 15 független Python-katalógushash ellenőrizve. Négy wire-fixture-ben kizárólag a katalógushash bájtjai változtak.
- Helyi szerver csak olvasva ellenőrizve: 94 játékosállapot-tábla üres; aktív katalógus változatlanul 1574 definíció.

## Nem tesztelt / nem telepített

Ez a jelölt nincs telepítve az aktív szerverre. Teljes TLS/login/ECS játékfolyamat és Addressables player build még nem futott; az SQL és Unity szerkesztői próbák nem helyettesítik ezt.
