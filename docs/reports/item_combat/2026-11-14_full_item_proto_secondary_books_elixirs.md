# Teljes item-protó konverzió – másodlagos könyvek és automata elixírek

Újabb részeredmény: [2026-11-14_full_item_proto_bulk_conversion](2026-11-14_full_item_proto_bulk_conversion.md) – 9 567 konvertált, 928 fennmaradó sor.

2026-11-14. **18 további protó**: 8 tanulókönyv és 10 automata HP/SP-elixír.
**9 406 konvertált, 491 kivezetett, 1 089 akadályos** sor a 10 986 forrásból.
6 488 natív definíció; katalógusséma 38; hash `cb2f8224a90978f14d9d61a8fc6b63e809de599e1f7de8cebe129286c4561320`.
Az eredeti 1 574 definíció és 29204 tartalmi sor, valamint az előző
jelölt 99822 tartalmi sora változatlan; az azonosítószámlálók nem csökkentek.

## Átvitt működés

- 50304/50305/50306 kombókönyv: 20/70/100% sikeresély; 30., illetve 50.
  karakterszint az első és második szinthez; a képesség maximuma 2.
- 50311/50312/50313 nyelvkönyv: saját birodalom nyelve tiltott; maximum 20.
- 50061 lóhívó könyv: a 131-es képesség, maximum 10. Nem a lóképességpont-készletet növeli.
- 50600 bányászkönyv: a valódi forráskezelő a 124-es bányászképességet tanítja,
  miközben a protó `value0=123` értéket tartalmaz. Az átvitel az explicit kezelőt követi.
- Mindegyik könyv a meglévő típusos olvasásablakhoz kapcsolódik. Elfogadott sikertelen
  próbánál is fogy egy könyv; az olvasási várakozás 18–30 óra, a könyvbónusz
  másfélszeres, egész százalékra csonkolt esélyt ad, maximum 100%-ot.
  A forrás szerinti egyesével és halmozva tárolt könyvek is külön ellenőrzöttek.
- 72723/72724/72726 HP-, 72727/72728/72730 SP-elixír, továbbá
  76004/76005 SP- és 76021/76022 HP-jutalompéldány. Explicit pool és véges töltet;
  másodpercenként legfeljebb a megfelelő maximum 7%-a. A két X-változat aktív
  maximum-bónusza 4%. Arénában nem használhatók. A natív profil egy példányban
  tárol egy töltetet; a kiadás szükség esetén külön példányokra oszt.
- A meglévő GC225/GC227 és GC242/GC244 formátumok változatlanul továbbítják az
  állapotot és a használati műveleteket. A futásidő explicit típusos profilt kap;
  a régi tárgyszám csak az offline konvertáló és az audit bemenete.

## A hiányzó képességek ellenőrzése

Az új másodlagos könyvek csak rögzített, létező képességdefinícióra konvertálhatók.
A konvertáló ellenőrzi a képességtípust és a szintkorlátot is, és elutasítja a
duplikált definíciókat. A helyi szerver definícióit csak olvastuk; azok nem módosultak.

50902/50903/50904 alkímiakönyv: a kezelő a **123-as képességet** igényli, amely
hiányzik a helyi `skill_proto` táblából. Ezek `missing_secondary_skill_definition:123`
indokkal akadályosak maradtak. A korábbi 21 soros próbajelölt ezért nem a folytatás
alapja; az új, ellenőrzött jelölt a **`full-proto-services-v38-final.json`**.
Petkönyvekhez továbbra is szükséges a forrás petképzési adatainak és használható
pecsétjeinek összekötése; ideiglenes tesztpet-profilra nem készül végleges konverzió.

## Tényleges ellenőrzések

- GameServer és a valódi ItemStore-t használó teszt-worker build sikeres.
- **56 konverziós teszt sikeres**, köztük az új esélyek, célképesség-eltérések,
  szintkapuk, speciális elixírbónusz, hiányzó képesség és elégtelen képességkorlát.
- **10 külön CTest-cél sikeres**: automata runtime és visszatöltés, képességkönyvek,
  olvasás és segédtárgyak, visszatöltési ütemezés, konverzió, Qt-szerkesztő,
  SQL-szerkesztő és a GameServer katalógus-runtime MariaDB-próbája.
- A teljes **6 488 definíciós** jelölt új, saját MariaDB-ben SQL-be írva és
  natívan visszaolvasva; a hash és a tartalom egyezik.
- **709 valódi item-worker ellenőrzés**, mind a **18 új forrásprotóval**:
  kiadás, könyv-előnézet, sikeres/sikertelen olvasás, fogyás, képességnövelés,
  várakozás, saját nyelv és maximális szint tiltása; automata be-/kikapcsolás,
  pontos töltetlevonás; ismételt műveletek és új owner-session.
- Unity **6000.3.27f1** fordítás, **13 validátorsorozat**, majd **81 valódi
  worker-állapot** a leltár-, metadata- és szolgáltatáscsomagokkal. A valódi
  olvasás- és elixírablak megkapta az állapotokat, és az előnézet alapján
  megfelelő megerősítési kérést küldött helyi fogyasztás/jóváírás nélkül.
  Eredeti ikonok, sikeresély, képességazonosító, mentett töltet, csonkolások és
  minden TCP-darabolási pont ellenőrzött.

A teljes belépés/auth/TLS/ECS-világfolyamat még nincs végigpróbálva ezzel a
katalógussal. A worker a töltet mentését és a natív visszatöltési tervet ellenőrzi;
az élő karakter HP/SP-mentésének szerverösszeomlási határát ez nem teszteli.

A teljes SQL-jelölt és a runtime SQL-próba a C: meghajtón futott. Az egyik
regressziós próba újra csökkentette a D: szabad helyét; a korábbi, leállított
fegyvereffekt-tesztadatbázis adatkönyvtárát visszaállíthatóan a C: meghajtóra
archiváltuk, fájlszám- és bájtméret-ellenőrzéssel. A játékadatbázist ez nem érintette.

## Fennmaradó munka

| Akadálycsoport | Forrássor |
|---|---:|
| Hiányzó ikon/modell/vizuális asset | 493 |
| Hiányzó vagy hibás jutalomadat, jutalomfüggőség | 284 |
| Hiányzó viselkedés vagy képességdefiníció | 204 |
| Forrásadat, korlát vagy más szabály | 108 |

Továbbra is nyitott például a petkönyvek, sálak, további speciális használati
tárgyak, lélekmag-átmenetek, kosztümszolgáltatások és általános példány-élettartam.
236 forrásjutalomcsoport hiányzik.

`release_ready=false`; a teljes katalógus nincs a játékba telepítve.
A helyi játékadatbázis 1 574 definíciós; account, karakter, régi/natív tárgypéldány,
céh és chatelőzmény: 0; 94 játékállapot-tábla üres, frissen visszaellenőrizve.

## Folytatási alap

- [2026-11-14_full_item_proto_secondary_books_elixirs.csv](2026-11-14_full_item_proto_secondary_books_elixirs.csv): minden forrássor és az új 18 konverzió.
- [2026-11-14_full_item_proto_secondary_books_elixirs.evidence.json](2026-11-14_full_item_proto_secondary_books_elixirs.evidence.json): tesztek, csomagok és fájlhash-ek.
- `build/full-proto-services-v38-final.json`: validált folytatási jelölt.
- `build/services-secondary-skills.json`: rögzített képességdefiníciók; a CLI-ben `--skills` bemenet.
- `C:/Users/ixtre/AppData/Local/Temp/AurigaItemValidation/services-v38-final/`: SQL-visszaolvasás és worker-csomagok.
