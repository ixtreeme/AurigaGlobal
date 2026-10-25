# Mission Engine – Node Library átvezetés, 2026-10-25

## Új követelmények és rögzített döntések

Forrás: a felhasználó által átadott
`AurigaGlobal_Mission_Studio_Node_Library_Codex_2026-10-25.md`. A korábbi audit
használható API- és függőségtérkép marad, de az alábbi termékdöntések felülírják
annak korábbi javaslatait.

- Az új Mission Engine **natív C++23/EnTT**, Lua- és AngelScript-extension nélkül.
  Nem készül `MissionScriptContext`. A meglévő Lua 5.5.1 még szükséges a régi
  questekhez és más alrendszerekhez; törlése csak igazolt natív paritás után jöhet.
- Egyetlen, verziózott **`.aurmission` fájl** a telepítési egység. A JSON
  szerkesztési forrás; nem generálunk C++-t küldetésenként, és a csomag nem
  tetszőleges kódot tartalmaz.
- Közös primitívek és típusregiszter szolgálják ki a vizuális node-katalógust;
  nem lesz minden vizuális node külön C++ osztály.
- A `Race` **első siker** szabályú. Egy hibás ág után a többi még teljesíthet;
  csak minden ág hibája jelent sikertelenséget. A korábbi első terminális
  eredményű szabály külön `first_terminal` policy, például timeout-versenyhez.
- A felhasználó válasza szerint a kapuknál **mindkét módszer alkalmazható**:
  `ANY_OF` egy kész útvonalhoz, `AT_LEAST_N(2)` két független előkészülethez.
  Ezt a szerző választja a kapunál; az M06 végleges tartalmába nincs csendben
  beégetett alapértelmezés. Mindkét szabály nyolc ténykombinációval tesztelt.
- A korábbi kifejezett hatókör megmarad: csak a régi BattlePass cserélődik le,
  a kolléga szezonális Mission Pass rendszere megmarad.

## Audit: mi létezik ténylegesen?

Gépi mellékletek:

- `mission_node_library_coverage_2026-10-25.json`: **208** node-/makrónév,
  **712** legacy Lua API, pontos regisztrációs források, a 69 quest migrációs állapota;
- `mission_campaign_manifest_2026-10-25.json`: újraellenőrzött **80** egyedi ID,
  **10** main, **64** side, **6** secret; mind az öt felvonásban **16** bejegyzés.
  Ez tervezési manifest, nem játszható vagy prototípusokhoz kötött kampány.

A node-leltár jelenlegi osztályozása: **19** domain-primitív/makró-leképezés,
**7** még be nem kötött domain-esemény, **22** részleges megoldás és **160**
tervezett kiterjesztés. A 32 exportált natív típus nem 32 külön kész gameplay
szolgáltatást jelent: 11 flow, 12 predikátum és 9 eseménytípus közös magja.

A Lua API-leltár feltételes fordítási regisztrációkat és megnyitott standard
könyvtárakat is tartalmaz; nem az aktuális bináris preprocesszált exportszáma.
Nem futtat Lua-kódot. A meglévő 69 questforrás hash-e az előző audittal egyezik.
A korábbi osztályozás 18 gráfjelölt, 2 opcionális scriptkiegészítéses és 49
hiányzó natív képességű jelölt volt. Az új döntés miatt a korábbi 2 scriptes
jelölt is natív kiegészítést igényel; semelyik quest nincs még konvertáltnak jelölve.

Újraellenőrzött tényleges kapcsolódási pontok:

| Terület | Létező kód / API | Szükséges munka |
|---|---|---|
| Kill credit | `ecs/systems/CombatSystem.cpp`, MP `Events::Kill`, quest `CQuestManager::Kill` | Egyszeri szerveroldali eseményazonosító; solo/party credit szabály |
| Pickup | `ecs/systems/ItemSystem.cpp::PickupItem` | Csak sikeresen bekerült mennyiség; loot/split/merge/trade forrás szerinti szabály |
| Itemhatás | `ItemSystem::AutoGiveItemEcs`, `RemoveSpecifyItemEcs` | Tartós action receipt; ezek önmagukban nem SQL-tranzakciók |
| Player ownership | `PlayerRuntime::FindByPlayerID`, `CPIDRegistry`, `entt::entity` generáció | Aszinkron válasznál újrakeresés, session- és revision-ellenőrzés |
| Logout | `ecs/systems/SessionSystem.cpp::Disconnect`, quest `DisconnectPC` | Dialógus és sessionmunkák megszakítása, progress mentése |
| NPC/dialógus | `CQuestManager::Click/TakeItem`, `quest_input.cpp` | Új natív dialógussession, NPC-távolság, opció- és állapotellenőrzés |
| Unity | `Metin2GamePacketCodec`, `Metin2QuestScript`, meglévő quest UI | Önálló Mission snapshot/delta és presentation kliens; nem a régi scriptpacket újrahasználata |
| SQL | `CAsyncSQL` prepared API-k, `SqlParam`, `SQLResult::FetchRow` | Egy munkaként birtokolt tranzakció, CAS, reconnect-határok és teljes result-drain |
| Jutalom | `Database/ItemAwardManager.cpp`, `item_award` | Ledger és award egy tranzakcióban; tényleges kézbesítési crash/retry tesztek |

