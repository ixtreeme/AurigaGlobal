# ADR IC-015 — Időzített stat-hatások

2026-10-28. Az IC-014 második szeletének stat-hatás része; csak helyi fejlesztés.

## Audit és határ

- `ItemSystem_LegacyBridge.cpp::UseItemEx`, `USE_ABILITY_UP`: a normál ital
  felülírja az azonos affectet; a 50801–50820 prémium ág azonos
  `(affect_type, apply_type)` esetén elutasít. Mozgás-, támadás-, varázslási
  sebesség, STR/DEX/CON/INT és támadó/védő fokozat szerepel a kódban.
- `USE_AFFECT`: a `value0/1/2/3` mezőkben típus/apply/érték/időtartam van;
  a prémium ág másik mezősorrendet és közös affect-típust használ.
- `ITEM_NOG_POCKET`, `ITEM_RAMADAN_CANDY`: három külön AddAffect hívás
  (mozgás, mall támadás, mall XP), közös csoport, ismételt használat tiltva.
- `AffectSystem::AddAffect` külön eltávolítja a régit, majd alkalmazza és
  publikálja az újat. Az új domain ehelyett egyetlen, mellékhatásmentes tervet
  ad a későbbi owner-thread commitnak. Ebben a fázisban nincs ECS/HP/SQL írás.

A teljes APPLY-regiszter, Valentin/tűzijáték/horgásztabletta és vizuális
effektusok még **nem** migráltak. Ismeretlen régi point nem alakítható tetszőleges
új bónusszámmá. Régi tárgyadatokat nem importálunk automatikusan.

## Új szerződés

- A fogyasztható tárgy pontosan egy gyógyítási **vagy** időzített profilra mutat.
- Az időzített profil névvel ellátott csoportot, időtartamot (ms), fogyó
  darabszámot, használati környezetet, PvP-szabályt, opcionális közös
  cooldown-profilt és `TimedEffect` forrású bónuszprofilt tartalmaz.
- A csoport közös szabálya `Reject` vagy `Replace`; így két tárgy nem
  definiálhat egymásnak ellentmondó halmozást. A csere **teljes csomagot**
  cserél, nem csak az átfedő statot. Nincs végtelen idő, időösszeadás vagy RNG.
- A csoport órája `OnlineTime` vagy `WallTime`, és külön jelöli a halálkor
  eltávolítást. Az owner később normalizált hátralevő időt ad a tervezőnek;
  lejárt (0 ms) rekord nem akadályoz új használatot. A tervező nem olvas órát.
- A terv érték szerint másolja a bónuszokat, az elfogyasztandó darabszámot,
  cooldown-kezdést és az esetleg lecserélendő aktív hatás ID-jét. A fogyasztás,
  időzítőfoglalás, revision/ownership-ellenőrzés és statfrissítés egy későbbi
  idempotens commit feladata (F3/F4).
- A `BonusSource::TimedEffect=3` bitje 4. Régi ID-k nem változnak.
  A sebességek relatív bp, attribútumok egész pont, XP és általános kimenő
  sebzés bp egységet kapnak; támadó/védő fokozat a meglévő fixpontos stat.
  Ezek új, explicit szemantikák, nem kész balance és nem régi pontkonverter.
- Az általános kimenő sebzés külön bucket (az action/mode bónuszok mellett),
  az XP külön reward-stage. Egyiket sem alkalmazza még a jelenlegi harcmotor.

## Formátum

Katalógus v8, `auriga.item_catalog.v8`. A draft alap-migrációk a bővített
regisztert hozzák létre; a 014-es séma hozzáadja a típusos időzített táblákat.
Ez új, izolált tesztadatbázisra szolgáló lánc, **nem** meglévő v7-adatbázis
automatikus frissítője. A helyi valódi katalógust és felhasználói tárgyakat
ebben a munkafázisban nem módosítjuk.
