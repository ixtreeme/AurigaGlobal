# Itemkatalógus — SQL-betöltési fázis

2026-10-27. Ág: `codex/mission-engine`. Kiinduló HEAD: `9dfb1def`.

Elkészült az új, kategóriákra bontott katalógus SQL-betöltője az öt már
megvalósított kategóriához. Egy tranzakcióban tölti a közös adatokat, a
kategóriasorokat és a teljes fejlesztési görbéket. A régi item_proto-ból nem
olvas, és új ItemKey-t nem fordít vissza vnumra.

A valódi MariaDB-fixture ugyanazt a kilenc prototípust és v5 tartalomhash-t
adja, mint a külön C++ fixture és a korábbi független digest-orákulum.
SQL-ből betöltve is működik a +9 → +10 → +30 görbe ugyanazzal a tárgykulccsal,
új proto nélkül. A +31 és a hiányos fejlesztési görbe elutasított.

A betöltő előbb a formátumot, az izolációt, a táblatípusokat és az adatkeretet
ellenőrzi, majd minden sort típusosan dekódol és a meglévő domainvalidátornak
ad át. Külön hibát ad a regisztereltérésre, árva/hiányzó/dupla sorra, hibás
számra, hash-eltérésre és nem visszaigazolt SQL-eredményre.
A szerződés és a keretek részletei: [IC-011](../../adr/item_combat_011_catalog_sql_reader.md).

## Ellenőrzés

| Helyi ellenőrzés | Eredmény |
|---|---:|
| ServerTests build és teljes normál CTest | 98/98 sikeres |
| Célzott ASAN CTest | 11/11 sikeres |
| ASAN natív SQL-betöltő saját MariaDB-vel | sikeres |
| Külön standalone domain/dekóder CTest | 10/10 sikeres |
| Tiszta SQL-snapshot dekóder | 105 ellenőrzés |
| Prepared completion határ, SQL-kapcsolat nélkül | 20 ellenőrzés |

A natív C++ katalógus, SQL-adapter és új tesztek ASAN-instrumentáltak;
Pythonra vagy magára a MariaDB-folyamatra ez nem állítás. A teljes normál
98-as kör a korábbi item-, SQL-, Mission-, dungeon- és retired-content
regressziókat is tartalmazza. Külön Unity-játékpróba ebben a fázisban nincs,
mert a runtime átállítása még nem történt meg.

A végleges futtatások eredményeit és a forrásállapotot a jelentés melletti
`2026-10-27_catalog_sql_reader.evidence.json` rögzíti. A tesztek külön
ideiglenes MariaDB 11.4.13 példányt és SELECT-only betöltőfiókot használnak.
Az SQL-sémától független C++ fixture mellett az eredeti v5 SQL-fixture tölti
fel az adatbázist. Az SQL által már eleve tiltott hibákat a teszt nem tekinti
a C++ betöltő érdemének.

A determinisztikus párhuzamossági próba két kapcsolódó mezőt egy tranzakcióban
frissít, miközben az olvasó már megkezdte a pillanatképét. Az első betöltés
teljes régi hash-t, a következő teljes új adatokat kap. Külön próba szakítja
meg a kapcsolatot olvasás közben: az addig összegyűlt sorokból nem lesz
publikálható katalógus. A CommitUnknown/rollback/hibás completion elutasítása
közvetlenül a completion határon is ellenőrzött.

## Határ és folytatás

Ez a fázis a kategóriás adatbázis és az immutable katalógus közti hiányzó
kapcsolatot készítette el. A GameServer/Database indulási és itemgyártási
útvonalai még a korábbi rendszerben működnek; a szerver induláskor nem vált át
automatikusan az új katalógusra. A release-regiszter/publisher, a többi kategória,
a példányállapot és tartós fejlesztési tranzakció, a régi +0…+9 sorok ellenőrzött
offline összevonása, valamint a Unity bekötése még hátravan.

A következő kapcsolódó lépés az új tárgypéldány és fejlesztési művelet
tartós szerződésének folytatása: ItemKey + külön refinement_level,
ownership/revision ellenőrzés és egyetlen anyag/pénz/állapot/receipt commit.
Ezzel párhuzamosan a további megmaradó kategóriák funkciónkénti portja szükséges;
unsupported tartalom addig nem publikálható.

Csak helyi forrás/build és saját ideiglenes adatbázis változott. Meglévő játék-DB,
futó szerver, távoli gép és Unity módosítása nem történt. Az offline shop,
régi BattlePass és DragonSoul nem került vissza a rendszerbe.
