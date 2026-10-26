# Mission Engine – közös NPC-adásvételi commit

Dátum: 2026-10-26. Részfázis: **4/D–4m**. Csak helyi fejlesztés és ellenőrzés.

## Miért kapcsolódik a Mission Engine-hez?

A Mission erőforrás-átadásának egyetlen hiteles player/item állapotból kell
indulnia. Az admission megakadályozza a cache-előkészítést futó bolti művelet
közben; a bolti művelet belső commitja pedig azt akadályozza meg, hogy egy
mentés vagy callback félig levont árat és még át nem adott árut lásson.
Ez továbbra sem a teljes gazdasági producer-stop vagy új Mission-aktiválás.

## Elkészült útvonal

A normál és extended NPC-bolt a natív `InventorySystem::BuyFromNpc` műveletet
használja. A manager eladása a `SellToNpc` útvonalra kerül. Az online
játékosbolt adásvétele még a külön legacy ágban marad; az admissionje megvan,
de a közös két szereplős átadás külön feladat.

A vásárlás arannyal, legfeljebb MAX_SHOP_PRICES tárgyár-tétellel és az
extended secondary-coin pénznemmel is előkészíthető. A tényleges, kanonikus
inventory-ankorokat választjuk, zárolt, cserélt, pending, idegen, felszerelt
vagy hibás footprintű stack nem fizetőeszköz. Több azonos pénznemű ártétel
ugyanazon költségtervbe kerül, egy stacket legfeljebb egyszer debitálunk.
A terv legfeljebb 64 külön fizető stacket használhat; a nagyobb kérés az első
írás előtt elutasul. Ez nem részenként commitoló fizetési ciklus.

Az előkészítés a következőket végzi végleges erőforrásírás nélkül:

1. Shop admission, az inventory-mozgatás/eldobás által is használt MoveAction,
   hiteles karakter/PID/map/pozíció, egyenleg, ajánlat és cooldown ellenőrzése.
2. DirtyTag és ShopTimers előkészítése; teljes költség- és payload-pillanatkép.
3. Saját, még el nem helyezett árupéldány gyártása és payload-ellenőrzése.
4. A meglévő kiegészítő-absorption inicializálása közvetlenül az előkészített
   socketkomponensben, mentés/publikálás nélkül, majd újraellenőrzés.
5. Szigorú payload-egyezésű merge-terv, illetve szükséges üres slot/grid
   előkészítése. Az extended bolt korábbi külön-stack viselkedése megmarad.
6. Rune vásárlásakor a meglévő automatikus wear-slot előkészítése; a közös
   felszerelési publikáló csak a teljes commit után fut.
7. A tényleges fogyasztási várólista memóriájának előzetes lefoglalása.

## Egyetlen memóriabeli véglegesítési határ

Az új `ConsumeOwnedItemCostsWithCommit` a meglévő PreparedCosts és
PendingConsumptions réteget használja. A callback nélküli companion után
az összes fizető stack mennyisége és pending tagsága is véglegesedik;
csak ezután léphetünk ki mentésbe, csomagba vagy megsemmisítési callbackbe.

A companion csak már előkészített komponenseket írhat, nem allokálhat,
nem publikálhat, és nem törölheti a költségentitásokat. `false` esetén nem
változtathat semmit; `true` után nincs új hibára visszatérő ellenőrzési pont.
Üres költséglista is használható tisztán aranyas vásárláshoz. Egy külön,
még leválasztott gyártmány is csatlakozhat a retirementhez teljes merge után.

A közös commit együtt írja a pénztárcát, a költségeket, merge-célokat,
új tárgy darabszámát és tulajdonosát, slot/grid állapotát, szükséges
quickslot-törléseket és a sikeres cooldown-t. Teljesen elfogyó fizető stack
leválik a leltárból; nem marad a callbackek számára felhasználható nulla stack.
Részleges fizetésnél a maradék és a quickslot megmarad. Nagy és extra cellák
nem csonkolódnak nyolc bitre. A teljesen merge-ölt új gyártmány nem kap
elhelyezést; nulla/pending állapotban, skipSave mellett takarítható.

A publikálás mindig az aktuális állapotot olvassa. Egy utólagos áthelyezés,
fogyasztás, újabb egyenlegváltozás vagy entity-megszűnés nem írható felül a
korábbi snapshotból. A már commitolt eredmény nem válik újra fizetési hibává.
Sikertelen retirement esetén a nulla/pending entitás a meglévő retry-sorban
marad; a takarítás nem debitálhat vagy jutalmazhat újra.

A BUY napló az eredeti vásárolt, elhelyezett és merge-ölt mennyiséget írja;
a SHOP_PAYMENT/SELL napló a fogyasztott, előtte és utána érvényes mennyiséget.
Így a teljesen elfogyó vagy merge-ölt példány nulla aktuális darabszáma sem
veszíti el a tranzakció tényleges mennyiségét.

## Eladás

A manager meglévő felület-/map-/távolság-/ajánlat-/árhatárellenőrzése
megmarad. A natív eladás ezen felül frissen ellenőrzi az ankort, PID-t,
teljes payloadot, aktuális egységárat, darabszámot, tiltó flag-et, walletet
és cooldown-t. A jóváírás és a teljes/részleges tárgyfogyasztás együtt
commitol, a forrásquickslot és a sikeres időbélyeg is ugyanebben a lépésben.

