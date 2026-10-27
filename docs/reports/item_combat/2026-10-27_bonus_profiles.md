# Item/combat migráció — bónuszregiszter és profilbekötés

Az ezt követő felhasználói prioritáspontosítás és a +9-en túli friss proof:
[profilból vezérelt fejlesztési maximum](2026-10-27_refinement_ceiling.md).
A későbbi négykategóriás katalógus és hash v3:
[ékszer/alapanyag bővítés](2026-10-27_accessory_material_catalog.md).

2026-10-27. Ág: `codex/mission-engine`; kiinduló HEAD: `ac4a7cf3`.
**Elkészült a típusos bonus-profile validátor és bekötése az izolált
weapon/armor katalógusba. A teljes item/combat migráció még folyamatban van.**

## Elkészült

- 25 zárt, draft BonusId: egység, célstat/szakasz, filterek, forrás, stack policy,
  capnév és egyedi értékhatár. Egy közös szerkesztői regiszterből készül a
  C++ header és SQL seed; build közben kötelező driftellenőrzés fut.
- Saját adatot birtokló, rendezett, validált bónuszprofilok. Ismeretlen/ismétlődő
  bónusz, túlméretes vagy üres profil, hibás forrás és tartománytúllépés hibát ad.
- Base és refinement külön forrás. Hiányzó/duplikált kontextus nem oldódik fel
  érkezési sorrend alapján. Az aktuális fejlesztési szint teljes extra profilja
  külön érhető el; az előző szinteket nem összegezzük újra.
- A katalógus sémája és SHA-256 formátuma v2-re váltott. A regiszter teljes
  szemantikája és a bónuszok értéke is a hash része. A régi verzió elutasítandó.
- Két új DDL/seed fájl; a fejlesztési `protos` séma most 16 táblából áll.
  Az SQL- és C++-próba külön ellenőrzi az egységeket, FK-kat, forrásokat és
  a nullás/aktuális szint szabályait. A DDL továbbra sem fut meglévő játék-DB-n.
- Bővült a bonus audit: 122 APPLY-név feltételes feature-jelöléssel. Közülük
  8 leképezési jelölt, 16 részletes szemantikai akadály és 98 nem vizsgált sor.
  A jelölt nem jóváhagyott konverzió vagy balance.

## Friss ellenőrzések

- **13/13 normál CTest**, benne **9/9 izolált MariaDB-eset**.
- **12/12 célzott CTest az ASAN buildből**. A Python-generátorteszt és digest
  oracle Pythonban fut; ezekből nem lesz ASAN-instrumentált teszt.
- **7/7 külön standalone CTest**, a GameServer/Database projektek nélkül.
- Natív suite-ok: **229 bonus-profile**, **87 katalógus**, **33 refinement**,
  **197442 ItemKey** ellenőrzés. A bónuszteszt minden regisztrált definíció
  pontos alsó/felső értékét és a határon kívüli elutasítást mindkét forrásnál
  vizsgálja. A generator 4 Python-esete malformed inputot, szemantikai
  kombinációkat, reprodukálhatóságot és írás nélküli driftellenőrzést tesztel.
- A meglévő Mission core/reward/resource, item runtime és combat state célok
  a célzott normál és ASAN körben sikeresek.

Naplók: `build/item-bonus-20261027/`. A forráshashek, tényleges tesztnevek,
DB-környezet és linkfüggőségek a jelentés melletti
`2026-10-27_bonus_profiles.evidence.json` fájlban szerepelnek.
A korábbi foundation/equipment evidence a saját korábbi munkafás pillanatát őrzi.

```powershell
cmake --build build --config RelWithDebInfo --target ItemKeyTests ItemCatalogTests RefinementProfileTests BonusProfileTests --parallel 2
cmake --build build-asan --config RelWithDebInfo --target ItemKeyTests ItemCatalogTests RefinementProfileTests BonusProfileTests --parallel 2
ctest --test-dir build -C RelWithDebInfo -R "^(item_key|item_catalog|refinement_profiles|bonus_profiles|item_bonus_generator|item_catalog_digest|item_category_generator|item_combat_schema_foundation|item_runtime|combat_state|mission_core|mission_rewards|mission_resources)$" --output-on-failure
ctest --test-dir build-asan -C RelWithDebInfo -R "^(item_key|item_catalog|refinement_profiles|bonus_profiles|item_bonus_generator|item_catalog_digest|item_category_generator|item_runtime|combat_state|mission_core|mission_rewards|mission_resources)$" --output-on-failure
```

## Következő lépések és korlátok

Az új profilok még nem változtatják meg a futó játék statjait. Nincs kész
StatBuilder, buff-stack feloldó, aggregált capkezelés, CombatKernel vagy
SQL→catalog loader. A 25 definíció nem fedi le az összes régi apply/POINT,
immune/proc, family/rank/element, skill, pool és source szemantikát.
A részletes szerződés: [ADR IC-005](../../adr/item_combat_005_bonus_profiles.md).

Következik a további kategóriák tényleges típusos profiljainak megépítése és
a kapcsolódó audit folytatása, majd a meglévő prepared SQL infrastruktúrára
épülő konzisztens katalógusbetöltő/publisher és a publikus kliensprojekció.
Például a material kategória DS-refine altípusait a jelenlegi `DragonSoul.cpp`
és `dragon_soul_table.cpp` is külön viselkedésként olvassa; ezeket nem lehet
egy üres material címkével veszteségmentesnek nevezni.

Továbbra is szükséges az instance-perzisztencia és a determinisztikus harcmag,
majd a Mission/Dungeon és Unity közvetlen migrációja. A teljes P0/P1/P2 nincs
lezárva, a kiadható új játék-runtime még nem készült el.

Csak helyi build és saját ideiglenes DB-próba történt. Nem változott a Unity,
a jelenlegi GameServer/Database működése, a helyi játékadatbázis vagy távoli gép.
Nem történt telepítés, push, FreeBSD-build, klienses smoke vagy production benchmark.
