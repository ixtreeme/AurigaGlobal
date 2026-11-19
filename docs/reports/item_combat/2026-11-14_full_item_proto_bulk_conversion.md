# Teljes item protó konverzió nagyobb csomagban

Újabb részeredmény: [2026-11-14_full_item_proto_wearables_rare_services](2026-11-14_full_item_proto_wearables_rare_services.md) – 9 677 konvertált, 818 fennmaradó sor.

2026-11-14. **161 további protó** három csoportban: 126 ékszer- és övállapot,
16 passzív tárgy, 19 állandó kapcsolható ital.
**9 567 konvertált, 491 kivezetett, 928 akadályos** sor a 10 986 forrásból.
6 649 natív definíció; katalógusséma 39; hash `85958d8086c39b43f90c9e8a22c9ca2abdb49e870070fd10c2a0bb828598bc0f`.
Az eredeti 1 574 definíció és 29204 tartalmi sor, továbbá az előző
jelölt 100064 tartalmi sora változatlan; az azonosítószámlálók nem csökkentek.

## Konvertált tárgyak és működés

- **126 felszerelési állapot**: 45 karkötő, 45 nyaklánc, 27 fülbevaló és 9 öv.
  A hiányzó állapotikon az explicit, egyértelmű fejlesztési lánc ellenőrzött
  elődjének valódi Unity-ikonját kapja. Az új konverziós szabály egyező
  forrásnév-családot, típust, altípust, méretet, korlátozásokat és egymást követő
  fejlesztési szinteket igényel. Numerikus közelségből nem választ ikont;
  a lánc minden sora és az ikon/metadata hash-e az auditban szerepel.
  A forrásbónuszok és a pontos fejlesztési szint külön natív állapotban maradnak.
- **126 fejlesztési recept** került a jelöltbe a tényleges forrásesélyekkel,
  költségekkel és anyagfüggőségekkel. A natív katalógus validálja őket;
  az új receptek tényleges kovácsolási folyamatát még nem próbáltuk végig.
  117 új ékszer explicit foglalatprofilt kapott; az övekhez nincs ércfoglalat.
- **16 passzív, régi 0-s típusú protó**: az addig tévesen értelmezett APPLY 51
  mező megmarad az auditban, aktív bónuszt nem ad. Az új tárgyak tárolhatók,
  mozgathatók és visszatölthetők. A felület nem kínál hozzájuk kitalált használatot.
- **19 állandó kapcsolható ital**: explicit `duration_ms=0` profil;
  az aktív állapot is nulla hátralévő idővel tárolódik. Ki- és bekapcsolhatók,
  bónuszuk egyszer érvényesül, új munkamenetben visszatöltődnek és hosszú
  aktív időtől sem fogynak el. Szabálytiltás kikapcsolja őket a példány törlése
  nélkül. A meglévő halál-, csoportütközés-, jogosultság- és cooldownszabályok
  érvényben maradnak. A tényleges bónusz nélküli 18 házassági ital akadályos maradt.

## Szerver és Unity bekötés

Az SQL 047-es migráció engedi az explicit nulla időt; a katalógus hash-tartománya
és sémája 39-re váltott. A Qt-szerkesztőben a nulla állandó hatást jelent.
GC225 v22 jelzi a kapcsoló állandó voltát; GC243 v2 támogatja az állandó
kapcsolható hatás előnézetét és állapotát. A Unity kodekek és a leltár-/hatásablak
ugyanazt a jelentést használják, és **Állandó** feliratot mutatnak.
A korábbi véges profilok csomagformátuma és időkezelése megmaradt.

## Tényleges ellenőrzések

- GameServer, katalóguseszközök, item-worker és 48 natív tesztcél sikeres buildje;
  az SQL-benchmark segédprogramja külön újrafordítva.
- **64 külön CTest-cél sikeres** a végső állapotban: natív domain, protokoll,
  perzisztencia, szerkesztők, SQL és katalógus-runtime regressziók.
  Az első kör két hibája után a sémafixture alkalmazza az új migrációt és
  a megfelelő verziót várja; a benchmark az aktuális segédprogrammal futott újra.
- **58 konverziós teszt**, **1 071 kapcsolhatóhatás-ellenőrzés**,
  **57 leltárprotokoll-ellenőrzés**, **15 független katalógusdigest-fixture**.
  Négy metadata-goldenben kizárólag a 32 bájtos katalógushash változott.
- A teljes **6 649 definíciós** jelölt új, saját MariaDB-ben SQL-be írva és
  natívan visszaolvasva; a hash és a tartalom egyezik. A tesztadatbázisok
  a C: meghajtón futottak, a játékszerver adatbázisa nem kapta meg a jelöltet.
- **22 759 valódi item-worker ellenőrzés**, mind a **161 új protóval**:
  kiadás, mentés, ismételt műveletek, új owner-session; felszerelés, pontos
  forrásbónuszok, levétel; passzív tárgy mozgatása; állandó ital kapcsolása,
  hosszú idő utáni megőrzése, bónusza és szabályhatár miatti kikapcsolása.
- Unity **6000.3.27f1** fordítás és **12 validátorsorozat**, majd
  **650 tényleges worker-állapot** a leltár-, metadata- és hatásablakokon.
  Mind a 161 ikon feloldódott; felszerelés/levétel és hatásmegerősítés kérése
  ellenőrzött. A megerősítés nem írja át helyben a szerver által kezelt állapotot.
  Csonkolás és minden TCP-darabolási pont formátumonként ellenőrzött.

A teljes belépés/auth/TLS/ECS-világfolyamat és az új fejlesztési receptek
játékbeli kovácsolása nincs végigpróbálva ezzel a katalógussal.
A protók ezért validált folytatási jelöltet alkotnak; `release_ready=false`.

## Fennmaradó munka

| Akadálycsoport | Forrássor |
|---|---:|
| Hiányzó ikon/modell/vizuális asset | 367 |
| Hiányzó vagy hibás jutalomadat, jutalomfüggőség | 284 |
| Hiányzó viselkedés vagy képességdefiníció | 204 |
| Forrásadat, korlát vagy más szabály | 73 |

167 forrásikon és 236 jutalomcsoport továbbra is hiányzik. Nyitott többek között
a petkönyvek, sálak, speciális használati tárgyak, lélekmag-átmenetek,
kosztümszolgáltatások és az általános példány-élettartam további bekötése.

A helyi játékadatbázis **1 574 definíciós** maradt. A korábban törölt accountok,
karakterek, régi/natív tárgypéldányok, céhek és chatelőzmények továbbra is üresek;
**94 játékállapot-tábla frissen visszaellenőrizve**. A teljes katalógus nincs telepítve.

## Folytatási alap

- [2026-11-14_full_item_proto_bulk_conversion.csv](2026-11-14_full_item_proto_bulk_conversion.csv): minden forrássor és a 161 új konverzió.
- [2026-11-14_full_item_proto_bulk_conversion.evidence.json](2026-11-14_full_item_proto_bulk_conversion.evidence.json): forrásláncok, tesztek, csomagok és fájlhash-ek.
- `build/full-proto-bulk-v39-final.json`: validált folytatási jelölt.
- `C:/Users/ixtre/AppData/Local/Temp/AurigaItemValidation/bulk-v39-1/`: SQL-visszaolvasás és worker-csomagok.
- `tests/server/SourceBulkCatalogTests.py`: a három csoport megismételhető szerverpróbája.
- `Assets/Metin2/Editor/Metin2SourceBulkValidator.cs`: az aktív Unity-workspace klienspróbája.
