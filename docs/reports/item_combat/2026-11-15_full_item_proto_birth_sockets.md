# Teljes item protó: eredeti foglalatok és utolsó kő kiemelése

2026-11-15. **9763 konvertált, 491 kivezetett, 732 blokkolt** a 10986 forrássorból.
Új protó: **71109**, a maradék **733 → 732**. Jelölt: **6845 natív definíció**,
**42-es katalógusformátum**, hash: `e0e4a83b013ccd10dc07dfb4d559e9cfa2c1ea89858f9251695032a22b53aef1`.

A fő változás **2813 már konvertált fegyver-/vértváltozat működését** egészíti ki.
Az eredeti `item_manager.cpp` létrehozási ciklusa a `bGainSocketPct` / `socket_pct`
mezőt a megnyitott normál foglalatok számának használja, legfeljebb háromig.
Az új rendszer eddig csak eltárolta ezt az adatot; a kiadott fegyverek/vértek
foglalatai zárva maradtak. A **050-es migráció** ezért külön
`birth_open_stone_sockets` mezőt ad, és a valódi kiadás ezt alkalmazza:
**413 állapot egy**, **320 kettő**, **2080 három** nyitott normál foglalatot kap.
A korábbi rögzített metaadat megmarad; nem alakult át véletlen foglalatsorsolássá.
Mozgatás vagy újrabelépés nem építi újra a példány foglalatait.

A **71109 Javítás tekercse** eredeti kódja a legutolsó foglalt foglalat kövét
emeli ki. Az új C5 profil pontosan ezt a meglévő natív műveletet használja.
A visszaadott kő típusa, szintje és a foglalat minősége megmarad; a korábbi
beágyazott kövek változatlanok. A tárgy fogyása, a céltárgy és a visszaadott kő
állapota egy tranzakció. Üres foglalatoknál nincs fogyasztás; ismételt kérés nem
duplázza a követ. A normál leltári/foglalatkezelő felület és GC235 végzi a műveletet.

Lefuttatott ellenőrzések:

- GameServer, Database, CatalogEditorTool, ItemBenchTool, valódi tárgykezelő és az
  érintett 48 natív tesztcél fordítása, a könyv/checkpoint tesztcélokkal együtt.
- **78 konverziós teszt**, **214 állandó tárgyteszt**, **64/64 CTest legutóbbi állapota**.
  Egy kézzel épített régi tesztadatból hiányzott az új kötelező mező;
  pótlása után az érintett SQL-es runtime teszt külön is sikeres.
- **11499 natív/SQL ellenőrzés** a teljes **2813 forrásállapoton**, a forrásadatból
  külön számított elvárt foglalatszámmal. **22 SQL-minta** fegyverfajtánként és
  foglalatszámonként: kiadás, új munkamenet, két tényleges kőbehelyezés,
  két pontos kiemelés, ismételt kérés és hatás nélküli használat.
- Unity **6000.3.27f1**, meglévő foglalat-/behelyezéspróbák és **14889 új ellenőrzés**:
  **50 valódi csomagsorozat**, eredeti tárgyikonok, foglalatállapotok, tényleges
  megerősítő gomb, kő-visszaadás, TCP-darabolás és csonkolt csomagok elutasítása.
- A jelölt pontosan reprodukálható. Az eredeti 1574 definíció **29204** adatsora és
  az előző jelölt **106214** sora megmaradt; az új mező hozzáadott adat.

**542** sor/függőség továbbra is a felhasználó által kért merge-re váró jegyzékben van.
Ettől külön **190** működési vagy szabálybeli tétel maradt. A 93 időkorlátos egyedi
kiegészítő órája és a születési véletlen bónuszok még hátralévő működések;
a teljes konverzió nincs kész.

**Nincs telepítve a futó helyi játékba.** Ott újra ellenőrizve **94 üres játékosállapot-tábla**
és **1574 definíció** van; régi játékosadat nem lett visszaállítva.
A teljes auth/TLS/bejelentkezés/ECS világfolyamat és az Addressables játékosbuild
nem volt tesztelve ezzel a jelölttel.

[Protójegyzék](2026-11-15_full_item_proto_birth_sockets.csv) · [Gépi bizonyíték](2026-11-15_full_item_proto_birth_sockets.evidence.json)