## Fennmaradó határok

Ez memóriabeli commit, nem új tartós inventory/world vagy NPC-trade
SQL-tranzakció. A legacy karakter-/item-mentések és törlések továbbra is
külön DB-műveletek; folyamatcrash esetére ebből nem következik új tartós
exactly-once garancia. A teljes writer-handoff, guarded live DML,
DB/live inventory-egyeztetés, ECS/cache rebase és új Mission-jutalmazás még
nincs aktiválva. A korábbi ASAN történeti nyugta-visszaolvasási észlelés
okának vizsgálata külön, aktiválás előtti nyitott tétel marad.

A még pending törlési retryk és a DB-ben esetleg még létező régi tárgysorok
egyeztetése is a későbbi teljes writer-handoff része; egy befejezett gameplay
hívás admissionje önmagában nem igazol tartósan kiürített retirement-sort.

A következő részfázis az online játékosbolt vevő/eladó pénzének és ugyanazon
tárgy átadásának közös véglegesítése. Ezután az offline bolt aszinkron
DB-kérései és az összes további producer következik.

## Ellenőrzés

A natív inventory- és fogyasztási réteg próbái külön futtatják a tényleges
InventorySystem.cpp és ItemAttributeSystem.cpp implementációt. A másik
szolgáltatási oldal kontrollált teszthatár, nem két teljes szerverfolyamat.
A shop/manager-próbák a tényleges UI/protokoll belépéseket futtatják a natív
tranzakció kontrollált helyettesítőjével. Az így rétegzett unit-próbák nem
helyettesítik a későbbi Unity/socket és FreeBSD végponti elfogadást.

A célzott, végső kódot futtató kör normál és ASAN alatt is **3/3** sikeres:

- Inventory/quickslot/NPC-adásvétel: **834829** ellenőrzés mindkét konfigurációban.
- Attribútum/költség/retirement: **17752** ellenőrzés mindkettőben.
- Shop/manager belépések és ajánlathatárok: **50207** ellenőrzés mindkettőben.

A próbák a teljes/részleges költséget, az első publikáláskor már együtt
látható állapotot, a merge-et és maradékot, a sikertelen előkészítést,
időközi session-/forrás-/egyenleg-/ajánlatváltozást, speciális ablakokat,
rune wear-t, kiegészítő-absorptiont, auditmennyiségeket, reentrancyt,
cleanup retryt és publikálási kivételt is vizsgálják.
Az API saját ár-/payloadmásolatát a hívó adatait átíró factory-callback is próbálja.

A tesztharness korábbi hibás-kilépési ágában a statikus descriptorok az Asio
engine után szűnhettek meg; ezt az első új, rosszul paraméterezett fake-gyári
assertion után az ASAN kimutatta. A fake gyár helyes NPC-paraméterezése mellett
a teszt sikeres és kivételes kilépése is explicit Resetet végez, amíg az engine
él. Ez tesztélettartam-javítás, nem a korábbi Mission DB-észlelés magyarázata.
A kezdeti diagnosztika a `build/mission-npc-trade-asan-initial-failure.log` fájlban megmaradt.

A GameServer, Database és LoginServer normál és ASAN buildje elkészült.
A végső kiegészítő-/auditjavítást is tartalmazó három célzott próba mindkét
konfigurációban 3/3 sikeres. A célzott regresszió **45/45 normál** és
**33/33 ASAN** eredménnyel lezárult. A normál kör öt izolált, helyi MariaDB
integrációt tartalmaz; ebben a részfázisban új ASAN MariaDB-kör nem futott.
Ez nem minden létező szerverteszt vagy minden gameplay-útvonal elfogadása.

A 34 védett szezonális Mission Pass-fájl és a nyolc korábbi Unity-fájl hash-e
változatlan. A tényleges Server2-Local adatbázis, a Unity projekt és a távoli
gép változatlan; nincs sémafrissítés, új Mission-jutalmazás vagy telepítés.
FreeBSD build és Unity/socket végponti elfogadás ebben a részfázisban nem történt.
Az egyszeri korábbi ASAN történeti DB-nyugtaeltérés okát ezek az eredmények
nem tisztázzák; az aktiválás előtti nyitott tétel megmarad.

A részfázis helyi ellenőrzési naplói:

- `build/mission-npc-trade-native-build.log` és `build/mission-npc-trade-asan-build.log`:
  a teljes célzott fordítás első sikeres köre.
- `build/mission-npc-trade-native-final-build.log` és `build/mission-npc-trade-asan-final-build.log`:
  a végső kódot tartalmazó GameServer/Database/LoginServer fordítás.
- `build/mission-npc-trade-native-unit-build.log` és `build/mission-npc-trade-asan-unit-build.log`:
  a végső célzott tesztbinárisok fordítása.
- `build/mission-npc-trade-native-unit-tests.log` és `build/mission-npc-trade-asan-unit-tests.log`:
  a három célzott teszt végső eredménye.
- `build/mission-npc-trade-native-regression.log` és `build/mission-npc-trade-asan-regression.log`:
  a lezárt 45/33 tesztes körök.
- `build/mission-npc-trade-protected-hashes.json`: a védett fájlok ellenőrzése.
