# Mission Engine – veszteségmentes eldobási commit

Dátum: 2026-10-26. Részfázis: **4/D–4k**. Kizárólag helyi fejlesztés és tesztelés.

## A régi hibautak

A korábbi DropItem részleges dobásnál az új tárgy gyártása előtt fogyasztotta
és mentette a forrásstacket. Gyártási hiba után a mennyiség már hiányozhatott.
A quickslotot is előre törölte; sikertelen földre helyezés esetén a függvény
mégis visszatérhetett sikerrel. A split csak socketet másolt, a teljes
attribútum-/megjelenítési payloadot nem.

A korábbi DropGold az előkészítés előtt felhasználta a cooldownját. A földre
helyezési hibát szintén követhette sikeres visszatérés. A tárgydarabszám és
a pénztárca eltérő számábrázolása miatt nagy összegnél a gyár normalizálhatta
a földi pénzkupac mennyiségét, miközben az eredeti összeget vontuk le.
A pénz levonása a világpublikálás után történt, ezért köztes callbackekkel
is számolni kell.

## Új előkészítés és commit

A tényleges DropItem és DropGold a natív InventorySystem.cpp fájlban él.
A publikus API és a hívók változatlanok; a nagy legacy bridge-ből a régi
implementációk kikerültek. A MoveAction és a GroundDrop admission a teljes
hívást, az előkészítést, a takarítást és a publikálást is körülfogja.

1. Karakter-, forrás-, quest-, inventory-, jogosultság- és limitellenőrzés.
2. A forrás payloadjának, karakter/PID-jának és map/pozíciójának rögzítése.
3. Split vagy arany esetén új, még el nem helyezett példány előkészítése.
4. A szükséges komponensek és a megsemmisítési timer előkészítése.
5. A tényleges SpatialService/SECTREE előkészíti a világkomponenseket és
   lefoglalja a tagsági tárolóbejegyzést. Eddig nincs forrásdebit vagy publikálás.
6. Friss ellenőrzés után egy csak komponenseket író companion együtt
   módosítja a stacket vagy a pénztárcát, a slotot/quickslotot, a ground
   metaadatokat, a timert és a sikeres limitet. A szektortagság véglegesedik.
7. Mentési, quickslot-, inventory-, világ-, pont- és naplópublikálás.

A companion nem hívhat szolgáltatást, callbacket vagy foglaló műveletet.
`false` esetén nem változtathat erőforrást; `true` után már nincs új
visszautasítási pont. Az előkészítési callbackek után újra ellenőrzött a
session-admission, az élő karakter/PID/map/pozíció, a teljes forráspayload,
az inventory-ankor és footprint, valamint a quest és a limit.

A teljes dobás megtartja az eredeti tárgy ID-ját. Részleges dobásnál a
forrás megmarad, a klón új ID/VID mellett megkapja a socketeket,
attribútumokat, flag-eket és a megjelenítési/SIG/transmutation adatokat.
Nullás, hibás mennyiségű vagy a forrás ID-ját visszaadó klón nem commitolható.
A klón takarítása csak a még saját, leválasztott RESERVED példányt érintheti;
callback által áthelyezett vagy átadott tárgyat nem törlünk.

A teljes stack dobása törli a hozzá tartozó quickslotot, a részleges dobás
megtartja. A nagy és extra-inventory cellaszámok nem csonkolódnak nyolc bitre.
A sikertelen dobás nem fogyaszt darabot, pénzt, quickslotot vagy sikeres
drop-limit/cooldown helyet. A már commitolt eredmény callbackes pickup,
áthelyezés vagy megsemmisülés után is commitolt eredmény marad.

## Világoldali szerződés

Az új SpatialService::InsertItemWithCommit és SECTREE::InsertItemWithCommit
kizárólag még nem elhelyezett itemhez használható. A normál InsertEntity
szerződése megmarad. A konstrukciós jelzések és a hash-tagság foglalása
megelőzik a companion első erőforrásírását. Visszautasított vagy kivétellel
kilépő companion után nem maradhat foglalt tagság vagy hamis SectorPlacement.

