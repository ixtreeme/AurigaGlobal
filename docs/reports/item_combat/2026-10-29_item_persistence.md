# F3 — Tartós tárgypéldány-mentés, első fázis

2026-10-29. Ág: `codex/mission-engine`, kiinduló HEAD `ed2f2d4f`.
Döntés: [IC-032](../../adr/item_combat_032_item_persistence.md).

A munkát a Codex kezdte; a hibás mentések és a regresszió ellenőrzése ebben a
körben fejeződött be.

## Elkészült

- `PersistentItems.hpp/.cpp`: példány-, tulajdonos-, tároló-, pénztárca- és
  műveletmodell; validátor; kanonikus kérés- és állapothash; az írási halmaz
  előállítása fejlesztés, bónuszszolgáltatás és foglalatszolgáltatás esetén.
- `ItemPersistenceSql.hpp/.cpp`: előkészített, egy tranzakciós író,
  nyugtakezelés és visszajátszás, valamint konzisztens betöltés.
- `db/migrations/item_instance/001_item_persistence.sql`.

## Ellenőrzés

| Helyi ellenőrzés | Eredmény |
|---|---:|
| `PersistentItemTests` (modell, validátor, hash, írási halmazok) | 86 ellenőrzés |
| `ItemPersistenceSqlTests --completion-only` (befejezési határ) | 13 ellenőrzés |
| `ItemPersistenceSqlTests.py` saját MariaDB-ben | 947 ellenőrzés |
| GameServer, Database, LoginServer build | sikeres |
| Teljes CTest (normál, izolált MariaDB-vel) | 136/136 |

A saját MariaDB-s próba lefedi:
- **Fejlesztés:** +9 → +10 ugyanazzal a kulccsal és ugyanazokkal a
  bónuszokkal/kövekkel; anyagfogyás és pénzlevonás egy tranzakcióban;
  ismétléskor nincs dupla levonás.
- **Átalakítás:** kard+20 → hosszú kard+0, megőrzött példány-ID-val,
  bónuszokkal és kövekkel, egyszeri díjjal.
- **Szolgáltatások:** bónuszcsere és törött foglalat tisztítása a mentési úton,
  az eszköz elfogyásával.
- **Hibás mentések** (mindegyik után bájtpontosan változatlan adatbázis):
  - elavult tulajdonos- vagy munkamenet-kerítés;
  - elavult elemrevízió;
  - közben megváltozott szint, bónusz vagy cella;
  - nem horgonycellás ütközés (a már levont pénz és felszabadított cella is
    visszagördül);
  - kikényszerített hiba mindhárom gyermektábla írásakor.
- **Egyéb:** párhuzamos azonos kérés (egy commit, egy visszajátszás); teljes
  cellás csere; karakter → account átadás; sérült/hiányzó gyermekadat
  elutasítása; régi munkamenet, rossz katalóguskötés, kikapcsolt FK/CHECK és nem
  `REPEATABLE READ` kapcsolat elutasítása.



## Határ

- A GameServer/Database nem használja (F4). A meglévő játékadatbázison és
  `player.item` táblán nem futott semmi.
- A nem támogatott kategóriák (jelmez, tároló, ital/hatás, pet, eszköz stb.)
  példányállapota még nincs leképezve.
- A régi tárgyak átvétele a még nem jóváhagyott F6 családtérképen múlik.
