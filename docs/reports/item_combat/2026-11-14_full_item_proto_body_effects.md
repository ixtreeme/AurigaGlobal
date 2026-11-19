# Teljes item-protó konverzió – páncéleffekt-kosztümök

Újabb részeredmény: [2026-11-14_full_item_proto_weapon_effects](2026-11-14_full_item_proto_weapon_effects.md) – 9 388 konvertált, 1 107 fennmaradó sor.

2026-11-14. **33 további protó**: az összes fennmaradt, 28/8 típusú páncéleffekt.
**9 361 konvertált, 491 kivezetett, 1 134 akadályos** forrássor a 10 986-ból.
6 443 natív definíció; katalógusséma 37; hash `175710f263e915a350c55bc1e27aead8dcba16ebe4f687716d762fcefceaedae`.
Az eredeti 1 574 definíció és 29204 tartalmi sor megőrzése ellenőrzött.

## Bekötött működés

- Új viselhető kosztümfajta és külön 27. felszerelési hely. Az eredeti effektpaletta
  hivatkozása explicit vizuális adat; a futásidőben nem keresünk vissza régi item-VNUM-ot.
- Az eredeti szerver viselkedését követve az effekt vérten vagy testjelmezen látszik.
  A saját bónuszait a viselt kosztüm önállóan adja. A vért, jelmez vagy effekt
  levétele, illetve lejárata újraszámolja és törli a megjelenést.
- 18 állandó, 3 létrehozástól és 12 első használattól számított protó.
  A meglévő kosztümmentés és órakezelés működik; ismételt kérés, újrafelszerelés
  és új session nem tölti vissza az eltelt időt.
- GC225 v20 továbbítja a definíciót és a mentett időállapotot. GC241 v3 továbbítja
  a szerver által kiválasztott effektet. A 8-as képességbit egyezteti az új
  megjelenési formátumot; a régi v1/v2 observer-formátum továbbra is kódolható.
- Unity: viseletablak új helye, használati leírás, meglévő felszerelési/levételi
  hálózati kérés, tényleges effektbetöltés és csontvázhoz kötés. A régi actor-part
  csomag nem írhatja felül a már átvett típusos effektállapotot.
- SQL: 26→27 tárolóbővítés revízióemeléssel, meglévő anchorok megtartásával;
  típusonként ellenőrzött effekt- és formaadatok. Qt: effektazonosító és óra szerkesztése.
- A Purple Moon ikon eredeti, belső szóközös fájlneve megmaradt. Az asset-ellenőrző
  engedi a fájlnév belső szóközeit, a vezérlőkaraktereket, útvonalbejárást és
  a szóközzel kezdődő/végződő útvonalkomponenseket továbbra is elutasítja.

## Tényleges ellenőrzések

- GameServer, item-worker és natív tesztprogramok fordítása sikeres.
- **64 külön CTest-cél** sikeres, köztük 15 független Python/C++ digest-minta.
  **2 936 kosztümellenőrzés** tartalmazza a négy órafajtát, a vért nélküli
  működést, a testjelmezt, lejáratot és v20-csonkolásokat. A négy metadata-mintában
  kizárólag a 32 bájtos katalógushash változott.
- **49 konverziós teszt**; hiányzó effekt, rossz csont-hozzárendelés és szóközös ikon is.
- **74 Qt/szerkesztőtesztből 73 sikeres, 1 kihagyott** SQL-konfiguráció hiányában.
  A CTest SQL-szerkesztőtesztje külön, új MariaDB-vel sikeres.
- A teljes 6 443 definíciós jelölt új, saját MariaDB-ben SQL-be írva és natívan
  visszaolvasva; hash, tartalom és azonosítószámlálók egyeznek.
- Valódi konvertált **78002, 78006, 88212** effektek és **11200** vért a valódi
  GameServer ItemStore osztályán: kiadás, felszerelés, vért nélküli állapot,
  vért fel- és levétele, ismétlés, új session, levétel, újrafelszerelés és lejárat.
  **618 ellenőrzés**. Az SQL-ben lejárt tombstone marad, az élő leltárban csak a vért.
- Unity **6000.3.27f1**, **11 validátorsorozat**, majd **20 valódi worker-állapot**
  leltár-, metadata- és megjelenési csomagja. Valódi játékoscsontvázon mind a
  **18 külön effekt** létrejött, hiányzó deklarált textúra vagy mesh nélkül.
  Mind a **33 eredeti ikon** feloldódott. A viseletablak és a valódi típusos
  megjelenéskezelő feldolgozta az állapotokat, az eltüntetést, lovaglási váltást,
  későn érkező régi partcsomagot, csonkolásokat és minden TCP-darabolási pontot.

Teljes belépés/auth/TLS/ECS-világfolyamat, élő observer-kiszolgálás, harc és az
összes karakterfaj új típusos effektfolyamata még nincs végigpróbálva.
Az item-worker–SQL–Unity kör ezt nem helyettesíti.

## Fennmaradó munka

| Akadálycsoport | Forrássor |
|---|---:|
| Hiányzó ikon/modell/vizuális asset | 486 |
| Hiányzó vagy hibás jutalomadat, jutalomfüggőség | 286 |
| Hiányzó viselkedés / példányon választott funkció | 255 |
| Forrásadat, korlát vagy más szabály | 107 |

Nyitott például a 33 fegyvereffekt, 28 sál, 121 speciális használati tárgy,
12 kisállatkönyv, lélekmag-átmenetek, további kosztümszolgáltatások,
általános születési/lejárati szabályok és külső questkezelők.
236 forrásjutalomcsoport hiányzik.

`release_ready=false`; a teljes katalógus nincs a játékba telepítve.
A helyi játékadatbázis továbbra is 1 574 definíciós; account, karakter,
régi/natív tárgypéldány, céh és chatelőzmény: 0; 94 játékállapot-tábla üres.
Régi játékosadatot nem állítottunk vissza.

## Folytatási alap

- [2026-11-14_full_item_proto_body_effects.csv](2026-11-14_full_item_proto_body_effects.csv): mind a 10 986 forrássor, a 33 új konverzió jelölésével.
- [2026-11-14_full_item_proto_body_effects.evidence.json](2026-11-14_full_item_proto_body_effects.evidence.json): tesztek és fájlhash-ek.
- `build/full-proto-body-effects-v37-final.json`: validált jelölt.
- `build/full-proto-body-effects-worker-sql/catalog-stage-result.json`: SQL-visszaolvasás.
- `build/full-proto-body-effects-worker-sql/source-body-effect-worker-result.json`: worker és valódi csomagok.
