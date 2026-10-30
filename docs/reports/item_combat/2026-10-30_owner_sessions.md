# F4.2 — Tárgytulajdonosi munkamenet

2026-10-30. Ág: `ccr-6afcbd39-c7bgeq`, kiinduló HEAD `0199196`.
Döntés: [IC-034](../../adr/item_combat_034_gameserver_runtime.md), „F4.2 döntés”.
A tárgyakat a GameServer írja közvetlenül (a felhasználó döntése).

## Elkészült

**Mentési réteg** (`ItemPersistenceSql`):
- `ProvisionItemOwner`: üres tulajdonos létrehozása (kerítés, tárolók,
  pénztárcák) egy tranzakcióban; meglévőt nem ír felül (`OwnerExists`).
- `ClaimItemOwner`: tartós munkamenet-generáció foglalása. Megvárja a
  folyamatban lévő írót; nem létező tulajdonosnál `MissingOwner`.
- `LoadItemOwner`: aszinkron változat (előkészítés + eredményfeldolgozás).
  A hiányzó tulajdonos, az elavult munkamenet és a soha ki nem adott generáció
  saját hibakódot kapott; korábban mind „sérült adat” volt.
- Mindhárom aszinkron befejezést pontosan az előkészített munkához egyeztet.
- Három új hibakód (a meglévők számozása nem változott), a próbapad magyar
  szövegeivel.

**GameServer** (`item/ItemOwnerSessions`):
- `ItemOwnerSessions`: tulajdonosonkénti állapotgép, bérlethez (betöltési
  kulcs + EnTT-entitás) kötve. Foglalás → betöltés → aktív → zárás. Az írások
  befogadása, és zárás előtt a folyamatban lévők megvárása.
- `AsyncItemSqlExecutor`: dedikált aszinkron SQL-kapcsolat; az eredményeket a
  főszál veszi át.
- `CharacterOwnerProvision`: új karakter szabványos tulajdonosa.
- A GameServer linkeli a tárgyírót (`AurigaItemPersistenceSql`).

**Tesztek:** `ItemOwnerSessionTests` (CTest `item_owner_sessions`) és
MariaDB-próba `ItemOwnerSessionTests.py` (CTest `item_owner_sessions_mariadb`,
Windowson). A két SQL-teszt közös katalógusfeltöltő segédet kapott
(`CatalogSqlPopulate.hpp`).

## Ellenőrzés

Linuxon (g++ 14, MariaDB 10.11), az F4.1-nél leírt helyi pótlásokkal.

| Ellenőrzés | Eredmény |
|---|---:|
| GameServer, Database build | sikeres |
| `ItemOwnerSessionTests --completion-only` | 66 ellenőrzés |
| `ItemOwnerSessionTests` saját MariaDB-ben | 128 ellenőrzés |
| Ugyanez AddressSanitizerrel és UB-sanitizerrel | 66 + 128, hiba nélkül |
| `ItemPersistenceSqlTests` saját MariaDB-ben (regresszió) | 947, változatlan |
| `ItemPersistenceSqlTests --completion-only`, `PersistentItemTests` | 13 + 86 |
| `ItemCatalogRuntimeTests` saját MariaDB-ben | 88 (lásd lent) |
| Valódi GameServer-indítás, 3 eset (F4.1) | mind a várt módon |
| `ServerConfigTests` | 111 |

Az `ItemCatalogRuntimeTests` most 88 ellenőrzést mutat a korábbi 217 helyett.
A tesztkatalógus sorainak beírása átkerült a közös segédbe, és már nem
számít ellenőrzésnek; a vizsgált esetek ugyanazok.

**Valódi MariaDB-n bizonyítva:**
- **Nincs kitalált tulajdonos:** nem létező tulajdonosra a foglalás és a
  betöltés is `MissingOwner`, és nem marad utánuk sor.
- **Létrehozás:** egyszer sikerül. A második kísérlet (más egyenleggel)
  `OwnerExists`, és a kerítés, az egyenleg és a tárolók nem változnak.
- **Betöltés:** az új tulajdonos betölthető; soha ki nem adott generációval
  `InvalidFence`.
- **Két core:**
  - A foglal (2. generáció), betölt és ír;
  - B átveszi (3. generáció), és látja A írását;
  - A késői írása elutasítva (`Conflict`), a tárgyszám nem változik;
  - A újratöltése `StaleSession`.
  - A memóriája magától nem tudja, hogy elavult; ezt az adatbázis dönti el.
- **Folyamatban lévő író:** egy nyitott tranzakció zárolja a kerítést.
  - A foglalás vár (400 ms után is függőben).
  - A commit után a foglalás az író revízióját látja.
  - A régebbi B munkamenet írása ezután elutasítva.
- **Ugyanazon a core-on, gyors újrabelépés:**
  - kilépés folyamatban lévő írással, közben új belépés → sorba áll;
  - az írás lezárása után foglal és betölt (6. generáció, a tárgyak megvannak).
- **Rossz beállítás:** szinkron kapcsolatra adott aszinkron munka →
  `SubmitFailed`, nem marad félkész munkamenet.

**Állapotgép (adatbázis nélkül):**
- **Elutasított megnyitás:** másik karakter bérlete, verzió nélküli kulcs,
  entitás nélküli bérlet.
- **Bérlet-határok:**
  - második bérlet `Busy`;
  - ugyanaz a bérlet `AlreadyOpen`;
  - más bérlet nem olvas, nem ír és nem zár.
- **Hibás befejezések** (egyik sem változtat állapotot):
  - ismeretlen jegy;
  - ismételt befejezés;
  - másik tulajdonos vagy módosított munka eredménye.
- **Kerítés:** csak ugyanannak a munkamenetnek az újabb revíziója viszi előre.
- **Zárás betöltés közben:** a későn érkező eredmény nem aktivál.
- **Hibák:** `MissingOwner` → Failed; utána új belépés indulhat.
- **SQL-befejezések:**
  - hiányzó tulajdonos és katalóguskötési ütközés külön kódot kap;
  - ismeretlen commit (`CommitUnknown`);
  - duplikált kulcs a kerítésen (`OwnerExists`) és máshol (`SqlFailure`).
- **Létrehozás validálása** (még SQL előtt):
  - többsoros felszerelés, duplikált vagy idegen tároló, tároló nélküli
    tulajdonos;
  - fiókpénznem karakteren, kitalált pénztárca-előzmény, túl nagy egyenleg,
    0-s tulajdonos.
- **Szabványos karakter:** az inventory rácsa egyezik a játék állandóival.

## Ami nem futott

- MSVC build és a Windowsos MariaDB-tesztmotor (`ItemOwnerSessionTests.py`).
- `CommitUnknown` valódi hálózati kiesésnél a foglalásra (csak szimulált
  befejezéssel tesztelve). A foglalás ismétlése biztonságos.

## Határ

- Az élő belépés, kilépés és karakterlétrehozás még nem hívja. Ez az F4.7
  része, az F6 importtal együtt. Indok és a bekötés pontos lépései: IC-034.
- A Database szerver nem változott.

## Következő kapu

F4.3: a betöltött `PersistentItem` ECS-komponensekké alakítása, és vissza,
azonos eredménnyel.
