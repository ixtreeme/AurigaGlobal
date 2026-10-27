# ADR IC-007 — Ékszerek és egyszerű alapanyagok típusos katalógusa

Történeti v3 részszállítás. Az aktuális v4 foglalatprofil és timer-szerződés:
[IC-008](item_combat_008_accessory_socket_profiles.md). Az alábbi hash és
fennmaradó kapuk ennek a korábbi részszállításnak az állapotát írják le.

2026-10-27. Állapot: **izolált, draft P1/P2-részmegvalósítás**.
A cél továbbra is egy szemantikai tárgytípushoz egyetlen prototípus,
fejlesztési szintenként új tárgysor nélkül.

## Ellenőrzött forráshatárok

| Jelenlegi szimbólum | Megfigyelés | Következmény |
|---|---|---|
| `common/item_length.h::EArmorSubTypes` | Wrist, neck és ear a régi armor enum részei | Az új katalógusban Accessory=3, saját slotkódokkal |
| `ItemSystem.cpp::FindEquipCell` | A három ékszert a megfelelő wear flag külön slotba helyezi | Slot nem vezethető le egy általános armor mezőből |
| `ItemAttributeSystem.cpp::GetItemAttributeSetIndex` | Wrist/neck/ear külön attribute set | Affix- és régi apply-konverzió még szükséges |
| `PointSystem.cpp::ComputeBattlePoints` | Armor value1/value5 alapvédelmet body/head/feet/shield, feltételesen pendant ad; wrist/neck/ear nincs ebben az ágban | Az új egyszerű ékszer statjai explicit bonus profilból jönnek; nincs rejtett armor alap |
| `InventorySystem.cpp::ModifyPoints` | Proto-apply, külön socket-grade hatás; a socket grade a bónuszt százalékkal és minimumértékkel növeli | A socket-grade nem refinement_level; külön typed profil és állapot kell |
| `ItemSystem.cpp::AccessorySocketDegrade`, `SetItemAccessorySocketGrade` | Grade és lejárati idő külön socketmezőkben, timeres visszaminősítés | A régi ékszerek teljes importját nem tekintjük megoldottnak |
| `ItemSystem.cpp::CanPutInto/CanPutInto2` | Konkrét vnumcsaládok és számtani illesztések mondják meg a kompatibilis ékszerkövet | Új explicit kulcs-/profilkapcsolat kell, runtime vnumképlet nélkül |
| `common/item_length.h::EMaterialSubTypes`, `ItemSystem_LegacyBridge.cpp::UseItemEx` | Leather/blood/root/needle/jewel egyszerű alapanyagok; az ITEM_MATERIAL nem kap közvetlen aktív effektust ebben az ágban | Az első öt ordinary MaterialKind alapreprezentációja elkészíthető |
| `DragonSoul.cpp::IsDragonSoulRefineMaterial/DoRefineStrength` | Három további material-altípus DS-strength fejlesztő; külön anyag/esély/költség és régi vnumképzés | Ezek nem közönséges jewel alapanyagok; külön Soul/catalyst port kell |
| `dragon_soul_table.cpp::GetRefineStrengthValues` | DS-típus, anyag és strength alapján fee/float probability tábla | A későbbi offline export explicit typed recipe/outcome adatokra bontja |
| `InventorySystem.cpp::EquipmentType`, `HasBeltItems` használata | Belt value5 és inventorytartalom befolyásolja a felszerelést/levételt | Ring/belt/unique teljes működését nem pótolja a mostani három ékszerslot |

Ez célzott forrásaudit, nem minden feature, timer vagy item call graph teljes
szemantikai lezárása. A régi enumokat nem castoljuk az új kódokra.

## Új domain és SQL

Az `ItemCategoryRow` most négy típust tartalmaz: weapon, armor, accessory,
material. A builder mindegyik base-hez pontosan egy megfelelő childot és minden
kötelező profilt ellenőriz. Hiányzó, ismétlődő, idegen kategóriájú vagy árva
child elutasítandó. A 18-as kategóriaregiszter változatlan draft; a többi
kategória publikálása továbbra is tiltott.