Az `AsyncSQL.cpp::TryExecutePrepared` jelenleg egy resultsetet dolgoz fel.
A külön queue-ba küldött BEGIN/UPDATE/COMMIT nem biztonságos tranzakciós store.
Ezt a következő tartóssági fázisban kell megoldani; nem másolunk át vakon egy
prepared CALL-t azzal a feltételezéssel, hogy minden eredményt kezel.

A `LuaOpenQuestLibraries`, `questlua_*.cpp`, `questmanager/questpc`, a régi
questpacketek és a data gyökerében levő Lua-beállítások továbbra is valós
függőségek. A natív cél nem jogosítja fel automatikusan ezek azonnali törlését.

## Elkészült implementáció

### Natív típusregiszter és magszemantika

`MissionNativeSchema.hpp/.cpp` stabil `type_id`, `schema_version`, port- és
paraméterazonosítókat, típust, kötelezőséget, ismétlődést, tartományt és típusos
alapértékeket exportál. A binary wire type ID-k nem a C++ enum sorrendjéből
származnak. A BranchArm feltétel/gyermek pár külön összetett típus.

A jelenlegi végrehajtási modell `structured_tree_v1`, beágyazott típusos
predikátumfával. Ez még nem tetszőleges adatfolyamgráf. Az export ezt külön
jelzi (`arbitrary_port_edges: false`), és nem hirdet kitalált gameplay
producereket (`gameplay_bound: false`). A teljes paraméterezhető EntityRef/
scope/action adatfolyam és a vizuális makró-lowering későbbi réteg.

Új tiszta predikátumok: `not` egy operandussal, `xor` pontosan kettővel,
`exactly_n` 0..N küszöbbel. A küszöbös feltételeknél a duplikált/azonos
tényforrás továbbra sem számít független bizonyítéknak. A Race/FirstTerminal
változás checkpoint-ellenőrzést és generált gráfteszteket is kapott.

### Egyfájlos bináris csomag

Új fájlok:

- `mission/MissionPackage.hpp`: immutábilis csomag, authoring modell és API;
- `mission/MissionPackageCodec.cpp`: típusos binary compiler/loader;
- `mission/MissionPackageSource.cpp`: szigorú offline JSON-beolvasás és UTF-8;
- `mission/MissionCatalog.cpp`: teljes katalógus atomi aktiválása;
- `mission/MissionPackageManager.cpp`: stage/activate/rollback, szűrt presentation;
- `SRC/Tools/MissionCompiler.cpp`: parancssori fordító, checker és sémaexport.

A formátum little-endian, nem C++ struktúrák memóriaképe. A fejléc:

| Offset | Méret | Mező |
|---:|---:|---|
| 0 | 8 | `AURMISN\0` magic |
| 8 | 2 | Formátumverzió, jelenleg 1 |
| 10 | 2 | Fejlécméret, 64 |
| 12 | 4 | Payload hossza |
| 16 / 18 | 2 / 2 | Gráf- és natív registry-séma, jelenleg 1 / 1 |
| 20 | 4 | Szekciószám |
| 24 | 32 | SHA-256, a hash mező nullázásával a teljes fájlról |
| 56 | 8 | Fenntartott, kötelezően nulla |

Szekciófejléc: `type:u16`, `version:u16`, `flags:u32`, `length:u32`.
A négy kötelező szekció: manifest, gráfok, natív követelmények, presentation/
lokalizáció. Ismeretlen kötelező szekció vagy type/schema verzió elutasított;
ismeretlen opcionális szekció átugorható. Minden mezőhossz, elemszám,
azonosító, mélység, ref, predikátum és UTF-8 szöveg ellenőrzött. A loader nem
JSON-ot alakít vissza és nem futtat scriptet.

