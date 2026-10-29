# ADR IC-014 — Használati tárgyak (USE) leképezése és szeletei

2026-10-27. Fázis: F2 ([IC-012](item_combat_012_item_runtime_first.md)).
Állapot: **elfogadott leképezés; az 1. szelet (pénzjegyek) a 2. szelet stat-hatás alapja a 3. szelet automata/arénakvóta/kapcsolható harmat alapja és a 4. szelet könyvkategóriája implementált**.

## Forrás

- A `UseItemEx` (`ItemSystem_LegacyBridge.cpp`) `ITEM_USE` ága kb. 3 800 sor.
- Altípusonként szétbontva és az F1 leltárral összevetve a 36 nem ital jellegű
  USE-altípus öt családra oszlik.
- A `USE_SPECIAL` maga is vnumonkénti esetek halmaza, kb. 35 viselkedéssel,
  nevesített konstansokkal (`ITEM_NOG_POCKET` stb.) és számliterálokkal.

## Leképezés

| Család | Régi forrás | Új hely | Szelet |
|---|---|---|---|
| Pénzjegy | `USE_SPECIAL`: rudak 39069, 80003–80008 (Yang); utalványok 39065, 89003–89007 (Sárkányérme) | 16 Pénzjegy: pénznem + egységérték | **1 (kész)** |
| Időzített hatás | `USE_ABILITY_UP`, `USE_AFFECT`, prémium 50801–50820; `USE_SPECIAL`: NOG/Ramadan/Valentin/tűzijáték/horgász-tabletta | 5 Fogyasztható: hatásprofil | 2 — a típusos stat-hatás és bónuszregiszter elkészült (IC-015); az egyedi eseményhatások még külön auditot igényelnek. Bővített regiszter: (mozgás-/támadás-/varázslási sebesség, STR/DEX/CON/INT, mágikus ellenállás, támadó/védő fokozat, tapasztalat-/sebzésbónusz) |
| Ital-változatok | `USE_POTION_NODELAY` aréna-/vnum-kivételei, `USE_NEW_POTIION`, automata italok (`ITEM_AUTO_*_RECOVERY_*`) | 5 Fogyasztható: helyreállítás bővítése (töltet, aréna-szabály) | 3 — automata/töltet/arénakvóta (IC-016) és kapcsolható harmat (IC-017) domain/katalógus/szerkesztő kész; runtime még hátra |
| Képességkönyv | `USE_SPECIAL` 50061, 50304–50336, kézműveskönyvek 50902–50904; külön `ITEM_HORSE_SKILL_TRAIN_BOOK`, `ITEM_MINING_SKILL_TRAIN_BOOK`; `ITEM_SKILLBOOK`, `ITEM_SKILLFORGET` | 9 Képességkönyv | **4 — domain/katalógus/szerkesztő kész (IC-018); runtime F3/F4** |
| Tárgyon végzett művelet | `USE_CHANGE_ATTRIBUTE(2)`, `USE_ADD_ATTRIBUTE(2)`, `USE_CLEAN_SOCKET`, `USE_ADD_ACCESSORY_SOCKET`, `USE_PUT_INTO_ACCESSORY/BELT_SOCKET`, `USE_TUNING`, `USE_DETACHMENT`, zárak (35–37), jelmez- és stólabónuszok (31, 32, 38–41, 45, 48), `USE_RUNE_PERC_CHARGE` | 5 Fogyasztható: célpolitikás műveletprofil | 5 — a katalógus előbb, a futás az F3 példánybónuszaival |
| Egyedi viselkedés | `USE_SPECIAL` többi esete (bátorság köpenye, házassági gyűrű, fehér zászló, holdfényláda, kagylók, húsvéti tojások stb.), `USE_TALISMAN`, `USE_CLEAR`, `USE_INVISIBILITY`, `USE_CREATE_STONE`, `USE_RECIPE` | Funkció szerint: 8 Tároló (dobozszerűek), 5 (hatások), 17 Különleges (névvel regisztrált viselkedés) | 6 |
| Máshová tartozó | `USE_BAIT`, `USE_FISH` → 12 Horgászat; `USE_BUCKET` → 11 Eszköz; `USE_PET_*` → 15 Társ | saját kategória | a kategóriával együtt |
| Kivezetett | `USE_RETIRED_27/42`; `USE_MOVE/TREASURE_BOX/MONEYBAG` (0 sor) | nem kerül át | — |

A 17-es kategória a terv szerint nem menekülőút: minden ott elhelyezett
viselkedés saját, névvel regisztrált, validált struktúrát és tesztet kap.

## 1. szelet — 16 Pénzjegy

- **Régi működés:**
  - Rudak: darabonként a rejtett `shop_buy_price` Yangban. Annyi darab
    váltódik be, amennyi a `GOLD_MAX` alá fér, és csak azok fogynak.
  - Utalványok: ugyanígy Sárkányérmében, `UINT32_MAX` korláttal. Ha csak
    részben fér be, a régi kód mégis a **teljes halmot** elfogyasztja, a
    különbözet elvész.
- **Új modell:** `item_currency_token(wallet_currency, amount_per_unit)`, az
  egységérték explicit mező.
- **Beváltás:** a `PlanTokenRedeem` mindkét pénznemre a rudak helyes
  szabályát alkalmazza. Csak a beférő darabokat fogyasztja, és annyit ír
  jóvá, amennyit azok érnek. Ha egy darab sem fér be, elutasít.
