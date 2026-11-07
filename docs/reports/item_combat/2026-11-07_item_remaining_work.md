# Az itemrendszer fennmaradó munkái — commit- és kódaudit

Átvizsgált szerverág: `20261103`, kiinduló commit `05ca0871`.
Unity: `/main/unity63-integration-20261103`, kiinduló changeset `cs:86`.

## Elkészült játékfolyamok

A `C1–C24`, `C25a–d` és `C26b–q` commitok alapján a külön típusos
felületen már működik a leltár és tárgymozgatás, felszerelés és stat,
gyógyítás és tartós HP/SP-hatások, kovácsolás, földi tárgyéletciklus,
NPC-bolt, játékoscsere, crafting, NPC-s tárgyleadó Mission, ládák és kulcsok,
pénzjegyek, foglalat- és attribútumszolgáltatások, könyvek és könyvsegédek,
átváltoztatás, halfeldolgozás, eszközök és gyűjtés/halászminijáték.
A fejlődő pet tervezett teljes csomagját a C26q lezárta, a
[külön jelentésben](2026-11-07_pet_completion.md) felsorolt funkciókkal.

Ezekhez szerver- és Unity-bekötés, natív/SQL-próba és helyi játékpróba
tartozik. A történeti ADR-ek korai „későbbi szelet” mondatai az adott
commit akkori határát írják le; az aktuális állapotot a későbbi runtime
commitok és jelentések adják.

## Következő munkacsomagok

| Sorrend | Még hiányzó rész | Kódban ellenőrzött határ |
|---|---|---|
| 1. C27a — elkészült | Állandó típusos hátasra felülés és leszállás, tartós szándékkal | A kiinduló kód csak Summon/Dismiss műveletet fogadott. Ebben a munkában elkészült a közös szerver/Unity-út, 171 élő ellenőrzéssel és háromféle újrabelépéssel. |
| 2. C27b — elkészült | Időkorlátos fix petek és hátasok; fix társak bónuszai; típusos repülési engedély | Mindkét fix társ négy órával támogatott; a bónusz a tényleges követéshez/felüléshez kötött. Saját typed repülési engedély, szerver/Unity-bekötés, nyolc élő eset és 829 ellenőrzés sikeres, lejárattal és újrabelépéssel. [Jelentés](2026-11-07_fixed_companions.md). |
| 3. C28 — elkészült | Test-, haj- és fegyverjelmez felszerelése, modellje és tartós időzítői | A tartós runtime, szerver/Unity-felszerelés, három jelmezmodell és mind a négy óra kész; 12 élő eset és 1606 ellenőrzés, felszerelt/levett/lejárt újrabelépéssel. [Jelentés](2026-11-07_costumes.md), [IC-063](../../adr/item_combat_063_costume_runtime.md). |
| 4. C29 — elkészült | Hat bónuszos stóla, hét rúna, részleges töltés, jelmezreset/ritka donor/törlés, kapcsolt addon | Tartós példányállapot, külön szerver/Unity-művelet, stat- és modell-/effektút. [Jelentés](2026-11-07_regalia_special_bonuses.md), [IC-064](../../adr/item_combat_064_regalia_special_bonuses.md). Az önálló Acce-elnyelés és a teljes APPLY-konverzió ettől külön feladat. |
| 5. Önálló lélektárgyak | Töltés, fejlesztés és a tényleges találathoz kötött egyszeri fogyás, tartós aktív állapot | Az új Soul domain és katalógus létezik, `ItemSoulRow` még nincs a tartós runtime támogatási listában. [IC-025](../../adr/item_combat_025_independent_soul_items.md). |
| 6. Lőszer és további fejlesztési segédek | Typed íj/lőszer-életciklus, elfogadott lövéshez kötött fogyás; még hiányzó kovácssegéd-utak | `EquipmentStats.cpp` elutasítja a `AmmoKind::None`-tól eltérő igényű fegyver felvételét. A könyvsegéd nem általános kovácssegéd. [IC-039](../../adr/item_combat_039_equipment_stats.md). |
| 7. Fiókpénztárca és további tárolók | Fiókszintű valuta, typed bank/mall/céh/levél tárgyutak és kapcsolódó gazdasági műveletek | A modell tároló-/tulajdonostípusa önmagában nem játékbeli bekötés. A jelenlegi typed item-hálózati kezelőkben ezeknek nincs végigvezetett kliensművelete; a fiókpénztárcát az IC-034 is nyitottnak jelöli. |
| 8. Mission és quest | Az általános Mission-események, tárgykövetelmények/jutalmak és Lua-questek teljes typed átállítása | A C12 az NPC-s tárgyleadó tranzakciót készítette el. A teljes missziórendszer ettől még külön, nagyobb munka. |
| 9. Teljes tartalom- és karakterkonverzió | Prototípusok, példányok, receptek, droppok, boltok és minden hivatkozás egyszeri konverziója; régi tárgyút kivezetése | A konverteralap és auditok megvannak; a normál legacy leltár átállítását a felhasználó korábban későbbre kérte. Nem kapcsoljuk át e részfeladat mellékhatásaként. |
| 10. Kiadási ellenőrzés és grafikai integráció | Teljes világjelenet, távoli avatarok typed megjelenésének végső próbája, standalone kliens, FreeBSD, teljes gazdasági/mission regresszió és terhelés | A C28 publikálja a typed alap- és jelmezmodelleket a megfigyelőnek, az izolált próba tényleges mesheket hoz létre mindkét actoron. A teljes világjelenetes, animációs és production Addressables-próba továbbra is szükséges. |

## Az audit módszere

Az összesített terv (`IC-034`) mellett a tényleges `git log` sorozat,
`PersistentItems.cpp::SupportsPersistentItem`, `MakeCompanionCommit`,
`TypedCompanions.cpp`, `EquipmentStats.cpp`, `MountFlightSystem.hpp` és
az érintett Unity leltár-/codecút került összevetésre. Egy domainmodell,
szerkesztőlap vagy SQL-séma megléte önmagában nem számít befejezett
játékfunkciónak.

Az első kiválasztott rész a C27a. Eredménye és pontos próbahatóköre a
[hátasjelentésben](2026-11-07_typed_mount_riding.md) szerepel.
A C27b, C28 és C29 is elkészült; a következő nyitott munkacsomag az önálló
lélektárgyak töltése, fejlesztése és találathoz kötött fogyása szerveren és Unityben.
