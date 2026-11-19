# Teljes item protó konverzió: lélekmagok és bónuszadók

2026-11-14. **21 további protó** egy munkafolyamatban; a maradék **818 → 797**.
Összesen **9 698 konvertált, 491 kivezetett és 797 blokkolt** forrássor a 10 986-ból.
A jelöltben 6 780 natív definíció van. Formátum: **40**, hash: `797a6d337b6cd93b9930b1e9701f87b8165ac5cbe2b31f439d9d9a153920158b`.

## Elkészült működés

- **70500–70509:** vörös és kék lélekmag, két külön sebzési csatornával és öt-öt forrásfokozattal.
  A töltésmaximum, töltődési idő, ütéskeret, három sebzésszorzó és a fokozatonkénti ikon megmarad.
  A fejlesztés explicit következő natív protóra vált; ugyanaz a példányazonosító marad.
  Siker esetén a következő protó kezdőállapota érvényesül, kudarc esetén a régi állapot marad.
  A 25%-os 70603 tekercs mindkét elfogadott eredménynél pontosan egy darabot fogyaszt.
  A legfelső fokozat nem enged további fejlesztést. A kliens a tényleges 1–5. fokozatot mutatja.
- **70402, 70403:** szerveroldalon, kiadáskor választott 5% / 10% kosztümbónusz.
  Öt egyenlő esélyű eredmény: szörny, főnök, metin, ember elleni erő vagy ember elleni ellenállás.
  A kiválasztott bónusz a tárgypéldányban mentődik; használat és visszatöltés nem sorsolja újra.
  A használat a mentett értéket adja át a korábban konvertált forráskostüm ritka helyére és elfogyasztja az adót.
  A donorhelyek véletlen ritkabónusz-lista nélkül is használhatók.
  A külön donorlista nem módosítja a közönséges bónuszok sorsolási súlyait.
- **70251:** kapcsolt normál-/képességsebzés-bónusz fegyveren, a forrás megfelelő
  `NormalDamagePercent` / `SkillDamagePercent` típusával. A korábbi általános sebzéstípus hibás használata javítva.
- **50060:** 50%-os lovasképző könyv, 50-es minimumszint, 20 szabad pont korlát.
  Az elfogadott kudarc is fogyaszt könyvet; várakozás siker után keletkezik.
  A könyvek külön példányai és a lovaspont egy tranzakcióban, visszajátszható nyugtával kezelődnek.
- **8885, 50080, 50203, 50270, 50271, 78103, 78104:** az új jutalmak miatt feloldott
  hét láda. Mind a hét ténylegesen nyitható a tesztben; előnézet, megerősítés,
  tárgyfogyás, jutalom és visszatöltés végigellenőrizve.

## Szerver, adatmodell és Unity

A 048-as migráció külön következőlélekmag-kulcsot és szerveres bónuszválaszték-táblát vezet be.
A natív katalógus ellenőrzi a cél típusát, csatornáját, a fejlesztési köröket,
a választék egyediségét és az értékhatárokat. Az új mezők a katalógus hashének részei.
A ServerManager mindkét szabályt visszaolvassa és menti.

A GC225 **23-as verziója** viszi a következő protót és a megjelenítendő fokozatot;
a korábbi csomagverziók megmaradnak. A Unity fejlesztési célválasztója kezeli a protóváltást.
A speciális bónuszok kliensoldali értékhatárai most ugyanabból a teljes natív
bónuszregiszterből generálódnak, így az új bónuszok fogadása is működik.

## Ténylegesen lefutott ellenőrzések

- GameServer, CatalogEditorTool, ItemBenchTool és az érintett natív tesztprogramok fordítása.
- **64/64 CTest**, benne valódi MariaDB-, tárolási, szerveres tárgykezelő-,
  szerkesztő-, protokoll- és 15 független katalógushash-ellenőrzés.
- A bővített SQL-korlátteszt külön is sikeres; **64 konverziós teszt** és
  **64 Qt modell-/űrlapteszt** sikeres.
- A teljes konverzió pontosan azonos eredménnyel újra előállítva.
- Saját, külön helyi MariaDB-n az igazi aszinkron tárgykezelő: **1 071 ellenőrzés**,
  valamennyi új protó, sikertelen és sikeres fejlesztés, ismételt kérés,
  új munkamenetből visszatöltés, öt donorváltozat kiadása, könyvfogyás és hét ládanyitás.
- Unity **6000.3.27f1**: fordítás, **14 ellenőrzőcsomag**, az új 21 forrás
  **118 tényleges szervercsomagja**, valódi előnézeti/megerősítési gombok és runtime kezelők,
  ikonfeloldás, hibás/truncált csomagok és TCP-darabolási pontok ellenőrzése.

Az eredeti 1 574 definíció 29 204 adatsora és az előző jelölt 103 937 adatsora
változatlanul megtalálható az új formátumban, az új oszlopokat figyelmen kívül hagyó vetületen.
A korábban kiadott helyi kulcsok és sorszámok megmaradtak; játékosadat nem lett visszaállítva.

## Maradék és korlátok

Még **797** forrássor blokkolt: 262 asset,
276 jutalomadat, 186 működés és
73 forrásadat/szabály. Ebből 236 ládához hiányzik a jutalomcsoport,
167 sorhoz a forrásikon, 20 horgászbothoz a `canna_2.gr2` modell.
Ezekhez nem készültek kitalált modellek vagy jutalmak.

**A jelölt nincs telepítve a futó helyi játékba.** A futó helyi adatbázisban továbbra is
1 574 definíció van, és a korábban ürített **94 játékosállapot-tábla üres**.
A teljes bejelentkezés/auth/TLS/ECS világbeli játékfolyamat és az Addressables játékosbuild
ezzel a jelölttel nem volt tesztelve. A korábban konvertált 50082 láda véletlen
születési bónuszszabálya továbbra is külön hiányzó működés.

Részletes sorlista: [2026-11-14_full_item_proto_souls_costume_grants.csv](2026-11-14_full_item_proto_souls_costume_grants.csv).
Gépi bizonyíték: [2026-11-14_full_item_proto_souls_costume_grants.evidence.json](2026-11-14_full_item_proto_souls_costume_grants.evidence.json).

Folytatás: [fogyóeszközök és kosztümreset](2026-11-15_full_item_proto_context_services.md) — 11 további protó, 786 maradék sor.
