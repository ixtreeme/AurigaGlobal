# ADR IC-045 — Ékszerfoglalatok, viselési idő és Unity

2026-11-02, C17, F4/F5. Előzmény: [IC-008](item_combat_008_accessory_socket_profiles.md),
[IC-030](item_combat_030_socket_services.md), [C16](item_combat_044_socket_insertion.md).

## Műveletek

Saját, leltárban lévő, profilos ékszer kapacitásának nyitása és aktív
fokozatának feltöltése. A forrásnak a célprofil megfelelő explicit
anyaglistájában kell szerepelnie. A profil határozza meg a darabszámot,
sikeresélyt, maximumot, időt és a bónusznövelést. A kapacitás és az aktív
fokozat külön állapot; egyik sem a tárgy +N fejlesztési szintje.

Siker és kudarc egyaránt elfogyasztja a profil anyagigényét. Kudarc esetén
az ékszer állapota megmarad. Sikeres charge a következő fokozat teljes
idejét állítja be, nem hozzáadja a korábbi maradékhoz. A végleges anyag
csak explicit profilkapcsolat alapján használható; aktív időzített és
végleges feltöltés nem keverhető. Felszerelt, más tulajdonoshoz kötött,
kivezetett, hibás vagy hatástalan cél nem módosul. A forrás use policy-je
érvényesül. A cél felvehetősége nem előfeltétel a leltárban végzett művelethez.

Előnézetkor nincs RNG. A szerver a teljes költséget, esélyt, sikeres és
sikertelen állapotot visszaadja. Megerősítéskor újraellenőrzi a snapshotot,
a katalógust és a kontextust, majd OpenSSL rejection samplinggel egyszer
dob. A material debit, teljes célállapot és a dobást rögzítő receipt egyetlen
SQL-tranzakció. A nyugta `ACC1` eredménye tartalmazza a műveletet, esélyt,
dobást és döntést; outcome `0x1701`/`0x1702`. A nonce ismétlése nem dob és
nem fogyaszt újra. Új loginban a régi nonce érvénytelen.

## Viselési idő

Nem kell új instance séma: az accessory capacity/grade/remainingMs/permanent
már része a teljes mentett példánynak. A game thread steady clockot használ;
nincs UTC-számítás és offline lejárati időbélyeg.

- Csak felszerelve fogy, inventoryban és kijelentkezve szünetel. A végleges
  fokozat nem fogy. Lejáratkor egy fokozattal csökken; a túlfutás az alsó
  fokozat teljes időtartamából is levonódik. A kinyitott kapacitás megmarad.
- A közös `RequestCommit[Many]` a felszerelt ékszerek addigi idejét az éppen
  végzett tárgyműveletbe illeszti. Egy már mozgatott/átadott tárgy before
  image-e változatlan, after image-e az elszámolt időt viszi tovább, és csak
  egy instance revisiont lép. Nincs külön visszaírás régi snapshotból.
- Üresjáratban másodpercenként külön idő-commit készül (`ATM1`, `0x1703`).
  Egy tulajdonosnak egyszerre egy írása van. Sikertelen mentés nem engedi el
  a még el nem számolt időt; CommitUnknown a meglévő locking receipt
  reconciliation útvonalon rendeződik.
- Kijelentkezéskor a clock lezárul, a folyamatban lévő művelet után az utolsó
  maradék is mentődik. Azonos core-on a következő session claim megvárja ezt.
  Szabályos szerverleállítás a clockokat is végigdraineli.
- A harci projekció az eltelt, még nem mentett idővel is számol. Egy lassú
  SQL-művelet vagy kereskedési zárolás nem tartja életben a már lejárt buffot.

Ez checkpointolt mentés: folyamatkilövés/áramvesztés vagy másik core-ra
átlépés közbeni session-race esetén a még nem commitolt rövid maradék
visszakapható. Az utolsó tartós checkpoint utáni teljes viselés pontos,
crashbiztos elszámolása külön kiadási kapu; offline időt emiatt sem vonunk le.

Az időmódosítás is tulajdonosi revisiont lép. A foglalat-, token- és
dobozmegerősítés legfeljebb 64, kizárólag idő-commitból álló revisionlépést
átvezethet a friss fence-re. Más művelet továbbra is elavulttá teszi az
előnézetet. A session és a teljes forrás/cél invariánsai változatlanok.

## Bónuszszámítás

A katalógus base és aktuális refinement profiljának azonos BonusId-jű
értékei előbb összeadódnak. A foglalat ezt az összevont, nem nulla értéket
egyszer növeli: `abs(value) + max(round(abs(value)*increaseBp/10000), minimum)`.
A minimum egységét a BonusDefinition Raw/BasisPoints/Integer típusa választja.
A kerekítés a közös equipment skálázásé, fél esetén nullától elfelé.
Az eredeti előjel megmarad; nulla összegből nem keletkezik új bónusz.
A példánybónuszok, beépített kövek és ellenállásprofil külön hozzáadások.
Minden összeadás és szorzás checked; a C11 pont-, sebzés- és sebességcapjei
maradnak érvényben. Az aktív ékszert tiltó korábbi C11 integrációs kaput
ez a mentett életciklus váltja ki.

## Unity és protokoll

**I → kompatibilis anyag → Foglalatok**. Céltárgyválasztás, külön nyitás és
feltöltés, esély, anyagigény, mindkét kimenet és külön megerősítés. A tárgy
részletei mutatják a kapacitást, aktív fokozatot, hátralévő viselési időt és
a szünetelő/végleges állapotot. A tényleges változást mindig a GC225 adja.

A CG235 továbbra is 26 byte + sequence: action 1 automatikus előnézet,
4 explicit nyitási előnézet, 5 explicit töltési előnézet, 2 confirm, 3 close.
Nincs kliensoldali esély vagy dobás. A GC235 v3 az 5/6 accessory kindokhoz
a v2 kőmezők után 54 byte kiegészítést ad: három teljes accessory állapot,
maximum, növelés és három minimum. A v1/v2 köves csomagok változatlanok.
A GC227 Socket bit már az explicit listákon szereplő 6-os anyagokra is érvényes.

A kliens teljesen ellenőrzi a v3 mezőket és az állapotátmeneteket. Felismeri
az olyan GC225-öt, amely kizárólag felszerelt ékszer idejét/fokozatát
csökkenti. Ez nem zárja be a többi tárgyablak megerősítését. Pénztárca,
inventory, kötés, más bónusz vagy katalógushash változása rendes frissítés.

## Ellenőrzés

`AccessoryRuntimeTests`, a bővített `ItemPersistenceSqlTests`, a natív
`typed_accessory_v3.hex` golden, a Unity accessory validator és a háromfázisú
`AccessoryUnityLiveSmoke.py` együtt fedik le a domain/SQL/wire/UI/TLS utat.
Részletes futási lelet: [C17 jelentés](../reports/item_combat/2026-11-02_accessory_socket_runtime.md).

Bónuszváltás/hozzáadás/zárolás, további use kategóriák, teljes konverzió és
a kiadási kapuk külön feladatok maradnak.
