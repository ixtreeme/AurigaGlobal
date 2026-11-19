# Az új tárgyrendszer helyi telepítése és élő próbája

A `D:/AurigaGlobal/Server2-Local` frissítve, a helyi játékos szolgáltatások futnak. A Unity kapcsolati profilja `127.0.0.1`, auth: `30052`, CH99 játékport: `30078`, natív tárgybackend bekapcsolva. Az átállítás kizárólag a tulajdonolt helyi MariaDB-n (`127.0.0.1:3307`) és helyi fájlokon történt. Távoli szervert nem módosítottunk.

A korábbi 1574 definíciós, 31-es katalógus helyett a teljes elérhető konverzió fut: **6935 natív definíció, 55-ös katalógusformátum, 106 exportált tábla**. Ez 9853 konvertált forrássornak felel meg; 496 kivezetett sor és a társ ágának merge-jéig engedélyezetten kihagyott 637 sor változatlan besorolású. A példányadatbázis a `040_gaya_wallet.sql` migrációig frissült. A csomag játékbinárisai és natív katalógus-/leltársegédprogramjai frissek, a telepített fájlok ellenőrzőösszege egyezik a manifesttel és az ellenőrzött builddel.

Az eredeti helyi szörnyzsákmányból 183 csoport és **9027 aktív dobási bejegyzés** került natív kulcsokra. 298 halasztott és 96 kivezetett protóra mutató bejegyzés kimarad; négy további eredeti bejegyzés nulla esélyű. Az eredeti float32 számítás és a négymilliós alap dobási tartomány aránya megmaradt. Az ismételt forráscsoportok független dobásai is megmaradtak. A szerver `items.drops=typed` beállítással fut.

A telepített natív katalógus hash-e:

`741577036e2516600031bd492d5b3f22d3f14b0561321586dfd431fa5a945c85`

Ez a `full_proto_converted_catalog.json` korábbi, változatlan itemdefinícióit és az új dobási táblákat tartalmazza. A natív dobási kiegészítés a `config/item_combat/full_proto_mob_drops.json`; az offline előállító a `SRC/Tools/ItemCatalogBuilder/convert_local_runtime_content.py`. A teljes telepített export a helyi telepítési mentésben található. A helyi telepítés nem állította a konverziós csomag `release_ready` mezőjét éles kiadásra készre.

## A kapcsolódási próbák során javított hibák

- Az üres account a tényleges, nulla birodalomazonosítót kapja vissza; a karakterlétrehozás érvényes birodalomválasztást követel. Korábban véletlen birodalom került a válaszba, miközben a létrehozás nulla birodalmat használt.
- A Database a sikeres karakterlétrehozás után, a sikerüzenet előtt frissíti a hiteles account-cache karakterhelyét. Így a munkamenet-védelem azonnal engedélyezi a létrehozott karakter kiválasztását; nem kell előbb kilépni és újra bejelentkezni.
- Az üres, nulla régi aranyú karakter automatikusan natív tulajdonost, nyolcoldalas leltárt, 48 oldalas speciális leltárt, felszerelési tárolót, pénztárcát és saját földre dobási tárolót kap. Régi tárggyal vagy régi arannyal rendelkező karaktert ez nem importál csendben.
- A natív katalógus használatakor az új karakter nem kap régi `player.item` sorokat. A 16 faj-/képességcsoport-választás kezdőcsomagja natív példányokból, egyetlen visszaellenőrizhető tranzakcióban születik meg. Ismételt belépés nem ad újabb csomagot. Az eredeti kézi bónuszok és a jelmezek kibocsátási szabályai megmaradnak. Az eredeti 71231-es tárgy a natív rendszerben megidéző pecsét: a leltárba kerül és onnan használható, nem kerül tévesen egy hátasjelmez felszerelési helyére.
- A már felvett állapotban kibocsátott, első használattól időzített jelmezek órája elindul; nem marad korlátlan ideig használható, el nem indított állapot.

A helyi 380-as Elveszett Kastély sablon visszakerült a térképjegyzékbe az eredeti, helyben meglévő `castle_dungeon_razor93` adatokkal. A 366/377/380 sablonok a CH99 engedélyezett térképei között szerepelnek. Új pályaadatot vagy hiányzó grafikát nem állítottunk elő.

## Eredeti boltok és receptek

