# IC-027 — Fejlődő pet: szint és evolúció

2026-10-29. IC-012 F2 folytatás, kezdő HEAD `0880bfff`.

## Audit és hatókör

A `service.h` bekapcsolja a `__NEWPET_SYSTEM__` és `ENABLE_NEW_PET_EDITS`
ágakat. A `New_PetSystem.cpp::SetActorExp` ebben a buildben a mob EXP-ből
önállóan is szintet léptet. A 40/60/80 kapunál külön evolúció kell;
`ActorIncreasePetEvolution` egy szintet is hozzáad és nullázza a két EXP-et.
Normál szintlépéskor nincs `IncreasePetBonus` ebben az ágban. A fejlődési
szint nem a hívótárgy kovács/refinement szintje.

A natív fejlődő pet kód hívói 55701..55711 quest típusú tárgyak, a 37-es
itemtípusban könyvek és szállítódoboz is van. A rögzített `pet_system.lua`
ugyanakkor e 11 tárgyra `pet.summon`-t hív: a `questlua_pet.cpp` a fix
PetSystemhez köt, nem a külön `newpet.summon`-hoz. A rögzített Lua-fájlokban
`newpet` hívás nem található. A korábbi quest-hivatkozási leltár ezért nem
végleges viselkedésbesorolás. Konverziókor nem szabad automatikusan fejlődővé
tenni a ma fixként használt pecsétet; a feloldás külön tartalmi döntés.
`pet_input.cpp::BraveRequestPetName` egg+300-at használ,
a proto `value0` pedig az 55407..55411 tojásoknál más hívót jelöl. A forrás
ellentmondásait csak offline, explicit tartalomdöntéssel oldjuk fel.

Ez a lépés a fejlődési profil, EXP és evolúció típusszerződését, katalógusát,
SQL-jét és szerkesztését valósítja meg. A keltetés, etetés, élettartam,
képességek, kor/bónusz, szállítás, feltámasztás, enchant és skin későbbi külön
műveletek. Nem deklaráljuk ezzel a teljes fejlődő pet működését késznek.

## Új szerződés

- A 15-ös `item_companion` harmadik típusa a fejlődő pet, kötelező
  `growth_profile_id` hivatkozással; fix pet/mountnál ez SQL NULL.
- A fejlődő pet nem lovagolható. Kezdeti formája a profil első szakaszának
  formája. További formák a meglevő append-only companion formaregiszterből
  jönnek; nincs új item-vnum vagy számjegy-aritmetika.
- A pet maximumszintje 1..255, minden 1..max szinthez explicit mob- és
  item-EXP küszöb tartozik. A végső szinten mindkét küszöb nulla.
- `MobOnly` és `MobAndItem` policy: az első a jelenlegi build külön mob-EXP
  szintlépését is leírja. Az item EXP külön megmarad; nem mosódik össze a mob
  EXP-pel. A két küszöb arányát a szerkesztő nem találja ki.
- A szakaszok 0-tól folytonosak, legfeljebb 16-an vannak; szigorúan növekvő
  szintplafonjuk utolsó értéke a maximum. A plafonra éréskor nincs automatikus
  evolúció. Az explicit evolúció a következő szakaszra és egy szinttel feljebb
  lép, a jelenlegi build szerint EXP-nullázással.
- Egy EXP-művelet legfeljebb egy szintet léptet. Sikeres szintlépés a két
  számlálót nullázza; a nem elkönyvelt többletet a terv külön jelzi. Kapunál
  és maximumon a fölös mennyiség ugyancsak explicit kimenet. Nincs overflow.
- A `GrowthPetProgress` csak a szintet, szakaszt és két EXP-et birtokolja.
  Név, életidő, kor, képesség, cooldown, bónusz, recall és szállítási állapot
  nem írható felül belőle. Betöltés/validálás soha nem szintléptetés.
- A hívótárgy refinement profilja `None`, +0. A fix companion +N rendszere és
  a pet saját szintje nem helyettesítheti egymást. A fix `PlanCompanion`
  visszautasítja a fejlődő petet; az nem kerülhet kerülőúton fix idézésre.

## Művelet és későbbi owner

A `PlanGrowthPetProgress` tiszta terv, egy szerver által feljogosított, élő
pethez. Kötelező owner/actor/példány/revision/operation ID, rögzített katalógus
és ruleset. Nincs SQL, ECS, RNG vagy kliens inputból származó hiteles EXP.
Award és evolve külön típusos parancs. A létrehozási factory nem reload API.

F3/F4-ben az owner ellenőrzi az aktív actor generációját, a sessiont, a
progression- és itemrevisiont, az operation jogosságát és a katalóguspint.
Egy receiptben menti a pontos előző/új progression állapotot, kizárólag annak
oszlopait. Minden más petadatot megőriz. CommitUnknown esetén operation ID
szerinti egyeztetés kell. Csak commit után változhat a forma, stat vagy UI.
A kapu önmagában nem engedély egy quest/NPC evolúciós szolgáltatására.

## Katalógus és ellenőrzés

Nem kiadott v20; kanonikus domain `auriga.item_catalog.v20`, új hash-tag 44.
Az `item_companion` nullable profilhivatkozást kap. Három új tábla:
`growth_pet_profile`, `growth_pet_level`, `growth_pet_stage`; 000..026 friss
migrációs lánc, 64 natív katalógustábla. A 025 draft meglévő kind-constraintje
csak nevet kapott, a jelentése ott változatlan. Létező v19-re ez nem általános
adatmegőrző telepítő; a működő v7 helyi katalógus nem módosul.

A Qt-petválasztó harmadik típusa a fejlődő pet. A maximum, policy, teljes EXP-
tábla és szakaszok szerkeszthetők. Az induló szakasz a követőforma-választót
használja, nem kér két eltérően írható kezdőformát. Új szintsorok 1 EXP-es
szerkesztési minták, nem a régi szerver balance-adatainak konverziói.
A profil másoláskor megosztható, eltérő szerkesztéskor copy-on-write; hibás
gyermekírás a formát/profilt és kulcsszámlálót is visszagörgeti.

Inkompatibilis maximum-, kapu- vagy EXP-küszöbváltozás kiadási migrációt vagy
régi pint igényel. A planner hibával áll meg; a mentett állapotot nem clampeli
új tartalomhoz. A 2026-10-28-as mentéshez sorhashes, név nélküli audit tartozik;
nem írjuk át a működő játékot vagy adatbázist.
