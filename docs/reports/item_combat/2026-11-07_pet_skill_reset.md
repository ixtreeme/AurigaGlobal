# C26j — Fejlődő pet képességeinek elfelejtése

A szerver `20261103` és a Unity `/main/unity63-integration-20261103`
ágán a kiválasztott és a teljes képességtörlés közös munkában készült.
A párosított Unity changeset a `cs:82`.

A pet adatlapjáról külön **Képesség elfelejtése** panel nyílik. A szerver
felsorolja az adott hatókörhöz megfelelő leltári ellátmányokat. A játékos
előnézetet kér, majd külön megerősíti a törlést. Egy `ResetOne` tárgy egy
tanult képességet, egy `ResetAll` tárgy az összeset törli. Az üres vagy
zárolt hely nem fogyaszt tárgyat. Az elfelejtéshez a pet tényleges követése
szükséges. A feloldott hely, a szint, az evolúció és a születési értékek
megmaradnak. A törölt képesség passzív bónusza azonnal eltűnik.

A CG250 rögzített kerete három új műveletet kap. A GC253 v5 a v4 tanulási
adatblokkját használja külön elfelejtési státuszokkal. Törlésnél nincs
rangnövelés vagy esélydobás; a kiválasztott hely, illetve az összes hely
hatókörét adja át. A régebbi csomagverziók továbbra is külön értelmeződnek.

A megerősítés a teljes ellátmánypéldányt, a képességhelyeket, a katalógus
hash-ét, az ajánlat idejét és a pet tényleges követését ismét ellenőrzi.
A képességtörlés és az egy tárgy levonása egy receiptelt tranzakció.
A SQL-író újratervezi a `PSR` bizonyítékban rögzített műveletet; a név vagy
egyéb kapcsolódó petadat önkényes módosítása elutasított. A tanulási
várakozási idő a tulajdonoshoz tartozik, ezért a törlés nem nullázza.

## Ellenőrzési módszer

A `PetPassiveUnityLiveFixture.py --skill-reset` külön fiókokat és új
katalógussorokat készít. A pet két nyitott és két zárolt hellyel, életkori
HP-bónusszal és születési tulajdonságokkal kel ki. Az éles tartalmi
profilok nem változnak.

A Unity normál TLS-kapcsolatban keltetett petet idéz és két külön képességet tanít.
A kiválasztott HP-képesség törlésekor a másik megmarad. A próba hibás
tokent, ismételt megerősítést és üres hely törlését is vizsgálja.
Újrabelépés után ugyanaz a pecsét az üres, továbbra is feloldott hellyel
és a másik tanult képességgel tér vissza. Az üres hely ezután egy harmadik
képességet tanul. A teljes törlés mindkét tanult képességet eltávolítja;
a természetes bónuszok megmaradnak. A második újrabelépés és a visszahívott
újrabelépés ellenőrzi a végső állapotot.

A MariaDB-próba külön vizsgálja a két törlési módot, az egyszeri levonást,
a sikertelen műveletet, a receipt újrajátszását, az újratanulást és a
tulajdonoshoz kötött várakozási idő megőrzését. A Unity offline próba
szétvágott és csonkolt TCP-csomagokat, verziókeverést és a tényleges panel
gombjait is ellenőrzi.

A szerver életidő-elszámolása rövid időre foglalttá teheti a tulajdonos
tárgyállapotát. Az élő futtató kizárólag a kifejezett „Az előző tárgyművelet
még folyamatban van.” válaszra ismétli az idézést vagy visszahívást,
illetve a `Busy` válaszra frissíti a képességlistát, legfeljebb öt
alkalommal. Más elutasítás továbbra is teszthiba.

## Fordítás és regresszió

A Windows Release GameServer build és a hat natív teszt sikeres:
`pet_lifecycle`, `equipment_stats`, `point_calculation`, `combat_state`,
`affect_lifecycle`, `item_persistence_mariadb`. A MariaDB-próba a második
rangú képesség törlését is visszaolvasta új tulajdonosi munkamenetben.

A Unity `6000.3.27f1` elkülönített forráspillanatképében a fordítás,
1189 petcsomag-, 1640 statcsomag-, 340 pecsét- és 4179 leltárellenőrzés
sikeres. A futtatott C# fájlok hash-e egyezik az aktív Unity munkatérrel.
A pillanatkép nem tartalmazza a teljes világ SpeedTree-, Crest- és VFX-
tartalmát; ez a próba a petműveleteket és a hálózati játékfolyamatot méri.

A build és futási naplók helyi bizonyítékok: `build/c26j-game-build.log`,
`build/c26j-native-build2.log`, `build/c26j-native-tests.log`,
`build/c26j-unity-offline.log`. A privát fixture és az adatbázismentés nem
kerül verziókezelésbe.

## Helyi eredmény

A végső Unity TLS-próba 268 ellenőrzése sikeres. Az előkészítés során
keltetett, checkpointban megőrzött saját pecséttel futott; a végső futás
nem keltetett újabb példányt.

| Állapot | Maximális HP |
| --- | ---: |
| Visszahívott pet | 31 260 |
| Életkori 500 HP + születési 3% + tanult 250 HP és 25% | 40 973 |
| A 250 HP-s képesség törlése után, újrabelépve is | 40 653 |
| Az összes tanult képesség törlése után, újrabelépve is | 32 713 |
| Visszahívás és újabb újrabelépés után | 31 260 |

A kiválasztott törlés után a második képesség és annak első rangja
megmaradt. Az első, üres hely másik képességet tanult, majd a teljes
törlés mindkét tanult képességet eltávolította. Mindkét nyitott és mindkét
zárolt hely eredeti állapota megmaradt. Hibás token, üres hely vagy üres
pet nem fogyasztott ellátmányt. Mindkét sikeres megerősítés ismétlése
változatlan tárgyszámot eredményezett. A három kiválasztott törlőből kettő,
a két teljes törlőből egy maradt. A születési idő, három tulajdonság,
szint és evolúciós fokozat a teljes folyamat végén is egyezett.

Az SQL-utóellenőrzés ugyanebből a pecsétből négy üres képességhelyet,
a megőrzött feloldásokat és a végső ellátmányszámokat olvasta vissza.
Mind a 359 korábbi példány, 38 pénztárca és a korábbi tanulási adatok
egyeztek a mentéssel. Az ideiglenes regenfájl és a hat tesztprototípus
helyreállt. A szokásos öt szerverfolyamat újra fut; a tesztfiókok és
tesztkatalógus a korábbi tartalom felülírása nélkül megmaradnak.

A végső élő napló a `build/c26j-unity-live3.log`. Az SQL-ellenőrzést a
`PetPassiveUnityLiveFixture.py verify` futtatja a
`build/c26j-reset2.private.json` privát fixture-rel és ezzel a naplóval.
A `restore` művelet leállított saját szerver mellett a mentett világot
állítja vissza; a helyreállítás naplója a `build/c26j-world-restore.log`.

Ebben a munkában a képességelfelejtés helyi játékfolyamata kapott új
ellenőrzést. A korábbi 18 hatásos teljes harci mátrix nem futott újra.