Az eredeti NPC-készlet most **46 alapértelmezett boltot, 57 questből megnyitható külön bolti útvonalat és 712 aktív ajánlatot** tartalmaz. 205 ajánlat tárgyas fizetőeszközt kér; az eredeti 364 ingyenes ajánlat ára kifejezetten nulla maradt. Az eredeti csomagár, darabszám, NPC-hozzárendelés és az engedélyezett idegen birodalmi árszorzó megmaradt. Egy vásárlás az összes szükséges tárgyas fizetőeszközt, Yangot és jutalmat közös natív tranzakcióban kezeli. A hiányzó/kivezetett tárgyra hivatkozó ajánlat nem válik véletlenül ingyenessé.

Az eredeti 180 receptből **173 működő recept került 13 műhelyhez**. 70 recept Gaya-költséges, 60 felszerelésből őrzi meg a bónuszokat és foglalatokat. Hét recept a felhasználó által halasztott protókra hivatkozik, ezért ezek kimaradnak. A régi kód által ténylegesen figyelmen kívül hagyott pet-/jelmez-/anyag-/öv-másolási jelzőkből nem készült új, kitalált állapotmásolás.

A Gaya külön, karakterhez tartozó, revízióval védett natív pénztárca. Az eredeti felső határ maradt: 999998. A friss karakter a kezdőcsomag kiosztása előtt nulla Yang- és Gaya-pénztárcát kap; meglévő natív tulajdonosnál a hiányzó Gaya-pénztárca belépéskor épül ki. Régi egyenleget ez nem importál. A questek Gaya-jóváírása/levonása az aszinkron, visszaigazolt pénztárcaírót használja.

Bónuszátvivő receptnél több megfelelő felszerelés esetén a leltárból kell konkrét példányt választani. Egyetlen próba fut; a szerver megőrzi a bónuszokat, zárolt bónuszhelyet, köveket, ékszerfoglalat-állapotot és kötöttséget, ha azok a célon is érvényesek. Összeférhetetlen állapotnál semmi nem fogy. Az eredeti sikertelenségi költség megmarad. A készítési küldetésjóváírás az új receptekkel is ugyanahhoz a tranzakcióhoz tartozik, ismétlésnél nem duplázódik.

A műhely-NPC-re kattintva a natív szolgáltatásválasztó nyílik: **Tárgykészítés**, az eredeti készlettel rendelkező NPC-nél **Kereskedés**, valamint **Beszélgetés** az eredeti questhez. A klienshez generált NPC-lista csak megjelenítési segítség; a jogosultságot, távolságot, konkrét forrást és árat minden kérésnél a szerver ellenőrzi. A megnyitás és a leltárból választás is be van kötve a normál Unity bootstrapba.

A csomagból **1384 régi karakter Gaya-állományát töröltük**; aktuális karakterhez tartozó állomány nem került törlésre. Régi személyes állományokról nem készült másolat. A `player.item` továbbra is üres.

Az offline előállító: `SRC/Tools/ItemCatalogBuilder/convert_world_trading.py`. A visszaellenőrizhető világforrások és a generált listák a `config/item_combat/original_npc_shops.source.json`, `original_cube.source.json`, `original_cube_prototypes.source.json`, illetve `config/typed_shops.generated.json`, `typed_crafting.generated.json` és `typed_world_services.generated.json` fájlok. A Unity `Resources/TypedWorldServices.json` tartalma egyezik az utóbbi generált listával.

A vezetékformátumok együtt frissültek: a Gaya-s leltár GC225 v32; az eredeti csomagáras/tárgyas ajánlat GC228 v2; a Gaya-s/bónuszátvivő műhely GC230 v2. A CG231 rögzített mérete megmaradt, a 4-es művelet a konkrét 64 bites forráspéldányt választja ki. A korábbi v1 bolt-/craft-fixture-ök továbbra is érvényesek.

## Ténylegesen lefutott ellenőrzések

