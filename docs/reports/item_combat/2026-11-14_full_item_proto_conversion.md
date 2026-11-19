# Teljes item proto konverzió – ellenőrzött részeredmény

A [legfrissebb funkcióbekötések és elszámolás](2026-11-14_full_item_proto_functions.md)
a 9 186 protós állapotot és a friss szerver/Unity ellenőrzéseket tartalmazza.

Ez a korábbi ellenőrzött állapot. A [folytatás és aktuális elszámolás](2026-11-14_full_item_proto_continuation.md)
149 további protó konverzióját és a friss szerver/Unity ellenőrzéseket tartalmazza.

2026-11-14. A teljes konverzió és a helyi játékba történő átállás **még nincs kész**.
A futó helyi szervert és adatbázisát ez a munka nem frissítette. A telepítő
elutasítja a `release_ready=false` jelöltet.

## Teljes forráselszámolás

| Eredmény | Forrássor |
|---|---:|
| Natív definícióra fordítva | 9 019 |
| Kivezetett / az aktív buildben nem használható típus | 491 |
| Pontosan megnevezett akadállyal fennmaradt | 1 476 |
| Összes régi `item_proto` sor | **10 986** |

Minden sor egyszer szerepel a [teljes elszámolásban](2026-11-14_full_item_proto_ledger.csv).
A 491 sor között a kivezetett Dragon Soul típusok, a buildben nem deklarált
fegyver/kosztüm altípusok, illetve minden támogatott karakter számára tiltott
prototípusok találhatók. Ez a besorolás nem törölte a forrásdefiníciókat.

A jelölt katalógus 6 101 natív definíciót tartalmaz. Az eredeti 1 574 natív
definíció és minden korábban kiadott katalógussor változatlanul megmaradt;
4 527 új definíció készült. Egy natív definíció több fejlesztési állapotot is
lefedhet, ezért a definíciók és a forrássorok száma eltér.

## Megvalósítás

- Új, önállóan futtatható offline fordító:
  `SRC/Tools/ItemCatalogBuilder/convert_full_catalog.py`.
- A hiányos sorozatok és eltérő modellek/ikonok önálló tárgyállapotként
  fordulnak. Az új `ExactState` profil csak a tényleges +N szintet fogadja el;
  köztes tárgyállapotokat nem hoz létre. A teljes talizmánsorozatok közös
  profilt használhatnak, szintenkénti bónusszal és ár/karakterszint szabállyal.
- A fejlesztési szintet a régi szerverhez hasonlóan a belső név adja. Az ettől
  eltérő fordítások az elszámolásban láthatók; nem változtatják meg a tárgy erejét.
- Talizmán, öv és két egyedi felszerelési hely: szerveroldali elhelyezés,
  statisztika, mentési rács és Unity metaadat/UI együtt bővült. Az új rács
  24 helyes; a korábbi horgonyok nem tolódnak el.
- A meglévő natív célokhoz fordulnak a felszerelések, alapanyagok, passzív és
  küldetéstárgy-definíciók, a támogatott kosztümök/mountok/stólák, italok,
  időzített/kapcsolható hatások, csalétek, könyvek, foglalattisztítók,
  feloldható kulcsok és a feloldható jutalomcsoportokkal rendelkező ládák.
- 5 138 új fejlesztési recept készült. További 187 recept hiányzó célra,
  alapanyagra vagy nem megfelelő forrásadatra hivatkozik; külön listázva maradt.
- Az extra proto APPLY bónuszai natív bónuszokká alakulnak. Ritkaság,
  időkezelési és használati korlátozások, valamint létrehozási valószínűségek
  a v32 definíciós szabálytáblában tárolódnak. Ezek runtime bekötése még hiányos.
- A natív felszerelés és időzített felszerelési állapot egységenként külön
  példány. A forrás halmozhatóságának ilyen normalizálása az elfogadott sor
  `native_stack_policy` mezőjében szerepel. A lélekerősítő tekercsek meglévő
  halmozási szabályai megmaradtak.
- A passzív régi tárgyak használaton kívüli bónuszmezői az
  `inactive_source_bonuses` auditmezőben maradnak meg; nem kapnak új aktív hatást.
