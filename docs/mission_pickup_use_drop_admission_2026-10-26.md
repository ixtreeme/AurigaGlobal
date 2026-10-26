# Mission Engine – felvétel, tárgyhasználat és földre dobás

Dátum: 2026-10-26. Részfázis: **4/D–4j**. Kizárólag helyi fejlesztés és tesztelés.

## Bekötött műveletek

A Core admission ismert listája hat műveletcsoportot tartalmaz:
`Exchange`, `InventoryMove`, `Equipment`, `Pickup`, `ItemUse`, `GroundDrop`.
A teljes ismert maszk 63. A bind/recovery/activity/pause kezelés ugyanazt a
közös domainlistát használja, és a natív cache-előkészítés valamennyit pause-olja.
Nem változott sessionazonosító, GD/DG keret vagy SQL-séma.

| Csoport | Tényleges belépési pont |
|---|---|
| Pickup | `ItemSystem::PickupItem` |
| ItemUse | `UseItemEcs`, kliensoldali kérésből hívott `UseItem`, közvetlen `UseItemEx` dispatcher |
| GroundDrop | `ItemSystem::DropItem`, `ItemSystem::DropGold` |

Pause alatt a belépés az első tárgy-/pénzmódosítás előtt elutasul. Az engedély
a teljes híváson, a delegált műveleteken és a publikálási callbackeken át él.
A használatból indított felszerelés vagy mozgatás a saját belső admissionjét
is teljesíti. A rekurzív delegálás megengedett; új natív session/pause nem
szerezhető a még aktív hívás köré.

## Tényleges kedvezményezett a felvételnél

A földi lootot a felvevő helyett egy másik, tulajdonjoggal rendelkező online
partytag is megkaphatja. A felvevő már a keresés alatt saját aktivitást tart.
A kiválasztás után a felvevő és a kedvezményezett együtt kap Pickup-engedélyt;
ha bármelyikük zárolt, nem indul merge, aranyretirement vagy ground-detach.

Az admission komponensek konstrukciója callbacket hívhat, ezért ezután
újra kiválasztjuk a kedvezményezettet. Másik claim, megszűnt partytagság,
megsemmisült entity vagy közben létrejött pause esetén a régi kiválasztás
nem használható tovább. A hívás nem tárol kölcsönzött party-/actor-pointersort.

**A GiveGold jelenleg egyetlen karaktert ír jóvá.** A régi party-aranyosztó
kód az ItemSystem_LegacyBridge.cpp fájlban kommentként szerepel; nem kezeltük
aktív elosztásként. A jelenlegi pickupnál legfeljebb két különböző karakter
érintett, függetlenül a harmincas maximális partymérettől. Az aranyosztás
későbbi visszakapcsolása új, minden tényleges kedvezményezettre kiterjedő
belépési tervet és tesztet követel.

Minden új merge/leválasztási lépés előtt aktuális scope és jogos kedvezményezett
kell. Ha a scope a ground-detach után szűnik meg, a még saját, leválasztott
tárgy visszakerülhet a földre. A callback által már másik tulajdonosnak átadott
tárgyat a helyreállítás nem veheti vissza. A már commitolt teljes merge vagy
placement eredménye nem lesz pusztán egy későbbi callback miatt sikertelen.

A részleges merge továbbra is részleges commit: az átadott darabok a
célstackben maradnak, egy későbbi elutasításnál a maradék a földön marad.
Nem vezettünk be új, a teljes pickupot visszagörgető tranzakciót.
Az arany földi példányának sikeres retirementje utáni befejezés a meglévő
szerződést követi; ez nem új tartós, crash-biztos aranytranzakció.

## Tesztelési határ és fennmaradó munka

A Core/ECS-próbák az összes ismert domain és maszk aktivitását, atomikus
belépését, pause-át és megszakított session utáni élettartamát futtatják.
A pickup- és UseItemEcs-próbák a teljes termelési ItemSystem.cpp kódját
használják kontrollált engine-szolgáltatásokkal. A tényleges stack-merge
algoritmus külön meglévő ItemAttribute-suite-ban tesztelt; itt a pickup
vezérlési útja, engedélye, célválasztása és callback-sorrendje a vizsgált rész.

A nagy legacy UseItem/UseItemEx/DropItem/DropGold belépési őrei teljes
GameServer-builddel ellenőrzöttek; e részfázis nem fogadja el játékban
az összes itemtípus és eldobási hibautat. A régi split/create/ground-placement
hibautak nem váltak ettől tranzakcióssá. Ezek további célzott próbát és
szükség szerint külön javítást igényelnek a teljes source-stop előtt.
A használat/eldobás gyökerén a hívó karakter követett; egy alrendszer vagy
callback más karakterre gyakorolt hatásai ettől nem kapnak automatikusan
többszereplős engedélyt. A pickup konkrét kedvezményezett-kiválasztása
külön, kétszereplős integráció.

Továbbra sincs teljes SourceStopped nyugta, Mission SQL-submit, tartós
writer-handoff, guarded legacy DML-aktiválás vagy ECS/cache rebase. A külön
placement/equip/consume primitívek, boltok, raktár, pet, timer, harc/quest/XP/
pénzforrások és közvetlen SQL-írók még külön bekötést igényelnek. Nincs
automatikus cache-előkészítési gameplay-hívó vagy új Mission-jutalmazás.
Az egyszeri korábbi ASAN történeti visszaolvasási eltérés oka továbbra is nyitott.

## Helyi eredmények

A végső kód helyi ellenőrzése:

| Ellenőrzés | Eredmény |
|---|---|
| Core/ECS admission, hat domain és összes részmaszk | 1501 ellenőrzés, normál/ASAN |
| Tényleges tárgyfutásidő, pickup és natív use | 389 ellenőrzés, normál/ASAN |
| Célzott normál CTest-kör | 44/44; öt izolált MariaDB-integrációval |
| Natív ASAN CTest-kör | 32/32 |
| Windows x64 / RelWithDebInfo build | GameServer, Database és LoginServer normál/ASAN |
| Védett fájlok SHA-256 ellenőrzése | 34 szezonális Mission Pass- és 8 korábbi Unity-fájl változatlan |

Az ASAN-kör natív próbákat tartalmaz; külön ASAN MariaDB-integrációt ebben
a részfázisban nem ismételtünk. A korábbi egyszeri történeti visszaolvasás
oka továbbra sem bizonyított. A fordítás `_CL_=/MP2`, `--parallel 2`
korlátozással készült. A megőrzött helyi naplók:

- `build/mission-pickup-admission-native-unit-build.log`
- `build/mission-pickup-admission-asan-unit-build.log`
- `build/mission-pickup-admission-native-unit-tests.log`
- `build/mission-pickup-admission-asan-unit-tests.log`
- `build/mission-pickup-admission-native-build.log`
- `build/mission-pickup-admission-asan-build.log`
- `build/mission-pickup-admission-native-regression.log`
- `build/mission-pickup-admission-asan-regression.log`
- `build/mission-pickup-admission-protected-hashes.json`

SQL-séma, tényleges helyi játékadatbázis, Unity-forrás vagy távoli gép nem
módosult. Nem történt FreeBSD-build, két teljes szerverfolyamat socketes
próbája, Unity gameplay-elfogadás vagy új Mission-telepítés.
