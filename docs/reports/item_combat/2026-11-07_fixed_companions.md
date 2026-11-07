# C27b — Időkorlátos fix társak, bónuszok és típusos repülés

Szerver: `20261103`; Unity: `/main/unity63-integration-20261103`,
Editor `6000.3.27f1`, párosított changeset `cs:88`.
Szerződés: [IC-062](../../adr/item_combat_062_fixed_companion_clocks_bonuses_flight.md).

A fix pet és a hátas mind a négy órafajtával használható a típusos
leltárból. Az idézés, felülés, leszállás, visszahívás és az automatikus
lejárat szerveren és Unityben együtt működik. A pet követéskor, a hátas
felülve adja a katalógus alap-, aktuális fejlesztési és példánybónuszait.
A repülést a hátas típusos katalógusengedélye szabályozza.

## Élő helyi eredmény

Két elkülönített `codexcompanion` karakterrel **8 eset és 829 ellenőrzés**
sikeres, tényleges helyi TLS auth/game kapcsolaton, a Unity leltárgombjaival.

| Eset | Ellenőrzött viselkedés |
|---|---|
| Állandó pet és hátas | Idézés, pontos bónuszforrás, aktív és visszahívott újrabelépés; nincs lejárat |
| Létrehozástól számított pet és hátas | A kiosztáskor indult UTC-határidő nem indul újra idézéstől vagy belépéstől; visszahívva is lejár |
| Első idézéstől számított pet és hátas | Kiosztáskor még nincs határidő; az első elfogadott idézés indítja, később nem áll vissza |
| Aktív idejű pet és hátas | Követés és felülés fogyasztja; visszahívva és offline pontosan megáll; folytatás után lejár |
| Fix társbónusz | Alapból +3 Erő/+1000 raw fizikai támadás és +1 fejlesztésből +2/+2000 egyszer adódik össze; visszahíváskor eltűnik, aktív újrabelépéskor visszatér |
| Hátas fel-/leszállása | Követőként nincs hátasbónusz; leszállás megszünteti, újabb felülés egyszer visszaadja |
| Repülés | Követőn és földi hátason tiltott; engedélyezett felült hátason felszállás, gyors mód és leszállás mindkét kliensnek helyes |
| Lejárat repülés közben | A hátasalak, repülési jogosultság és bónusz eltűnik; lejárt pecsét újrabelépéskor sem tér vissza |

A visszahívás utolsó időtartozásának rendezése után a pet **43997 ms**,
a hátas **42381 ms** kerettel maradt. A későbbi várakozás és a kijelentkezés
utáni SQL-betöltés pontosan ugyanezeket az értékeket adta. A lejárat és
az azt követő újrabelépés mindkét aktív óránál sikeres.

Az élő próba közben javított statszámítási hiba: a lovaglás alapstat-
minimuma korábban elnyelhette a typed Erő-bónuszt. A minimum most a
bónusz előtti alapra hat; a tesztkarakter 6-os Erője 36-os hátasminimum
és +5 társbónusz mellett helyesen **41**. A bónuszvizsgálat a tényleges
társforrást, teljes typed bónusztáblát és a két érintett értéket ellenőrzi;
a belépési affectektől változó, nem érintett sebesség/SP pontokat nem
kezeli állandónak. A teszt a teljes inventory/stat válasz végéig olvas,
így korábbról maradt csomag nem igazolhat új műveletet.

Az SQL-utóellenőrzés **6 lejárt tombstone-t és 2 megmaradt új állandó
pecsétet** talált. A két eredeti kontrollpecsét változatlan. A lejárt
rekordok nulla darabszámúak, szándék nélküliek, és megőrzik a verziózott
lejárati blobot. A **833 másik példányrekord**, pénztárcák és tanulási
adatok hash/revízió szerint változatlanok. A tesztkarakterek nem kaptak
legacy tárgyakat.

## További elvégzett ellenőrzések

