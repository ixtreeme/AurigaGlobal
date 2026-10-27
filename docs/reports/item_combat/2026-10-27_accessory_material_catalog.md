# Itemkatalógus — ékszerek és egyszerű alapanyagok

Történeti v3 lelet. A későbbi foglalatprofil/timer bővítés és aktuális
ellenőrzés: [v4 jelentés](2026-10-27_accessory_socket_profiles.md).

2026-10-27. Ág: `codex/mission-engine`; kiinduló HEAD: `ac4a7cf3`.
**Az izolált katalógus négy kategóriát kezel: weapon, armor, egyszerű accessory
és ordinary material. A teljes migráció és az összes kategória még nincs kész.**

Elkészült az új `ItemAccessoryRow` és `ItemMaterialRow`, a kategóriákhoz tartozó
publikálási ellenőrzés, külön SQL-tábla és kanonikus hash. Az alap+child együtt
egyetlen logikai prototípus; +0..+N szintenként nem jön létre új tárgykulcs.

- Ékszer: karkötő, nyaklánc, fülbevaló; kötelező slot és resistance profil,
  egyesével tárolt példányok, külön base- és refinement-bónuszok. A +15-ös
  referencia átmegy a +9→+10 határon. A raw alap/növekmény nem számolja be újra
  ugyanazt a bonus-profile tartalmat.
- Egyszerű alapanyag: leather, blood, root, needle, jewel. Explicit típus és
  pozitív halmozási maximum, nem fejleszthető None profillal. Nem vehet át
  felszerelési szintet, OnEquip kötést vagy passzív harci bónuszt.
- Hiányzó/duplikált/árva/más kategóriájú sor és érvénytelen profil blokkolja a
  katalógust. Elhagyott típus/slot nem kap csendes, érvényes alapértelmezést.
- A katalógusséma és hash v3; a korábbi formátumok elutasítandók. Független
  Python encoder igazolja a négykategóriás fixture-t és az új mezők kódolását.
- A fejlesztési `protos` most 18 táblás; a privát `ic_audit_*` fixture-táblák
  ebbe nem számítanak bele. A kész SQL→catalog loader továbbra is külön feladat.

## Ellenőrzés

Normál CTest: **8/8**; ASAN buildből: **7/7**; külön standalone build: **7/7**.
A normál kör saját MariaDB **11.4.13** példányában **12/12 SQL-eset** futott.
Az engine és a Python tesztek nem ASAN-instrumentáltak; a natív C++ célok igen.

A forrássorrend, módosított slot/kind/stack/hash, minden támogatott egyszerű
altípus, kategoriális tilalmak és a korábbi, független példányszintű +30-as
próba is része a célzott ellenőrzésnek. Naplók: `build/item-categories-20261027/`.
A pontos ellenőrzésszámokat, forráshasheket, SQL-környezetet és buildfüggőségeket
a jelentés melletti `2026-10-27_accessory_material_catalog.evidence.json` rögzíti.
A korábbi evidence fájlok saját korábbi munkafás pillanatukat őrzik.

## Következő munka

A célzott audit feltárta az ékszer-socket grade/lejárat, vnum-alapú kőillesztés,
belt inventory és DS-refinement material külön szemantikáját. Ezek következő
típusos profiljai, valamint a többi itemkategória, a katalógusbetöltő, az instance
mentés/fejlesztési tranzakció és a Unity bekötése még szükséges. A részletes
forrásbizonyíték és a jelenlegi határ: [IC-007](../../adr/item_combat_007_accessory_material_catalog.md).

Nincs régi adat eldobása vagy teljes importnak nevezett részleges konverzió.
A teljes P0/P1/P2 audit és kiadási kapu továbbra is nyitott. Csak helyi forrás-
és buildmódosítás, illetve saját ideiglenes DB-próba történt; a meglévő
játékadatbázis, a futó szerver, a Unity és a távoli gép változatlan.