- **85/85 CTest sikeres a végső C91 körben**, köztük hat MariaDB-s integrációs teszt, a Gaya-revízióütközés teljes visszagörgetése, megőrzött felszerelés és két pénztárca újrabelépése, valamint a készítési küldetésesemény egyszeri jóváírása. A próbapad és a katalógus-runtime SQL-próbája is átment a végleges natív tárgykiadóval.
- **14 konverterteszt**: öt a zsákmány/kezdőcsomag, kilenc az eredeti bolt-/receptátalakítás ellenőrzésére; a végső CTest-körben is futottak. A telepített bolt-/receptlisták újragenerálás után egyeznek.
- **Unity 6000.3.27f1 fordítás és validátorok sikeresek**: a készítés ellenőrzője **17958 ellenőrzés**, beleértve a v1/v2 TCP-feldarabolást, Gaya-s v32 leltárt, forráspéldány-választást, valódi megerősítő gombokat és az NPC-menüt. A bolti ellenőrzőben az eredeti tárgyas fizetés v2 formátuma is szerepel.
- **Élő TLS-es bolti próba: 22 ellenőrzés**, eredeti NPC 20016, eredeti ajánlat 300, 10000000 Yang és öt külön tárgyas fizetőeszköz. Valódi megerősítő gomb, egyszeri vásárlás, ismételt kérés, hiányzó fizetőeszköz elutasítása és újrabelépés is sikeres.
- **Élő TLS-es készítési próba: 26 ellenőrzés**, eredeti NPC 20094 a 219-es térképen, eredeti 99-es recept: 40000000 Yang, 2 Gaya és minden eredeti alapanyag. Két azonos megfelelő fegyverből a leltárban kiválasztott, bónuszolt példány alakult át; a másik megmaradt. A bónuszok/foglalatok, fizetés, ismétlés, elégtelen Gaya, hamis forrásazonosító és újrabelépés ellenőrizve.
- **Élő normál NPC-menüs megnyitás: 8 ellenőrzés**, a végleges GameServer-frissítés után is megismételve. A játékban használt szolgáltatásválasztó gombja nyitotta meg a natív műhelyt; megnyitáskor nem történt fizetés.
- A korábbi **29 ellenőrzéses élő újkarakter-próba** az eredeti C69 ellenőrzési pontban sikeres volt: két új karakter normál létrehozása és automatikus natív kezdőfelszerelése, ital, mozgatás, felszerelés/fegyvercsere, levétel, részleges földre dobás/felvétel, újrabelépés. A Gaya-val bővített frisskarakter-kiosztást a mostani SQL-kör is ellenőrzi.
- A csomagellenőrzés sikeres: konfigurációk, 514 JSON, 14 Lua, 76 quest, TLS-kulcspár. A végső audit öt futó szolgáltatást, egyező SQL-/konfigurációs katalógus-hasht, egyező telepített binárisokat és **nulla régi `player.item` sort** talált.

A próbák az aktív Editorból, a futó helyi GameServer/Database és a tényleges hálózati csomagok használatával futottak. Nem történt teljes vizuális Play-mode/Addressables játékosbuild, tényleges szörnyöléses zsákmánypróba, teljes kazamata-végigjátszás vagy minden eredeti questág ellenőrzése. A régebbi becsomagolt grafikus szervermenedzser teljes felületét nem építettük újra és nem teszteltük végig. Az inaktív esemény-NPC-k világba helyezését, illetve a hiányzó eredeti Gaya-piaci tartalom pótlását nem találtuk ki.

## Bizonyíték és mentések

A gépi ellenőrzés: `2026-11-16_local_native_item_deployment.evidence.json`. A C92 záró audit: `build/local-content-c92-audit.log`; a végső natív kör `build/local-content-c91-final-tests.log`, a kliens-/élő bizonyíték `build/local-content-c92-unity-success.log`, a csomagellenőrzés `build/local-content-c92-package-check.log`. A korábbi alaptelepítés fontos naplói: `build/local-deployment-c69-regression.log`, `build/local-deployment-c68-content-tests.log`, `build/local-deployment-c69-resumed-live.log`, `build/local-deployment-c69-unity-success.log`, `build/local-deployment-c69-package-check.log`, `build/local-deployment-c69-audit.log`. A tesztjelszavak kizárólag helyi, verziókezelésen kívüli fixture-fájlokban vannak; a jelentés nem tartalmazza őket.

Helyi mentések: `D:/AurigaGlobal/Server2-Local/run/deployments/local-native-v55-c61` és `D:/AurigaGlobal/Server2-Local/run/deployments/local-native-runtime-c66`. Az eredeti boltok mentése a `local-original-shops-c79`, a Gaya-/recepttelepítésé a `local-native-crafting-c86`, a végső készítési küldetésjóváírásé a `local-native-crafting-c91` almappában található. A végleges telepítési naplók `verified` állapotúak; az eredeti második napló is rögzíti az akkori állapotot, a legfrissebb napló rögzíti a végső katalógus-hasht, az ellenőrzéseket és a gépi bizonyíték hash-ét.

A munka a `20261103` szerverágon és a `/main/unity63-integration-20261103` Unity-ágon maradt. Másik régi Unity-munkaterületet nem módosítottunk; commit, check-in és branch-merge nem történt.
