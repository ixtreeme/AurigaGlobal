# F2 — Használati tárgyak: leképezés és 1. szelet (pénzjegy)

2026-10-27. Ág: `codex/mission-engine`, kiinduló HEAD `c4ccb126`.
Döntés: [IC-014](../../adr/item_combat_014_use_items.md).

## Audit

- A `UseItemEx` `ITEM_USE` ágát altípusonként szétbontottam. Minden ágnál
  rögzítettem:
  - az olvasott `value`/`socket` mezőket;
  - a hatásokat és pontváltozásokat;
  - a beégetett vnumokat;
  - a quest-hivatkozásokat;
  - a fogyasztást.
- A 36 nem ital jellegű altípus öt családba esik; a családonkénti célkategóriát
  és szeletsorrendet az IC-014 tartalmazza.
- A `USE_SPECIAL` kb. 35 vnum-szerinti viselkedés gyűjteménye:
  - nevesített konstansok, pl. `ITEM_NOG_POCKET`=50216, `ITEM_MARRIAGE_RING`=70302,
    `UNIQUE_ITEM_CAPE_OF_COURAGE`=70038;
  - számliterálok;
  - automata italok.
- A helyi adatbázis USE-példányainak többsége ebből a csoportból jön: bátorság
  köpenye 104 db, ezüst-/aranyrudak kb. 190 db, utalványok.

## 1. szelet — 16 Pénzjegy

- **Új kategória:** `item_currency_token`, pénznemmel (Yang / Sárkányérme) és
  explicit egységértékkel.
- **Beváltás:** a `PlanTokenRedeem` annyi darabot vált be, amennyi a
  pénztárca-korlát alá fér, és csak ezeket fogyasztja.
- **Régi hiba, amely nem öröklődik:** a régi utalvány-ág korlátozott jóváírás
  mellett is a teljes halmot elfogyasztotta.
- **Formátum:** v7 (`013_currency_tokens.sql`), kanonikus 22. tábla.
- **Szerkesztő:** új „Pénzjegy” lap (pénznem, érték darabonként).

### Ellenőrzés

| Helyi ellenőrzés | Eredmény |
|---|---:|
| `CurrencyTokenTests` (beváltás, korlát, túlcsordulás, katalógusszabályok) | 25 ellenőrzés |
| Független Python hash-orákulum ↔ C++ (`3a757163…`) | egyezik |
| `ItemCombatSchemaTests` saját MariaDB-ben | 21/21 |
| `CatalogSqlReaderTests` saját MariaDB-ben (v5–v7 fixture) | 148 ellenőrzés |
| `CatalogEditorTests` (modell, Qt-űrlap, valódi MariaDB-commit) | 19/19 |

### Helyi adatbázis

A Server2-Local `protos` adatbázisa v6-ról v7-re frissült.
- Előtte teljes mentés készült:
  `run/protos-install/20261027-225412-778fde1a/protos-before-013.sql`.
- A meglévő tárgy megmaradt, és a szerkesztő olvasási útja v7-en működik.

## Határ és következő szelet

A GameServer még a régi `USE_SPECIAL` ágat futtatja (F4). A régi rudak és
utalványok átalakítása az F6 konverter feladata: a rejtett `shop_buy_price`
értékből explicit egységérték lesz.

A következő szelet az időzített hatások (`USE_ABILITY_UP`, `USE_AFFECT` és a
`USE_SPECIAL` hatásai). Előtte a bónuszregisztert ki kell bővíteni a régi
képességstatokkal; ezek leképezését az APPLY-leltár alapján tételesen kell
igazolni.