`ItemAccessoryRow`: ItemKey, slot és resistance_profile. Új slotkódok:
wrist=6, neck=7, ear=8. A slot explicit kötelező adat; az elhagyott/nullás érték
nem válik automatikusan karkötővé. A resistance hivatkozásnak léteznie kell.
Az ékszer max_stack=1. A base bonus_profile és a refinement aktuális extra
profilja adja a flat és bp statokat a későbbi StatBuildernek.

`RefinementKind::Accessory=4`: teljes 0..max görbe, a 0. szint invariánsai és
a típusos bonus-reference szabályok változatlanok. Minden raw gain mező nulla,
a profil bónuszokkal fejez ki változást. A raw base stat is nulla: így ugyanaz
a bónusz nem kerülhet be egyszer armor gainként, majd másodszor bonusként.
A +9→+10 és +15 működik ugyanazzal az ItemKey-vel. None profilú, nem fejleszthető
ékszer is leírható; a maximumot a tárgy profilja határozza meg.

`ItemMaterialRow`: ItemKey és kötelező MaterialKind. Új kódok: Leather=1,
Blood=2, Root=3, Needle=4, Jewel=5. Az elhagyott/0 vagy nem támogatott kind hibát
ad. Ezek draft új domainkódok, nem a régi enum számai. A max_stack pozitív
uint32 érték; az ordinary material csak None refinement-profilt használhat,
és nem viselhető felszerelés. Nincs OnEquip binding, model-appearance vagy
passzív base-bonus profil. Az ikon továbbra is kötelező közös mező.

Az új `008_accessory_material_catalog.sql` két saját child táblát ad. A közös
base táblában a támogatott kategóriák 1,2,3,6; csak a 6-os stackelhető. A DDL
PK/FK/CHECK szabályokat ad a közvetlen sorfeltételekre. Material→None refinement,
accessory→Accessory/None refinement, teljes level coverage és pontosan egy
child cross-table szabályok, amelyeket a builder ellenőriz.

A módosult 004/005 és új 008 **friss, kiadatlan fejlesztési séma**. Ez nem
meglévő DB-n futtatható élő ALTER/migráció, és a csomag init-db-ja nem futtatja.
Az SQL→C++ loader még nincs kész; a két oldal külön bizonyított.

## Hash v3

Csak schemaVersion=3 fogadható el. A domain `auriga.item_catalog.v3`, az IC-005
táblasorrendje után 13-as taggel accessory (key, u8 slot, u32 resistance ID),
14-es taggel material (key, u8 kind) következik. A key explicit hatbájtos
little-endian adat; a sorok ItemKey szerint rendeződnek. Az új enum- és
profilkódok az érintett sorokban szerepelnek. Nincs v1/v2 fallback.

A négykategóriás fixture SHA-256-ja:
`2a69cd8370a70d3703cf454765cc3ad5869c60b40540056b37b18abe329eccb4`.
Független Python encoder ellenőrzi a C++ bájtfolyamot is. A release revision
továbbra sem része a tartalmi hashnek.

## Fennmaradó kapuk

Socket-grade/compatibility/timer, ring/belt/unique, aktív vagy speciális
material, DS-catalyst/Soul, ore/resource, teljes affix/apply és item-expiry
tartalom nincs portolva. Ezeket nem lehet a mostani egyszerű sorokra
veszteségmentesnek minősített importtal lefordítani. Az offline konverternek
minden ilyen forrásadatot explicit besorolással kell lefednie, vagy blokkolnia
kell az érintett tartalom kiadását; nincs implicit default vagy runtime adapter.

A runtime ItemFactory/instance/refinement-tranzakció, StatBuilder és a Unity
átállítása továbbra is hátravan. Ez nem új éles item-rendszer vagy balance-kiadás.
