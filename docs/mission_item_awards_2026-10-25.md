# Mission Engine 4/D–3 – Mission → award → tárgy

Dátum: 2026-10-25. Ág: `codex/mission-engine`.

## Elkészült szerződés

Az `IssueItemAwards` egy teljes, kizárólag tárgyakat adó, verzióhoz kötött
RewardBundle-hez saját adatokat birtokló prepared tranzakciót készít. A hívó
aszinkron Mission SQL-workerre küldi; a helper nem vár SQL-re és nem érint ECS-t.
A natív API elkészült és a tényleges kernel/store/cache/kézbesítő útvonalon
izoláltan tesztelt. Élő session/core-dispatcher és gameplay binding még nincs bekapcsolva.

Egy tranzakció véglegesíti:

1. Az owner-epoch/token, storage revision, checkpoint digest és teljes outbox-action ellenőrzését.
2. A Mission entitlement-ledgert és az outbox állapotát.
3. A hiteles karakter → account ID → normalizált login SQL-összerendelését.
4. A teljes bundle minden `player.item_award` rekordját.
5. Az action/payload/címzett → konkrét award ID tartós mappingjét.
6. A teljes kéréshez kötött műveleti nyugtát.

Bármely hiba mindezt visszagörgeti. A valódi award INSERT és a mapping közötti
kapcsolatvesztés sem hagy külön átvételi jogot vagy hiányzó tárgyjogosultságot.
A rollback hézagokat hagyhat az AUTO_INCREMENT számozásban.
Az előző fázis commit-sorrend-biztos cache-felderítése ezeket a hézagokat is kezeli.

## Tárolás és kiadás

Új tábla: `mission_engine_item_awards`, a külön Mission-adatbázisban.
Sémaverzió **3**, összesen **kilenc InnoDB-tábla, nyolc FK**. A harmadik additív
migráció a verziót utoljára emeli; a régebbi migrációk újrafuttatása nem csökkenti.
Az önálló `db/mission_schema.sql` mindhárom migrációból készül.
A `manage.py init-mission-db` frissíti a már létező külön Mission-sémát is,
a saját adatokat megőrizve. A tesztek kizárólag saját ideiglenes SQL-példányt módosítottak.

A mapping az action teljes identitásához és payload digestjéhez kötött; soronként
rögzíti a sorrendet, eredeti award ID-t, vnum/countot, account ID/logint,
account/player sémát és player-táblapostfixet. A teljes kérés digestjébe a
címzett és a célok is bekerülnek. Azonos műveletazonosító más kéréssel konfliktus.
A mapping az award vagy a tényleges tárgy későbbi törlése után is megmarad;
a receipt nem utasítás új tárgy létrehozására.

A canonical award saját AUTO_INCREMENT számozása változatlan. A mapping az
ugyanazon kapcsolaton közvetlenül előző, egyetlen sort beszúró INSERT
`LAST_INSERT_ID()` értékét használja. Nem MAX(id), és nem generál item ID-t.
A valódi item ID-t továbbra is a hiteles Database meglévő tartománya foglalja.

A jutalom SAFEBOX-ba átvehető jogként készül. Az egész bundle awardjai együtt
jönnek létre; a raktár nyitásakor a konkrét tárgyak a közös, awardonként atomi
kézbesítővel készülnek el. Telt raktárnál a fennmaradó jogok később átvehetők.
Ez nem egy nyitott inventoryba történő, minden tárgyat egyszerre publikáló ECS-művelet.
A kernel a teljes, igazolt kiadási receipt után nyugtázhatja a jutalompontot.

## Egyeztetés és határok

A `ResolveItemAwards` ugyanazzal a karakterzárral megvárja az eredeti írót,
majd aktuális locking readekkel olvassa a műveleti és kiadási nyugtát.
A `DecodeItemAwards` teljes, pontos mappinget fogad el. Hiányos, sérült,
más payloadhoz/címzetthez tartozó receiptet elutasít. Új műveleti tokennel
érkező ismételt kérés is az eredeti award ID-kat kapja vissza.
Egy korábbi szimbolikus entitlement mapping nélkül hibás/hiányzó kiadásnak
számít: nem tekintjük tárgyjutalomnak és nem pótoljuk vakon.

