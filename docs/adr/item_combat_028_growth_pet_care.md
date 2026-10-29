# IC-028 — Petkeltetés, életidő és etetés

2026-10-29, IC-012 F2. Kezdő HEAD: `4e4ccf99`.

## Audit

A `pet_input.cpp::BraveRequestPetName` 100000 pénzt, egy tojást, háromnapos
aktív életidőt és három külön rögzítendő születési tulajdonságot használ.
A keltetés célját a régi kód egg+300-ból veszi, amely öt proto `value0`
céljával ütközik. Az új profil explicit ItemKey-célt ad; a régi hivatkozások
nem kerülnek automatikusan kiadásra.

A születési értékek a `NewPetActorState::bonusPet` 69/63/119 forrásai alapján
max HP %, szörny elleni sebzés % és normál PvE sebzés % tizedei. A jelenlegi
`GiveBuff` egész osztást végez 10-zel; a tulajdonságok tárolása nem jelent
engedélyt a későbbi alkalmazás pontosságának észrevétlen megváltoztatására.
E lépés nem alkalmaz petbónuszt vagy képességet.

A `New_PetSystem.cpp::UpdateActorTime` percenként fogyaszt aktív életidőt,
offline nem. A 525600 percnél nagyobb kapacitást korlátlanként kezeli.
Az új modell korlátlan órája explicit; a mentett kapacitás nem kerül vissza
a mai proto időtartamára. A nulla életidő megtartott, újraéleszthető pet,
nem törlendő tárgy. A hívó újraidézése nem töltheti újra az időkeretet.

A cube etetés tojással vagy hívóval 3%-ot, a külön 55001 étellel 50%-ot
tölt; az egész kijelölt tárgyhalmot törli. A közvetlen 55001 használat élő
petet teljesre, lejártat fél kapacitásra tölt, egy egység fogyasztásával.
Az 55001 sor nem található a rögzített protókban. A fegyver/páncél etetés
a vételár felét adja külön item EXP-ként. Ezek külön kontextusok és szabályok,
nem azonos műveletek. Nem másoljuk át a régi számokra épülő feltételeket.

## Típusos szerződés

- A tojás a 8-as konténerkategória `PetEgg` fajtája. Nincs lootprofilja vagy
  kulcsa, saját `pet_hatch_profile`-t hivatkozik. A normál doboznyitó explicit
  elutasítja; csak a keltetési terv fogadja el.
- A keltetési profil célja egy fejlődő pet. A költség és a három tulajdonság
  minimum/maximum/lépés mezője explicit. A szerver rögzített döntési értékeit
  a domain ellenőrzi; RNG-t nem hív. Az új példányazonosító és a validált név
  foglalása a későbbi owner feladata.
- A keltetési terv egy tojás levonását, pénzköltséget, új ItemKey-t, +0
  tárgyszintet, 1-es petszintet, 0-s evolúciót, születési UTC-időt,
  tulajdonságokat és friss életidőt ad. Nem létező tárgyból vagy névhibából
  nem keletkezhet részben elkészült pet. Ez nem egy betöltési factory.
- A `GrowthPetLife` külön tárolja az órafajtát, eredeti kapacitást és maradékot.
  Aktív óránál egész ezredmásodpercekkel fogy, kizárólag hiteles aktív időből.
  Korlátlan óránál mindkét mennyiség nulla. Órafajtaváltás explicit migrációt
  igényel; a még meglevő kapacitás eltérhet az új protóétól.
- A karbantartási terv csak életidőt módosít. Lejáráskor a követő eltávolítását
  kéri, a tárgy/progression/név/képesség/bónusz megmarad. Újraélesztést csak
  explicit, erőforrást fogyasztó szabály végezhet.
- A `pet_feed_rule` konkrét forrástárgyhoz és cube/közvetlen kontextushoz
  tartozik. A három viselkedés: kapacitásarányos töltés; teljesre töltés vagy
  lejárt pet arányos újraélesztése; fegyver/páncél ár alapján item EXP.
  Pontos egység vagy teljes halom fogyasztása explicit választás.
- Cube aktív petet, közvetlen töltés inaktív petet igényel. A nulla életű
  aktív jelölt és az idegen, cserében levő, viselt vagy zárolt forrás tiltott.
  A saját hívó és a tartós petadatot hordozó másik tárgy nem etethető meg.
  A régi hívóáldozás biztonságos gyermekállapot-törlése külön későbbi feladat.
- Nincs hatás nélküli fogyasztás: tele/korlátlan keret, nulla kerekített
  növekmény, nulla ár vagy maximális EXP esetén a terv elutasít.

## Owner, perzisztencia és kiadás

Minden terv birtokolt másolatokat, operation ID-t, tulajdont, item/pet/wallet
revisionöket és katalógus/ruleset pint hordoz. Nincs SQL/ECS/Lua vagy packet.
Az aktív színész generációját az owner újraellenőrzi. Etetés előtt a clockot
aktuális állapotra kell hoznia; várakozó terv revisionje elavulhat.

F3/F4-ben a receipt, tojás/étel, pénz, új példány és petállapot ugyanabban a
tranzakcióban commitolandó. A név- és ID-foglalásnak is ugyanahhoz a művelethez
kell tartoznia, a cél inventory-hely és a kötési policy alkalmazásával együtt.
CommitUnknown esetén tartós operation ID szerinti egyeztetés
kell. A pet progressziója etetéskor csak EXP-módban módosítható; más petadat
nem írható felül. Új petnél a négy képességhely zárolt inicializálása a teljes
birth factory kötelezettsége; képesség-, kor- és bónuszruntime még nincs itt.

A katalogizálás és a tervek elkészülte nem élő játékbekötés. A rögzített
mentésben nincs fejlődő pet, így valódi korábbi petadat roundtripje nem
igazolható ebből. A távoli gép és a működő helyi v7 adatbázis változatlan.
