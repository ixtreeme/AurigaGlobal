# Mission Engine – csere és tárgymozgatás műveleti engedélyei

Dátum: 2026-10-26. Részfázis: **4/D–4h**. Helyi fejlesztés és tesztelés.

## Elkészült határ

Az új natív admission réteg két konkrét műveletcsoportot követ:

| Csoport | Tényleges bekötés |
|---|---|
| Játékosok közötti csere | `ExchangeSystem::Start`, ajánlatmódosítások, `Accept`, `Complete`, lezárás és publikálás |
| Inventory drag/drop és split | A közös `MoveItemImpl`, beleértve a tárgymozgatásból hívott merge/equip/unequip ágakat és a companion splitet |

Ez a tárgymozgatás védelme; a karakter világon belüli mozgását nem állítja meg.
**Nem teljes erőforrás-source-stop:** a más útvonalon hívott equip, használat,
pickup/drop, bolt, harc/quest/XP/pénz, timer, raktár, pet és külön SQL-író még
nincs ettől megállítva. Ezekre a két csoport zárja nem ad visszaigazolást.

## Atomikus belépés és élettartam

A `Core/resource_admission` külön, önállóan tesztelhető C++ réteg.
Egy művelet minden résztvevőhöz egyszerre kap engedélyt, vagy egyikhez sem.
A résztvevőhalmaz legfeljebb nyolc elemű; az ismételt szereplőt egyszer
számolja. Rendezett state-lockok védik a több szereplőt érintő belépést.
Az engedély és a pause mozgatható, nem másolható, és birtokolt állapotot tart
életben; nem ECS-komponensre vagy descriptorra mutató pointert őriz.

Az aktivitás már a natív session bekötése előtt is követett. A legacy
művelet közben érkező callback ezért nem hozhat létre új natív kötést és
zárlatot a még futó művelet köré. Session nélkül a pause nem adható ki.
A natív kötéshez teljes session-key, karakter, account és EnTT-generáció kell;
a hívó előtte a valódi descriptor/session hitelességét ellenőrzi.

A csere külön, a csere entityjéhez kötött engedélyt tart fenn a nyitott
ajánlat teljes életére. Emellett a futó API-hívásnak is van saját engedélye.
Ha egy START/END/mentési callback közben megszűnik a cseresession, a hívás
engedélye a visszatéréséig él. A pénz/item store-írások előtt új ellenőrzés
történik. A cancel/teardown takarítása akkor is lefut, ha a session közben
érvényét vesztette; nem maradnak emiatt lezárt ajánlati tárgyak.

A mozgatás engedélye a teljes `MoveItemImpl` hívásra, az előkészítésre,
a callbackekre és a publikálásra is kiterjed. A split és a közvetlen
relocation commit előtt ismét ellenőrzi az engedélyt. Hibás előkészítésnél
nincs darabszámlevonás; a létrehozott split-tárgy a meglévő rollbacken távozik.
A már commitolt művelet eredményét egy későbbi publikálási callback nem
változtatja vissza sikertelenre.

Zárlat alatt az érintett művelet a módosítás előtt `false` eredménnyel
elutasítható. Nem indítjuk el félig, nem vonjuk le előre az ellenértéket,
és nem ígérünk egy rejtett késleltetett végrehajtást. A hívó újrapróbálhatja.
Ez nem általános gameplay-eseményhalasztó sor.

## Natív cache-előkészítés

A meglévő, hiteles `RequestPlayerCacheDrain` az alábbi sorrendet használja:

1. A korábbi GAME/load/session/account/PID/EnTT és nyitottablak-ellenőrzések.
2. Natív actor-kötés, majd a két ismert csoport pause-ának megszerzése.
   Futó mozgatás vagy nyitott csere esetén ez nem sikerülhet.
3. A már elkészült player/item pillanatkép és a mentések → Begin bájttömb.
4. A pause az entity saját `NativeResourceAdmission` komponensébe kerül.
5. A pozitív cache-nyugta csak az aktuális scope, tényleges PID, pontos
   actor-state példány és még tartott pause mellett olvasható vissza.

A pause másik actorból – még azonos key/entity adatok mellett is – nem
helyettesíthető be. Komponensáthelyezés után a tényleges state azonossága
marad meg; új session/rebind esetén a régi állapot érvénytelen lesz.
A régi pause megsemmisítése nem oldhatja fel az új session zárlatát.

Sikertelen preflight/capture esetén a helyi RAII pause felszabadul.
Terminális negatív nyugta feloldja a pause-t, a hibaválasz visszaolvasható
marad. Új GD-írás előbb érvényteleníti a cache-bizonyítékot, majd feloldja
a hozzá tartozó pause-t. Ez olyan fázisváltásnál is megtörténik, ahol a
karakter már nem GAME/LOADING állapotú; idegen új sessiont nem érinthet.

