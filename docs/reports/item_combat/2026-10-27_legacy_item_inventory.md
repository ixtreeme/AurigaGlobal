# Régi tárgytartalom leltára és kategória-leképezés (F1)

2026-10-27. Ág: `codex/mission-engine`, HEAD `94d29303`. Döntés: [IC-012](../../adr/item_combat_012_item_runtime_first.md).

Forrás: a Server2-Local helyi MariaDB-je, csak olvasva (SELECT, a csomag
saját `player` felhasználójával). A típus- és altípusnevek az
`item_length.h` felsorolásaiból származnak, a `service.h` ténylegesen
bekapcsolt kapcsolóival kiértékelve. Részletes sorok: a mellette lévő
`2026-10-27_legacy_item_inventory.evidence.json`.

Ez a **helyi tesztadatbázis**. Az éles adatbázis darabszámai eltérhetnek, mert
azon nem futott lekérdezés. A konverternek ezért minden besorolatlan
típust/altípust hibaként kell jeleznie, nem csak az itt látottakat.

## Összkép

- 10 714 `item_proto` sor, 31 használt típus.
- 3 084 játékos-tárgypéldány: 1 721 felszerelve, 676 inventoryban, 687 extra
  inventoryban. Árva (proto nélküli) példány: 0.
- 402 fejlesztési lánc, körbefutó lánc 0, hiányzó célra mutató `refined_vnum` 0.
- 24 tárgynévben nyers sorvége-karakter van (CR/LF). Ezt a konverternek kezelnie kell.

## Javasolt leképezés

Oszlopok: prototípus, ebből fejleszthető (`refined_vnum`), játékos-példány,
bolti sor.

| Régi tartalom → új kategória | Proto | Fejl. | Példány | Bolt |
|---|---:|---:|---:|---:|
| 1 Fegyver — kard, tőr, íj, kétkezes, csengő, legyező, lovas lándzsa | 1941 | 1807 | 221 | 80 |
| 1 Fegyver — nyíl és tegez (új lőszer-altípus) | 14 | 0 | 3 | 5 |
| 2 Páncél — test, fej, pajzs, láb | 2123 | 1966 | 854 | 83 |
| 3 Ékszer — csukló, nyak, fül | 728 | 674 | 648 | 33 |
| 3 Ékszer — öv (új hely és övfoglalat) | 311 | 135 | 0 | 1 |
| 4 Jelmez — test, haj, hátas, váll, fegyver, stóla, effekt, 17–20 | 1385 | 46 | 83 | 573 |
| 4 Jelmez — rúnahelyek (RUNE_SLOT1–7) | 7 | 0 | 7 | 0 |
| 5 Ital — gyógyító italok (POTION, POTION_NODELAY, NEW_POTION) | 132 | 0 | 27 | 29 |
| 5/17 Használati tárgyak — 36 további USE-altípus, új viselkedésekkel | 361 | 0 | 424 | 27 |
| 6 Alapanyag — bőr, tű, ékkő | 555 | 0 | 137 | 18 |
| 6 Alapanyag — RESOURCE (új alapanyagfajták) | 14 | 0 | 1 | 2 |
| 7 Foglalatkő — METIN | 189 | 8 | 25 | 8 |
| 8 Tároló — CONTAINER, TREASURE_BOX, GIFTBOX | 362 | 0 | 383 | 1 |
| 9 Képességkönyv — SKILLBOOK, SKILLFORGET | 79 | 0 | 7 | 4 |
| 10 Quest — QUEST | 479 | 0 | 64 | 45 |
| 11 Eszköz — horgászbot, csákány, vödör | 51 | 46 | 1 | 2 |
| 12 Horgászat — hal, csali, USE_FISH | 88 | 0 | 1 | 3 |
| 13 Átváltozás — POLYMORPH | 6 | 0 | 0 | 0 |
| 14 Lélek — SOUL | 10 | 8 | 0 | 0 |
| 15 Társ — PET, PET_REVIVE, PET_ENCHANT | 19 | 0 | 0 | 0 |
| 16 Pénzjegy — ELK, SECONDARY_COIN, ISHOP | 2 | 0 | 0 | 0 |
| 17 Különleges — térkép, tábortűz, keverék, UNIQUE, átruházó tekercs | 178 | 2 | 2 | 7 |
| 18 Kulcs — TREASURE_KEY | 4 | 0 | 0 | 0 |
| **Vizsgálandó** — ITEM_NONE (lélekgömb, kövek, kagylók; esemény-/questvaluta) | 246 | 0 | 194 | 0 |
| **Karantén** — talizmán (ARMOR 7): `ENABLE_PENDANT` nincs bekapcsolva | 1206 | 1194 | 2 | 0 |
| **Karantén** — karom/fáklya (WEAPON 8) és csokor (WEAPON 10): a szerver nem ismeri | 122 | 110 | 0 | 1 |
| **Karantén** — kivezetett alapanyag-altípusok (5, 6, 7, 10, 22) | 6 | 0 | 0 | 0 |
| **Kivezetett** — DragonSoul (29), RETIRED_31, USE_RETIRED_27/42 | 96 | 0 | 0 | 0 |
| **Összesen** | **10714** | | **3084** | |

