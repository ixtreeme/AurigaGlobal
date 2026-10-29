# Fejlesztési receptek és Tárgyteszt próbapad

2026-10-29. Ág: `codex/mission-engine`, kiinduló HEAD `eac22531`.
Döntés: [IC-033](../../adr/item_combat_033_item_test_bench.md).

## Elkészült

- **Receptszerkesztés** (`item_catalog.py`, `item_page.py`):
  - a Fejlesztés lap „Recept” oszlopa, szintenként;
  - recept-párbeszédablak: Yang, esély, kudarcszabály, hozzávalók;
  - egyenletes kitöltés a Yang-díjra és az esélyre;
  - a „+” gomb a receptet is folytatja;
  - „Átalakítások…” ablak, ha a cél egy másik tárgy.
- **Natív próbapad** (`SRC/ItemDomain/ItemBenchTool.cpp`):
  - műveletek: adás, áthelyezés, megsemmisítés, Yang, fejlesztés, bónusz- és
    foglalatszolgáltatás;
  - katalógusváltás követése teljes újraellenőrzéssel;
  - magyar hibaüzenetek.
- **Szerverkezelő:**
  - új „Tárgyteszt” lap (`bench_page.py`): inventoryrács, tárgylista,
    műveletnapló;
  - Python-réteg (`item_bench.py`), a táblatelepítővel;
  - a csomagba bekerül az `ItemBenchTool` és a mentési séma.

## Ellenőrzés

| Ellenőrzés | Eredmény |
|---|---:|
| `CatalogEditorTests` PySide6-tal, mind a 6 osztály (köztük az új `RecipeModelTests`, `RecipePanelTests`, `BenchPageTests`) | 60/60 |
| `ItemBenchToolTests.py` saját MariaDB-ben (CTest `item_bench_mariadb`) | OK |
| `ServerManagerGuiTests` a csomag ideiglenes másolatán (főablak az új lappal) | 14/14 |
| GameServer, Database, LoginServer, ServerTests build | sikeres, figyelmeztetés nélkül |
| Teljes CTest (`-j 6`) | 136/137 |

A teljes futásban egy teszt bukott: a `mission_persistence_mariadb`
(küldetésrendszer, ezt a munkát nem érinti). Ugyanakkor a szerverkezelő
exe is épült, így a gép erősen terhelt volt. Egyedül futtatva 3/3-szor
átment, tehát terhelés alatt ingadozó, nem regresszió.

Az `ItemBenchToolTests` egy folyamatban, valódi szerkesztő-commitokkal
felvitt tartalmon ellenőrzi:
- **Adás:** darab és szint; a halomkorlát és a nem létező szint elutasítva.
- **Áthelyezés:** szabad cellára sikerül, foglalt cellára elutasítva.
- **Fejlesztés:**
  - 100%-os recept, egy commitban vonja le a Yangot és az anyagot;
  - kényszerített kudarc: visszaesés +0-ra;
  - rossz forrásszint elutasítva;
  - hiányzó anyag elutasítva, változatlan állapottal;
  - kevés Yang elutasítva;
  - 0%-os recept: mindig kudarc, a szint megmarad.
- **Bónuszszolgáltatás:** a bónusz felkerül, az eszköz fogy.
- **Katalógusváltás:**
  - árváltozás után a próbapad egyszer átáll, utána nincs újabb átállás;
  - a tárolt +2-es szintet törlő katalógust elutasítja, és a
    katalóguskötés a régi marad.
- **Visszaállítás és megsemmisítés.**

## Helyi telepítés

- 2026-10-29 20:32-kor a `D:\AurigaGlobal\Server2-Local` `player`
  adatbázisába került a 9 mentési tábla. A játékszerver közben le volt
  állítva.
- Mentés: `run/item-bench/20261029-203209-05f368cd/player-before.sql`
  (34 MB).
- A régi táblák nem változtak.
- A katalógus a v24-es (`86eeba5a…`).

## Határ

- A GameServer még nem használja az új tárgyakat; ez az F4.
- A próbapadon nincs felszerelés, kőberakás és kőkiemelés.
- A mentés első fázisában nem támogatott kategóriákat a próbapad nem adja
  ki; ilyenkor magyar hibaüzenetet ad.
