# Teljes item-protó konverzió – horgászbotok és csákányok

Újabb részeredmény: [2026-11-14_full_item_proto_companion_skins](2026-11-14_full_item_proto_companion_skins.md) – 9 328 konvertált, 1 167 fennmaradó sor.

2026-11-14. **30 további protó** fordul át: 20 alap horgászbot és 10 csákány.
A teljes átállás továbbra is folyamatban van. A konvertált protó a natív
katalógusban ábrázolható adat; az összes tárgy teljes játékbeli próbája külön feladat.

| Állapot | Előző | Most |
|---|---:|---:|
| Konvertált forrássor | 9 266 | 9 296 |
| Kivezetett forrássor | 491 | 491 |
| Fennmaradó akadállyal | 1 229 | 1 199 |
| Összes forrássor | 10 986 | 10 986 |

**6 378 natív definíció**, katalógusséma **35**, hash `c6b81d3863efa9e6ca6cfb33d2fe2560807da03e153d6a09b2bba3280a9b565d`.
Az eredeti 1 574 definíció és 29 204 eredeti tartalmi sor megőrzése ellenőrzött.

## Elkészült működés

- Natív, külön protók közötti fejlesztési átmenetek. A siker és a kudarc másik
  natív tárgyra is mutathat, az önmagára mutató átmenet is megmarad. Nincs
  kitalált köztes fejlesztési lépcső. Minden forrásállapot külön tárgydefiníció;
  a kijelzett fokozat elválik a belső állapotindexétől.
- A forrás gyakorlópont-korlátja, N/D pontszerzési esélye, fejlesztési esélye,
  sikertelen célja és bányászati fokozatbónusza bekerült. A gyakorlás továbbra
  is szerver által befejezett gyűjtési műveletből származik, nincs kliensoldali
  „pontot kérek” akció. A botok a vízterület zsákmánytábláját használhatják.
- A régi protó cseréjét követő állapot-visszaállítás atomikus: gyakorlópont és
  csali nullázódik, a natív tárgypéldány azonosítója megmarad. A katalógus
  visszautasít hiányzó, nyugdíjazott, más fajtájú vagy eltérő foglalásméretű célt.
- GC254 v3 és a Unity codec/ablak együtt frissült; v2 olvasása is működik.
  Az előnézet a siker és a kudarc pontos céltárgyát, az eredmény a mentett
  tárgyat mutatja. A kliens a céltárgy metaadatait is megkapja.
- Két pontos, meglévő FBX-ből natív betölthető felszerelés-asset készült:
  `fishing_rod` és `digger_1`. A tényleges hálózati felszerelésbetöltő
  (`Metin2ContentProvider`) mindkettőt feloldotta.
- Az admin eszközlap megőrzi és megjeleníti az új átmeneteket, a fokozatot
  és a vízterülethez kötött zsákmányszabályt; a mentés/visszatöltés tesztelt.

## Forrásadatok külön kezelése

A sikeres cél nélküli végállapotok nem fejleszthetők és nem rontják le magukat.
Ez tudatos javítás: a régi függvény egyes ilyen soroknál megpróbálhatott nem létező
sikeres tárgyat létrehozni, illetve kudarcban visszalépni. A döntés a soronkénti
naplóban `source_terminal_refine_disabled` jelölést kap.

A 29102 belső forrásneve +0; a régi bányászati fokozatszámítás is ebből dolgozik.
A konverzió ezt megtartja, a fordítások eltéréseit külön naplózza.

A 20 professzionális bot logikája is átmenetgráfként kezelhető, de a külön
`item/etc/canna_2.gr2` modell hiányzik az aktív projektből. Ezek továbbra is
akadályos sorok, helyettesítő modell nem került hozzájuk.

## Tényleges ellenőrzések

- GameServer, item-kezelő és natív tesztprogramok fordítása.
- 64 külön CTest-cél sikeres: 60 regressziós cél és 4 célzott eszköz/gyűjtési/
  tényleges item-worker ellenőrzés. Köztük 15 független Python/C++ digest-minta.
- 42 konverziós teszt; az átmenetgráf hibájánál teljes visszagörgetés tesztelt.
- Szerkesztő/Qt: 71 tesztből 70 sikeres, 1 konfigurációt igénylő SQL-teszt
  kihagyva; ezen felül az új átmenetes szerkesztési kör célzott próbája sikeres.
  Az SQL-es szerkesztőteszt CTestben külön sikeres.
- A teljes jelölt új, elkülönített MariaDB-ben SQL-be írva és natívan visszaolvasva;
  a hash és az azonosítószámlálók egyeznek.
- **A tényleges konvertált protók** (27580 → siker: 27590 / kudarc: 27570)
  a valódi GameServer `ItemStore` osztályán mentek keresztül: grant, felszerelés,
  szerveres gyakorlás 159-ről 160 pontra, levétel, siker/kudarc, azonos művelet
  újraküldése és visszalépés. A céltárgy, azonosító, revízió és nullázott állapot
  megmaradt; az ismétlés ugyanazt a bizonylatot használta.
- A fenti worker két exportált csomagját a tényleges Unity eszközablak is
  feldolgozta. Unity **6000.3.27f1** fordítás és **20 integrációs validátorsorozat**
  sikeres, az eszközcodec-sorozat 90,190 ellenőrzéssel, TCP-darabolással
  és csonkolt csomagok elutasításával együtt.

Teljes belépés/auth/TLS/ECS-világfolyamatot és minden új protót játékosként
még nem próbáltunk végig. Az izolált item-worker–SQL–Unity kör ezt nem helyettesíti.

## Fennmaradó munka

| Akadálycsoport | Forrássor |
|---|---:|
| Hiányzó ikon/modell/vizuális asset | 486 |
| Hiányzó jutalomcsoport vagy jutalomfüggőség | 286 |
| Hiányzó viselkedés / példányon választott funkció | 320 |
| Forrásadat, korlát vagy más szabály | 107 |

Még nyitott többek között a kisállatkönyvek és -kinézetek, mountkinézetek,
lélekmag-átmenetek, egyes kosztümszolgáltatások, általános születési és lejárati
szabályok, valamint a külső questkezelők bekötése. A loot-akadályokból 236-nál
hiányzik a forrás jutalomcsoport.

`release_ready=false`. A még hiányos teljes katalógus nincs a játékba telepítve.
A helyi játékadatbázis továbbra is 1 574 definíciós; account, karakter, régi tárgy,
natív tárgypéldány, céh és chatelőzmény darabszáma 0, 94 játékállapot-tábla üres.

## Folytatási alap

- [2026-11-14_full_item_proto_tools.csv](2026-11-14_full_item_proto_tools.csv): teljes, 10 986 soros állapotnapló a 30 új protó jelölésével.
- [2026-11-14_full_item_proto_tools.evidence.json](2026-11-14_full_item_proto_tools.evidence.json): tesztek, input- és fájlhash-ek.
- `build/full-proto-tools-v35.json`: tiszta, eredeti statikus bemenetből újrafordított jelölt.
- `build/full-proto-tools-worker-sql/catalog-stage-result.json`: SQL-visszaolvasás.
- `build/full-proto-tools-worker-sql/source-tool-worker-result.json`: tényleges konvertált protók worker-próbája és csomagjai.
