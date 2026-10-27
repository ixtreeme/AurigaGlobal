# F2 — Tároló és kulcs kategória

2026-10-27. Ág: `codex/mission-engine`, kiinduló HEAD `290f8460`.
Döntés: [IC-013](../../adr/item_combat_013_containers.md).

Elkészült a 8-as **Tároló** és a 18-as **Kulcs** kategória a katalógus minden
rétegén:
- domain és validátor;
- determinisztikus sorsolás és nyitási terv;
- kanonikus hash (v6);
- SQL-séma (`012_containers.sql`);
- SQL-betöltő;
- `CatalogEditorTool`;
- Python szerkesztőmodell;
- a szerverkezelő Tárgyak oldala.

A szerkesztőben új „Tároló” és „Kulcs” tárgy hozható létre:
- nyitásmód, zárazonosító;
- sorsolásmód (súlyozott egy nyeremény / soronkénti esély);
- jutalomtábla kereshető tárgyválasztóval, élő esélyszámítással és mind a 8
  jutalomfajtával.

A megosztott tartalomprofil szerkesztés előtt lemásolódik. A questes dobozzá
alakított tároló fölöslegessé vált tartalma törlődik.

## Ellenőrzés

| Helyi ellenőrzés | Eredmény |
|---|---:|
| `ContainerTests` (validátor, sorsolás, nyitási terv, katalógusszabályok) | 69 ellenőrzés |
| Független Python hash-orákulum ↔ C++ | egyezik |
| Katalógus/snapshot/ital/foglalat egységtesztek a bővített fixture-rel | sikeres |
| `ItemCombatSchemaTests` saját MariaDB-ben (új tároló-korlátokkal) | 20/20 |
| `CatalogSqlReaderTests` saját MariaDB-ben (v5 + v6 fixture) | 148 ellenőrzés |
| `CatalogEditorTests` (modell, Qt-űrlap, valódi MariaDB-commit) | 19/19 |

A régi tesztekben két dolog változott, mert a fixture-ben az azonnali ital és
a kard most tárolójutalom is:
- a kivezetéses hash-próba a páncélt vezeti ki;
- a kulcsos láda jutalma a tartalék ital lett.

Ez szándékos szabály: kivezetett tárgy nem lehet jutalom.

## Helyi adatbázis

A Server2-Local `protos` adatbázisa v5-ről v6-ra frissült.
- Előtte teljes mentés készült:
  `run/protos-install/20261027-224112-b90cb952/protos-before-012.sql`,
  nyugtával és SHA-256-tal.
- Ellenőrzött: a csomag saját MariaDB-folyamata és adatkönyvtára.
- Eredmény: a meglévő 1 tárgy megmaradt, és a szerkesztő olvasási útja v6-on működik.
- Más adatbázis nem változott.

## Határ

- A GameServer még a régi `special_item_group` utat használja. Az új tervező
  bekötése, a tranzakciós kiosztás, a nyitási időkorlát és a ranglistapont
  F4-feladat.
- A régi dobozok és csoportok átalakítása az F6 konverter feladata. Ide
  tartozik a 273 csoport nélküli doboz besorolása questes vagy kivezetett
  dobozként.