A sikeres cache-nyugta után is legfeljebb a Begin eredeti tíz másodperces
előkészítési határáig tartható a pause. A poller a lejárt nyugtát visszavonja,
és csak utána oldja fel a két csoportot. Kapcsolatszakadás, karakterválasztás,
descriptorzárás és entity-megsemmisítés érvényteleníti az actor állapotát.

Megszakított előkészítés/session után az új legacy művelet csak akkor kezdhet
új, session nélküli aktivitásállapotot, amikor a korábbi engedélyek és pause-ok
már lezárultak. Ettől a régi natív kötés és cache-bizonyíték nem éled újra;
session nélküli actor nem kaphat pause-t. Ez elkerüli, hogy egy előkészítési
hiba vagy DB-link reset tartósan megbénítsa a még létező legacy karaktert.
Ez a helyreállítás is kizárólag a jelenlegi, Mission SQL-submit nélküli
szakasz szerződése; tartós writer-tulajdonlás mellé külön recovery mód kell.

**A timeoutos feloldás csak erre az előkészítő szakaszra érvényes.** Nincs
Mission SQL-submit ebben a lépésben. Egy későbbi `CommitUnknown` állapotot
nem szabad ezzel automatikusan feloldani; a tartós írókoordinátornak külön
submit/reconcile/rebase élettartam kell.

## Továbbra is zárt kapuk

Nincs automatikus gameplay-hívó a cache-előkészítésre, `SourceStopped`
nyugtázás, writer-lease, guarded legacy DML-aktiválás, erőforrás-rebase vagy
Mission-jutalmazás. A két pause-bit nem jelenti az összes producer lefedését.
A többi író és a teljes DB/live inventory-egyeztetés még szükséges.

A következő munka a többi inventory-gazdasági belépési pont portolása
ugyanebbe az admission szerződésbe, a folyamatos/timer/quest/harci források
eseménymegőrzésével és a külön SQL-lane-ek rendezésével. Csak ezek után
kapcsolható valódi teljes source-stop a már meglévő fence backendhez.
A korábban egyszer észlelt ASAN történeti hatásnyugta-visszaolvasási eltérés
oka ettől külön, aktiválás előtti nyitott tétel marad.

## Ellenőrzés

A natív kernel és ECS-híd tesztje a tényleges Core- és komponenskódot futtatja.
A csere- és mozgatási próbák a teljes termelési `exchange.cpp`, illetve
`InventorySystem.cpp` kódját használják, kontrollált engine-szolgáltatásokkal.
A protokollpoller, descriptor- és Database-link életciklus bekötése teljes
szerverbuilddel ellenőrzött; ez nem két teljes folyamat közti socketes vagy
Unity gameplay-elfogadás.

A végső kód ellenőrzése:

| Próba | Eredmény |
|---|---|
| Core/ECS admission, élettartam és callback-sorrend | 1081 ellenőrzés |
| Tényleges játékoscsere | 14736 ellenőrzés |
| Tényleges inventory/quickslot és split | 821002 ellenőrzés |
| Célzott normál CTest-regresszió | 44/44 sikeres, köztük öt izolált MariaDB-integráció |
| Natív ASAN CTest-regresszió | 32/32 sikeres |
| Teljes Windows x64 / RelWithDebInfo build | GameServer, Database és LoginServer normál és ASAN alatt |
| Védett fájlok SHA-256 ellenőrzése | 34 szezonális Mission Pass- és 8 korábbi Unity-fájl változatlan |

Az új admission-, csere- és inventory-próbák normál és ASAN változatban is
sikeresek. A 32-es ASAN-kör nem tartalmaz valódi MariaDB-integrációt; ebben
a részfázisban külön ASAN DB-próbát nem ismételtünk. A korábbi ASAN
visszaolvasási észlelés ettől továbbra is nyitott.

A fordítás korlátozott párhuzamossággal (`_CL_=/MP2`, `--parallel 2`) futott.
A megőrzött helyi naplók és ellenőrzési eredmény:

- `build/mission-operation-admission-native-final-build.log`
- `build/mission-operation-admission-asan-final-build.log`
- `build/mission-operation-admission-native-regression.log`
- `build/mission-operation-admission-asan-regression.log`
- `build/mission-operation-admission-protected-hashes.json`

Nem módosult SQL-séma, a tényleges helyi játékadatbázis, Unity-forrás vagy
távoli gép. FreeBSD-build, két teljes szerverfolyamat socketes próbája és
Unity end-to-end elfogadás ebben a részfázisban nem történt.
