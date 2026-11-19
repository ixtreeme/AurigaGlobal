# Item proto konverzió és funkcióbekötések

Újabb részeredmény: [2026-11-14_full_item_proto_reroll_accessories](2026-11-14_full_item_proto_reroll_accessories.md) – 9 266 konvertált, 1 229 fennmaradó forrássor.


2026-11-14. A teljes átállás továbbra is folyamatban van. Az előző részeredményhez
képest **18 további protó** fordul; a mostani fő változás a működés és az ellenőrzés.

| Állapot | Előző | Most |
|---|---:|---:|
| Konvertált forrássor | 9 168 | 9 186 |
| Kivezetett / aktív buildben nem használható | 491 | 491 |
| Fennmaradó akadállyal | 1 327 | 1 309 |
| Összes forrássor | 10 986 | 10 986 |

A jelölt **6 268 natív definíciót** tartalmaz. Az eredeti 1 574 definíció és
29 204 korábban kiadott katalógussor mezőértékei megmaradtak. A v33 új
jutalomszint-oszlopa a régi jutalomsoroknál 0; a formátum és a hash változott.
A [teljes CSV](2026-11-14_full_item_proto_functions.csv) minden forrássort pontosan egyszer számol el.

## Fejlesztett ládajutalom

- A jutalomnak saját natív fejlesztési szintje van az SQL-táblában, a katalógusban,
  az atomikus kiosztásban és a GC233 v2 előnézet/eredmény csomagban. A Unity a
  v1 csomagokat továbbra is +0-ként olvassa, a v2-ben a tényleges +N szintet mutatja.
- Nem létező állapot, 255 feletti szint, valamint pénzhez/hatáshoz rendelt
  fejlesztési szint elutasított. Az önálló ExactState +200 állapot is kiosztható
  köztes +0 állapot létrehozása nélkül.
- Javítva lett a katalógus jutalomhivatkozás-ellenőrzése: a validált tartalmat
  járja be, a korábban kiürült, áthelyezett forráslista helyett.
- A GameServer valódi `ItemStore` worker-kódja egy új, kizárólag a teszt számára
  indított adatbázisban végrehajtotta a ládanyitást. A doboz elfogyott, a +2-es
  tárgy mentése/visszaolvasása, az ismételt azonos művelet egyszeri eredménye és
  az új belépési munkamenet utáni megtartása ellenőrzött. Az ebből létrejött
  GC233 csomagot a Unity valódi eredményablaka is helyesen megjelenítette.

Ez a worker/SQL/csomag/UI integráció nem teljes realm/auth/TLS játékpróba:
a worker-tesztben nem fut játékosokat kezelő ECS-világ és játékosóra.

## Definíciós korlátozások

- A nem levehető tárgyat a normál levétel és másik felszerelésre cserélés sem
  mozdíthatja ki. A mentés előtti központi ellenőrzés a kézzel összeállított
  mozgást is elutasítja; az idő miatti megsemmisülést nem tiltja.
- A raktárba nem tehető tárgy új fiókraktári behelyezése elutasított. A korábban
  már bent tárolt példány kivehető marad, így egy szabályváltozás nem zárja be.
- A felvételi tiltást a földi tárgy felvétele és a végleges földi művelet
  mentési ellenőrzése is vizsgálja. A szabályok a tényleges tárgyszinthez tartoznak.

A jelöltben 105 raktározást tiltó,
6 felvételt tiltó és 4
levételt tiltó szabálysor van. A hiányzó szabály a korábban meglévő natív
alapértelmezést tartja meg. Birodalmi és közös egyedi csoportkorlátozások még hátravannak.

## További protók és assetek

- 5 Yang-tárgy és 5 Sárkányérme-utalvány a tényleges `shop_buy_price` értéket
  használja. A Yang-tárgy a karakter pénztárcájába, az utalvány a már meglévő
  közös fiókpénztárca-folyamatba vezet. Nem a névből származik a jóváírás.
- 7 további láda jutalomhivatkozása feloldódott.
- Az aktív Unity-projekt 76 307 releváns assetfájljának és a felszereléskatalógus
  tényleges hivatkozásainak átnézése után a 11010 protó ikonja helyreállt.
  A pontos forrásútvonalhoz tartozó meglévő `QuestIcons/level_07.png` Sprite került
  vissza a katalógusba; a konverter ennek GUID-ját és fájlhash-ét is ellenőrzi.
  Így ez a felszerelés is konvertálódik.
- A szerkesztő megtartja a jutalom fejlesztési szintjét és az explicit
  „Nincs jutalom” kimenetelt. A felület visszaolvasása és az így készített
  katalógus natív validációja is ellenőrzött.

## Ellenőrzések és jelenlegi korlát

**18 célzott CTest sikeres**, benne 32 konverziós tesztesettel és 15 független
katalógushash-változat ellenőrzésével. A katalógusszerkesztő további **53 tesztje
sikeres, 5 kihagyott**. A GameServer és a szükséges eszközök lefordultak.
Unity **6000.3.27f1** újrafordítás, konténer/worker és companion/storage/felszerelés/UI
validátorok sikeresek. A teljes jelölt külön MariaDB-be írása és natív
visszaolvasása azonos hash-t adott. A részletek az [evidence fájlban](2026-11-14_full_item_proto_functions.evidence.json) vannak.

| Fennmaradó akadály | Forrássor |
|---|---:|
| Viselkedés / külső kezelő vagy példányadat | 422 |
| Hiányzó vagy nem azonosítható ikon/modell/hivatkozás | 466 |
| Hiányzó vagy még feloldhatatlan ládajutalom | 286 |
| Forrásadat vagy natív szabály eltérése | 135 |

Az általános időzítők, születési bónuszok/foglalatok/páros addonok, egyes
bónuszforgatók, eszközök, skinek, vállpántok, küldetés- és világbeli hivatkozások,
valamint a teljes új karakter/starter folyamat még igényelnek munkát.
Az adatcsomagban nem azonosított tartalmat a konverter nem helyettesíti találomra.

A jelölt **nem került a futó helyi játékba**, továbbra is `release_ready=false`.
A helyi szerver maradt séma 31-en, 1 574 definícióval. A törölt accountok,
karakterek és tárgyak száma továbbra is 0; 94 játékosállapot-tábla üres.
A tesztek kizárólag új, saját ideiglenes adatbázisokat használtak.

Jelölt: `build/full-proto-functions-assets-final.json`.
Natív hash: `fcf43fe87df87c6d1ce06bb79a5a3f238d76f195a8124b02b60b797cba4fd7dd`.
