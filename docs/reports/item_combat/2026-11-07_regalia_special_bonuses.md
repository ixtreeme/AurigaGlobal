# C29 — Stóla, rúna és speciális bónuszműveletek

Szerver: `20261103`; Unity: `/main/unity63-integration-20261103`,
Editor `6000.3.27f1`, párosított changeset **cs:90**. Szerződés:
[IC-064](../../adr/item_combat_064_regalia_special_bonuses.md).

A külön típusos leltár stólát, hét rúnát és hat bónuszeszközt kezel.
A stóla hat példánybónusza, a rúnák kiadott kapacitása, maradéka és
kapcsolója, a donor pontos bónusza és a részlegesen használt töltőüveg
maradéka tartós. A szerver számolja a bónuszt, a kliens valódi
áttekintési/megerősítési kérést küld; a forrásfogyás és célmódosítás atomi.

## Bekötött funkciók

- Stóla felszerelése, levétele, hat saját bónusza és közös újrabűvölése.
  Az explicit stólamodell a tulajdonosnak és a megfigyelőnek is megjelenik.
- Hat töltéses rúna külön kapcsolóval, töltésfüggő két bónusszal;
  a hetedik két készletbónusza hat aktív, legalább félig töltött rúnával.
  A teljes készlet a tényleges rune effektet kapcsolja.
- Csak aktív online viseléskor fogyó keret, fél töltésnél azonnali
  stat-/effektváltás, nullánál kikapcsolás és újratölthető azonos példány.
  Kikapcsolás, levétel és újrabelépés rendezi az utolsó időtartozást.
- Rúnatöltés az üveg szükséges részével; a maradék ugyanazon üvegen marad.
- Jelmezreset három normál sorral és külön húzási szinteloszlásokkal;
  két ritka donorhely és kiválasztott ritka sor törlése. A jelmez órája,
  kötése és fejlesztése megmarad.
- Kapcsolt normál/skill addon a legacy Gauss-képlettel; negatív értékek,
  nulla sor kihagyása, pontosan egyszer érvényesülő statforrás.
- Qt-kezelő mindkét új kategóriához, mintaszabályokkal és natív
  katalógusvalidálással. Teljes előfeltételvizsgálat RNG előtt,
  egyszer használható nonce és újrajátszható SBR1 bizonylat.

## Élő helyi Unity-próba

Két elkülönített karakter valódi localhost TLS auth/game kapcsolatán
**11 fázis és 1171 ellenőrzés sikeres**, 11 tulajdonosi újrabelépéssel.
A felszerelést a leltár tényleges gombjai, a szolgáltatásokat az új ablak
áttekintési és megerősítési gombjai indítják.

| Lefedés | Eredmény |
|---|---|
| Stóla | Viselt modell és hat bónusz, levétel, hatos újrabűvölés, pontosan egy anyaglevonás és pontos újrabelépési visszaolvasás |
| Jelmez | Két eltérő ritka donor, ismételt típus és üres törlés fogyás nélküli elutasítása, reset három külön húzással, ritka tömörítés, a megkezdett elsőhasználati óra változatlan határideje |
| Kapcsolt addon | A két sebzésérték képlete, felszerelt statforrás, pontos bónuszállapot újrabelépés után |
| Kérésismétlés | Új dobás és második anyaglevonás nélkül ugyanaz az eltárolt sikeres válasz, teljesen elfogyott donorral is |
| Aktív rúnák | Hét felszerelési hely, hat kapcsoló, +53 Erő, aktív újrabelépés feltöltés nélkül; tulajdonosi és megfigyelői valódi effekt |
| Fél töltés és töltés | Az első rúna alsó fokozata és a hetedik bónusz elvesztése +37 Erőre vált; részleges üveg és visszatért készletbónusz; az üveg maradéka pontosan visszaolvasható |
| Kifogyás | Azonos instance, nulla töltés és inaktív állapot újrabelépés után; maradék üveg egyszeri elfogyasztása, második üveges feltöltés és újbóli aktiválás |
| Kikapcsolás és levétel | Az összes utolsó időtartozás rendezése után a maradék várakozva, kikapcsolt és leltári újrabelépéskor pontosan változatlan; stat- és effektforrás eltűnik |

A hat végső szünetelő maradék: 1. hely **29661 ms**, 2. hely **545834 ms**, 3. hely **545880 ms**, 4. hely **545785 ms**, 5. hely **545834 ms**, 6. hely **545835 ms**.

Az SQL-utóellenőrzés ugyanazt a hat keretet olvasta vissza. Egy stóla és
hét rúna azonos ID-val, kötötten, inaktívan és leltárban maradt; a jelmez
három normál sorral és ritka sor nélkül, a kard a kapcsolt addonnal maradt
meg. A két kontrollpecsét, valamint **932 másik typed példány**,
a védett pénztárcák és tanulási adatok változatlanok. Legacy item nem
került a két tesztkarakterre.