Az 1, 2, 3, 5 (csak gyógyítás) és 6 (bőr/tű/ékkő) kategória ma is tölthető. A
többi sor F2-munka: új kategória, új altípus/hely, vagy új viselkedés.

## Leletek, amelyek a konvertert és a runtime-ot meghatározzák

**Fejlesztési láncok.**
- A 402 láncból 275 a klasszikus, 10 tagú +0…+9 család.
- A többi hosszabb: 14, 34, 38, 44, 48, 54, 58, 74 tagú láncok vannak.
  - Ezek fegyvereknél és páncéloknál a +9 után másik vnum-tartományban
    folytatódnak (pl. `10 → … → 8579`). Ez a bekapcsolt `ENABLE_INFINITE_RAFINES`
    és a magasabb szintű tárgyakba „továbbfejlődés” keveréke.
  - A talizmánoknál 81, 120 és 170 tagú láncok is vannak, és van +81-ről,
    illetve +31-ről induló láncfej.
- A konverternek ezeket két esetre kell bontania: „ugyanaz a tárgy, magasabb
  szint” és „másik tárgy”. A puszta lánc nem elég. Kell hozzá a név +N
  végződése, a vnum-tartomány és a statok folytonossága, valamint ütközéskor
  egy kézi döntési lista.

**ITEM_NONE.** A 0-s típus nem üres.
- 246 prototípus tartozik ide: lélekgömb, harcos/nindzsa/sura/sámán elleni kövek,
  Aquakövek és kagylók.
- 194 játékos-példány van belőle (190 lélekgömb, 4 Kuchiyose pergamen).
- A mai szerver ezeket nem tudja használni: a `UseItemEx`
  (`ItemSystem_LegacyBridge.cpp`, `case ITEM_NONE`) csak hibát naplóz.
- A lélekgömb (30135) a `LELEKGOMB_EVENT` által dobott cseretárgy
  (`entity/char_manager.cpp`). Ezek tehát esemény- és questvaluták.
- Javaslat: a 10 Quest vagy a 16 Pénzjegy kategória. A többi 0-s sor
  questbeli szerepét a konverzió előtt tételesen meg kell nézni.

**Beégetett vnum-hivatkozások.**
- C++ kód:
  - 62 tárgygyártás szám-literállal (`CreateItem(30135, …)` stb.) 3 fájlban;
  - kb. 72 vnum-összehasonlítás;
  - 175 szám-literálos `case`/`==` csak a `ItemSystem_LegacyBridge.cpp`-ben.
- Questek: legalább 28 fájl hivatkozik tárgy-vnumra.
- Az F4-ben ezek stabil tartalomkódos hivatkozást kapnak. Példa:
  `event.soul_orb` → ItemKey, katalógusbetöltéskor feloldva, hiányzó kódnál
  a szerver nem indul. A konverter a régi vnumhoz rendelt tartalomkódokat
  listában adja ki.

**Kikapcsolt tartalom.**
- A talizmánok (1 206 sor, 2 példány) a mai szerverben használhatatlanok, mert a
  hozzájuk tartozó kód `ENABLE_PENDANT` mögött van, ami nincs bekapcsolva.
- A 8-as fegyver-altípus (karom, fáklya, lándzsa) az `EWeaponSubTypes`-ban nem
  is szerepel.
- Az IC-012 alapelve szerint ezek karanténba kerülnek. A 2 talizmánpéldány
  külön jelentésbe kerül, és nem törlődik döntés nélkül.

**Ismeretlen jelmez-altípusok.** A 17–20 altípus az `ECostumeSubTypes`-ban
nincs (a felsorolás 17-nél véget ér). Ezek mégis jelmeznek látszanak (test,
haj, fegyver, vállszalag). A mai viselésüket a kódban ellenőrizni kell.

**Öv.**
- Az öv típus (34) a valódi öveken kívül „PvP kártya” és „[Goku]”, „[Anime]”
  stb. nevű sorokat is tartalmaz.
- Ezek valószínűleg címek vagy kozmetikumok, ellenőrizni kell őket a konverzió
  előtt.

## Következő lépés (F2)

1. Tételes kódaudit a „Vizsgálandó” és a jelmez 17–20 csoportra. Ennek
   eredménye: kategória vagy karantén.
2. Hiányzó kategóriák a mai viselkedésük szerint. Minden kategória egy
   kör: domain, SQL, betöltő, szerkesztő, teszt.
   - Kezdés a legtöbb élő példányt érintő csoportokkal: tároló/ajándékdoboz
     (383 példány), használati tárgyak (424), foglalatkő, quest, jelmez.
   - Utána a kisebbek.
3. A meglévő fegyver/páncél/ékszer/alapanyag kategóriák bővítése:
   - lőszer-altípus;
   - öv hely és övfoglalat;
   - RESOURCE alapanyagfajták.