A compiler determinisztikus: nincs benne időbélyeg, abszolút gépi út vagy
véletlen tartalom. A missionlista ID szerint rendezett, a gráfon belüli
gyermeksorrend szemantikus. Azonos source azonos bájtokat ad. A kimenet
ellenőrzés után, azonos mappában létrehozott kizárólagos ideiglenes fájlból
cserélődik; fordítási hiba megőrzi a korábbi jó csomagot.

Korlátok: **16 MiB** csomag/source, **1 MiB** binary gráf, **128** mission,
**8** locale, összesen **16 384** szöveg, szövegenként **4096** UTF-8 bájt,
**256** natív követelmény és legfeljebb **16** szekció. Nincs tömörítés vagy
tetszőleges erőforrású alias-kifejtés: definíciónként **65 536** függőségi
munkalépés, katalógusonként **262 144**, a megtartott registryben összesen
**1 048 576** indexelt függőségi elem a határ. Nincs
tetszőleges bytecode-szekció. A SHA-256 integritásellenőrzés, **nem aláírás**:
a majdani publish csatornának hitelesítettnek kell lennie.

### Aktiválás, pinning, rollback

`LoadPackage`/`LoadPackageFile` a staging: még nem módosítja a registryt.
`MissionPackageManager::Activate` a tulajdonosszál safe pointján egyszerre
váltja az egész aktív katalógust. Hibás csomag, változó ID/verzió-tartalom,
choice-domain ütközés vagy kapacitáshiba nem hagy részben publikált állapotot.
Katalóguskezelés után az egyenkénti `Publish` nem kerülheti meg az aktiválást.

Az aktív runtime a korábbi immutable definíciót tartja. A csomagból kivett
mission új indításhoz eltűnik, de korábbi verziója nem törlődik a futó
instance/checkpoint alól. `Latest` az aktív verziót jelenti, nem feltétlenül a
valaha legnagyobb számot. Explicit rollback a megelőző jó csomagot választja;
normál aktiválás nem fogad el korábbi vagy azonos kiadást.

Alapértelmezett **Gameplay** módban az esemény- és reward-receipt képességekhez
igazolt natív binding szükséges. Jelenleg ezek nincsenek bekötve, ezért a
production aktiválás nem minősíti a mintát játszhatónak. Az izolált tesztek
explicit **DomainSimulation** módban futnak; ez nem éles bypass kapcsoló.
A későbbi engine bootstrap ténylegesen létrehozott producer-/service-registryből
adhatja a verified binding listát, nem a csomag vagy a kliens állítja azt.

A rollback és a történeti verziók most **memóriában** élnek. Folyamat-újraindítás
utáni helyreállításhoz a következő DB-fázisnak a csomag/definícióverziókat is
archiválnia kell. Az új csomag egyfájlos telepítése nem jelenti a futó régi
verziók háttértárolási törlését.

### Unity presentation előkészítés

A csomag tartalmazhat lokalizációt és mission/objective megjelenítési kulcsokat.
A `ProjectPresentation` csak a külön engedélyezett mission és node-lista
megjelenítési szövegeit másolja ki; nem ad gráfot, titkos flag-et, predikátumot
vagy reward-handler logikát. Ismeretlen locale az alapnyelvre vált.

Ez még nem session-engedélyező és nem hálózati protokoll. Az engedélyezett
mission/node listát a későbbi szerveradapter állítja össze a tényleges állapotból;
nem kerülhet ellenőrzés nélkül kliens által küldött lista ebbe az API-ba.

## Használat

```powershell
cmake -S . -B build
cmake --build build --config RelWithDebInfo --target MissionCompiler MissionPackageTests MissionCoreTests GameServer --parallel 4
& build/SRC/Server/GameServer/RelWithDebInfo/MissionCompiler.exe compile config/mission_engine_package.example.json build/demo.campaign.aurmission
& build/SRC/Server/GameServer/RelWithDebInfo/MissionCompiler.exe check build/demo.campaign.aurmission
& build/SRC/Server/GameServer/RelWithDebInfo/MissionCompiler.exe schema build/mission-native-schema.json
```

A mintában szereplő `demo.*` azonosítók továbbra is szimbolikusak. Nem töltöttem
fel csomagot távolra, nem kapcsoltam be az új motort a futó szerveren, és nem
változott Unity-forrás vagy adatbázis.

## Módosított megvalósítási sorrend

