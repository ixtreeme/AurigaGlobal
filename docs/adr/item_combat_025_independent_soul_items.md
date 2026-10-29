# ADR IC-025 — Különálló vörös/kék lélekrendszer

2026-10-29. IC-012 F2, helyi fejlesztés. Kezdő HEAD: `d2a3085a`.

## Hatókör és forrásbizonyíték

A megmaradó `ITEM_SOUL` külön rendszer: vörös lélek normál támadáshoz,
kék lélek képességhez. A 14-es, már létező kategóriaregiszter-bejegyzést
használja; nincs újraszámozás. A kivezetett DragonSoul modellje, slotjai,
grade/stage képletei és tartalma nem térnek vissza.

Vizsgált forrás: `ItemSystem_LegacyBridge.cpp::ITEM_SOUL/soul_item_event/
DoRefineItemSoul/RefineItem`, `PlayerRuntimeSystem.cpp::GetSoulItemDamage`,
`ItemSystem.cpp::StartSoulItemEventEcs`, `item_manager.cpp::CreateItem`,
az `AffectSystem` életciklus, valamint a `CombatSystem`, `battle.cpp` és
`SkillSystem` Soul-hívásai. A pontos fájlhash-ek a kísérő auditban vannak.

A rögzített mentésben öt vörös és öt kék prototípus, valamint három
Soul-fejlesztőtekercs van. A két ötsoros fejlesztési lánc 70500..70504 és
70505..70509; a tekercsek 70602, 70603, 88958, 100%, 25%, 25% forráseséllyel.
A `value1` a láncokban 1..5; ennek pontos szemantikája és az új 0-alapú
fejlesztési szintekkel való kapcsolata még offline mapping-feladat.
A négy leltározott instance/award táblában e 13 kulcshoz nincs példány.
Ez csak a rögzített mentésre és a leltározott táblákra állítás.

## Típusos prototípusok és állapot

Az `item_soul` két explicit működése `Core` és `RefinementScroll`.
A lélek csatornát és `soul_profile`-t, a tekercs saját sikeresélyt hordoz.
Tekercsnél a csatorna/profil NULL, léleknél a tekercsesély nulla.
Lélekből egy darab lehet egy példányon; tekercs halmozható.

A `soul_level` minden +0..+max szinten megadja a töltéskapacitást,
ciklusköltséget, ciklusonkénti ütéskeretet, kezdeti töltést/ütéskeretet,
egy töltési egység idejét és a három töltési szakasz teljes sebzésszorzóját.
A profilnak pontosan annyi sora van, mint a közös refinement profilnak.
Az új `RefinementKind::Soul=8` kizárólag szintet ad; nem ad nyers felszerelés-
statot, alapbónuszt vagy szintbónuszt. A maximum 0..255; a +10→+11 és +30
maximum tesztelt. Egy tárgy egyetlen prototípus marad minden szintjén.

A kapacitás 1, 2 vagy 3 teljes ciklus, a ciklusköltség pozitív, az ütéskeret
pozitív. A kezdeti állapot kapacitáson belüli, pozitív és a cikluskeretnél
nem nagyobb ütésszámú. A teljes szorzó 10000..1000000 bázispont (1×..100×).
Minden idő 1..INT64_MAX ms; a szerkezeti korlát nem jóváhagyott balance.
A három szorzó neve és jelentése explicit, nem általános value-tömb.

A `SoulState` külön mezői a töltés, a megmaradt ütések, a részintervallum és
az aktív állapot. Teljes töltésnél nincs felhalmozott részintervallum.
A runtime nem értelmez `perc*10000+ütés` socketcsomagolást, item-vnumot vagy
azonosítóból képzett fejlesztési szintet. A `MakeSoulState` a katalógus és
példányszint alapján adja az explicit kezdeti állapotot a majdani factorynak.

## Mellékhatásmentes tervek

A `PlanSoul` teljesen birtokolt bemenetet és eredményt használ, admissionnel,
operation ID-val, ruleset revisionnel, tulajdon/instance/revision és
zárolás/csere/viselés ellenőrzésével. Az aktív lélek saját használati zárolása
nem azonos a másik művelet által tartott `lockedByOther` zárolással.
A terv nem fér registryhez, SQL-hez, Lua-hoz, hálózathoz vagy RNG-hez.

- Aktiválás: saját csatornája szabad, legalább egy ciklusnyi töltés kell;
  nincs tárgyfogyasztás. Csatornánként csak egy aktív példány lehet.
- Deaktiválás: a strukturálisan érvényes állapot takarítása megengedett
  megváltozott használati policy vagy kivezetett prototípus esetén is.
- Töltés: kizárólag a hitelesített owner-óra eltelt idejéből, egy egység/
  intervallum, kapacitásig. Nem tölti vissza az ütéskeretet. Nincs offline
  UTC-időből vagy kliens által küldött időből jóváírás.
