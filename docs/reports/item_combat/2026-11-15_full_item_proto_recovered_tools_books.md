# Teljes item protó: visszanyert horgászbot és példányhoz kötött könyvek

2026-11-15. **22 további forrássor**, a maradék **771 → 749**.
Összesen **9746 konvertált, 491 kivezetett, 749 blokkolt**, 10986 forrássorból.
A jelölt **6828 natív definíció**, a katalógusformátum változatlanul 40.
Hash: `aa0118f014390646f5a055947da7be60d531d0d6368cf310583f252df3be8105`.

A 27200–27390 közötti, tízesével számozott húsz professzionális horgászbot az eredeti
`canna_2.gr2` modellt és `canna_tex_2.jpg` textúrát használja. A forrás a
`F:/ymir work__/item/ymir work/item` könyvtárból került elő. A tényleges 2083 csúcsú,
3022 háromszögű merev mesh lett átvéve; helyettesítő grafika nem készült.
A modell megjelenik a tényleges karakter felszerelési pontján, a normál tartalombetöltővel.
A forrás szerinti hat önmagára mutató fejlesztési átmenet és két végállapot megmaradt.

A GC254 kiterjesztett szerszámválasza v5 lett, mert a korábbi v3 verziót a
Unity a gyűjtögetés válaszának tekintette. A v2 szerszámcsomag változatlan,
a régi v3 csomag továbbra is külön dekódolható. A kezelőablak rendesen betölti
a szervertől kapott tárgyneveket és az eredeti modelleket.

Az **50300 képességkönyv** és a **70037 felejtéskönyv** a létrehozáskor kapja meg
a szerver által kiválasztott képességet az eredeti 48 fő képesség közül.
A választás a kiadott példányhoz vagy köteghez tartozik, SQL-ben megmarad,
és mozgatás, újrabelépés vagy ismételt kérés nem sorsolja újra.
Másik megengedett képességre átírt meglévő példány művelete is elutasított.
A felejtés pontosan egy fő képességpontot térít vissza. A kaszt- és képességcsoport
szerinti használhatóságot a meglévő szerveres képességellenőrzés tartja be.
A petkönyvek nem kapták meg ezt az eltérő működést.

Lefuttatott ellenőrzések:

- GameServer, CatalogEditorTool, tényleges aszinkron tárgykezelő és 48 natív tesztcél fordítása.
- **74 konverziós teszt**, **64/64 CTest**, **2893 könyvellenőrzés**.
- Saját MariaDB-n **1510 botellenőrzés**, 35 fejlesztési kísérlet, 95 csomagsorozat;
  **2442 könyvellenőrzés**, mind a 48 képesség tanulása és felejtése, 288 csomagsorozat.
  Mentés, új munkamenet, ismételt művelet és az egyszeri költség ellenőrizve.
- Unity **6000.3.27f1** fordítás és az érintett meglévő ellenőrzőcsomagok;
  **13803 bot- és 14966 könyvellenőrzés** a tényleges szervercsomagokkal,
  eredeti ikonokkal, valódi ablakgombokkal, csomagdarabolási és csonkítási próbákkal.
- A teljes konverzió pontosan reprodukálható. Az eredeti 1574 definíció **29204** sora
  és az előző jelölt **104918** sora megmaradt.

A hiányzó vagy hibás eredeti adatokhoz tartozó sorok a felhasználó kérésére a
közelgő branch-merge-re várnak; nem lettek kivezetve vagy kitalált adatokkal pótolva.
A még portolandó, létező eredeti működések ettől külön fennmaradó munkák.
**Ez ellenőrzött részállapot, a teljes konverzió még nincs kész.**

A jelölt nincs telepítve a futó helyi játékba. A teljes auth/TLS/bejelentkezés/ECS
világfolyamat és az Addressables játékosbuild ezzel a jelölttel nem volt tesztelve.
Régi account-, karakter- vagy tárgyadat nem lett visszaállítva.

[Teljes protójegyzék](2026-11-15_full_item_proto_recovered_tools_books.csv) · [Gépi bizonyíték](2026-11-15_full_item_proto_recovered_tools_books.evidence.json)
