# Droptáblák szerkesztése és újratöltése futó szerveren

Elkészült és a saját helyi csomagban telepítve. A Szerverkezelő **Tárgykatalógus → Droptáblák** felületén futó játékszerver mellett is létrehozható, módosítható és törölhető droptábla. A mentés után a futó core-ok automatikusan átveszik az új verziót; a külön **Dropok újratöltése** gombbal a mentett verzió betöltése és visszaigazolása ismét ellenőrizhető. A 637 merge-re váró forrástétel nem változott.

## Használat és visszajelzés

1. A helyi adatbázis legyen elindítva. A játékszerver működhet közben.
2. Nyisd meg a droptáblát, vagy készíts újat, majd válaszd ki a meglévő natív tárgyakat, darabszámokat, fejlesztési szinteket és dobási szabályokat.
3. Mentéskor a natív validátor ellenőrzi a teljes dropváltozatot. A szerver core-jai körülbelül másodpercenként nézik az új verziót, háttérben betöltik és ellenőrzik, majd egyben cserélik a saját dropnézetüket.
4. A felület csak akkor jelez teljes betöltési sikert, amikor **minden futó game core** a kért verziót igazolta vissza. Elhúzódó vagy sikertelen visszaigazolásnál a szöveg egyértelműen „mentve, újratöltés függőben”; a mentett módosítást nem kell újra beküldeni.

Leállított szervernél is lehet menteni: a következő indulás a mentett aktív változatot tölti be. A normál betöltési módosítókat és a dropkiosztás korábbi szabályait ez a munka nem változtatja meg.

CLI: a csomag `manage.py reload-drops` művelete a mentett kiadást olvassa és minden futó core visszaigazolását ellenőrzi; a `manage.py drop-status` az aktuális core-állapotokat adja vissza.

## A katalógus és a folyamatban lévő tárgyműveletek megőrzése

A teljes statikus itemkatalógus hash-ébe az eredeti droptáblák is beletartoznak, és ezt a hash-t a tartós itemállapot/nyugták rögzítik. Ezért az online módosítás külön, változtathatatlan dropkiadást kap. A 043-as migráció négy táblát ad hozzá a `protos` adatbázishoz:

- `mob_drop_live_release`: kiadásazonosító, alap- és drophash, sorszámok helyett ellenőrzött sorlétszámok, időpont;
- `mob_drop_live_group` és `mob_drop_live_entry`: a kiadás típusos SQL-oszlopokban tárolt csoportjai és tárgysorai;
- `mob_drop_live_head`: az aktív kiadás mutatója.

A kliens nem kap új droplista-protokollt: a szerver sorsol, a meglévő natív jutalmazási és leltárcsomagok adják át az eredményt. A Unity runtime kódjának és packetformátumainak módosítása nem volt szükséges; az élő Unity-validator új forgatókönyvet kapott.

Az aktív dropnézet ugyanazzal az itemkatalógus-builderrel készül, mint a statikus katalógus. A `CurrentCatalog`, az item worker katalógusa, a meglévő példányok, a persistence-pin és a már előállított jutalomkérések változatlanok. A droppal kapcsolatos sorsolás a külön `CurrentDropCatalog` nézetet használja. Egy folyamatban lévő olvasó tovább tarthatja a korábbi változatot.

A SQL-mentés tranzakcióban írja az új kiadást és az aktív mutatót. Két szerkesztő ütközése régi kiadásazonosító/hash esetén visszautasítást ad. Egy online dropmentés nem módosíthat más itemtáblát. Az elveszett COMMIT-válasz változtathatatlan kérésazonosító alapján visszaolvasható; hibánál nem történik vak újrabeküldés. A korábbi kiadások auditként megmaradnak. Meglévő kiadássorok közvetlen SQL-átírása nem támogatott szerkesztési mód.

A csere **core-onként atomi**, a külön folyamatok nem ugyanabban az órajelben váltanak. A kezelő minden futó core friss, tulajdonolt folyamatazonosítóhoz és supervisor-tokenhashhez kötött visszajelzését megvárja. Hibás kiadásnál az érintett core megtartja az utolsó jó dropnézetet és jelzi a hibát. Más core-ok eközben már átvehették az új kiadást; ilyenkor a felület nem állítja, hogy a teljes frissítés kész.

