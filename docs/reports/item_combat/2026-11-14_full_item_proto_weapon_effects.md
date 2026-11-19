# Teljes item-protó konverzió – fegyvereffekt-kosztümök

Újabb részeredmény: [2026-11-14_full_item_proto_secondary_books_elixirs](2026-11-14_full_item_proto_secondary_books_elixirs.md) – 9 406 konvertált, 1 089 fennmaradó sor.

2026-11-14. **27 további protó**: 25 fegyvereffekt és 2 függőségként feloldott láda.
**9 388 konvertált, 491 kivezetett, 1 107 akadályos** sor a 10 986 forrásból.
6 470 natív definíció; katalógusséma 38; hash `d36c5ab06d43722b3c0a82e435efe73d9722ab6247fb84d8734c3acb06f650fb`.
Az eredeti 1 574 definíció és 29204 tartalmi sor megőrzése ellenőrzött.

## Bekötött működés

- Önálló fegyvereffekt-kosztüm és 28. felszerelési hely. A katalógus nyolc explicit
  választást tárol: kard, tőr, íj, kétkezes kard, harang, legyező, alternatív tőr és
  alternatív legyező. Az eredeti fegyvervariáns szabálya finomítási szintenként
  típusos adatként megmarad; a futásidő nem keres régi item-VNUM-ot.
- A szerver az éppen viselt valódi fegyverhez választ effektet; fegyver nélkül,
  szerszámmal vagy nem támogatott fegyverrel nincs fegyvereffekt. A kosztüm saját
  bónuszai a meglévő, önálló kosztümstatisztikán keresztül érvényesülnek.
- Fel- és levétel, mentett óra, ismételt kérés, új session és lejárat a meglévő
  tranzakciós kosztümfolyamathoz kapcsolódik. Lejáratkor a példány eltűnik az élő
  leltárból, a vizuális állapot törlődik.
- GC225 v21 a nyolc választást és a mentett időállapotot, GC241 v4 a szerver által
  kiválasztott effektet viszi. A 16-os képességbit egyezteti az új megjelenést;
  a korábbi megjelenési formátumok továbbra is kódolhatók.
- Unity: viseletablak, leírás, meglévő hálózati fel-/levételi művelet, valódi
  fegyvermodell és effektbetöltés; jobb/bal kéz és lovaglási váltás. A későn
  érkező régi actor-part állapot nem írhatja felül a típusos effektet.
- Qt: mind a nyolc effektazonosító szerkeszthető, validálva. Az új fegyvervariáns
  szabály importálás és szerkesztői visszaírás után is megmarad.
- SQL: ellenőrzött mezők és kosztümfajta, felszerelés 27→28 helyre bővítése az
  owner-revízió emelésével; meglévő tárgyhelyek megtartásával.
- **78107 és 50077** láda eredeti jutalomfüggősége feloldódott. Mindkettő tényleges
  nyitása, mentett nyereménye és a kliens eredményablaka külön ellenőrzött.

## Tényleges ellenőrzések

- GameServer, helper, item-worker és érintett natív tesztek sikeresen fordultak.
- **64 külön CTest-cél sikeres**; a végső két futás mindegyik cél legfrissebb
  eredményét tartalmazza. **11 405 natív kosztümellenőrzés**, továbbá 15 független
  Python/C++ katalógus-digest minta. A négy metadata-mintában kizárólag a
  katalógushash 32 bájtja változott.
- **52 konverziós teszt sikeres**; értéksorrend, alternatívák, hiányzó asset,
  második kéz és hibás csont-hozzárendelés is. **76 Qt-tesztből 75 sikeres,
  1 kihagyott** SQL-konfiguráció hiányában. A CTest SQL-szerkesztőtesztje külön futott.
- A teljes **6 470 definíciós** jelölt saját, új MariaDB-ben SQL-be írva,
  natívan visszaolvasva; hash, tartalom és azonosítószámlálók egyeznek.
