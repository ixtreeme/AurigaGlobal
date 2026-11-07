# C26k–l — Petéletidő és tulajdonságfejlesztés

Párosított Unity changeset: `cs:83`.

A szerver `20261103` és Unity `/main/unity63-integration-20261103` ágán
közös felület és receiptelt mentés kezeli az `ExtendLife`, `PermanentLife`
és `Enchant` ellátmányokat. A petnek visszahívva kell lennie. Az időnövelés
előtt a szerver megvárja a visszahívás utolsó aktív időszakának elszámolását.

A megerősítés a teljes forrás- és pecsétpéldányt, a katalógust, a kétperces
ajánlatot és a karakter állapotát újra ellenőrzi. A `PSU` bizonyítékból a
natív commitellenőrző újratervezi a megengedett változást. Az ellátmány és
a pet egy SQL-tranzakcióban változik. A megismételt megerősítés vagy receipt
nem használhat fel újabb egységet.

Az időhosszabbítás a tényleges megmaradt időhöz ad; az új kapacitás is ez
az összeg. A profil kapacitáshatára felett nincs fogyasztás. A korlátlanító
külön, tartós `permanentGrant` engedélyt ment: az óra állandó, a kapacitás és
maradék nulla. Ez túléli a követést, visszahívást és újrabelépést.

Mindhárom születési tulajdonság külön fejleszthető. A szerver generálja a
dobást. Siker esetén a profil lépését adja, legfeljebb a határig; sikertelen
elfogadott próbánál egy egység elfogy, a tulajdonságok megmaradnak. A
tulajdonságok aktív bónuszai továbbra is a korábbi egész százalékra csonkolást
használják. A név, szint, evolúció és képességek nem íródnak felül.

## Ellenőrzés

- Windows Release GameServer build sikeres.
- `pet_lifecycle` és `item_persistence_mariadb` sikeres. Utóbbi az időnövelés,
  korlátlanítás, sikeres/sikertelen enchant és receipt-visszajátszás tartós
  adatait új tulajdonosi munkamenetben is ellenőrizte.
- Unity `6000.3.27f1` fordítás, 2811 petcsomag-, 1640 statcsomag-,
  340 pecsét- és 4179 leltárellenőrzés sikeres.
- A végső helyi Unity TLS-próba 371 ellenőrzése sikeres.

A tesztpet kezdeti kapacitása 300 000 ms. A követés és visszahívás után
295 201 ms maradt; a 60 000 ms-os növelés és újrabelépés után pontosan
355 201 ms maradék és kapacitás állt vissza. A nyers születési tulajdonságok
35/205/305-ről 40/210/310-re nőttek, és újrabelépés után megmaradtak.
Idézéskor az aktív százalékos forrás 4/21/31%-ot mutatott. Az értékhatáron
elutasított próbánál nem fogyott tárgy; a 0%-os sikertelen próbánál pontosan
egy fogyott. Az egyes sikeres és sikertelen megerősítések ismétlése sem
fogyasztott többet.

A korlátlanítás után újrabelépve is állandó óra és nulla időmennyiségek
álltak vissza. Az aktív újrabelépés visszaállította a követőt. A további
időhosszabbító és korlátlanító használat hatás nélkül elutasított maradt.
A végső visszahívott újrabelépés visszaállította a karakter pet nélküli statjait.

Az SQL-utóellenőrzés 387 korábbi tárgypéldányt, 43 pénztárcát és a korábbi
tanulási adatokat változatlannak talált. A tesztvilág és hat prototípus
helyreállt, az öt megszokott szerverfolyamat fut.

Bizonyítékok: `build/c26k-native-build.log`, `build/c26k-native-tests.log`,
`build/c26kl-game-build2.log`, `build/c26k-unity-offline.log`,
`build/c26kl-unity-live2.log`. A privát fixture
`build/c26kl-services.private.json`; nem része a commitnak. A futtató
`PetPassiveUnityLiveFixture.py --services`. A snapshot nem tartalmazza a
teljes világ összes grafikai elemét; a pet UI- és hálózati játékfolyamatot
teszteli. A korábbi 18 képességhatás teljes harci mátrixa itt nem futott újra.
