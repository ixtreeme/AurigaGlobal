# Protos — telepítés a helyi tesztadatbázisba

2026-10-27. A felhasználó külön kérésére a Server2-Local meglévő, saját
MariaDB-példányába települt az új `protos` adatbázis.

| Kapcsolat | Érték |
|---|---|
| Kiszolgáló | `127.0.0.1` |
| TCP-port | `3307` |
| Adatbázis | `protos` |
| Meglévő helyi felhasználó | `auriga_local` |
| Jelszó | A meglévő helyi SQL-kapcsolaté; nem változott |
| Verzió | MariaDB 11.4.13 |
| Katalógusformátum | schemaVersion=5, `auriga.item_catalog.v5` |

A telepítő a folyamat tárolt identitását és a tényleges `@@datadir` értéket
a `D:/AurigaGlobal/Server2-Local/run/mariadb/data` könyvtárhoz ellenőrizte.
A `protos` korábban nem létezett. A tizenkét draft SQL-fájl, 000-tól 011-ig,
új adatbázist hozott létre; nem frissített vagy törölt régi játéktáblát.

## Telepített tartalom

- 28 tényleges tábla, mind InnoDB BASE TABLE.
- 18 kategóriaregiszter-sor, kategóriánként 1-ről induló számlálóval.
- 25, a fordított szerződéshez tartozó bónuszdefiníció.
- Egy pontos v5 formátumjelző.
- Üres tárgy- és profiladatok: az `item_base` jelenleg 0 soros.

Nem kerültek bele mesterséges fixture-tárgyak. A régi `player.item_proto`
adatok nem lettek bemásolva vagy konvertálva; a +0…+9 protócsaládok összevonása
és a fejlesztési profilok tartalmi megfeleltetése külön migrációs munka marad.
A regiszter 18 kategóriahelye nem jelent 18 elkészült kategóriaimplementációt;
az új katalógusban jelenleg öt kategória tölthető.

## Jogosultság és ellenőrzés

A meglévő `auriga_local@127.0.0.1` felhasználó SELECT-jogot kapott a
`protos.*` táblákra. A jelszava és a régi adatbázisokhoz tartozó jogai
változatlanok. A Navicat/SQL-kliens meglévő helyi kapcsolatán adatbázislista-
frissítés után megjelenik az új séma. Ez az olvasási jogosultság a jelenlegi
immutable katalógusbetöltő szerződésének felel meg; a későbbi tartalomszerkesztő
külön író/publisher jogosultságot igényel.

Ellenőrzött a teljes tábla-/engine-készlet, minden kategória- és bónuszmező,
a számlálók kezdőértéke, a formátum és az összes táblasorszám. A tényleges
helyi felhasználóval, REPEATABLE READ / read-only tranzakcióban a C++ betöltő
mind a 27 táblájának explicit oszloplistás SELECT-je előkészítve és olvasva
is sikeres. Ez telepítési SQL-ellenőrzés; nem Unity-játékpróba vagy az új
runtime bekapcsolása.

Az account/common/log/player/mission táblák neve, engine-je és típusa a
telepítés előtt és után megegyezett. A végrehajtott migrációk kizárólag
`protos` nevű objektumokat írnak; más séma adataihoz nem készült írás.

Az ellenőrzési nyugta és az alkalmazott SQL-fájlok bájtpontos másolata:
`D:/AurigaGlobal/Server2-Local/run/protos-install/20261027-184502-c9d480bd/`.
A `receipt.json` tartalmazza az egyes fájlok SHA-256-ját, a végrehajtott
lépéseket, a tábla- és sorszámokat. Hitelesítési adat nincs a nyugtában.

A GameServer indulása és a `server.json` ettől nem változott. A távoli gépen
nem történt művelet.