Az élő próbában javított két szerverhiba a legacy hex packetazonosítóval
ütköző kezdeti CG241 (végleges CG214), illetve az EnTT anonim `Session`
típusnevének ütközése a boltrendszerrel. Az új komponens egyedi
`TypedSpecialBonusSession` nevet használ. A teszt végső óraellenőrzése
korrelálható preview-válasszal üríti ki a korábbi snapshotokat, mielőtt
szünetelő keretet rögzít. Sikertelen részpróba fogyasztott állapotát nem
állítottuk vissza; a teljes sikeres kör friss 24 tárgyas készletet használt.

## Natív, SQL és szerkesztői ellenőrzés

Mind a **15 kiválasztott natív/SQL tesztcsoport sikeres**:
`special_bonuses`, `costumes`, `persistent_items`, `companion_items`,
`equipment_stats`, `typed_inventory_wire`, `item_persistence_mariadb`,
`item_bench_mariadb`, `item_key`, `item_catalog`, `catalog_sql_mariadb`,
`instance_attributes`, `attribute_runtime`, `item_combat_schema_foundation`,
`item_catalog_digest`.

A speciális teszt **4913 ellenőrzése** az igazi gyári grantot, hatféle szolgáltatást, hét
felszerelési helyet, töltési fokozatot, készletfeltételt, pontos fogyást,
hamisított eredmény/forráslevonás/receipt tiltását és RNG előtti
elutasítást ellenőrzi. A v14 wire minden csonkolása elutasított.
A 15 független katalógus-hash fixture a Python-orákulussal egyezik.
A Qt-űrlapok **9 natívan validált, újratöltéssel is ellenőrzött esete**
sikeres; a csomagazonosítók hexadecimális legacy értékekkel együtt
ütközésmentesek.

## Unity-fordítás és protokoll

A végleges aktív forrásokkal azonos Unity 6.3 másolat fordítása és
**8003 új ellenőrzése sikeres**, a korábbi 700 companion-, 4179 leltár-,
2447 stat- és 397 jelmezellenőrzéssel együtt. A native goldenek,
minden csonkolás, GC220/GC241 v2 és a régi 237-es time-sync közös
TCP-folyamának minden darabolása, valódi UI-preview/confirm és runtime
dispatch szerepel. A használati költségnél kisebb megmaradt eszközstack
érvényes inventory; ilyenkor a szerver tagadja meg az új használatot.

## Migráció és helyi csomag

A `034_regalia_special_bonuses.sql` v28-ra emeli a katalógust, hozzáadja
a 19/20 kategóriát, saját local-ID sorozatot és négy definíciós táblát.
A `017_regalia_special_bonuses.sql` hozzáadja az RGL1/BTL1 állapotoszlopokat
és legalább 19 equipment helyet biztosít. Mindkét migráció kétszer
lefutott a saját leállított stacken, teljes player/protos mentéssel.
Mind a **860 megelőző példány** és a korábbi katalógussorok változatlanok
maradtak; más kategóriák új állapotmezői NULL-ok.

A friss GameServer, ItemBenchTool, CatalogEditorTool és a **292 fájlos**
Qt-kezelő a `Server2-Local` csomagba került, mentéssel és manifesttel.
A telepített három bináris, migráció és mind a 292 kezelőfájl manifest-hash-e
pontos. A 49 ellenőrzött Unity forrás/meta és a 196 vizuális függőség az
aktív workspace-szel azonos. Az öt saját szerverfolyamat fut.
A tesztfiókok és új katalógussorok elkülönítettek; fogyasztott tárgyat és
órát a teszt nem állít vissza. A hitelesítési adatok és mentések ignorált
helyen maradnak.

## Próbahatár

Ez a hat saját bónuszos stóla; az önálló Acce-elnyelés és a teljes régi
APPLY-/tartalomkonverzió nem része ennek. A normál legacy leltár átállítása
továbbra is külön feladat. A következő tervezett runtime csomag az önálló
lélektárgyak töltése, fejlesztése és találathoz kötött fogyása.

A két fixture-actor a valódi rendererrel, konvertált sash modellel és
rune effekttel dolgozik, az izolált benchmark-resolveren. A C28 114
asset/meta függősége mellé 82 szükséges aktív stóla-/rúnafüggőség került.
A production Addressables-út nem változott. Teljes világjelenetes,
animációs vagy standalone próba, FreeBSD-build, terhelésmérés és
production Addressables-letöltés nem futott. A másolat hiányzó SpeedTree-,
VFX- és Crest-adatainak meglévő inicializálási hibái külön maradnak.

Bizonyítékok az ignorált `build` alatt: `c29-game-build7.log`,
`c29-native-build11.log`, `c29-native-build12.log`, `c29-native-tests5.log`,
`c29-unity-offline8.log`, `c29-special-live5.log`, `c29-special-sql1.log`,
`c29-migration-evidence.json`, `c29-manager-build2.log`,
`c29-manager-deployment.json`, `c29-deployment-evidence.json` és
`c29-regalia-visual.png`.
