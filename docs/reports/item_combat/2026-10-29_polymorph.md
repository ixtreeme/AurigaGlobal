# Átváltoztató tárgyak — helyi fejlesztési eredmény

2026-10-29, `codex/mission-engine`. Kezdő HEAD: `04ee1b4e`.

## Elkészült a forrásban

- Natív modell az elfogyó átváltoztató gömbhöz és a megmaradó gyakorlókönyvhöz.
  A forma, könyvfokozat és hátralévő gyakorlás típusos példányállapot.
- Rögzített vagy példányhoz kötött forma, külön formakóddal és mobhivatkozással.
  A már létrehozott forma jelentése nem írható át és nem törölhető.
- Konfigurálható könyvfokozat-maximum 0..255 között, egyetlen prototípuson.
  A 10→11 átmenet, a maximum és a befejezetlen gyakorlás tiltása tesztelt.
- Birtokolt használati/fejlesztési terv, szerveres admissionnel, rögzített
  döntéssel és ellenőrzött egészszámos határidőkkel. Hiányzó formára nincs
  fogyasztás; sikertelen könyvhasználat nem módosítja a tárgyat.
- A mozgásbónusz sebességpont, a támadás/védelem bázispont; külön egységet kapnak.
- SQL v17, natív betöltő, kanonikus hash, tranzakciós mentés és copy-on-write.
  Szándékos SQL-íráshibánál nincs félbemaradt profil, tárgy vagy ID-kiosztás.
- Grafikus panel a formákhoz, a 41 készségszinthez és a négy mesterfok
  időtartamaihoz. A gömb-, könyv- és szabálynézet képi ellenőrzése megtörtént.

## Ellenőrzések

| Ellenőrzés | Eredmény |
|---|---:|
| Új natív polymorph teszt | 62 ellenőrzés |
| Teljes normál CTest | 114/114 |
| Célzott AddressSanitizer domain/dekóder/hash/konverter kör | 11/11 |
| Qt + modell + saját MariaDB mentés normál helperrel | 55/55 |
| Ugyanez AddressSanitizer helperrel | 55/55 |
| Végső panelpróba a megjelenítési pontosítás után | 1/1 |

A mentési próba a negatív könyvalapesély SQL-visszaolvasását, a másodpercben
megadott mesterfok-időket, a megosztott profil másolását, a formaazonosság
védelmét és a teljes rollbacket is ellenőrzi.

A független Python encoder kitöltött v17 polymorph fixture hash-e:
`7a97926c8cbd3e49609ddf3b82d3955f6577e0562408da3ea5b23026be1c56a4`.

A meglévő felhasználói katalógus offline v7→v17 felkészítése, új saját MariaDB-be
importja és natív visszaolvasása sikeres. A kulcsok, cellák és számlálók megmaradtak;
ellenőrzött tartalomhash:
`2b6caa3835112cf231f1de016a47c6bf2703d278e6c48c909ffe44dc40bdbd2b`.
A sandbox a próba végén leállt.

Az aktív helyi `protos` mind a 33 táblájának metaadata és cellái változatlanul
egyeznek a mentéssel: v7, egy felhasználói tárgy. A telepített szerverkezelő,
a játék és a Unity ebben a lépésben nem változott. Távoli művelet nem történt.

## Audit és hátralévő munka

A rögzített forrásban hat polimorf prototípus van: öt gömb és egy könyv.
Ezek sorhash-sel leltározva vannak, de még nem teljesen konvertált tartalom.
A régi könyvidő másodperc, nem perc. A készségerő táblában a 40-es index
implicit 0 volt; ezt most explicit és szerkeszthető sor, valamint szerkesztői
jelzés teszi láthatóvá. A végleges balance-érték továbbra is kiadási feltétel.

Az általános loot-jutalom még nem tud kezdeti formát/könyvállapotot hordozni,
ezért a validátor tiltja az ilyen hiányos jutalom publikálását. Típusos grant,
tartós owner, effektlejárat, karakterstatok és kliensszinkron még bekötendő.

**A teljes itemkonverzió és a játékbekötés nincs kész.** A következő F2 munka
a megmaradó speciális működések auditja és típusos lefedése: külön ITEM_SOUL,
pet/mount, speciális használati tárgyak, resource-feldolgozás és az egyéb
altípusok hiányai. Ezután F3..F7-ben a példányok és összes referencia offline
konverziója, a Database/GameServer/Unity átállítása és a helyi játékpróba jön.
A kivezetett DragonSoul, offline shop és régi BattlePass nem tér vissza.

Szerződés: `docs/adr/item_combat_024_polymorph_items.md`.
Kísérő leltár: `2026-10-29_polymorph.audit.json`.
Forráshash-ek és tesztbizonyítékok: `2026-10-29_polymorph.evidence.json`.
