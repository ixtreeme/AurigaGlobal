# Fejlődő petek — EXP és evolúció

2026-10-29, `codex/mission-engine`. Kezdő HEAD: `0880bfff`.

## Elkészült

- Típusos fejlődési profil és külön példány-progresszió: szint, evolúciós
  szakasz, mob EXP és tárgy EXP. A többi petadatnak külön tulajdonosa marad.
- A 15-ös kategória harmadik, fejlődő pet típusa profilhivatkozást kapott.
  Egy hívótárgy-prototípus marad; a pet saját szintje külön adat a tárgy +N
  fejlesztésétől. Fejlődő pethez a kovács/refinement profil +0/None.
- Beállítható 1..255 maximum, teljes szintenkénti EXP-tábla, legfeljebb 16
  evolúciós szakasz, szakaszonként explicit forma. A 40/60/80 kapus, 120-as
  maximumú és a 255-ös maximumú működés is tesztelt.
- Mob EXP-ből önálló, illetve mindkét EXP-t igénylő szintlépési policy.
  Kapunál külön evolúció kell. Az evolúció egy szintet is lép és nullázza
  az EXP-et; ezt a jelenlegi `ENABLE_NEW_PET_EDITS` ág alapján rögzítettük.
- Birtokolt, mellékhatásmentes műveleti terv: owner-, példány-, itemrevision-,
  progressionrevision-, operation ID-, ruleset- és katalóguspinnel. Nincs RNG,
  SQL/ECS-írás vagy betöltésből kiváltott szintlépés. A fölös EXP explicit
  kimenet, a túlcsordulás és inkompatibilis mentett állapot elutasított.
- SQL v20, natív betöltés és kanonikus hash. A három új tábla:
  `growth_pet_profile`, `growth_pet_level`, `growth_pet_stage`. A teljes friss
  000..026 lánc 64 natív katalógustáblát kezel. A régi v19 közvetlen betöltése
  elutasított; ez még nem működő adatbázisra telepíthető általános updater.
- A Qt-tárgyszerkesztőben külön fejlődő pet választás, maximum, EXP-tábla,
  evolúciós kapuk és formák. Másolás után eltérő profil csak az új tárgyat
  módosítja. A kezdeti forma egy helyen szerkeszthető. A maximum ideiglenes
  csökkentése nem veszti el a magasabb szintek piszkozatait.

## Ellenőrzések

| Ellenőrzés | Eredmény |
|---|---:|
| Natív progression ellenőrzések normál és ASAN buildben | 179 és 179 |
| Teljes normál szerver CTest, Mission/raid regressziókkal | 129/129 |
| Célzott végső ASAN-kör | 15/15 |
| Qt + modell + izolált MariaDB normál helperrel | 64/64 |
| Ugyanez ASAN helperrel | 64/64 |
| Friss séma SQL-ellenőrzései | 36/36 |
| Végső panel/modell/rollback ellenőrzés normál és ASAN helperrel | 3/3 és 3/3 |
| Végső natív companion/progression/hash ellenőrzés | 3/3 |

A tesztek a teljes 1–120-as utat, külön evolúciós kapukat, két EXP-poolt,
maximumot, egészszámhatárokat, hibás állapotokat, rögzített formákat,
copy-on-write profilt és szándékos gyermekíráshiba teljes rollbackjét fedik.
A végső ASAN target közvetlen sanitizer-beállítása és a generált projekt
`/fsanitize=address` kapcsolója ellenőrizve. A Qt-panel képi ellenőrzése is
megtörtént. A tesztek helyben, Windows/MSVC alatt futottak; FreeBSD és Unity
integrációs próba ebben a lépésben nem volt.

Független Python encoder és a natív C++ kód szerint a kitöltött progression
fixture SHA-256 értéke:
`ebea12e743b34d17045adcd8865e2eb0e3cf48d1b82a2b27d888a6d372058b58`.

A meglévő felhasználói katalógus v7→v20 átemelése külön SQL-sandboxban,
kulcs/cella/számláló-megőrzéssel visszaellenőrizve:
`96f2bc6c9f3adfbaee55736a821c3746574c6b9f7a86936a03406b2eb63ff1c8`.
Ez az egy meglévő fegyver megőrzési próbája, nem a teljes régi item_proto
vagy petállomány kész konverziója. Az aktív helyi `protos` 33 táblája továbbra
is cellára/metaadatra egyezik a rögzített v7 mentéssel. A sandboxok leálltak;
telepített appot, játékot, Unityt és távoli gépet nem módosítottunk.

## A forrásleltár fontos eredményei

A rögzített mentésben 11 natív fejlődőpet-hívó, 11 tojás, 12 képességkönyv,
1 szállítódoboz, 4 feltámasztó/időnövelő, 2 enchant és 3 skill-reset tárgy
szerepel. A 37-es régi típus a könyveket és a dobozt is tartalmazza; ez önmagában
nem jelenti, hogy egy tárgy fejlődő pet. A korábbi companion audit
`fixed_pet` jelölése quest-hivatkozási jelöltlista volt; 11 sora a natív
fejlődőpet-kód által is kezelt pecsét.

**A rögzített quest e 11 pecsétnél még `pet.summon`-t, azaz a fix PetSystemet
hívja.** A fejlődő rendszer külön `newpet` API-ja létezik a forrásban, de a
rögzített Lua-fájlokban nem található hozzá hívás. Ez tartalmi/dispatch
ellentmondás: az új modell nem kapcsolja át észrevétlenül a mai fix működést
fejlődővé. A konverziós megfeleltetés külön ellenőrzött döntést igényel.

Öt tojásnál az item proto `value0` célja eltér a régi keltetőkód egg+300
céljától. A régi szállítás egy korábbi pet-sor ID-ját és szűkített attr/socket
projekciót tárol; ezekből nem szabad teljes petállapotot újragyártani.
A `minAge` mezőt a jelenlegi kód UTC-korkezdetként használja, nem eltelt időként.

A mentésben **0 `new_petsystem` sor és 0 érintett játékostárgy** található.
Ezért valódi, meglévő fejlődő pet betöltési/visszamentési próbája nem igazolható
ebből a mentésből; a tesztek explicit fixture-ökkel futottak. A leltár nem
tartalmaz játékos- vagy petneveket, minden forrástartalom `converted=false`.

## Ami még hátravan

**A teljes fejlődő pet és az új itemrendszer játékbekötése még nincs kész.**
Következő a fejlődő pet létrehozása/keltetése, életideje, etetése és a hozzá
tartozó tárgyak. Utána képesség/kor/bónusz, szállítás, revive/enchant és skin.
A progression-writernek csak a saját oszlopait szabad módosítania; név,
képesség, bónusz, életidő és egyéb állapot nem rekonstruálható a progressionből.

Maradnak további F2 típusok és az F3..F7 tartós writer, GameServer/Database,
Unity/protokoll, teljes offline konverzió és helyi játékpróba. A másik
prototípusra vezető fegyverátalakítás bónusz-/foglalat-átviteli szabálya és
tartós végrehajtása szintén a még elkészítendő műveletek része.

- [IC-027 szerződés](../../adr/item_combat_027_growth_pet_progression.md)
- [Forrásleltár](2026-10-29_growth_pets.audit.json)
- [Forráshash-ek és tesztbizonyítékok](2026-10-29_growth_pets.evidence.json)