A SpatialLifecycleTests a tényleges SpatialService.cpp és sectree.cpp
implementációkat futtatja, benne a foglalás előtti/utáni sorrenddel,
visszautasítással, kivétellel, retryjal és konstrukció közbeni megszűnéssel.
A QuickslotTests a teljes termelési InventorySystem.cpp DropItem/DropGold
útját futtatja kontrollált gyár-, hálózat-, SQL- és spatial-szolgáltatásokkal.

## Arany

A földi ITEM_ELK darabszáma előjeles int, ezért egy dobás legfeljebb INT_MAX
aranyat tartalmazhat. A nagyobb kérés módosítás nélkül elutasul; nincs
csonkolás vagy eltérő összegű debit. A gyár által ténylegesen előállított
darabszámnak pontosan egyeznie kell a kéréssel.

A pénztárca rögzített egyenlegét újraellenőrizzük a commitnál. A levonás
közvetlenül a GoldAmount komponensben, publikálás előtt történik. A pontcsomag
és a karaktermentés már a commitolt állapotot látja; későbbi callback egyenlegét
nem írjuk vissza az eredeti snapshotból. A cooldown csak commitkor frissül,
az uint32 időforrás körbefordulását eltelt idő alapján kezeljük.

## Határok

Ez helyi, memóriabeli műveleti integritás, nem új tartós inventory/world
SQL-tranzakció vagy crash-biztos földi loot. A világ/DB-mentés és a teljes
Mission writer-handoff külön feladat. Nincs teljes SourceStopped nyugta,
új Mission SQL-submit vagy jutalmazás, automatikus előkészítési gameplay-hívó,
guarded legacy DML-aktiválás vagy hiteles ECS/cache rebase.
A boltok, raktár, pet, timer, harc/quest/XP/pénz források és a közvetlen
SQL-írók további összehangolást igényelnek. A korábbi egyszeri ASAN történeti
hatásnyugta-visszaolvasás oka továbbra is nyitott.

## Helyi ellenőrzés

A végső natív kód célzott próbái sikeresek:

- Inventory/quickslot/eldobási suite: **830019** ellenőrzés normál és ASAN alatt.
- Tényleges spatial/sectree suite: **1189** ellenőrzés normál és ASAN alatt.
- A két célzott CTest normál és ASAN konfigurációban is **2/2** sikeres.

A végső kódon a teljes helyi ellenőrzés is sikeres:

- A GameServer, Database és LoginServer normál és ASAN konfigurációban is lefordult.
- A kibővített normál CTest-regresszió **45/45**, benne öt izolált MariaDB-integrációval.
- A natív ASAN CTest-regresszió **33/33**; ebben a részfázisban új ASAN MariaDB-kör nem futott.
- A szezonális Mission Pass 34 védett fájlja és a nyolc korábbi Unity-fájl hash-e változatlan.
- A tényleges Server2-Local adatbázis és a távoli gép változatlan; nincs telepítés,
  sémafrissítés, FreeBSD-build vagy Unity/socket végpontok közötti játékpróba.

Naplók a helyi `build` könyvtárban:

- `mission-ground-drop-native-unit-build.log`, `mission-ground-drop-asan-unit-build.log`
- `mission-ground-drop-native-unit-tests.log`, `mission-ground-drop-asan-unit-tests.log`
- `mission-ground-drop-native-build.log`, `mission-ground-drop-asan-build.log`
- `mission-ground-drop-native-regression.log`, `mission-ground-drop-asan-regression.log`
- `mission-ground-drop-protected-hashes.json`

A következő részfeladat az NPC-bolt vétel/eladás műveleti belépésének és
hibautainak rendezése; utána a játékosboltok két résztvevős koordinációja.
Az élő Mission-jutalmazás addig zárva marad, amíg az összes író átvezetése,
a tartós writer-handoff, a hiteles rebase és a nyitott ASAN-észlelés rendezése nincs kész.
