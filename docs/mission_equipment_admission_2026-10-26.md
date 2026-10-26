# Mission Engine – közvetlen felszerelés és levétel

Dátum: 2026-10-26. Részfázis: **4/D–4i**. Kizárólag helyi fejlesztés és tesztelés.

## Bekötött műveletek

A 4h csere- és inventory-mozgatási engedélyei mellé külön `Equipment`
műveletcsoport került. A Core az ismert csoportok közös listájából keresi az
aktivitásszámlálót; a belépés, a pause, a sessionkötés és az anonim legacy
helyreállítás mindhárom csoport aktivitását figyelembe veszi.

A tényleges `InventorySystem.cpp` alábbi belépései tartanak engedélyt:

- `ItemSystem::EquipItemEcs`: közvetlen felszerelés és felszereléscsere.
- `ItemSystem::UnequipItemEcs`: automatikusan választott inventory-cél.
- `ItemSystem::UnequipItemToEcs`: kifejezetten megadott inventory-cél.

A tárgymozgatásból delegált felszerelés mind a `InventoryMove`, mind a
`Equipment` belépési feltételét teljesíti. Egy külön Equipment-pause alatt a
szokásos inventory-áthelyezés még működhet, a viselési helyre húzás elutasul.
A natív cache-előkészítés a bővült `Supported` maszkkal mindhárom ismert
csoportot pause-olja; ehhez nem változott GD/DG üzenet vagy SQL-séma.

## Élettartam, callbackek és helyreállítás

Az engedély az első felszerelési szolgáltatáshívás előtt megszületik, és
visszatérésig él: az effekt-, pont-, timer-, socket-, mentési és publikálási
callbackek, valamint a hibás csere helyreállítása alatt is. Az admission
komponens létrehozása után a karakter és tárgy érvényességét újraellenőrizzük.

Az új teszt egy korábbi admission-hibát is előhozott: az EnTT
`get_or_emplace` a konstrukciós jelzés után akkor is referenciát keres,
ha a hívó eldobja az eredményét. Ha a jelzés közben megszűnik a komponens
vagy a karakter, ez assertionhöz vezethet. Az admission közös konstrukciója
most egyetlen elemes, move-iteratoros `insert` műveletet használ, amely
nem ad vissza referenciát. Utána friss lookup validál. A saját komponensét
vagy entitását törlő callback külön Core/ECS-próbát kapott; ez a 4h csere-
és mozgatási belépéseit is javítja.

Pause alatt a közvetlen felszerelés vagy levétel módosítás nélkül elutasul.
Feloldás után ugyanaz a kérés újrapróbálható. A belső, rekurzív
fegyverkosztüm-levétel az eredeti engedélyt használja; nem hoz létre új sessiont.

A szabályellenőrzés után és egy csere újabb, még el nem kezdett lába előtt
az engedély aktuális érvényessége is feltétel. Ha a scope a két leválasztás
között megszűnik, nem indul el a következő átadás. Az eredeti leválasztott
tárgyak a már meglévő helyreállítási útvonalon térhetnek vissza.

A helyreállítás alacsony szintű `EquipTo`/placement műveleteit nem blokkoljuk
új admissionnel: az eredeti, még élő engedély alatt kell befejeződniük.
Ezek a primitívek más hívóktól továbbra sincsenek általánosan védve.
A sikeresen commitolt felszerelést egy későbbi publikálási callback miatti
scope-érvénytelenítés önmagában nem változtatja sikertelenné.

Ez az admission atomikus megszerzése és a teljes hívás követése, nem egy új,
minden effektet visszagörgető felszerelési tranzakció. A korábbi alacsony szintű
effect/placement/teardown szerződések megmaradnak. Tartós Mission SQL-submit
vagy writer-lease e részfázisban továbbra sincs.

## Fennmaradó írók

A teljes source-stop még nem kész. A következő csoportok külön munkát igényelnek:

- Felvétel: az összes tényleges kedvezményezett követése. A pickupot kezdeményező
  karakter helyett másik partytag is kaphat tárgyat vagy aranyat. Utólagos
  pontosítás a 4j forrásellenőrzése alapján: a party-aranyosztás kommentben van,
  a jelenlegi GiveGold kizárólag a kiválasztott karaktert írja jóvá.
- Tárgyhasználat és eldobás: a natív belépések mellett a nagy legacy dispatcher,
  külön quest/consume/átadás és földre helyezési hibautak rendezése.
- Közvetlen, más rendszerekből hívott equip/unequip/placement primitívek,
  boltok, raktár, pet, időzítők és külön SQL-írók.
- Harci, quest-, XP- és pénzforrások eseményvesztés nélküli összehangolása.

Ezt követi a teljes DB/live inventory-egyeztetés, tartós writer-handoff,
guarded legacy DML és a hiteles ECS/cache rebase. Az új Mission-jutalmazás
és az automatikus cache-előkészítési gameplay-hívó továbbra is zárt.
A korábbi egyszeri ASAN történeti hatásnyugta-visszaolvasás oka külön,
aktiválás előtti nyitott tétel marad.

## Helyi ellenőrzés

A Core/ECS domainpróbák a tényleges admission réteget, a felszerelési próbák
a teljes termelési `InventorySystem.cpp` kódot használják kontrollált engine
szolgáltatásokkal. Külön próbák vizsgálják a zárolt felszerelést/levételt,
a maszkszétválasztást, a timer/socket/pont/publikálási callbackek élettartamát,
a félbeszakított csere helyreállítását és az entity-megsemmisítést konstrukció
közben. Ez nem két teljes szerverfolyamat vagy Unity gameplay-elfogadás.

A végső kód helyi eredményei:

| Ellenőrzés | Eredmény |
|---|---|
| Core/ECS admission és konstrukciós hibautak | 1126 ellenőrzés, normál/ASAN |
| Tényleges inventory/quickslot/felszerelés | 823662 ellenőrzés, normál/ASAN |
| Célzott normál CTest-kör | 44/44, köztük öt izolált MariaDB-integráció |
| Natív ASAN CTest-kör | 32/32 |
| Windows x64 / RelWithDebInfo build | GameServer, Database és LoginServer normál/ASAN |
| Védett fájlok SHA-256 ellenőrzése | 34 szezonális Mission Pass- és 8 korábbi Unity-fájl változatlan |

Az ASAN-kör natív teszteket tartalmaz; ebben a részfázisban külön ASAN
MariaDB-integráció nem futott. A korábbi történeti visszaolvasási eltérés
oka ettől nem vált bizonyítottá. A fordítás `_CL_=/MP2`, `--parallel 2`
korlátozással készült. Megőrzött helyi naplók:

- `build/mission-equipment-admission-native-unit-build.log`
- `build/mission-equipment-admission-asan-unit-build.log`
- `build/mission-equipment-admission-native-unit-tests.log`
- `build/mission-equipment-admission-asan-unit-tests.log`
- `build/mission-equipment-admission-native-build.log`
- `build/mission-equipment-admission-asan-build.log`
- `build/mission-equipment-admission-native-regression.log`
- `build/mission-equipment-admission-asan-regression.log`
- `build/mission-equipment-admission-protected-hashes.json`

SQL-séma, tényleges helyi játékadatbázis, Unity-forrás és távoli gép nem
módosult. Nem történt FreeBSD-build, két teljes szerverfolyamat socketes
próbája, új Mission gameplay-elfogadás vagy telepítés.
