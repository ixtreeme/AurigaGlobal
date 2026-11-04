# ADR IC-054 — Gyűjtőeszközök és csali szerveres–Unity útja

2026-11-04, C25b. Alapmodell: [IC-023](item_combat_023_gathering_tools_and_fishing.md).
Előzmény: [IC-053](item_combat_053_fish_processing_runtime.md).

Ez a szelet az eszközpéldány mentését, felszerelését, adatait, fejlesztését és
a felszerelt horgászbot csalizását viszi végig. A tényleges fogás/gyűjtés és
az annak befejezéséért járó gyakorlópont a C25c része. A teljes C25 még nyitott.
A normál legacy leltár átállítása a felhasználó döntése szerint továbbra is vár.

## Példány és felszerelés

A 11-es kategória saját `ToolInstanceState` payloadot kap: gyakorlópont és
csalierő, mindkettő u32. Az élő eszköz egy darabos, szintje a profilban létező
szint; pontja nem haladhatja meg a szint követelményét. Csalit csak a bot tart,
ereje legfeljebb INT32_MAX. Más kategórián ez a payload tiltott.

A `009_gathering_tools.sql` két nullable oszloppal bővíti az item_instance-t.
Mindkét érték része a példányhash-nek és a teljes before-state CAS-nak; a korábbi
payload nélküli tárgyak hash-e változatlan. A grant üres állapotot inicializál,
a helyi adminpróbapad explicit tesztpontokat is megadhat. Minden általános
mozgatás a teljes példányt őrzi.

A csákány, horgászbot, fejsze és metszőolló a fegyver kézhelyét használja:
Equipment/storage 1, anchor 0. A felszerelés az ott lévő fegyverrel a meglévő
atomi csereútvonalon történik. A gyűjtési erő nem válik harci támadóértékké.
A szerver külön eszközfajtát küld; Unity ezt a típust és a katalógus explicit
modelljét használja. A bot halászati mozgásmódot kér. Runtime vnum-adapter nincs.

## Műveletek

Az eszköz adatai a leltárból és felszerelve is megtekinthetők. A fejlesztéshez
levett, teljes gyakorlatú, nem maximális eszköz és megfelelő közeli NPC kell:
bothoz horgász (9009), a többihez kovács (20016 vagy 20091). A szolgáltató
élő, ugyanazon a mapon álló valódi világentitás, távolsága 1000 alatti.
Az előnézet rögzíti az entitás generációját és VID-jét; megerősítéskor a
közelség és a szolgáltatói szerep újra ellenőrzött.

Siker esetén ugyanaz a példány egy szintet nő, gyakorlata nullázódik, csalija
megmarad. Kudarc esetén a profil pontos, egész számos pontvesztése vagy
szint-visszaállítása érvényes. A 0 és 100 százalékos eset nem kér véletlen
dobást; köztes esélyhez torzítatlan kriptográfiai dobás tartozik.

A csali előnézete a saját felszerelt botot választja. A külön megerősítés
az előnézet célpéldányára szól. Egy csali fogy, az új erő pontosan felülírja
a korábbit, gyengébb csalinál is; nincs összeadás vagy maximumválasztás.
A fogyasztás, a bot módosítása és az egyszeri receipt egy tranzakció.
Az utolsó darab elfogyása után a mentett eredményablak olvasható marad.

A preview rögzíti a katalógust, owner-sessiont és a teljes forrás/célpéldányt.
Confirm előtt ezek és a karakter engedélyezett állapota ismét ellenőrzöttek.
Egy folyamatban lévő mentés blokkolja az új tárgyműveletet; hibás reload bontja
a kapcsolatot. A visszahívás csak az eredeti login tokenjéhez alkalmazható.
Az ismételt megerősítés az elmentett eredményt küldi, új fogyasztás nélkül.

A gyakorlás domain API-ja csak `completedResourceAction` mellett fogad el
egy befejezett szerveres eseményt, legfeljebb egy ponttal és ugyanazzal az
operation ID-val. Ezt domain- és SQL-tesztek ellenőrzik. Publikus gyakorlás-
csomag vagy gomb nincs; a valódi eseménybekötés C25c. A typed karakter legacy
halászati/bányászati indítása addig tiltott. Aktív gyűjtés alatt csalizás,
fejlesztés és tárgymozgatás sem engedett.

## Protokoll és kliens

| Csomag | Változás |
|---|---|
| GC225 | Feltételes v6, flag 64: pont:u32 és csali:u32 a meglévő payloadok után; v1–v5 megmarad |
| GC227 | v2 Tool action 16384; eszköznek kézhely és Equip, csalinak Tool művelet |
| GC231 | Feltételes v2, a v1 végén eszközfajta:u8; weapon=0 |
| CG254 | A meglévő 26+sequence bájtos recovery szerkezet: action 4 info, 5 fejlesztés-preview, 6 confirm, 7 csali-preview, 8 confirm, 9 close |
| GC254 | v2 eszközválasz; a C25a v1 halválasza változatlan |

A GC254 v2 minimum 130 bájt, little-endian. Sorrend:
header/size/version/status/error; nonce; source ID/revision/ItemKey/count/action;
tool ID/revision/ItemKey/level/kind; power/bonus/points/required/bait/pool;
practice numerator/denominator; refine chance/failure kind/failure level/loss;
equipped/service VID/new bait power; outcome/roll/after level/points/bait;
GC227-hossz és GC227. A hiányzó failure level 65535. A szigorú kodek
ellenőrzi az azonosítókat, tartományokat, műveletformákat, végállapotot és
a metaadat-kulcsokat. Hat natív golden rögzíti a wire-szerződést.

Unityben az `Eszköz`/`Csalizás` inventorygomb külön ablakot nyit. Az adatok után
a fejlesztéshez külön szerveres előnézet, majd áttekintés és megerősítés kell.
A csali régi és új ereje is látható. A UI nem von le tárgyat és nem módosít
szintet helyben; a szerver snapshotja a tekintély. Megváltozott forrás vagy
bot érvényteleníti a megerősítést, kijelentkezés törli a munkamenetet.

Ellenőrzések és nyitott kiadási kapuk:
[C25b jelentés](../reports/item_combat/2026-11-04_tools_runtime.md).
