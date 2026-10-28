# Item proto — időzített fogyasztható hatások

2026-10-28. Ág: `codex/mission-engine`, kiinduló HEAD:
`45e76b230f83a9adc2a244881f488786679ef7e7`.
A munkafa a korábbi Claude-review javításait is tartalmazta; azokat megtartottam.
Ebben a körben nem készült commit vagy telepítés.

## Elkészült

- A USE-tárgyak második szeletének **stat-hatás alapja**: névvel ellátott
  csoportok, csoportonként elutasítás/teljes csere, online/valós idejű óra,
  halálkor megszűnés, időtartam, fogyó darabszám, használati környezet,
  PvP-szabály és opcionális közös cooldown.
- `BonusSource::TimedEffect`; a regiszter 25-ről 34 definícióra bővült.
  Új: mozgási/támadási/varázslási sebesség, négy attribútum, XP-bónusz és
  általános kimenő sebzés. A régi definíciók/azonosítók nem számozódtak át.
- Tiszta `PlanTimedEffectUse`: nem fogyaszt tárgyat és nem ír karakterstatot.
  Ellenőrzi a jogosultságokat, cooldown/halom/aktív-csoport állapotot;
  érték szerint adja vissza a teljes bónuszcsomagot és a csere azonosítóját.
  Lejárt hatás nem blokkol, csere nem igényel új aktív helyet.
- Immutable katalógus, mezőnként kódolt kanonikus SHA-256, teljes SQL-snapshot
  és natív, tranzakciós szerkesztői mentés. A fogyasztható pontosan egy
  gyógyítási vagy időzített profilra mutat. A két út nem hívható egymásként.
- Helyi Qt tárgyszerkesztő: típusváltás, csoportválasztás/új csoport,
  bónuszcsomag, pontos idő/egész/százalék szerkesztés. Megosztott profil
  szerkesztése másolatot készít; típusváltáskor a fölös privát profilok
  felszabadulnak. A kiadott csoport szabályait a natív író is védi.
- 1100×760 és 1400×950 méretű offscreen ellenőrzés: az időzített mezők
  nem lógnak ki oldalra, kisebb ablakban függőlegesen görgethetők.

## Formátum és adatok

- **v8**, `auriga.item_catalog.v8`; két új típusos tábla:
  `timed_effect_group`, `timed_effect_profile`.
- A draft alap-regiszter/seed és a 014-es migráció együtt alkotja a friss
  séma láncát. **Ez nem meglévő v7-adatbázis frissítője.**
- Az önálló SQL-fixture és a C++-fixture egyezését független Python encoder
  ellenőrzi. Golden SHA-256:
  `e7f086fd194f534776c75a8943a950a1705bfac60cae6bab53d8df341b65605a`.
- Csak a tesztek által indított, véletlen útvonalú/portú MariaDB-példányokon
  történt SQL-írás. A telepített helyi app, a valódi helyi katalógus és a
  távoli gép ebben a körben változatlan maradt.

## Ellenőrzés

| Ellenőrzés | Eredmény | Log |
|---|---|---|
| Normál ServerTests build + teljes CTest | 103/103 | `build/timed-consumables-final-ctest.log` |
| Qt/model/native writer + izolált MariaDB | 27/27 | `build/timed-editor-final.log` |
| ASAN natív katalógus/codec/generátor/loader | 14/14 | `build-asan/timed-consumables-final-tests.log` |
| Ugyanaz a 27 szerkesztőteszt ASAN helperrel | 27/27 | `build-asan/timed-editor-final.log` |
| Teljes szerverkezelő GUI, eldobható csomagmásolat | 14/14 | `build/timed-manager-gui-tests.log` |
| Diff whitespace és generált regiszter drift | sikeres | `git diff --check`, CMake registry check |

A kezdeti tesztkör a bővített fixture miatti régi darabszámok/verzió-visszaállítások,
illetve egy megváltozott hibakód-elvárás javítását igényelte. A végső körök
a táblázat szerint sikeresek. Nem történt játékbeli hatás-/időzítőteszt:
a live runtime még nem használja ezt a domainmodult.

## Határ és következő sorrend

1. A második USE-szelet egyedi eseményhatásai (Valentin, tűzijáték,
   horgásztabletta és vizuális jelzések) külön szemantikai auditot igényelnek.
   Nem kerültek ismeretlen stat vagy általános értéktömb mögé.
2. USE 3. szelet: ital-változatok, automata töltetek és aréna-szabályok;
   majd képességkönyvek és tárgyon végzett műveletek.
3. A meglévő helyi v7-katalógushoz adatokkal és visszaállítási próbával
   ellenőrzött frissítőt kell készíteni, mielőtt a v8 appot telepítjük rá.
4. F3/F4: tartós item-instance, ownership/revision/idempotens commit,
   inventory/felszerelés, aktív hatások ECS-tulajdona és lejárati ütemezése.
5. Protokoll/Unity és egyszeri offline tartalom-/adatkonverzió.

A prototípus továbbra is egy szemantikai tárgyat jelent; a fejlesztési szint
példányadat és profilmaximum. Az időzített használat nem hoz létre újabb
azonos prototípust. A kivezetett rendszerek nem kerültek vissza.

Részletes szerződés: `docs/adr/item_combat_015_timed_consumables.md`.
Auditinduló hash-ek: `2026-10-28_timed_consumables.audit.json`.
Végső teszt-/forráshash-ek: `2026-10-28_timed_consumables.evidence.json`.