- Windows GameServer, ItemBenchTool és CatalogEditorTool fordítás sikeres.
- Kilenc natív/SQL tesztcsoport sikeres: `companion_items`,
  `persistent_items`, `equipment_stats`, `typed_inventory_wire`,
  `item_catalog_digest`, `item_persistence_mariadb`,
  `item_combat_schema_foundation`, `catalog_sql_mariadb`, `item_bench_mariadb`.
  Lefedés: mindkét fix társ és négy órája, alap/fejlesztési bónuszösszeg,
  dungeonkorlátozás, visszahívás utáni maradék idő, lejárat, új munkamenet,
  replay, hamisított élettartam elutasítása és tombstone-audit.
  A nyolc társ-/órakombináció külön regressziója a viselt ékszer aktív
  idejének egyidejű levonását is ellenőrzi, fordított itemvektorral és
  jogosulatlan ékszerfejlesztés elutasításával.
- A próbapad kiosztása is ugyanazzal a gyárral indítja az órákat; nyolc
  külön kiosztási eset és új munkamenetes visszaolvasás sikeres.
- Unity 6.3 fordítás az aktív módosított forrásokkal egyező ellenőrzőmásolatban;
  **700 companion-, 2447 stat- és 4179 leltárellenőrzés** sikeres.
  A csomagverziók, hibás órák/jelzők, csonkolás, TCP-darabolás és a valódi
  statablak forrásfelirata is ellenőrzött.
- Az önálló Python kanonikus encoder 15 katalógusfixture-je egyezik a
  natív v27 hash-ekkel. A részletező goldenben csak a katalógushash változott.
- Két Qt companion szerkesztőteszt sikeres, beleértve a repülési jelzőt
  és a petre váltáskor mentett hamis értéket. Az opcionális Qt SQL-csoport
  nem futott; az élő fixture a natív katalógusszerkesztő SQL-útját használta.

## Séma és helyi telepítés

A katalógus **v27**, új mezője a `033_companion_flight.sql` szerinti
`flying_mount`. A `015_companion_lifetime.sql` a példány verziózott óráját
menti. A GC225 **v12** az órát és repülési engedélyt, a GC231 **v5** a
fix társ pontos bónuszforrását küldi; a korábbi csomagok olvashatók.

A két migráció a saját leállított stacken kétszer is lefutott.
A teljes player/protos SQL-mentés, korábbi katalógus és példányhash-ek
megmaradtak. Mind a **770 migráció előtti példány** változatlan; a régi
katalógussorok az új alapértelmezett repülési mezőt és formátumváltást
leszámítva egyeznek. A katalógus követése az összes tárolt tulajdonost
betöltötte és validálta az új pin elfogadása előtt.

A friss három natív bináris, 015-ös migráció és 289 fájlos Qt-kezelő
telepítve a `Server2-Local` csomagban. A manifest, fordított binárisok,
aktív Unity-források és tesztmásolat hash-e egyezik; az öt saját
szerverfolyamat fut. Ideiglenes map-/szörnyadatot ez a próba nem telepített.

Bizonyítékok az ignorált `build` alatt:
`c27b-native-tests2.log`, `c27b-schema-tests2.log`, `c27b-final-sql-tests.log`,
`c27b-bench-tests4.log`, `c27b-ancillary-tests.log`,
`c27b-game-build4.log`, `c27b-unity-offline1.log`,
`c27b-fixed-live7.log`, `c27b-fixed-sql7.log`,
`c27b-migration-evidence.json`, `c27b-manager-build.log`,
`c27b-manager-deployment.json`, `c27b-deployment-evidence.json`.
A fixture és az SQL-mentések hozzáférési adatai nem kerülnek verziókezelésbe.

Nem futott teljes világjelenetes modell-/animációs vagy standalone
klienspróba, FreeBSD-build, terheléses mérés vagy külön élő dungeon-,
halál-, aréna- és karavánkorlátozási próba. A két kliensnek küldött
tényleges társ-/hátasalak, repülés és a mentésből visszatöltött állapot
ellenőrzött. A következő általános itemcsomag a jelmezek runtime bekötése.
