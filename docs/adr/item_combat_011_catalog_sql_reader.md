# ADR IC-011 — Típusos itemkatalógus betöltése SQL-pillanatképből

2026-10-27. Állapot: implementált, izolált v5 katalógusbetöltő az öt eddig
megvalósított kategóriához. A GameServer/Database indulási útvonala még nem
használja; nincs éles publikálás vagy régi itemadatok átalakítása.

## Döntés

A `CatalogSqlReader::ReadCatalog` a meglévő `CAsyncSQL` prepared-tranzakciós
motorját használja. Külön, dedikált direct kapcsolat szükséges, `REPEATABLE
READ` izolációval és a `protos` táblákra csak SELECT-jogot kapó felhasználóval.
Nem készült második SQL-motor, vnum-feloldó vagy visszaesés a régi item_proto-ra.
A csak olvasási szerepkört az üzemeltető provisionálja; a betöltő SQL-je kizárólag
SELECT. A teszt valódi SELECT-only accounttal fut, és egy tiltott írást is ellenőriz.

A `PrepareRead` egyetlen tulajdonolt munkát állít elő: négy ellenőrző SELECT,
majd 27 konkrét, név szerint felsorolt táblakiolvasás. Értékek prepared
paraméterek; adatbázis-, tábla- és oszlopnevek fordításkor rögzítettek.
A publikus terv az ugyanilyen tulajdonolt feladatot fogadó meglévő SQL-sorhoz
is használható. Nem felhasználói SQL vagy futás közbeni módosítható profiladat.

1. A kapcsolat session izolációja pontosan REPEATABLE-READ legyen.
2. A `catalog_format` egyetlen sora schemaVersion=5 és
   `auriga.item_catalog.v5` domaint adjon. Ez az első InnoDB tartalomolvasás.
3. Minden forrástábla sor- és becsült byte-kerete férjen el. A teljes táblák
   bejárása egyúttal metadata lockot szerez a további olvasásokhoz.
4. Mind a 27 forrás tényleges InnoDB BASE TABLE legyen. View, MyISAM vagy
   hiányzó táblakészlet nem fogadható el.
5. Az explicit oszlopok ugyanabban a tranzakcióban, ugyanabból a read view-ból
   érkeznek. A kliensoldali feldolgozás csak visszaigazolt COMMIT után indul.

Normál, tranzakcióban publikált adatfrissítés közben így a teljes korábbi vagy
a teljes későbbi adatállapot látszik. Ez nem állítás több adatbázisszerver
közötti atomi release-ről. A DDL nem katalógus-publikálási művelet; kiadott
rendszerben külön telepítési eljárás szükséges.

## Tulajdon és érvényesítés

A `CatalogSnapshot` saját stringeket és explicit SQL NULL értékeket tartalmaz;
nincs MYSQL_ROW-ra vagy régi `TItemTable`-re mutató katalógusmező. A tiszta
`DecodeSnapshot` az SQL-könyvtár nélkül, külön standalone buildben is fut.

A dekóder szigorúan ellenőrzi a sor/oszlop alakot, a számok teljes és
veszteségmentes konverzióját, a booleant, a NULL/non-NULL különbséget,
az ItemKey-t, a szülő/gyermek sorokat és a recovery head pontosan egy
megfelelő típusú gyermekét. Hiányzó, duplikált és árva sor hibás.
Az SQL category- és bonus-regiszter minden mezője egyezzen a fordított
regiszterrel; SQL-adat nem vezethet be új bónuszviselkedést.

Ezután a meglévő `BuildItemCatalog` ellenőrzi az összes domainfeltételt és
elkészíti a kanonikus hash-t. Nincs külön, lazább SQL-balance-validátor.
Fegyver, páncél, egyszerű ékszer, ordinary material és recovery-only consumable
tölthető be. Más kategória tartalomsora továbbra is publikálási hiba.
DS, offline shop és a régi BattlePass nem kap új ágat.

A sorhatárok a domain kereteiből származnak: legfeljebb 100000 item,
4096 profil típusonként, a gyermekeknél a hozzájuk tartozó szorzó.
A teljes capture kerete 256 MiB: cellánként 160 byte becsült overhead és
a visszakapott érték byte-hossza. Ezt az SQL preflight a materializálás előtt,
a completion feldolgozás és a tiszta dekóder újra ellenőrzi. Ez
**nem a folyamat RSS-ére adott felső korlát**: a connector eredménye,
a saját snapshot és az épülő katalógus átmenetileg együtt élhet.
A byte-keret hamarabb is korlátozhat, mint valamelyik táblánkénti sorszámhatár.
A sikertelen keretellenőrzés nem tölti be a nagy eredményhalmazokat.

Az eredmény `shared_ptr<const ItemCatalog>` vagy típusos hiba; részleges
katalógus nincs. A korábbi snapshot helyettesítéséről a későbbi owner/publisher
dönt, kizárólag siker után. SQL-hiba, megszakadt kapcsolat, rollback,
guard-konfliktus és CommitUnknown nem ad át katalógust. A betöltő nem indít
automatikus újrapróbálást; az esetleges következő betöltés új teljes feladat.
A meglévő SQL-motor a tranzakción belüli reconnectet és replayt tiltja.

## Formátum és fejlesztési szintek

A friss, kiadatlan séma `011_catalog_format.sql` fájlja egyetlen új metadata
táblát ad. Ez a 28. tényleges `protos` tábla; a local-ID kiosztó számláló
nem része az olvasott 27 tartalom-/metadata-táblának. A marker nem content
release tábla, nem hash és nem katalógusrevision-kiosztó.

A kanonikus tartalom v5 marad; a meglévő kilencitemes fixture hash-e változatlan:
`ecffcba2e2774c3f67f54c4e95b58075bf2755663f2f8e905836298f29fb0c59`.
A `ReadPin.revision` pozitív, explicit builderbemenet; optional expectedHash
esetén pontos egyezés szükséges. A valódi release-hez szükséges monoton
revision-foglalás, hitelesített manifest és atomikus publikálás külön kapu.

Az SQL fejlesztési profilból a teljes 0..max görbe töltődik be. Ugyanaz a
fegyver-ItemKey +9, +10 és +30 szinten is azonos marad; nem keletkezik
szintenként másik prototípus. A próbában a profilmaximum +30, a +31 hibás,
a hiányzó +10 sor a teljes katalógust blokkolja. Ez fixture-bizonyíték,
nem minden éles tárgy maximumának vagy balance-ának megválasztása.

## Ellenőrzés és következmény

Tiszta sor-dekóder, tényleges prepared completion, független SQL-fixture,
csak olvasási DB-felhasználó, két sor párhuzamos tranzakciós frissítése,
kapcsolat kilövése a betöltés közben, +30 fejlesztési görbe, hibás regiszter,
izoláció, engine, gyermek és sorkeret ellenőrzése tartozik ehhez a fázishoz.
A párhuzamossági próbák névvel ellátott SQL-lockból építenek tesztbarriert;
a normál betöltőben nincs tesztcallback vagy GET_LOCK.

A friss eredmények: [tesztjelentés](../reports/item_combat/2026-10-27_catalog_sql_reader.md).
Hátravan a többi megmaradó itemviselkedés kategóriás portja, a tényleges
instance/factory és tartós gazdasági/refinement commit, az egyszeri offline
összevonás, majd a szerverprotokoll és Unity átállítása. A mostani betöltő
ezekhez ellenőrzött katalógusbemenetet ad; önmagában nem runtime-migráció.
