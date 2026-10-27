# ADR IC-005 — Típusos bónuszregiszter és katalógusprofilok

A bónuszprofil-szerződés megmarad; a hash v2-t és a két kategóriára szűkített
akkori sémaállapotot az [IC-007 hash v3 és kategóriabővítés](item_combat_007_accessory_material_catalog.md) követte.

2026-10-27. Állapot: **izolált P2-részmegvalósítás; draft szabályleírás,
nem jóváhagyott balance és nem runtime combat integráció**.

## Forrás és határ

A specifikáció 7.2. pontjának első zárt regisztere 25 új `BonusId`-t ír le.
Egyetlen szerkesztett forrása a `config/item_combat/bonus_registry.json`; ebből
ugyanaz a generátor állítja elő a C++ definíciókat és a friss `protos` SQL seedjét.
A build ellenőrzi mindkét kimenet változatlanságát. Hibás inputnál nincs generálás;
a `--check` nem ír fájlt. A kiadott ID-k nem számozhatók át vagy használhatók újra.

A JSON szerkesztői regiszter, nem általános JSON item-proto. A runtime célstruktúra
típusos és immutable; nem parse-ol régi APPLY-t, POINT-ot, vnumot vagy value-tömböt.

Minden definícióban van név, megjelenítési kulcs, egység, célstat, pipeline-szakasz,
channel/action/mode/family/rank/element filter, stack policy, aggregált cap neve,
engedélyezett forrás és egyedi alsó/felső határ. A generátor az enumokon túl a
cél–egység–szakasz–filter–cap kombinációt is ellenőrzi. Például normál támadás
bónusza nem kerülhet egyidejűleg PvE módhoz kötött bucketbe, a fizikai penetráció
nem lehet implicit összcsatornás.

Az első 25 definíció: négy flat támadó/védő stat, két támadó statszázalék,
normal/skill és PvE/PvP outgoing/incoming, fizikai/mágikus flat és százalékos
penetráció, fizikai/mágikus resistance, kritikus esély/ellenállás/extra szorzó,
block reduction és evasion. Mindegyik aktuális definíció Add policy-t használ.
A további stack enumok fenntartott típuskódok; működő buff-összevonót ez a rész
nem implementál. Family/rank/element specifikus és proc bónusz még nem regisztrált;
ilyen szemantikát a generátor nem enged be általános ismeretlen filterrel.

## Egységek és capek

A `BonusEntry.valueScaled` int64 értékét a regisztrált BonusId egysége értelmezi.
Raw: 1000/pont, strukturális tartomány legfeljebb ±10^15. Bp: 10000=100%; az
általános egyedi strukturális határ ±100000. Penetráció százalék és block esetén
az első definíció egyedi tartománya 0..10000. Ezek draft inputkorlátok; nem az
összevont gameplay értékek végleges balance capjei.

Az egyedi sor kívül eső értéke hibát ad, soha nem clampel. Az aggregált cap
azonosítóját a későbbi verziózott ruleset/StatBuilder fogja számszerűsíteni és
alkalmazni. Itt nincs összeadás, százalékszorzás, RNG, HP-írás vagy régi harci
képletre való visszahívás. A negative flat/bp módosító strukturálisan támogatott;
kezelése az új StatBuilder rögzített sorrendjének feladata.

## Profil- és refinement-szerződés

A `BonusProfile` egyetlen forrástípushoz tartozik: ItemBase=1 vagy Refinement=2.
Pozitív ID, egyedi tartalmi kód, 1..128 bejegyzés kell; a sorok BonusId szerint
rendeződnek, ismétlődő/ismeretlen ID elutasítandó. A megengedett forrást és
értéktartományt minden bejegyzésnél a regiszter ellenőrzi. Legfeljebb 4096 profil
lehet. Az üres profil nem jelent csendes bónuszelhagyást; hiányzó bónuszhoz null
referencia kell.