- Ütés: aktív, megfelelő csatornájú lélek; játékos→NPC PvE, átváltozás nélkül.
  A terv rögzíti a damage-event és a cél stabil identitását/revisionjét is.
  Pozitív bemeneti sebzés kell. A szakaszt a töltés/ciklusköltség határozza meg.
  Teljes eredmény `floor(inputRaw * multiplierBp / 10000)`, egyszer kerekítve.
  A hányados/maradék alapú ellenőrzés elutasítja az int64 overflow-t.
  Egy ütés fogy; a ciklus utolsó ütésekor a töltés csökken és az ütéskeret
  újraindul. Ha nem marad teljes ciklus, a lélek deaktiválódik.
- Fejlesztés: inaktív lélek, következő létező szint és saját, élő Soul-tekercs
  kell. A tekercs osztály/nem/szint/harci használati policyje is ellenőrzött.
  Egy tekercs mindkét kimenetnél fogy. Kudarc a teljes lélekállapotot tartja;
  siker ugyanazon ItemKey következő szintjére lép, explicit kezdeti Soul-
  állapottal. Nincs új prototípus vagy új item-instance ID.

0% és 100% sikeresélynél a rögzített dobás 0, egyébként 1..10000.
A szokásos smith-recept lélekre/tekercsre tiltott, mert megkerülné ezt az utat
vagy elveszíthetné a példányállapotot. A generikus loot 0-s szintű lélekhez
a típusos factory kezdeti állapotát használhatja; nincs hiányzó formakötés.

Ez még nem perzisztencia- vagy harci író. F3/F4-ben az owner egyetlen rendezett
műveletben újraellenőrzi a sessiont, cél-incarnációt, damage-eventet, mindkét
item revisionjét, katalógus- és ruleset-pineket. Az operation receipt,
ütés-/töltésváltozás, tekercsfogyasztás/szintlépés és a kapcsolódó harci commit
atomikus. Ugyanaz a damage-event nem terhelhet kétszer ütéskeretet.
CommitUnknown után tartós operation-ID egyeztetés előzi meg az újrapróbálást.
Effekt, packet és Mission/Mission Pass esemény csak sikeres commitból születhet.
Csatornacsere két példány deaktiválás/aktiválását együtt foglalja és commitolja.

## Szerkesztő és SQL

A panelen egy helyen választható lélek vagy tekercs. Léleknél külön
maximumállítás és szinttábla van; a közös felszerelésstat/bónusz panel nem
alkalmazható. Ideiglenes maximumcsökkentés után az el nem mentett sorok
visszaállíthatók. A tekercsesély és a szín/profil az alkalmazható módban aktív;
egy hibás, de inaktív mező nem akadályozza a mentést.

Megosztott Soul-profil szerkesztése copy-on-write. A core→scroll→core váltás
natív SQL-visszaolvasással ellenőrzött; az eredeti másik tárgy változatlan.
Hibás íráskor a profil, szintszabályok, tárgy és ID-kiosztás együtt rollbackel.
A mentést a meglévő natív helper és S_SQL prepared-tranzakció végzi.

Még nem kiadott v18, `auriga.item_catalog.v18`, kanonikus tagek 40..41.
Három új tábla, `024_soul_items.sql`; a friss telepítési lánc 000..024.
A telepített v7 nem frissül ezzel helyben. A felhasználói katalógus külön
MariaDB-sandboxban v7→v18 átemelve, kulcs/cella/számláló megőrzéssel ellenőrzött.

## Tudatos eltérések és nyitott kapuk

A régi timer percenként nő, tesztmódban öt másodpercenként. A `value0==1`
megkerülheti a felső korlátot; mind a tíz rögzített léleknél ez 0. Az új
modell korlátos, a tesztsebesség explicit intervallum. Más forrásváltozat
csak felülvizsgálat után konvertálható; nem csonkolunk töltést csendben.

A régi harc lebegőpontos szorzóval dolgozik és a keretet a számoláskor csökkenti;
az új terv egzakt egészekből számol és 0 sebzésre nem készít debit-tervet.
Ez még nem változtatja meg a futó harcot. A későbbi bekötésnek az elfogadott
damage-commit időpontját és eseményenkénti egyszeri elszámolását igazolnia kell.
A raw scroll path a generikus tuning altípust ellenőrzi, de nem minden Soul-
diszkriminátort; az új domain pontosan validált Core/RefinementScroll párt kér.

Az auditált prototípusok nem kész konvertált release. A teljes policy/asset,
ár, refine-szint, instance- és referencia-leképezés, az aktív állapot tartós
életciklusa, lejelentkezés/halál és Unity-megjelenítés még F3..F6 feladat.
Távoli változtatás vagy éles telepítés ebből az ADR-ből nem következik.