Leállított szerver mellett végzett további itemdefiníció-szerkesztés az aktív dropkiadást ugyanabban a tranzakcióban új alaphoz köti, és visszautasítja az itemváltoztatás által érvénytelenné tett dropot. Ez a funkció a droptáblák online frissítését valósítja meg; a teljes itemdefiníció-katalógus cseréje továbbra is külön művelet.

## Ellenőrzések

- MSVC Release `GameServer`, `CatalogEditorTool`, `MobDropTests` és `ItemCatalogRuntimeTests` sikeres fordítás.
- Öt releváns CTest sikeres: `mob_drops`, `catalog_editor`, `drop_reload_control`, `live_mob_drops_mariadb`, `item_catalog_runtime_mariadb`. A szerkesztő 84 modell-/natív validációs/Qt-esetét a projektben rögzített PySide6 Essentials 6.12.0-val futtattuk, a Qt-próbák nem maradtak kihagyva.
- Nyolc valódi, elkülönített MariaDB-kiadási próba: változatlan alapkatalógus, két szerkesztő ütközése, triggerrel előidézett teljes rollback, ismételt kérés, idegen itemmódosítás tiltása, hibás fejlesztési szint, minden drop törlése, itemváltoztatással közös atomi újraalapozás.
- Hét core-visszaigazolási/vezérlési próba: azonos verzió minden core-on, lemaradó core, visszautasított kiadás, régi PID/token/időbélyeg, leállás, tokenek elrejtése, a droppublikálási zár függetlensége a futó supervisor élettartam-zárától.
- A valódi item worker 126 ellenőrzéses próbája: közzététel előtt sorba állított itemmentés megmaradó eredeti pinnel, új drop a következő sorsolásban, korábbi olvasói nézet megőrzése, hibás/csonka kiadás elutasítása, utolsó jó változat megtartása, törlés, valamint workerindulás a már tartósan közzétett dropverzióval.
- A Windowsos Szerverkezelő újracsomagolása a rögzített Qt/PyInstaller függőségekkel. Mindkét csomagolt natív segédeszköz önálló indítása sikeres; a Qt ablak elindult és a helyi szerver futó állapotát olvasta. A dropszerkesztő és az újratöltési visszajelzések offscreen Qt-tesztekkel ellenőrizve.
- Unity 6000.3.27f1 fordítás és **valódi helyi TLS-játékpróba**: saját tesztkarakter, normál 103-as szörny tényleges megtámadása/megölése, első drop 2 darab; éles szerkesztési útvonalon átállítás 3 darabra; ugyanazon klienskapcsolat következő öléséből 3 darab. Újrabelépés után mind az 5 új tárgy megmaradt. Mindhárom futó game core visszaigazolta mindkét változatot.
- Az öt szerverfolyamat PID-je végig azonos maradt a dropmódosítások és a visszaállítás alatt. Az összes többi natív tárgy és pénztárca hash-e egyezett. A 17 kivezetett állapottábla továbbra is hiányzik.
- A próba végén az eredeti 183 csoport/9027 dropsor és az ideiglenesen, kizárólag a saját PID-re bővített meglévő quest eredeti bájtjai visszaálltak. A quest visszatöltése is élőben történt, a stabil questazonosítók megtartásával.

A helyi csomag egyszeri binárisfrissítése az új szerverkód telepítéséhez szükséges volt. A fent igazolt dropmentésekhez, újratöltésekhez és a teszt utáni visszaállításhoz **nem történt szerverújraindítás**. A friss Szerverkezelő is telepítve van a `D:/AurigaGlobal/Server2-Local/manager` mappában.

A végső aktív drophash és a statikus katalógus hash-e ismét az eredeti `741577036e2516600031bd492d5b3f22d3f14b0561321586dfd431fa5a945c85`. A forrás-konverziós katalógus SHA256-ja változatlan: `9ab3dac7f81d2898ab89aeeca641bffc4f59b28ac5f7683d080282ea9913babd`. A 637-es halasztott halmazhoz nem nyúltunk.

Nem futott: FreeBSD build, standalone/mobile Unity-build, távoli szervercsomag szerkesztése, terhelési és többgépes hálózatkiesési próba. A core-onkénti hibamegőrzést és késleltetett visszaigazolást külön natív/SQL/vezérlési tesztek fedik. A korábbi teljes legacy-forrástakarítás ettől továbbra is külön nyitott munka.

[Gépi bizonyíték](2026-11-17_live_drop_reload.evidence.json).
