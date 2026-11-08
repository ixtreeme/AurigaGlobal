# C32 — További kovácssegédek

Szerver: `20261103`; Unity: `/main/unity63-integration-20261103`,
Editor `6000.3.27f1`, párosított Unity-changeset **cs:93**.
Szerződés: [IC-067](../../adr/item_combat_067_typed_forge_aids.md).

Elkészült a hét tekercscsalád és a három csoportba sorolt esélynövelő
típusos katalógusa, szerkesztője, közös szerver/Unity-művelete és tartós
mentése. A Unity-leltár **Kovácssegéd** gombja külön ablakot nyit;
a tárgy, recept, tekercs és három esélynövelő választható. Az **Áttekintés**
a szerver pontos esélyét és kudarcszabályát mutatja, a **Megerősítés**
az egyetlen tartós írás eredményére vár.

## Valódi helyi játékpróba

Két elkülönített TLS-karakterrel, tényleges Unity-leltár- és kovácsablak-
gombokkal **11 eset, 516 ellenőrzés és négy újrabelépés** sikeres.

| Eset | Igazolt eredmény |
| --- | --- |
| Áldástekercs | Sikertelen +9 → +8, eredeti példányazonosítóval |
| Mágikus fém | Kudarc, nulla darabos, felszabadított helyű auditrekord |
| Sárkányisten-tekercs | A szintenkénti táblázat biztos sikerű kezdő szintje |
| Kovácskönyv | A szintenkénti táblázat biztos sikerű +1 → +2 ága |
| Musin | A nullás receptesély felülírása, +9 → +10 |
| Emléktekercs | Pontos +0 forrásszint, sikeres +1; rossz szinten nincs fogyás |
| Feketesárkány-tekercs | +4 behelyezhető kő kudarckori +3 állapota |
| Három esélynövelő | Közeli normál kovácsnál 70% + 5 + 10 + 15 = 100% |
| Tekercs és három segéd | Együttes használat, 100%-nál telítődő esély |
| Páncél | Ugyanaz a tartós segédes fejlesztési út |
| Ékszer | Ugyanaz a tartós út; az utolsó fejleszthető cél után is működő ablak |

A foglalatkő élő kudarcesete külön, szándékosan 0%-os tesztprofilt kapott.
A valódi 80%-os mintaszabály 8000/8001 dobáshatárát a natív teszt igazolja.
Véletlen dobás helyett kizárólag elkülönített katalógustartalommal tettük
determinisztikussá az élő kudarceseteket; játékosállapotot nem írtunk vissza.

Minden siker és kudarc után pontosan egy kísérletnyi segéd, három anyag
és 100 Yang fogyott. Összesen **16 segédegység, 33 anyag és 1100 Yang**
levonása szerepel **11 egyedi Forge-receiptben**. Minden megerősítés
ismétlése változatlan állapotot adott, további költség nélkül.
Az eredeti cél-ID-k, szintek, segéd- és anyagmennyiségek, valamint Yang
mind a négy újrabelépés után megmaradtak.

Az anyag előnézet utáni áthelyezése érvénytelenítette az előnézetet;
a régi megerősítés és a megszakítás sem fogyasztott. Tekercs nélküli
esélynövelő kovács nélkül elutasított. A végső SQL-próba **1052 másik
typed példányt**, más tulajdonosok pénztárcáit és tanulási adatait
változatlanul találta. Legacy tárgy nem került a tesztkarakterekre.

A korábbi részpróbák saját tárgyai megmaradtak. Az ideiglenesen hozzáadott
normál kovács regenjét a próba után az eredeti bájtokra állítottuk vissza.

## Natív, SQL, Unity és kezelő

- **47 kiválasztott natív tesztcsoport sikeres**, a javítások utáni célzott
  újrafuttatásokkal együtt. A Forge-runtime **3526 ellenőrzése** többek közt
  a szintenkénti esélyeket, csoportkorlátot, célmaszkot, költséget,
  példánypayload-megőrzést, receiptmanipulációt és utolsó darabos
  segéd-/anyag-tombstone-t ellenőrzi.
- A valódi MariaDB-writer próbája igazolja a sikeres és sikertelen
  költséglevonást, az újrabelépést, az operation replayt és a régi session
  teljes visszagörgetését. A native snapshot és SQL-katalógus egyezik.
- A **036 migráció kétszer** lefutott; mind az **1004 korábbi tárgyrekord**,
  tároló és korábbi katalógussor megmaradt. A katalógus v30; új játékos-
  állapottábla nem kellett.
- Unity 6.3 fordítás és a korábbi offline validátorok sikeresek.
  Az új kovácsvalidátor **6993 ellenőrzése** natív goldeneket,
  csonkolásokat, minden TCP-darabolási pontot, valódi ablakgombokat és
  sequence-méreteket ellenőriz. A még meg nem nyitott ablak válasza,
  valamint az üresen maradó receptlista külön regressziós eset.
- A kezelő **56 modell-/Qt-tesztje** és a végső tartományigazítás célzott
  próbája sikeres. Tíz szerkeszthető minta, szinttábla, másolás és natív
  publikálás működik. Az új kezelőcsomag és a három natív bináris telepítve.

A helyi bizonyítékok: `build/c32-validation-evidence.json`,
`build/c32-migration-evidence.json`, `build/c32-deployment-evidence.json`,
`build/c32-unity-offline-final2.log`, `build/c32-unity-live4.log`.
A privát fiókadatok kizárólag a helyi build-/backupkönyvtárakban maradtak.

## Hatókör

A próba a tényleges hálózati/session útvonalat és Unity UI-komponenseket
használó, forráshash szerint egyeztetett, elkülönített Unity-projektben futott.
Teljes világjelenetes grafikai próba, standalone kliens, FreeBSD és
production tartalomkonverzió nem futott. A snapshotból szándékosan
kihagyott VFX-, Crest- és SpeedTree-eszközök indulási üzenetei a korábbi
alapállapothoz tartoznak; az item-validációk ettől függetlenül sikeresek.

A normál legacy leltár átállítása, a további tárolók/fiókpénztárca és
a teljes Mission-/quest-integráció továbbra is külön munkacsomag.