- Hiányzó hatás helyett nem készül helyettesítő ital vagy jutalmat adó alapláda.
  Sikertelen sor után a részleges táblák, foglalt kulcsok és fordítások visszaállnak.

## Hátralévő munka

| Akadálycsoport | Sor |
|---|---:|
| Új szemantikai fordító / használati funkció szükséges | 590 |
| Hiányzó ikon, modell vagy Unity assethivatkozás | 461 |
| Hiányzó / feloldhatatlan ládajutalom vagy jutalomszint | 295 |
| Forrásadat vagy natív szabály eltérése | 130 |

Az egyedi használati tárgyak, halak, horgászbotok/csákányok, polymorph,
lélek/pet variánsok, felszívó vállpántok, skinek, effektek, rúnák és több
speciális bónuszszolgáltatás fordítása még nem teljes. A részletes CSV-ben
forrásazonosító, nevek, típus, cél és konkrét hiba szerepel.

A már eltárolt definíciós szabályokhoz is szükséges munka:

1. Az általános tárgyidőzítők létrehozása, indulása, fogyása és lejárata a
   kosztüm/társ rendszereken kívül is.
2. Raktározási, felvételi, levételi és birodalmi korlátozások, valamint az
   egyedi tárgyak közös kizárási csoportjának teljes érvényesítése.
3. A születési mágikus bónuszok, foglalatok és páros addonok szerveroldali
   létrehozása a konvertált valószínűségekből.
4. A maradék használati funkciók, küldetéskezelők és tartalomhivatkozások
   bekötése, majd valódi helyi játékfolyam ellenőrzése.

Ezek nélkül a konverzió nem nevezhető kész, játszható funkciónak.

## Ellenőrzés és fájlok

- A teljes GameServer és a katalógus/mentési eszközök lefordultak.
- A natív katalógus validáció sikeres. A teljes jelölt egy új, elkülönített
  MariaDB-be bekerült, majd a natív SQL-olvasó azonos hash-sel olvasta vissza.
- A célzott natív és SQL tesztek, valamint a konverziós tesztek lefutottak;
  a pontos eredmények az [evidence fájlban](2026-11-14_full_item_proto_conversion.evidence.json) találhatók.
- Unity `6000.3.27f1`: felszerelés, craft, trade, shop és tárgymetaadat
  ellenőrzés sikeres; a +200 egyetlen állapotos natív fixture-t is beolvassa.
- Új katalógussal helyi játékfolyam még nem futott. A futó szerver maradt
  v31 / 1 574 definíció mellett. Account, karakter, régi tárgy és natív
  tárgypéldány továbbra is nulla; semmi nem került vissza a törölt állapotból.

Az utolsó tiszta offline futás eredménye:
`build/full-proto-reproducible.json`. Natív hash:
`04c0b602be5e6d901c29285d34151742d198c595441a89f6d57faf0baf909a26`.
A SQL visszaolvasási bizonyíték:
`build/full-proto-final-sql/catalog-stage-result.json`.

A definíciókból álló bemenet `build/full-proto-inputs.json`, a korábban kiadott
forráshoz rendelés `build/full-proto-issued-mapping.json`. A nyers forrástáblák
azonosságát a korábbi attribútum-loader felvétellel ellenőriztük; ezért a
loader tényleges, duplikációkat feloldó attribútumtáblái használhatók.
Player/account vagy tárgypéldány nincs ezekben a bemenetekben.

Új offline kimenet előállítása, a szerver módosítása nélkül:

```powershell
py SRC/Tools/ItemCatalogBuilder/convert_full_catalog.py `
  --source build/full-proto-inputs.json `
  --preserved build/full-proto-preserved.json `
  --known build/full-proto-issued-mapping.json `
  --project D:/Auriga-Unity6/SRC/AurigaUnity-20261103 `
  --groups D:/AurigaGlobal/Server2-Local/data/special_item_group.json `
  --helper build/c26b-game-native/SRC/ItemDomain/CatalogEditorTool.exe `
  --output build/full-proto-next.json
```

Az új SQL-migrációk (`item_combat/039`, `040` és `item_instance/026`) csak a
teszt-adatbázisokban futottak le. A helyi játék adatbázisán nem.