A trusted koordinátor feladata a session hitelesítése, a tényleges tárgyproto és
stack-korlát ellenőrzése, a megadott sémák és SQL-végpontok helyes kiválasztása,
és a callback karakter/session/generáció ellenőrzése. A helper az SQL-ben is
összeveti a karakter/account/login hármast, de ez nem önálló kliensjogosultság-ellenőrző.
A prototípuskorlátokat a hívó adja, nem az authoring csomag vagy a kliens.
Egyedi prototípusmigrációnál a már függő awardokat is figyelembe kell venni.

A Mission, player és a hitelesítéshez használt account séma ehhez az adapterhez
azonos MariaDB-példányban kell legyen. A konfiguráció külön távoli accountja
nem akadályozza a Mission store-t; ezt a tárgyadaptert ilyen felállásban a
későbbi binding nem engedélyezheti külön identitásprotokoll nélkül.
A saját helyi telepítő csak együtt elérhető accountnak ad SELECT-jogot,
és az effektív Database/account sémanevet használja. Külső SQL-jogokat nem módosít automatikusan.

Az érintett forrástáblák InnoDB-k legyenek, a canonical award ID legyen
AUTO_INCREMENT elsődleges kulcs, a források és Mission-séma triggermentesek,
a FK/CHECK ellenőrzések pedig bekapcsolva. Az egyedi DDL-módosítás közbeni
folyamatos üzem nem támogatott telepítési mód. A korábbi cache-pending sorok
payloadját külső író nem módosíthatja kézbesítés közben.

Az XP-t, pénzt vagy tárgyfogyasztást is tartalmazó bundle teljes egészében
elutasításra kerül, üres SQL-tervvel. Nincs csendes részleges jutalom vagy
az SQL-tranzakción kívüli élő inventory-/XP-/pénzmódosítás.
A Gameplay binding kapuja továbbra is zárt.

## Ellenőrzés

A teszt az eredeti `db/schema.sql` account/player/item/item_award/safebox DDL-jét
használja három külön, véletlen nevű sémában, saját MariaDB 11.4.13 példánnyal.

- Valódi kernel-esemény → jutalompont → checkpoint/outbox → teljes award-kiadás.
- Normál és új tokenes retry, más címzettel újrahasznált token, két egyidejű kiadó.
- Hamis account/login, hiányzó/túl kicsi proto-korlát; maximum 32 tételes bundle SQL-budgetje.
- XP, pénz és fogyasztás külön-külön is teljes elutasítás.
- Rollback a ledger után, award és mapping között, illetve minden írás után.
- MyISAM és trigger elutasítása, hiányzó INSERT-jog mellett teljes rollback,
  majd minimális cross-schema joggal sikeres újrapróbálás.
- Tényleges cache-felderítés → telt 45 cellás raktár → felszabaduló cella → valódi item.
- Ismételt claim, elfogyasztott/törölt item és változatlan eredeti kiadási receipt.
- Elavult owner, megszakított küldetés, sérült mapping, mapping nélküli szimbolikus ledger.
- Függő award INSERT után kilőtt SQL-worker; az egyeztető megvárja a rollbacket.
- A valódi COMMIT-válasz eldobása; MariaDB kényszerített leállítása és friss C++ folyamatból visszaellenőrzés.

Normál célzott regresszió: **34/34**. ASAN célzott CTest: **11/11**.
A végleges Mission MariaDB-kör normál/ASAN **1063 + 91 cold-process** ellenőrzés;
a polling száma futásonként változhat. A külön SQL-telepítő/bootstrap teszt **13/13**.
A cache/delivery kör normál **2823 + 111**, ASAN **2827 + 111** ellenőrzés.
GameServer és Database normál/ASAN Windows x64 build sikeres.
A szezonális Mission Pass 34 védett fájlja és a korábbi nyolc Unity-módosítás változatlan.

Nem történt push, futó helyi/távoli szerver telepítése, valódi játékosadat-módosítás,
FreeBSD fordítás vagy Unity-játékmeneti elfogadás. Az elkészült native adapter
és a lezárt hibapróbák után a kevert erőforráshatások és a session/core-koordinátor következik.

Naplók: `build/mission-item-issuance-regression-tests.log`,
`build/mission-item-issuance-final-asan-db-tests.log`,
`build/mission-item-issuance-asan-unit-tests.log`,
`build/mission-item-issuance-final-bootstrap-tests.log`.