- **Korlátok:** a pénztárca-korlát futási beállítás (F4), nem katalógusadat.
- **ELK:** az `ITEM_ELK` földön heverő Yang. Az új modellben pénztárca-művelet,
  nem tárgypéldány; a konverter nem hoz létre belőle tárgyat.
- **Formátum:** v7 (`013_currency_tokens.sql`, kanonikus 22. tábla).

## 2. szelet — stat-hatások (2026-10-28)

A [IC-015](item_combat_015_timed_consumables.md) szerinti csoportok,
profilok, használati tervek, v8 SQL-katalógus és helyi tárgyszerkesztő elkészült.
Ez tartalmi alap és tiszta használati terv; a futó karakter statjait/időzítőit
még nem módosítja. A régi tárgyak tényleges konverziója továbbra is F6.
Valentin/tűzijáték/horgásztabletta és vizuális effektusok nincsenek általános
stat-bónusszá visszaminősítve. Ezek, a runtime részek, valamint az 5–6. szelet hátravannak. A 4. szeletet IC-018 rögzíti. A kapcsolható harmatok külön portját lásd alább, IC-017.

## 3. szelet — automata helyreállítás és arénakvóta (2026-10-28)

[IC-016](item_combat_016_automatic_recovery.md): automata HP/SP, véges/végtelen
ellátás, példányonkénti töltet/időzítés, típusos kapcsolási és tick-terv,
poolonkénti kizárólagosság, szerver-/párbaj-szabályok külön döntése,
explicit arénakvóta, v9 katalógus/SQL/szerkesztő. Nem a régi runtime emulációja:
a tényleges gyógyítás terheli a töltetet, nincs teljes hiány előzetes lefoglalása.
A törlés/HP/időzítő/bónusz/receipt commit és a régi mentések offline konverziója
F3/F4/F6. `USE_NEW_POTIION` tárgyhoz kötött, kapcsolható harmatjai nem
HP/SP-palackok; külön típusos portjukat IC-017 rögzíti.


## 3. szelet kiegészítése — kapcsolható harmatok (2026-10-28)

[IC-017](item_combat_017_toggle_effects.md): példányhoz kötött, nem halmozható
buff; véges aktív online idő, külön csoport/variáns, házasság/partyvezető
feltétel, kontextus/PvP és explicit owner-szabály. Ugyanazon példány újabb
használata kikapcsol; azonos csoport+variáns másik példánya elutasított,
ellentétes variáns atomikus cserét javasol. Feltételvesztés, halál vagy retire
nem blokkolja a kikapcsolást. Az advance pontos ms-elszámolást és lejárati
tervet ad; nincs ismételt bónuszgrant vagy stackbontás.

V10 katalógus, két típusos SQL-tábla és egy új kategóriahivatkozás; natív
snapshot/validátor/atomikus tartalommentés és Qt-űrlap. A TimedEffect bónuszforrás
közös az időhöz kötött aktív hatásoknál, az item viselkedése külön típusos.
A játékbeli ownership/lock/stat/timer/receipt commit továbbra is F3/F4.
A kiadott helyi v7 adatbázist és appot nem frissíti a draft migrációlánc;
adatmegőrző upgrade és visszaállítási próba szükséges a telepítésük előtt.


## 4. szelet — képességkönyvek (2026-10-28)

[IC-018](item_combat_018_skill_books.md): 9-es kategória, típusos tanulás,
felejtés/pontvisszaadás és lóképességpont. Fix cél vagy tartós példányadatban
kötött képesség engedélylistával; explicit szintlépések, sikeres olvasásszám,
esély/bónusz, XP és képességhez tartozó várakozás. A szerver döntéseivel
feloldható, megváltoztathatatlan használati kérés; nincs rejtett RNG, questflag
vagy régi skill/item callback. A felejtés lenullázza az olvasási előrehaladást.

V11 SQL-katalógus, snapshot/hash, natív atomi tartalommentés és Qt-szerkesztő
nyolc szabálymintával. A minták nem jóváhagyott balance vagy valódi tartalomimport.
Az owner kötelezően ellenőrzi a külön skill-tartalom létezését, maximumát,
kaszt/csoport és előfeltétel szabályait. Ez a keresztellenőrzés és a tartós
item/skill/pont/cooldown/receipt commit továbbra is az F3/F4 kapu része.

## 5. szelet — foglalatkarbantartás (2026-10-29)

[IC-030](item_combat_030_socket_services.md): tisztítás, minden/utolsó kő
kiemelése, valamint az ékszerfoglalat-nyitás és -töltés teljes befogadási
terve elkészült, v23 katalógussal és Qt-szerkesztéssel. A beépített kő
ItemKey-ja és szintje megmarad; minden visszaadott kőhöz külön foglalás kell.
Ez nem zárja le a teljes 5. szeletet: bónuszhozzáadás/-váltás, zárolás,
jelmez-/stólabónuszok és fejlesztési segédanyagok még külön portot igényelnek.
Az atomi gazdasági végrehajtás F3/F4, a valódi forráskonverzió F6.

## 5. szelet — példánybónuszok (2026-10-29)

[IC-031](item_combat_031_instance_attributes.md): normál/ritka/nyolcadik
pályák, típusos jelöltek és értékek, hozzáadás, csoportváltás és normál
bónuszhely védelme/áthelyezése/törlése; v24 SQL-katalógus, explicit
állapotcodec és Qt-szerkesztők. A kapcsolt Gauss-addon, jelmezreset és
példányadatos bónusz-adók, stóla/rúna és fejlesztési segédanyagok továbbra
is külön viselkedések. A meglevő játékospéldányok átvétele és a gazdasági
végrehajtás még nincs aktiválva.
