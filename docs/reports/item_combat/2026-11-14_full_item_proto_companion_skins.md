# Teljes item-protó konverzió – kedvenc- és hátasmegjelenések

Újabb részeredmény: [2026-11-14_full_item_proto_body_effects](2026-11-14_full_item_proto_body_effects.md) – 9 361 konvertált, 1 134 fennmaradó sor.

2026-11-14. **32 további protó**: 24 kedvenc- és 8 hátasmegjelenés.
**9 328 konvertált, 491 kivezetett, 1 167 akadályos** forrássor a 10 986-ból.
6 410 natív definíció; katalógusséma 36; hash `c68a2b9529d35257cfce2529cdf601b6da0618c38a269486df46a326704e4a35`.
Az eredeti 1 574 definíció és 29,204 tartalmi sor változatlanul megmaradt.

## Bekötött működés

- Viselhető kosztümfajták, két külön felszerelési hellyel (25/26). A skin saját
  forrásbónuszai aktív kedvenc nélkül is működnek. A tárgy nem idéz meg lényt.
- Külön formahivatkozás; a kedvencskin a fix és növekvő kedvencet, a hátasskin
  a követő és meglovagolt hátast írja felül. A meglévő világprojekció cseréli
  vagy visszaállítja a megjelenést levételkor és lejáratkor.
- Az eredeti állandó, létrehozástól és első használattól számított idők bekerültek.
  A meglévő kosztümmentés, lejárati bizonylat és offline rendezés kezeli őket;
  újbóli felszerelés és ismételt kérés nem hosszabbíthatja a határidőt.
- GC225 v19: a formaazonosító továbbítása és szigorú szerver/Unity feldolgozás.
  Régi kosztümcsomagok támogatása megmaradt. A karakter test/haj/fegyver
  megjelenésébe ezek a formák nem kerülnek be.
- Unity: a viseletablak két új, feliratozott helye, felszerelési és levételi kérés,
  típusleírás és meglévő időmegjelenítés; a szerkesztőben forma- és időkezelés.
- SQL: a meglévő 24 helyes tároló 26 helyesre bővül az anchorok áthelyezése nélkül,
  tulajdonosi revízióemeléssel. Új kosztümforma FK és fajtánkénti SQL-ellenőrzés.

A 78106 láda újonnan elérhető 60103 jutalmánál `rare=30101` szerepel.
Ez nem érvényes százalék. A konverter külön hibakóddal blokkolja a ládát;
nem találgatja az érték jelentését, és nem csonkolja vagy korlátozza automatikusan.

## Tényleges ellenőrzések

- GameServer és a natív item-kezelő fordítása sikeres.
- **64 külön CTest-cél** legutóbbi eredménye sikeres, köztük 15 független
  Python/C++ katalógusdigest. A korábbi v19-ismeretlen teszt v20-ra frissült;
  a négy metadata-mintában kizárólag a 32 bájtos katalógushash változott.
- **45 konverziós teszt**; hiányzó modell és hibás jutalomadat visszagörgetése is.
- Qt/szerkesztő: 72 tesztből 71 sikeres, 1 SQL-konfiguráció miatt kihagyott;
  az új skinforma körbejárási teszt külön sikeres. A CTest SQL-szerkesztőtesztje
  is sikeres volt saját, új MariaDB-vel.
- A teljes 6 410 definíciós jelölt új MariaDB-ben felépítve és natívan visszaolvasva;
  tartalomhash és számlálók egyeznek.
- **Valódi konvertált 88020, 88021, 88110, 88111** protók, valódi GameServer
  ItemStore és SQL: kiadás, felszerelés, bónuszok, levétel, újrafelszerelés,
  azonos művelet ismétlése és új sessionben visszaolvasás. 110 ellenőrzés.
  A lejárat után nincs élő tárgy vagy foglalt cella; az SQL-ben megmaradt a lejárt
  állapotot tartalmazó tombstone. A 24→26 tárolóbővítés és revízióemelés is ellenőrzött.
- Unity **6000.3.27f1**, **11 validátorsorozat**, majd a fenti SQL-worker
  **14 tényleges csomagja**, valódi viseletablakkal és felszerelési kéréssel.
  Csonkolások és minden TCP-darabolási pont ellenőrizve.
  A hálózati race-betöltő mind a **17 külön modellt** betöltötte a 32 protóhoz.

Teljes belépés/auth/TLS/ECS-világfolyamat, megfigyelő kliens, élő lovaglás/repülés
és mind a 32 megjelenés játékosként történő végigpróbálása még nem történt meg.
Az item-worker–SQL–Unity ellenőrzés ezt nem helyettesíti.

## Fennmaradó munka

| Akadálycsoport | Forrássor |
|---|---:|
| Hiányzó ikon/modell/vizuális asset | 486 |
| Hiányzó vagy hibás jutalomadat, jutalomfüggőség | 286 |
| Hiányzó viselkedés / példányon választott funkció | 288 |
| Forrásadat, korlát vagy más szabály | 107 |

Nyitott például a 121 speciális használati tárgy, a sálak és további kosztümtípusok,
a kisállatkönyvek, a lélekmag-átmenetek, további kosztümszolgáltatások,
általános születési/lejárati szabályok és külső questkezelők. 236 forrásjutalomcsoport hiányzik.

`release_ready=false`; a teljes katalógus nincs a játékba telepítve.
A helyi játékadatbázis továbbra is 1 574 definíciós; account, karakter,
régi/natív tárgypéldány, céh és chatelőzmény: 0; 94 játékállapot-tábla üres.
Régi játékosadatot nem állítottunk vissza.

## Folytatási alap

- [2026-11-14_full_item_proto_companion_skins.csv](2026-11-14_full_item_proto_companion_skins.csv): mind a 10 986 forrássor, a 32 új konverzió megjelölésével.
- [2026-11-14_full_item_proto_companion_skins.evidence.json](2026-11-14_full_item_proto_companion_skins.evidence.json): tesztek és fájlhash-ek.
- `build/full-proto-companion-skins-v36-final.json`: validált statikus jelölt.
- `build/full-proto-skins-worker-sql-final/catalog-stage-result.json`: SQL-visszaolvasás.
- `build/full-proto-skins-worker-sql-final/source-skin-worker-result.json`: item-worker és csomagok.