- A valódi GameServer ItemStore **3 274 ellenőrzése**: 88211 állandó és 88214
  első használattól időzített effekt; nyolc tényleges forrásfegyverrel
  (10, 1000, 2000, 3000, 5000, 7000, 1090, 7190). Kiadás, fegyvertelen viselés,
  fegyver fel-/levétele, ismételt kérés, új session, effekt fel-/levétele, lejárat,
  továbbá a két új láda nyitása és a 27→28 SQL-migráció is szerepel.
- Unity **6000.3.27f1** fordítás és **11 validátorsorozat** sikeres. Ezek után
  **104 valódi worker-állapot** leltár-, metadata- és megjelenési csomagja jutott
  a klienshez. Mind a **145 hivatkozott effektasset** és **25 eredeti ikon**
  feloldódott; a deklarált textúrák/meshek ellenőrzöttek. Valódi karaktercsontváz,
  tényleges fegyvermodellek, kézpárok, lovaglás, eltüntetés, régi partcsomag,
  csonkolások és minden TCP-darabolási pont ellenőrzött. A két láda valódi GC233
  nyitási eredményét a kliens eredményablaka feldolgozta.

Teljes belépés/auth/TLS/ECS-világfolyamat, élő observer-kiszolgálás, harc és az
összes karakterfaj új típusos effektfolyamata még nincs végigpróbálva.
Az item-worker–SQL–Unity kör ezt nem helyettesíti.

A D: meghajtó betelt az ellenőrzések alatt; az első SQL-próba emiatt hibázott,
és a Unity is helyhiányt jelzett. Az ideiglenes adatbázis törlését az automatikus
ellenőrzés elutasította (`blocked by policy`). Öt saját, leállított tesztadatbázist
adatvesztés nélkül a C: meghajtóra archiváltunk; az útvonalak és ellenőrzött
bájtméretek a `build/weapon-effects-sandbox-archive-result.json` fájlban vannak.
A végső sikeres CTest SQL-próba a `AURIGA_ITEM_SQL_TEST_ROOT` környezeti változóval
a C: meghajtón futott. A helyi játékszerver adatbázisát ez nem érintette.

## Fennmaradó munka

| Akadálycsoport | Forrássor |
|---|---:|
| Hiányzó ikon/modell/vizuális asset | 493 |
| Hiányzó vagy hibás jutalomadat, jutalomfüggőség | 284 |
| Hiányzó viselkedés / példányon választott funkció | 222 |
| Forrásadat, korlát vagy más szabály | 108 |

Nyolc fegyvereffekt még akadályos: 78001, 78003, 78004, 78007, 78008, 78011,
78012 hiányzó palettabejegyzés miatt (1297, 1326, 1335, 1344); 88260 pedig
163-as testeffektre hivatkozik fegyvereffektként. Nincs helyettesítő effekt vagy
automatikusan kitalált hozzárendelés. Nyitott továbbá például 28 sál, 121 speciális
használati tárgy, kisállatkönyvek, lélekmag-átmenetek, további kosztümszolgáltatások,
általános születési/lejárati szabályok és questkezelők. 236 jutalomcsoport hiányzik.

`release_ready=false`; a teljes katalógus nincs a játékba telepítve.
A helyi játékadatbázis továbbra is 1 574 definíciós; account, karakter,
régi/natív tárgypéldány, céh és chatelőzmény: 0; 94 játékállapot-tábla üres.
Régi játékosadatot nem állítottunk vissza; ezt friss visszaolvasás ellenőrizte.

## Folytatási alap

- [2026-11-14_full_item_proto_weapon_effects.csv](2026-11-14_full_item_proto_weapon_effects.csv): mind a 10 986 forrássor, a 27 új konverzió jelölésével.
- [2026-11-14_full_item_proto_weapon_effects.evidence.json](2026-11-14_full_item_proto_weapon_effects.evidence.json): tesztek és fájlhash-ek.
- `build/full-proto-weapon-effects-v38.json`: validált jelölt.
- `build/full-proto-weapon-effects-worker-sql-final/catalog-stage-result.json`: SQL-visszaolvasás.
- `build/full-proto-weapon-effects-worker-sql-final/source-weapon-effect-worker-result.json`: worker és valódi csomagok.
