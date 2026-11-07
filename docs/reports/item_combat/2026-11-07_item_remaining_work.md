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
| 2. C27b | Időkorlátos fix petek és hátasok; fix társak bónuszai; típusos repülési engedély | `SupportsPersistentItem` a nem fejlődő társaknál csak Permanent órát enged. A fix társak bónuszai nem jutnak a typed statforrásba. A repülés jelenleg legacy felvett pecsétet ellenőriz. |
| 3. Jelmezek | Test-, haj- és fegyverjelmez felszerelése, modellje és tartós időzítői | Az F2 domain/katalógus/szerkesztő létezik, `ItemCostumeRow` még nincs a támogatott tartós runtime tárgyak között. [IC-022](../../adr/item_combat_022_costumes_and_lifetime.md). |
| 4. Stóla, rúna és speciális bónuszműveletek | Saját példányállapot, használat/felszerelés, bónuszátvitel és kapcsolt addon | A normál C18 attribútumszolgáltatás nem zárta le ezeket. [IC-031](../../adr/item_combat_031_instance_attributes.md), [IC-046](../../adr/item_combat_046_attribute_services_runtime.md). |
| 5. Önálló lélektárgyak | Töltés, fejlesztés és a tényleges találathoz kötött egyszeri fogyás, tartós aktív állapot | Az új Soul domain és katalógus létezik, `ItemSoulRow` még nincs a tartós runtime támogatási listában. [IC-025](../../adr/item_combat_025_independent_soul_items.md). |
| 6. Lőszer és további fejlesztési segédek | Typed íj/lőszer-életciklus, elfogadott lövéshez kötött fogyás; még hiányzó kovácssegéd-utak | `EquipmentStats.cpp` elutasítja a `AmmoKind::None`-tól eltérő igényű fegyver felvételét. A könyvsegéd nem általános kovácssegéd. [IC-039](../../adr/item_combat_039_equipment_stats.md). |
| 7. Fiókpénztárca és további tárolók | Fiókszintű valuta, typed bank/mall/céh/levél tárgyutak és kapcsolódó gazdasági műveletek | A modell tároló-/tulajdonostípusa önmagában nem játékbeli bekötés. A jelenlegi typed item-hálózati kezelőkben ezeknek nincs végigvezetett kliensművelete; a fiókpénztárcát az IC-034 is nyitottnak jelöli. |
| 8. Mission és quest | Az általános Mission-események, tárgykövetelmények/jutalmak és Lua-questek teljes typed átállítása | A C12 az NPC-s tárgyleadó tranzakciót készítette el. A teljes missziórendszer ettől még külön, nagyobb munka. |
| 9. Teljes tartalom- és karakterkonverzió | Prototípusok, példányok, receptek, droppok, boltok és minden hivatkozás egyszeri konverziója; régi tárgyút kivezetése | A konverteralap és auditok megvannak; a normál legacy leltár átállítását a felhasználó korábban későbbre kérte. Nem kapcsoljuk át e részfeladat mellékhatásaként. |
| 10. Kiadási ellenőrzés és grafikai integráció | Teljes világjelenet, távoli avatarok typed megjelenése, standalone kliens, FreeBSD, teljes gazdasági/mission regresszió és terhelés | Az izolált Unity-próbák és Windows natív build nem helyettesítik ezeket. A távoli avatar typed felszerelésmodell-publikálása az IC-039 nyitott határa. |

## Az audit módszere

Az összesített terv (`IC-034`) mellett a tényleges `git log` sorozat,
`PersistentItems.cpp::SupportsPersistentItem`, `MakeCompanionCommit`,
`TypedCompanions.cpp`, `EquipmentStats.cpp`, `MountFlightSystem.hpp` és
az érintett Unity leltár-/codecút került összevetésre. Egy domainmodell,
szerkesztőlap vagy SQL-séma megléte önmagában nem számít befejezett
játékfunkciónak.

Az első kiválasztott rész a C27a. Eredménye és pontos próbahatóköre a
[hátasjelentésben](2026-11-07_typed_mount_riding.md) szerepel.
