# C27a — Típusos hátasra felülés, leszállás és újrabelépés

Szerverág: `20261103`. Unity-ág: `/main/unity63-integration-20261103`,
Editor: `6000.3.27f1`. Szerződés: [IC-061](../../adr/item_combat_061_typed_mount_riding.md).
Párosított Unity changeset: `cs:87`.

A típusos leltár **Felülés**, **Leszállás** és **Visszahívás** művelete
együtt működik az állandó hátaspecséten. Az idézési és felülési szándék
külön tartós adat; a megjelenés az élő szerverállapotból származik.
Felüléskor eltűnik a követő, leszálláskor pontosan egy követő tér vissza.
Újrabelépés megőrzi a választott állapotot. Másik típusos társ nem
foglalhatja el közben ugyanazt a helyet.

## Elvégzett ellenőrzések

- Windows GameServer, ItemBenchTool és CatalogEditorTool fordítás sikeres.
- Öt natív tesztcsomag sikeres: `companion_items`, `typed_inventory_wire`,
  `persistent_items`, `pet_lifecycle`, `item_persistence_mariadb`.
  Az új SQL-próba a felülést, replayt, új munkamenetet, leszállást,
  ismételt felülést és felült állapotból visszahívást is ellenőrizte.
  A módosított felülési bit hash-hibát, a meghamisított nyugta elutasítást
  eredményezett. A korábbi petmentési és etetési SQL-esetek is átmentek.
  A végső külön futás a NULL idézési állapot SQL CHECK-elutasítását is
  ellenőrizte (`build/c27-sql-tests2.log`).
- Unity-fordítás és **665 companion-/felülésellenőrzés**, valamint
  **4179 típusos leltárellenőrzés** sikeres. A v11 állapotok, a
  régi verzió/pet általi téves felülés tiltása, csonkolás, három összefűzött
  snapshot minden TCP-darabolása, a függő leszállási gomb és a katalógus
  által tiltott felülés is szerepelt a próbában.
- Az új Qt-kezelő a 014-es migrációval és a friss natív segédekkel
  lefordult, és a saját helyi csomagba került, mentett előző verzió mellett.

## Élő helyi TLS/Unity-próba

A két elkülönített `codexcompanion` tesztkarakteren **171 ellenőrzés**
sikeres. A régi fix pet és fix hátas idézési/visszahívási regressziója
is végigfutott, aktív és nyugalmi újrabelépéssel.

| Próba | Megfigyelt eredmény |
|---|---|
| Közvetlen felülés meg nem idézett pecsétre | Elutasítva, a pecsét változatlan |
| Megidézett hátasra felülés | A követő eltűnik; a tulajdonos és második kliens a katalógus hátasalakját kapja |
| Ismételt felülési kérés | Nincs új tranzakció vagy követő |
| Másik típusos pet idézése felülve | Elutasítva, a felült hátas megmarad |
| Újrabelépés felült állapotban | Ugyanaz a pecsét, helyes felült alak, külön követő nélkül |
| Leszállás és ismételt leszállási kérés | A karakterről lekerül a hátasalak, egyetlen követő jelenik meg |
| Újrabelépés leszállás után | Követő áll vissza, a karakter gyalogos |
| Újabb felülés, majd visszahívás | Mindkét szándék törlődik, nincs hátasalak és követő |
| Újrabelépés a visszahívás után | A pecsét nyugalmi, nem tér vissza szereplő |

Az SQL-utóellenőrzés pontosan **kilenc elfogadott állapotátmenetet**
talált: a petpecsét revíziója kettővel, a hátaspecsété héttel nőtt.
Minden más példánymező, azonosító, hely, darabszám, szint és kötés egyezett.
A **768 többi tárgy**, pénztárcák és tanulási adatok változatlanok maradtak.
A próba végén mindkét pecsét visszahívott állapotú.
A teljes élő kör a szigorított végső SQL-sémával is megismétlődött,
ugyanúgy 171 sikeres ellenőrzéssel és kilenc elfogadott átmenettel.

## Helyi telepítés és bizonyítékok

A `014_mount_riding.sql` a leállított saját stacken kétszer is lefutott,
**770 korábbi tárgy** állapothashének és revíziójának megváltoztatása
nélkül. A korábbi séma és csomagmanifest mentése megmaradt. A friss
szerverbinárisok, natív segédek, 014-es migráció és 288 fájlos Qt-kezelő
a `Server2-Local` csomagban vannak. Az öt normál szerverfolyamat fut;
ideiglenes világ- vagy szörnyadatot ez a próba nem telepített.

Naplók: `build/c27-native-build.log`, `build/c27-game-build.log`,
`build/c27-native-tests.log`, `build/c27-unity-offline2.log`,
`build/c27-mount-live2.log`, `build/c27-mount-sql2.log`,
`build/c27-manager-build.log`, `build/c27-manager-deployment.json`.
Az utolsó hash-egyeztetés a telepített három natív binárist és a sémát,
valamint az aktív Unity és a tesztmásolat négy módosított forrását is
azonosnak találta: `build/c27-deployment-evidence.json`.
A hozzáférési adatokat tartalmazó fixture és az SQL-pillanatkép az
ignorált `build/c27-mount2.private.json*` állományokban marad.

Nem futott teljes világjelenetes grafikai/animációs vagy standalone
klienspróba, FreeBSD-build, terheléses mérés, illetve külön élő
halál-/aréna-/karavánkorlátozási próba. Az újrabelépés és a két kliensnek
publikált tényleges szerveres hátasalak ellenőrzött. Az időkorlátos társ,
a fix társbónusz és a typed repülési engedély a következő csomag.
