# Eszközök és halászati tárgyak — helyi fejlesztési eredmény

2026-10-28, `codex/mission-engine`. Kezdő HEAD: `19971874`.
A korábban elkészült kosztümréteg `19971874` commitba került a folytatás előtt.

## Elkészült

- Csákány, horgászbot, fejsze és metszőolló natív eszközmodellje és szerkesztője.
- Külön gyakorlópont, pontos N/D gyakorlási esély és szintenkénti fejlesztési
  szabályok. Siker, pontvesztés és szintvisszaállítás rögzített döntésből.
- Egy prototípus +0..+max szinttel; a maximum 0..255. A tesztek +30 profilt,
  +10→+11 átmenetet és a gyakorlópont felső egészszámhatárát is lefedik.
- Élő/döglött hal és csali, explicit loot- és sütési eredménykulcsokkal.
  A sütés megőrzi a teljes halom mennyiségét; a csali cseréje felülírja a bot
  csalierejét, nem összeadja. Nincs vnum-eltolással számolt céltárgy.
- SQL v16, natív betöltő, kanonikus hash és tranzakciós mentés. A másolt
  fejlődési profil nem írja át az eredetit. Szándékos SQL-íráshibánál a profil,
  tárgy és azonosítószámláló együtt visszagördül.
- Grafikus eszköz- és halpanel, inaktív nem alkalmazható mezőkkel. A két új
  panel helyi képi ellenőrzése megtörtént; a +0 sor egy átmenetileg rárajzolódó
  régi Qt cellavezérlőjét is javítottam.

## Tesztek

| Ellenőrzés | Eredmény |
|---|---:|
| Új natív gathering teszt | 74 ellenőrzés |
| Teljes normál CTest | 113/113 |
| Célzott ASAN domain/dekóder/konverter/hash kör | 10/10 |
| Qt + modell + saját MariaDB mentés normál helperrel | 51/51 |
| Ugyanez ASAN helperrel | 51/51 |
| Végső bővített +30 SQL- és panelpróba normál/ASAN | 2/2 és 2/2 |

A végső célzott próba teljes, kitöltött +30 tool profilt (N/D esély, nem NULL
kudarcszint), élő/döglött halat, sütési hivatkozást, csalit és rollbacket olvas
vissza SQL-ből. A közönséges smith-receptek nem kerülhetik meg a tool traininget.

A független Python encoder kitöltött v16 gathering fixture hash-e:
`23c9a08d2b92686f4fe4e6edb50747fec3daa8a82230913d3993a8f85ace7cbe`.

A megőrzendő valódi felhasználói katalógus offline v7→v16 felkészítése, új saját
MariaDB-be importja és natív visszaolvasása sikerült. Kulcsai, adatai és számlálói
megmaradtak; ellenőrzött hash:
`cc3982022a8a8baf310ff227e28780999bb06f1aed73bb043393b06a0ff63a5e`.

Az aktív helyi `protos` mind a 33 táblájának metaadata és cellái továbbra is a
mentéssel egyeznek (v7, egy felhasználói tárgy). A sandboxok leálltak.
Távoli és Unity-módosítás nem történt, a telepített app sem lett lecserélve.

## Audit és fennmaradó munka

139 forrássor kapott sorhash-t és besorolást: 70 eszköz, 50 hal, 4 csali,
14 resource-anyag és 1 tábortűz. Hat bot fejlesztési éle önmagára mutat.
A régi axe/shears fegyverprototípusok harci jelentését és a botfejlesztés
fordítási egységenként eltérő makróhasználatát a teljes konverter előtt
rendezni kell. Ezek nem automatikusan publikált receptek vagy balance-döntések.

Ez az IC-012 F2 egy újabb része; **a teljes régi tartalom konverziója és a
játékbekötés nincs kész**. A tábortűz, resource-feldolgozás, teljes fogási táblák,
speciális altípusok, polimorf, külön ITEM_SOUL és pet/mount rendszerek is maradtak.
Ezután következik a példányok és referenciák teljes offline konverziója,
Database/GameServer/Unity átállítása és a helyi játékpróba.

Részletes szerződés: `docs/adr/item_combat_023_gathering_tools_and_fishing.md`.
Kísérő leltár: `2026-10-28_gathering.audit.json`.
Forráshash-ek és tesztbizonyítékok: `2026-10-28_gathering.evidence.json`.