1. **Kész alap:** auditfrissítés, stabil típusazonosítók, pontos flow-szabályok,
   determinisztikus `.aurmission`, staging/katalógusaktiválás és domain-tesztek.
2. **Tartós állapot és akciószerződés:** additív MariaDB-séma, archivált
   definícióverziók/csomaghash, prepared tranzakciós munka és teljes result-drain,
   karaktertulajdonlás és revision/CAS, dedup és outbox/ledger. Külön crash,
   két-core-verseny, újrapróbálás és tele inventory tesztek.
3. **Natív P0 service-registry és gameplay adapterek:** szint/tárgy/mission
   feltételek; kill/pickup/NPC/region események; GiveItem/RemoveItem és egyéb
   igazolt hatások, pontos scope és jogosultság. Csak valódi binding publikálható.
4. **Natív dialógus + Unity:** szerveroldali NPC/session/range/opció-ellenőrzés,
   snapshot/delta, presentation-cache verzió, napló/tracker/choice felület;
   NPC → kill/collect → choice → reward → completion vertikális szelet.
5. **Mission Studio:** önálló alkalmazás a közös regiszter/séma fölött;
   vizuális makrók, valódi port/lowering típusellenőrzés, simulator,
   jogosított publish és diff/debug.
6. **P1/P2 és konverzió:** scope/party/dungeon/boss/loop/retry/variable/RNG/
   signal/advanced objective képességek a leltár szerint, teljes kampány és
   legacy questmentés átvezetése. Lua csak a legutolsó függőség kiváltása után
   kerülhet ki; nincs átmenetileg élesített második jutalmazási út.

A DB-szerződés megelőzi az első live gameplay-hatást, hogy a P0 ne kapjon
ideiglenes, duplázásveszélyes jutalmazást. A teljes vizuális katalógus és a
P0 Unity end-to-end elfogadás még nincs készként megjelölve.

## Ellenőrzések

A natív mag és a csomagtesztek valódi C++ implementációt futtatnak. A csomag-
tesztek ellenőrzik a determinisztikus exportot, a típusos oda-vissza alakítást,
aktiválást, rollbacket, megőrzött futó verziót, atomi részhiba-kezelést,
binding-gate-et, szűrt presentationt, UTF-8-at és az új Race-szemantikát.
Minden rövidebb fájlhossz, több hibás fejléc/szekció/képesség, valamint **2000**
rögzített seedű, újrahash-elt payloadmutáció is lefut. Ez regressziós
mutációteszt, nem egy teljes külső fuzzingkampány helyettesítése.

A CLI-tesztek a valódi programmal dolgoznak ideiglenes mappában: fordítás,
ellenőrzés, azonos kimenet, sérült bemenet, sémaexport és korábbi jó csomag
megőrzése. A végső Windows x64 / RelWithDebInfo ellenőrzésben **8723 core +
4842 csomagellenőrzés = 13 565** natív ellenőrzés sikeres. A teljes GameServer
lefordult; a célzott regressziós kör **24/24**, a külön AddressSanitizer build
köre **4/4** sikeres. Ebben a fázisban nem történt FreeBSD build vagy Unity
end-to-end próba.

A tényleges compilerrel előállt helyi minta `build/demo.campaign.aurmission`
(1357 bájt), a típusleírás `build/mission-native-schema.json`. Ezek domain-
minták; nem élesre telepített küldetések. A buildnaplók:
`mission-core-checks.log`, `mission-package-checks.log`,
`mission-package-tests.log`, `mission-package-asan-tests.log`,
`mission-package-final-build.log` a helyi `build` könyvtárban.

A tágabb regresszió közben a korábbi `ServerPackageToolsTests.py` közvetlen
állapotfájl-olvasása egyszer Windows `PermissionError` hibát kapott az atomikus
fájlcsere mellett. A teszt most legfeljebb egy másodpercig újrapróbálja csak
ezt a megosztási hibát; hibás JSON-t vagy tartós hibát nem nyel el, és a
felügyelő kilépés-/tulajdonlás-/crash-ellenőrzései változatlanul megmaradnak.
Ehhez nem változott a production supervisor működése.

Az audit újragenerálható a `SRC/Tools/mission_node_audit.py` programmal;
a `--spec`, `--campaign-spec`, `--output`, `--campaign-output` argumentumokkal.
Az eszköz forrást és helyi korábbi auditot olvas, Lua-kódot nem futtat, és
nem kapcsolódik távoli géphez vagy adatbázishoz.