A katalógus minden profilt ellenőriz, az árva/nem használtakat is. Base csak
ItemBase profilt, refinement csak Refinement profilt hivatkozhat. A refinement
validátora a teljes validált kontextust kapja; hiányzó, nulla vagy duplikált ID,
mozgatás után üres profil és rossz forrás nem adhat érvényes referenciát.
A kontextus sorrendje nem befolyásolja az eredményt; a szintek lookupja rendezett
indexből történik. A katalógus későbbi bonus-lookupja bináris keresés.

A `BonusProfilesAt(key, level)` az alap és **pontosan az aktuális szint** profilját
adja vissza. A szintek extra profiljai a +0-hoz képest teljes, kumulatív tartalmak,
nem az előző szinthez adandó delták. Ha +3-on van extra profil és +4-en nincs,
+4-en nincs extra bónusz; tartalomépítőnek kell explicit minden kívánt szintet
megadnia. A +0 extra profilja tiltott. A `RefinedBaseStats` továbbra is kizárólag
alap+növekményt számol, így egy másik statalkalmazó nem kap előre hozzáadott bónuszt.
A visszaadott mutatók használata alatt meg kell tartani a katalógus snapshotját.

## SQL és hash v2

A fejlesztési `bonus_profile` fej kapott `source_kind` mezőt;
`006_bonus_profiles.sql` külön definition és típusos profile_entry táblát ad.
PK/FK/CHECK védi az identitást és strukturális tartományt. A profil nemüressége,
128-as elemszáma, per-definition szűkebb értékhatára és forrásegyezése cross-row
szabály: ezeket a C++ validátor végzi, nem állítunk teljes SQL constraint lefedést.
A későbbi SQL loadernek az adatbázisbeli definíciók és a lefordított regiszter
teljes egyezését is igazolnia kell. SQL-ben átírt unit nem módosíthat runtime
BonusId-jelentést. A loader még nincs bekötve.

A `BuildItemCatalog` most csak schemaVersion=2-t fogad. Hash domain:
`auriga.item_catalog.v2`. Az IC-004 tíz táblája után u32 registry schema,
hosszal kódolt registry status, 11-es tagű definition tábla, majd 12-es tagű
bonus-profile tábla következik. A definition sorrend: u32 ID, string code/display,
u8 unit/target/stage/channel/action/mode, u16 family/rank,
u8 element/stack/cap/source mask, i64 min/max. A profil: u32 ID, string code,
u8 source, u32 elemszám, soronként u32 BonusId és i64 valueScaled.
Minden integer explicit little-endian; a signed érték modulo 2^N kódja nem
struct-reprezentáció. A profilok és bejegyzések ID szerint rendezettek.

A teljes regiszter minden szemantikája a hash része, a még nem hivatkozott
definícióké is. A release revision továbbra is külön identitás. Nincs v1/v2
runtime fallback, régi klienshez kompatibilitási adapter vagy publikus manifest
ezzel késznek nyilvánítva. A független Python v2 fixture digestje:
`783e8a81843d3d537e1a3c9dc40a9e3c516d552a267a90aeb76dab2491af6482`.

## Migrációs auditkapu

A `docs/audits/item_bonus_mapping.csv` 122 APPLY-nevet leltároz az enumforrásból,
beleértve a feltételes feature neveket. Ez nem 122 kész szemantikai megfeleltetés:
8 ellenőrzött jelölt és 16 dokumentált szemantikai akadály mellett 98 sor még
nem vizsgált. A jelölt unitkonverzió is offline, checked művelet lesz; a nyolc
jelölt sem production tartalomengedély. A régi képletek sorrendje és egyes
normal/skill/PvP kivételei eltérnek az új specifikációtól.

A HP/SP arányos változtatás, bitcsomagolt skill bonus, random block/dodge/pierce,
rekurzív reflect, statfüggőségek és további source/capability mapping az érintett
tartalom publikálását továbbra is blokkolja. A teljes P0/P2 audit, további
kategóriák és a StatBuilder/CombatKernel ettől még nincsenek készen.
