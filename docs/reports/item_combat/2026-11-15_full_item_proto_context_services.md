# Teljes item protó konverzió: fogyóeszközök és kosztümreset

2026-11-15. **11 további protó**, a maradék **797 → 786**.
Összesen **9 709 konvertált, 491 kivezetett, 786 blokkolt** forrássor a 10 986-ból.
A jelölt **6 791 natív definíció**, változatlan **40-es formátummal**.
Katalógushash: `d6e21226acf01612ccba5ce74fcede35de92982c95ab28b37919f955de928da1`.

## Bekötött működések

- **39008 és 72310:** egy következő megfelelő könyvolvasás várakozását átlépő töltet.
  A töltet kiadása és a forrástárgy fogyása együtt mentődik. A maximum egy;
  ismételt feltöltés nem fogyaszt tárgyat. A töltet kilépés után megmarad,
  és az elfogadott könyvolvasás használja fel.
- **71094:** egyszer használható könyvolvasási esélybónusz. A forrás 512-es
  hatása a könyvbónuszt jelenti, de ennél a protónál nulla időtartam szerepel.
  Ez dokumentált javítás: a nulla idejű affect helyett a natív, következő olvasáskor
  elfogyó töltet működik. A próbában a 70%-os könyv 100%-os, bónuszos előnézetet kapott.
- **50085 és 50086:** azonnali gyógyító ital, pontosan 500 HP, illetve 500 HP + 500 SP.
  Teljes készleteknél nincs tárgyfogyás. A forrás `SeedBottleTime` mezőjét csak
  felírja a régi szerver: nincs olvasója vagy működő cooldown-ellenőrzése.
  Emiatt nem került hozzá kitalált újrahasználati idő.
- **50093, 50094, 50817, 50818:** támadás-/védelemnövelő italok.
  A normál `USE_ABILITY_UP` forráskezelő valóban fogadja az 53-as és 54-es
  bónusztípust; a korábbi konverter indokolatlanul kihagyta őket.
  A pontos érték, a 30 / 180 másodperces időtartam, a mentés és a hatás lejárata ellenőrizve.
  A csak egyedi tárgyaknál engedett mágikus ellenállás továbbra is külön szabály marad.
- **70063:** kosztüm három normál bónuszának újrasorsolása. A forrásból kiolvasott
  első dobás 30/40/20/8/2%, a másik kettő 50/40/10/0/0% értékszint-eloszlást használ.
  A ritka bónusz és a kosztüm órája megmarad; pontosan egy resetanyag fogy.
  A próba korábban konvertált forráskostümöt, valódi natív bónuszadó műveletet
  és a már meglévő natív normálbónusz-anyagot használta előkészítésként.
- **27999:** a forrás kősorsoló tárgyának teljes 14 elemű, egyenletes sorsolása.
  A csomagban csak négy célprotó van: 28000, 28004, 28008 és 28012.
  A többi tíz számhoz a régi szerver sem tud tárgyat létrehozni, de a forrástárgy elfogy.
  Ezért **4/14 tárgyjutalom és 10/14 üres eredmény** marad; mind a 14 kimenet
  külön, tényleges SQL-művelettel ellenőrizve. Hiányzó célprotóhoz nem készült pótló tárgy.

## Szerver és Unity ellenőrzés

A meglévő natív műveletekhez kerültek a források; új séma vagy csomagverzió nem kellett.
Az előnézetek, az esélybónusz megjelenítése, a megerősítő gombok és a használati kérések
a működő Unity ablakokon futottak végig. A kliens csak kérést küld, a tárgyfogyást
és az eredményt a szerver menti és küldi vissza.

- GameServer és CatalogEditorTool fordítási ellenőrzése; a kibővített tényleges
  ItemCatalogRuntimeTests tárgykezelő fordítása.
- **69 konverziós teszt**, **64/64 CTest**; az SQL-, tárolási, szerkesztő-,
  műveleti és protokolltesztek sikeresek.
- A teljes konverzió két előállítása pontosan azonos eredményt adott.
- Saját helyi MariaDB-n az igazi aszinkron tárgykezelő **624 ellenőrzése**:
  mentés, új munkamenetből visszatöltés, ismételt kérés, töltetkorlát,
  tényleges bónuszos/várakozást átlépő olvasás, ritka bónusz megőrzése,
  italhatás lejárata és mind a tizennégy kősorsolási eredmény.
- Unity **6000.3.27f1**: fordítás, **15 ellenőrzőcsomag**, az új
  protók **65 tényleges szervercsomagja**, ikonfeloldás, használati/megerősítési
  gombok, hibás csomagok elutasítása és TCP-darabolási pontok.

Az eredeti 1 574 definíció **29 204** adatsora és az előző jelölt
**104 375** adatsora változatlanul megmaradt. A korábbi helyi kulcsok nem változtak.

## Maradék és telepítés

Még **786** forrássor blokkolt: 262 asset, 276 jutalomadat,
182 működés, 66 forrásadat/szabály.
A teljes bejelentkezés/auth/TLS/ECS játékfolyamat és az Addressables játékosbuild
ezzel a jelölttel nem volt tesztelve. A régebbi, véletlen születési bónuszok
külön hiányzó működések maradtak, köztük az 50082 láda szabálya.

**A jelölt nincs telepítve a futó helyi játékba.** A futó helyi adatbázisban továbbra is
1 574 definíció és 94 üres játékosállapot-tábla van. Játékosadat nem lett visszaállítva.

Részletes protójegyzék: [2026-11-15_full_item_proto_context_services.csv](2026-11-15_full_item_proto_context_services.csv).
Gépi ellenőrzési bizonyíték: [2026-11-15_full_item_proto_context_services.evidence.json](2026-11-15_full_item_proto_context_services.evidence.json).

## Folytatás

A következő kör: [időkorlátos és effektből álló stólák](2026-11-15_full_item_proto_stoles.md); 15 további protó, **786 → 771**.
