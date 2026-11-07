# C26i — A fejlődő pet életkori és születési bónuszai

A szerver a `20261103`, a párosított Unity `cs:81` a
`/main/unity63-integration-20261103` ágon készült, Unity `6000.3.27f1` mellett.

A szerver a követő pet teljes bónuszcsomagját állítja elő: életkori
fokozat, születési tulajdonságok és megtanult képességek. Az azonos
hatások forrásonkénti értékei egy ellenőrzött összegbe kerülnek, és a
karakter számítása ezt az összeget egyszer alkalmazza.

Az életkori profil egész maximális HP-t, szörnyellenállást és
játékosok elleni ellenállást ad. A három születési tulajdonság a
maximális HP százalékát, a szörnyek elleni sebzést és a normál PvE
sebzést növeli. A korábbi domain-szabály megmarad: a tizedszázalékban
mentett tulajdonság aktív bónusza egész százalékra lefelé kerekül.
Így a próba 3,5 / 20,5 / 30,5%-os születési értékei 3 / 20 / 30%-ot adnak.

A szerver a tényleges követőt, az életidőt és a világ feltételeit
ellenőrzi. Visszahíváskor, lejáratkor vagy tiltott állapotban az egész
forrás eltűnik. Az életkort a már mentett születési időből és a megőrzött
legnagyobb életkorból számolja. Az életkori határ követés közben is új
statpillanatképet eredményez, újabb idézés nélkül. A pillanatkép csak a
fokozat küszöbét őrzi, ezért az eltelt milliszekundumok nem kényszerítenek
minden tickben karakterstat-újraszámítást.

A GC231 v4 az összesített petbónuszok mellett külön életkori, születési
és képességbónusz-csoportokat, valamint a jelenlegi életkori küszöböt
küldi. A Unity ellenőrzi, hogy a csoportok összege egyezik a teljes
értékkel. A korábbi v1/v2 csomagok és a csak képességeket adó v3 út
megmarad. A **Harci statok** külön mutatja mindhárom forrást és az
effektív értékeket. Új SQL-migráció nem szükséges: a mentett petállapot
már tartalmazza az életkort és a tulajdonságokat.

## Helyi ellenőrzés módszere

A `PetPassiveUnityLiveFixture.py --natural` új, saját tesztfiókokat és
külön katalógussorokat hoz létre. A katalógus meglévő sorai és a többi
tulajdonos tárgyai, pénztárcái, tanulási adatai megmaradnak. A próba
életkori küszöbe 20 másodperc; a normál tartalmi profilok változatlanok.

A Unity valódi TLS-munkamenetben keltet, idéz és könyvet használ.
Először tanult képesség nélkül méri az életkori és születési forrást,
majd kivárja az automatikus fokozatváltást. Ezután ugyanaz a pet egy
250 egész HP-t adó képességet tanul. A próba az összesített HP-t,
a születési sebzésbónuszokat és az életkori PvP-ellenállást vizsgálja
idézve, aktív újrabelépésben, visszahívva és visszahívott újrabelépésben.
Az ismételt idézés nem halmozhatja a bónuszokat.

A szörnyellenállás ugyanezeket az állapotokat külön harci állomáson
járja végig. A GC135 a közelben álló klienseknek is megérkezik, és nem
tartalmaz támadóazonosítót; ezért a PvP- és a szörnymérés fizikailag
elkülönül. A szörny támadását a rendes kliensművelet indítja. A teszt
a tényleges szerveres találatokat méri, legfeljebb két-három egységnyi
eltéréssel a végső kerekítésekhez.

A végén a pet születési idejét és tizedszázalékos értékeit újra lekéri,
majd az SQL-utóellenőrzés a tartós életkort, a tulajdonságokat és a
megtanult képességet olvassa vissza. Az ideiglenes prototípusok és a
regenfájl a mentésből állíthatók helyre.

## Eredmény

A Windows Release GameServer build és a hat natív teszt sikeres:
`pet_lifecycle`, `equipment_stats`, `point_calculation`, `combat_state`,
`affect_lifecycle`, `item_persistence_mariadb`. A Unity 6.3 fordítás,
1640 statcsomag-, 390 petcsomag-, 340 pecsét- és 4179 leltárellenőrzés
átment. A végső helyi TLS-próba 365 ellenőrzése sikeres.

| Mérés | Pet nélkül / visszahívva | Idézve |
| --- | ---: | ---: |
| Maximális HP, első életkori fokozat, tanult képesség nélkül | 31 260 | 32 713 |
| Maximális HP, automatikusan elért második fokozat | 31 260 | 33 434 |
| Maximális HP, második fokozat + tanult 250 HP | 31 260 | 33 691 |
| Normál PvE-találat, mindkét születési sebzésbónusszal | 417 | 651 |
| Beérkező PvP-találat, 25% életkori ellenállással | 371 | 279 |
| Beérkező szörnytalálat, 20% életkori ellenállással | 392 | 314 |

A második fokozat és a tanult képesség összetett csomagjával mind a négy
állapotot végigjárta a próba. Három további idézés sem halmozta a bónuszt.
A születési idő és a nyers tulajdonságok az utolsó újrabelépés után is
egyeztek. Az SQL-utóellenőrzés a tartós életkort, a három nyers tulajdonságot
és az első rangú HP-képességet visszaolvasta. Mind a 312 korábbi tárgypéldány,
32 pénztárca és a korábbi tanulási adatok egyeztek a mentéssel.

A hat természetes hatás és a tanult HP együtt alkalmazása kapott új élő
próbát. A korábbi, mind a 18 tanult hatást mérő C26h teljes harci mátrixa
ebben a munkában nem futott újra; a hat natív regresszió igen.

Az ideiglenes regenfájl és a hat tesztprototípus helyreállítása sikeres.
A szokásos öt szerverfolyamat újra fut, az SQL-utóellenőrzés a helyreállítás
után is sikeres. A tesztfiókok és a saját tesztkatalógus megmaradnak a
vizsgálhatóság érdekében; a játékosok meglévő tartalmát nem írják felül.

## Bizonyítékok és ismétlés

- Natív build és tesztek: `build/c26i-game-build.log`,
  `build/c26i-native-build.log`, `build/c26i-native-tests.log`.
- Unity fordítás és offline próbák: `build/c26i-unity-offline.log`.
- Végső helyi játékpróba: `build/c26i-unity-live2.log`.
- SQL-utóellenőrzés: `PetPassiveUnityLiveFixture.py verify`,
  `--fixture build/c26i-natural2.private.json`, `--log build/c26i-unity-live2.log`.
- Világhelyreállítás: ugyanennek a futtatónak a `restore` művelete,
  leállított saját szerver mellett; utána a csatorna 1 és 99 újraindítása.

A privát fixture, az adatbázismentés és a futási naplók helyi bizonyítékok,
nem részei a commitnak. A kapcsolódó futtatók a
`tests/server/PetPassiveUnityLiveFixture.py`,
`tests/server/PetPassiveUnityLiveSmoke.py` és a Unity
`Metin2TypedPetCombatLiveValidator.cs`.
