# Mission Engine: forráskód-audit és megvalósítási terv

2026-10-24. Ez az átadott specifikáció **első, audit fázisa**. Nem készült
új éles játékrendszer, nem történt kódeltávolítás, DB-migráció vagy telepítés.

## 1. Jóváhagyott hatókör és vizsgált állapot

A felhasználó külön pontosította: **csak a régi BattlePass-t váltjuk ki**.
A kolléga szezonális **Auriga Mission Pass** rendszere megmarad, a saját
szabályaival, jutalmaival, SQL-tábláival és Unity-felületével együtt.
Az új történeti Mission Engine ettől független, karakterenkénti rendszer.
A hagyományos NPC-questek célállapota ugyanaz az új Mission Engine, nem egy
második állandó questfuttató. BattlePass-adapter vagy párhuzamos BattlePass
runtime nem készül. AngelScript nem kerül a projektbe.

Vizsgált helyek:

| Terület | Tényleges hely / állapot |
|---|---|
| Szerver Git | `D:/AurigaGlobal/LiveWork/AurigaGlobal`, `ECS_+_BGFX`, `9cda87fd` |
| Előzmény | `239c9672` hozta a külön Mission Pass-t, `39e0cac9` integrálta |
| Unity | `D:/Auriga-Unity/SRC/AurigaUnity`, `/main`, `cs:29` |
| Helyi szervercsomag | `D:/AurigaGlobal/Server2-Local` |
| Távoli tesztprofil | `Auriga-NC-TESZT`, `/Auriga/Source`, `/Auriga/Server` |
| Régi SQL-mentések | `D:/AurigaGlobal/Database/{account,common,log,player}.sql`, 2026-10-20 |
| Követelményleírás | `C:/Users/ixtre/Downloads/AurigaGlobal_Mission_System_Codex_v2.md` |

A követelményleírás SHA-256 értéke és minden keresési találat pontos
fájl-/sorszáma a mellékelt `mission_engine_audit_2026-10-24_inventory.json`
állományban van. A leltár 151 szerver/legacy kliens/tool/test és 78 Unity
szöveges találati fájlt tartalmaz, **mindkét pass rendszert megkülönböztetve**.
Külön szerepelnek a nevük alapján érintett assetek: a 273 Unity-útból 248
BattlePass nevű jelölt. Ezek száma meta- és dokumentációs fájlokat is tartalmaz,
nem 248 külön játékfunkciót jelent. Nem mind törölhető automatikusan.

Az audit alatt a Unity meglévő nyolc helyi módosítását nem változtattuk meg.

## 2. Biztosan létező alapok és fő hiányok

| Téma | Kódból igazolt állapot | Következmény |
|---|---|---|
| Nyelv | Gyökér `CMakeLists.txt`: C++23 | Nem kell nyelvi verziót váltani |
| Lua | `SRC/3rd/LUA55/src/lua.h`: **5.5.1**; `SRC/CMakeLists.txt` ezt kapcsolja | A régi `SRC/3rd/LUA` fa létezik, de nem ez a szerver VM-je |
| ECS | `ecs/Registry.hpp`: egy globális `g_registry`, kizárólag a fő játékszálon | SQL callback nem érhet szabadon az ECS-hez |
| Események | `ecs/EventDispatcher.hpp`, `ecs/events.hpp`, `g_dispatcher` | Nem minden deklarált eseménynek van megbízható gameplay kibocsátója |
| Prepared SQL | `SQL/AsyncSQL.h`, `GameServer/network/db.h` | Van valódi paraméterkötés; nem kell szöveges SQL-t építeni |
| Jutalom | `ItemSystem::AutoGiveItemEcs`, `PointSystem::Change`, DB `ItemAwardManager` | Létező szolgáltatások, de nem kész Mission tranzakciós könyvelés |
| Quest hot reload | `RuntimeGeneration`, `RuntimeScope`, megosztott generációk, kétlépcsős publikálás | Érdemes a verziókezelési elvet átvinni, a régi runtime-ot nem kell örökösen megtartani |
| Mission graph | Nincs a specifikáció szerinti motor | Definition/evaluator/router/store/reward layer új megvalósítás |
| Studio | Nem találtam közös Quest Studio gráfmagot a vizsgált forrásban | A meglévő Qt6/PySide6 eszközkészlet használható alapként, de gráfszerkesztő nincs készen |
| Kampány | 80 szöveges katalógusbejegyzés, nem bekötött játéktartalom | Hiányoznak a konkrét NPC-, item-, map-, terület-, dialógus- és bossmechanika-hozzárendelések |

## 3. Régi BattlePass: törlési és módosítási leltár

A géppel olvasható melléklet a teljes találati listát tartalmazza. A következő
csoportok határozzák meg, mely fájl egészben törölhető, és melyikből csak
BattlePass-specifikus rész távolítható el.

### 3.1. Kizárólagos implementációk

Szerver:

- `SRC/Server/GameServer/combat/battle_pass.cpp`
- `SRC/Server/GameServer/combat/battle_pass.h`
- `SRC/Server/GameServer/combat/battle_pass_input.cpp`

Unity, a hozzájuk tartozó `.meta` fájlokkal:

- `Assets/Metin2/Runtime/Network/Metin2BattlePassPanel.cs`
- `Assets/Metin2/Runtime/Network/Metin2BattlePassLocale.cs`
- `Assets/Metin2/Generated/UI/Layouts/battlepasswindow.json`
- `Assets/Metin2/Generated/UI/Layouts/battlepasswindow_back.json`
- `Assets/Metin2/Generated/UI/Prefabs/Metin2Ui_battlepasswindow.prefab`
- `Assets/Metin2/Generated/UI/Prefabs/Metin2Ui_battlepasswindow_back.prefab`

Régi kliens-/generálási forrás:

- `root/uibattlepass.py`
- `root/uiscript/battlepasswindow.py`
- `root/uiscript/battlepasswindow_back.py`
- `root/locale/*/battle_pass.txt`

A Unity `Resources/Metin2/Locales/*_battlepass.json`, a BattlePass wiki- és
képfájlok csak GUID- és katalógustulajdonlási vizsgálat után törölhetők.
A teljes pontos útlista a melléklet `named_unity_files` mezőjében található.
A forrásban említett `Migration/Tools/stage_battle_pass_locale.py` a vizsgált
helyeken nem található; ezért egy külső importáló újbóli futtatása ellen is kell
tiltás/ellenőrzés. A generált asset egyszeri törlése önmagában nem elég.

### 3.2. Megtartandó fájlok, eltávolítandó integrációs részek

| Fájlcsoport | Eltávolítandó rész | Megtartandó |
|---|---|---|
| `ecs/components/activity_components.hpp`, `status_components.hpp`, `character_runtime_components.hpp` | `BattlePassMissions`, `BattlePassTiming`, BP-specifikus mezők | Fishing, mining, anti-flood, normál karakterállapot |
| `ecs/EntityFactory.cpp`, `PlayerRuntimeSystem.{cpp,hpp}` | BP létrehozás, hozzáférők, progress, boost, számlálók, teardown | Teljes ECS-létrehozás, normál karakteréletciklus |
| `core/main.cpp` | `CBattlePass` példány, include és régi BP indítás | `AurigaMissionPass::Tick/Flush`, újonnan javított mozgási sorrend |
| `CombatSystem.cpp`, `PointSystem.cpp`, `MovementSystem.cpp`, `SessionSystem.cpp`, `ActivitySystem.cpp` | BP küldetésfrissítések, BP online-idő eseményei | Harc, mozgás, mentés, session, gyűjtés és szezonális MP-horgok |
| `ItemSystem.cpp`, `ItemSystem_LegacyBridge.cpp` | BP pickup/use/refine/destroy/boost/aktiválás ágai | Inventory, stackek, tárgyhasználat, jutalmazás, crafting |
| `combat/affect{.h,_db.cpp}`, `AffectSystem.{cpp,hpp}` | BP affect és lejárat viselkedése | Közös affect-tároló és minden más tartós affect |
| `world/fishing.cpp`, `item/cuberenewal.cpp`, `social/{chat_input,new_switchbot,shop,shop_manager}.cpp` | Csak BP esemény-/számlálóhívások | Halászat, craft, chat, switchbot, kereskedelem |
| `dungeon/{OrcsDungeon,RuneDungeon,TritonTempleDungeon}.cpp` | Régi BP completion részek | Dungeon állapotgép és az önálló szezonális MP completion |
| `quest/{questlua_pc,questlua_dungeon}.cpp` | BP-only Lua API-k, ranking, régi pass-progress | Más quest API és dungeon működés az átállás végéig |
| `network/{input,input_main,input_db,input_login,packet_info}.*` | BP packet routing/handler és DB betöltés | Általános protokoll, TLS, input validation |
| `Database/{ClientManager,ClientManagerBoot,ClientManagerPlayer,DatabaseConfig,Main,QID,player_save}.*` | BP lekérdezések, ranking, config, player mező | Quest/item/account persistence és szezonális MP |
| `common/{service,length,tables}.h`, `Headers/{Header_CG,Header_GC,CG_Packet,GC_Packet}.h` | BP feature/type/packet/serialized mezők | Minden más típuskód és numerikus protokollazonosság |
| `SRC/Tools/{server_package/data.py,txt2json/txt2json_group.py,ServerManager/quest_api_catalog.py}` | `battle_pass.json` konverzió/listázás és BP-only completion API | Közös JSON-kezelő, quest szerkesztő és adatcsomagoló |
| `SRC/Launcher/UserInterface/*`, `root/{game,interfacemodule,localeinfo,uiinventory,uitaskbar,ui}.py`, közös UI-scriptek | Régi BP entrypointok, packetek, Python API-k és gombok | Más legacy kliens- és migrációs adatok |
| Unity `Metin2{AuthSession,GameRuntime,GamePacketCodec,PacketFramer,AurigaCustomSystems,ClientBootstrap,GameUi,InputController}.cs`, `Mobile/Metin2MobileHudView.cs` | BP események, codec/frame regisztráció, F8/gomb, régi panel | Közös UI, mobil input, új MP, quest dialógusok |
| UI-/sprite-/locale-/mobile katalógusok | BP-only bejegyzések | Közös sprite-ok, fontok, itemikonok és locale-adatok |

A közös `CJsonGroupTreeLoader`, `g_dispatcher`, `DBManager`, `CAsyncSQL`,
`ItemAwardManager`, inventory-, affect-, dungeon- és ECS-rendszer **nem törölhető**.

Eltávolítandó régi feature-kapcsolók: `ENABLE_BATTLE_PASS`,
`ENABLE_BATTLE_PASS_STAY_ONLINE`, `ENABLE_BATTLE_PASS_CHAT_CNT`,
`ENABLE_BATTLE_PASS_SECURITY_KILL`, `ENABLE_BATTLE_PASS_RELOAD`, valamint az
érintett helyeken kizárólag ezt vezérlő `ENABLE_FREE_PASS_RAZOR93`.
Nem elég kikommentezni vagy végleg `OFF` állapotban bent hagyni a régi runtime-ot.

### 3.3. Konkrét protokoll- és ABI-kockázatok

- CG BattlePass: **40**; GC open/update/ranking: **160 / 161 / 162**.
- Game→DB save/register-ranking/ranking: **82 / 83 / 84**.
- DB→Game load/ranking: **78 / 79**.
- BP affect: **538**, boost: **1012**; `POINT_BATTLE_PASS_ID` jelenlegi helye
  az aktív enumkiosztásban **164**.
- `TPlayerTable::dwBattlePassEndTime`, `TPlayerBattlePassMission` és a többi
  BP-only struktúra a Game/DB binárisok közös szerződésének része.

Eltávolításkor nem számozzuk át a többi feature azonosítóit. A felszabaduló
számok nem lesznek automatikusan az új Mission Engine számai. A BP kód/handler
eltűnik; a többi pontkód megmaradását explicit értékekkel és layoutteszttel
biztosítjuk. GameServer, Database és kliens kiadását össze kell hangolni.

### 3.4. Tesztek és build

A szerver forrásgyűjtője `SRC/Server/CMakeLists.txt` alatt rekurzív globot
használ. A törlés után CMake újragenerálás kell; nem csak egy kézzel írt
forráslista javítása. BP-specifikus tesztdublőrök/ellenőrzések találhatók:

`tests/server/{AffectLifecycle,CombatState,DatabaseTime,DungeonLifecycle,
ItemAttribute,ItemRuntime,ShopLifecycle,SpatialLifecycle,SwitchbotLifecycle}Tests.cpp`.

Ezeket a tesztprogramokat megtartjuk; a BP-only esetek/stubok kerülnek ki.
`MissionPassRulesTests.cpp` megtartandó: a régi BattlePass említése benne
nem jelenti, hogy ez a törlendő régi rendszer tesztje.
Unity oldalon a `Metin2ProtocolCodecValidator`, `Metin2AurigaCustomSystemsValidator`,
`Metin2AurigaAccessParityValidator`, `Metin2DeepseekUiRegressionValidator` és
`Tools/ProtocolAudit/AurigaPacketSizes.cpp` BP-only ellenőrzései is érintettek.

## 4. Adatok, megszerzett jogok és visszaállítás

A távoli **tesztprofil** olvasási auditja MariaDB **11.4.13**-at mutatott.
Ez nem állítás egy másik, későbbi éles adatbázis állapotáról. Az olvasások
prepared utasításokkal, read-only sessionben futottak; nem történt adatírás.

| Ténylegesen olvasott adat | Eredmény |
|---|---:|
| `player.battle_pass` sor | 9 565 |
| Érintett külön karakter | 2 922 |
| Completed sor | 1 468 |
| `player.battle_pass_ranking` sor | 1 |
| Tárolt `player.player.battle_pass_end` | 212 |
| Jövőbeli lejárat | 9 |
| `player.affect`, típus 538 | 3 356 sor |
| `player.affect`, típus 1012 | nincs sor |
| `quest`: `battle_pass.final_reward.*` | 2 flag, 1 karakter |
| `player.item`, vnum 70611 | jelenleg 0 stack |
| Szezonális MP jutalomként vnum 70611 | jelenleg 0 bejegyzés |

Az érintett player/item/item_award/quest/affect/pass táblák mind InnoDB-k.
A jövőbeli dátum és az affect megléte külön adat: a ténylegesen élő jogosultságok
listáját a cutover idején kapcsolással és lejáratellenőrzéssel kell véglegesíteni.

A régi jogosultság **nem csak a két BattlePass-táblában él**. Lejárat a player
sorban, aktív jelzés/boost az affectben, végső jutalom átvétele quest flagként
is tárolódik. A `70611` tárgy használatának két fordítási ága aktiválást vagy
boostot jelent; mindkét ágat rendezni kell, nem hagyhatunk hibás tárgyfogyasztást.

A régi jutalmazás `AutoGiveItemEcs` hívásokat használ. A már kiadott tárgyak
a normál `item`, safebox/offlineshop és kapcsolódó tulajdonosi rendszerekhez
tartoznak; **nem vonjuk vissza és nem adjuk ki újra** őket pusztán a régi
completed flag alapján. A régi flag nem teljes tranzakciós jutalomledger,
ezért egy korábbi crash utáni kétértelmű állapotot nem lehet biztosan
automatikus újrajuttatással rendezni.

Az audit során nem találtam külön, kizárólag BattlePass-ból élő kozmetikai
unlock-táblát. Ez nem igazolja, hogy egyedi régi script nem adott tartós
jogot/affectet; a végleges entitlement exportnak ezeket is ellenőriznie kell.

### Kötelező cutover-adatkezelés

1. Rögzített maintenance ablak, régi BP írások leállítása és mentések ürítése.
2. Új, konzisztens DB-mentés a régi BP táblákról, player sorokról,
   quest/affect adatokról, inventoryról, item_awardról és a ténylegesen használt
   további tárolókról. Schema, routines, grants, konfiguráció, questgeneráció
   és binárisok is mentendők. A szezonális MP állapotát változatlanul megőrizzük.
3. SHA-256 manifest, sorszám-/darabszám-összevetés és **külön adatbázisban
   végzett visszaállítási próba**. A 2026-10-20-i helyi SQL-ek nem friss
   cutover-mentések; ezen auditban nem teszteltük a visszaállításukat.
4. Offline, egyszer futó entitlement export: lejárat, aktív/lejárt állapot,
   boost, átvételi jelzés és még fel nem használt aktiválótárgyak. Idempotens,
   naplózott átvezetés, csak az elfogadott rendezési szabály szerint.
5. A kilenc potenciálisan még élő időszak kezeléséről külön döntés kell:
   természetes kifutás még a lekapcsolás előtt, vagy meghatározott egyszeri
   kompenzáció/jogáthelyezés. Nem alakítjuk automatikusan történeti Mission
   progresszé vagy szezonális MP premiummá.
6. Előbb a runtime kód és lekérdezések kerülnek ki. A már nem hivatkozott
   régi táblák egy ideig csak visszaállítási célú, inaktív adatként maradhatnak.
   Ez nem kompatibilitási runtime. **DROP / oszloptörlés külön, későbbi,
   kifejezett jóváhagyással**, sikeres export és restore-próba után történhet.

Törlendő DB-objektumok jelöltjei: `player.battle_pass`,
`player.battle_pass_ranking`, később `player.player.battle_pass_end` és a
pontosan azonosított BP-only affect/quest adatok. A közös quest/affect/item
táblák és a szezonális `account_mission_pass`, `mission_pass_*`, illetve a
`mission_pass_snapshot/commit/claim` rutinok **nem törlendők**.

Visszaállítás: Game/DB indulás tiltása; az előző egymáshoz illő bináris,
klienskiadás, konfiguráció és adatsnapshot visszahelyezése; entitlement-export
ledgerrel való egyeztetés; izolált config/SQL ellenőrzés; azonos core-választás
indítása. Újonnan kiosztott jutalmakat nem lehet vakon egy régi teljes DB-re
rollbackelni. Vagy még nincs új állapotírás, vagy előre tesztelt visszafelé
konverzió és cutover óta keletkezett adatok egyeztetése szükséges.

## 5. ECS és gameplay integráció: tényleges csatlakozási pontok

Az `entt::entity` generációt tartalmaz, a PID/VID önmagában nem runtime-élettartam.
Létező megoldások: `CPIDRegistry::Find`, `CHARACTER_MANAGER::FindEntityByPID`,
`PlayerRuntime::FindByPlayerID`; törlésnél `DestroyCharacterStatePre/Post`;
logoutnál `SessionSystem.cpp` jelenleg `CQuestManager::LogoutPC`-t hív.

Az új `MissionComponent` csak generációvédett progress-handle-t és revisiont
tartalmazna. Ez **tervezett új típus**, jelenleg nincs ilyen komponens.
Az async munka csak tartós karakterazonosítót, munkamenet-/tulajdonosi
tokent és revisiont vihet tovább. A callback a játékszálon újrakeresi és
ellenőrzi a tulajdonost; régi kapcsolatnak nem küld állapotot. Logout/törlés
megszünteti a feliratkozásokat, függő dialógust és az adott session műveleteit.

| Esemény | Létező bizonyíték / megfelelő határ | Szükséges kiegészítés |
|---|---|---|
| Mob/boss kill | `CombatSystem.cpp`: sikeres ölési kredit mellett jelenleg MP `Events::Kill` és quest `Kill` | Egyszeri, szerver által adott kill-azonosító, tárgyalt party-/részesedési szabály |
| Inventory collection | `ItemSystem.cpp::PickupItem`, meglévő `collected(count)` határ | Sikeresen bekerült mennyiség és forrás; split/merge, átadás, loot/gathering ne számoljon duplán |
| Craft | `item/cuberenewal.cpp` | Csak committed craft, saját tranzakcióazonosító; sikertelen kísérlet nem teljesítés |
| NPC/dialógus | `CQuestManager::Click/TakeItem`, `quest_input.cpp`, `trigger.cpp` | Új sessionhöz kötött dialogue token, NPC-távolság, választás és node ellenőrzés |
| Terület | Ténylegesen publikált `PositionChangedEvent`, `PositionSync.hpp`, MovementSystem | Csak releváns régióhatár átlépése adjon discoveryt; mozgási tick ne járja végig az összes küldetést |
| Dungeon | `DungeonLifecycleTests`, dungeon rendszerek és külön MP completion horgok | Sikeres run-ID és tagság-pillanatkép; cleanup/despawn/timeout nem bossgyőzelem |
| Puzzle/escort | Nincs általános, a specifikációnak megfelelő esemény/API | Natív szerveroldali állapot és hitelesített eredmény szükséges |
| Timer | `world/event.*`, `questevent.*` | Tartós határidő és generáció, egyedi timer-ID, logout/restart/fail/recovery szabály |

Az `EvEntityDied`, `EvItemPickup`, `EvCharDisconnect` deklarációja nem jelenti,
hogy az összes megfelelő gameplay-ág kibocsátja. Például a keresés az
`EvEntityDied` kibocsátóit elsősorban Lua NPC-/PC-kill API-kban találta;
nem építünk rá automatikusan minden normál ölés számlálását. `EvDungeonEnd`
sem azonos általánosan a sikeres dungeon-teljesítéssel.

A router az aktív objective-eket eseménytípus és target-filter szerint
indexeli. Egy esemény csak a releváns feliratkozásokat érinti. Időzítők
esedékességi sorból futnak, nem per-tick teljes Mission-lista ellenőrzésből.
Az accountszintű szezonális MP és a karakterenkénti új Mission Engine
ugyanazt a gameplay-t külön szabályokkal figyelheti; egyik eseménye vagy
játékszintű sikerjelzése nem számít automatikusan a másik rendszer claimjének.

## 6. Persistence és jutalom: használható API-k, hiányzó szerződés

Tényleges prepared API-k:

- `DBManager::{QueryPrepared,DirectQueryPrepared,ReturnQueryPrepared,
  FuncQueryPrepared,FuncAfterQueryPrepared}` a `GameServer/network/db.h`-ban;
- `CAsyncSQL::{AsyncQueryPrepared,ReturnQueryPrepared,DirectQueryPrepared}`;
- `SqlParam::{UInt,Int,Text,Blob,Null}`, `SQLResult::FetchRow()`.

A változó értékek mindig külön paraméterek. Konfigurálható schema-/table-
azonosítók csak induláskor validált, engedélyezett névkészletből jöhetnek;
adatértéket nem illesztünk SQL-szövegbe.

**Konkrét hiány:** a prepared végrehajtó jelenleg egy resultsetet dolgoz fel,
nem találtam `mysql_stmt_next_result` kezelést. A prepared `CALL` és a
tranzakciós store megvalósítása előtt ehhez ismételt hívásos, több resultsetes
MariaDB integrációs teszt és szükség esetén teljes result-drain kell.
Külön queue-elemként beadott `BEGIN`, `SELECT`, `UPDATE`, `COMMIT` nem elég:
azokat idegen munka nem szakíthatja meg, és reconnect nem folytathat fél
tranzakciót. Vagy egy munkaként kezelt prepared tranzakció, vagy gondosan
tesztelt, bound paraméteres, tranzakciót birtokló stored procedure szükséges.

Új, önálló schema javaslat: `mission_definition_versions`,
`character_mission_instances/objectives/flags/choices`,
`mission_reward_ledger`, `mission_action_outbox`, szükség szerinti
`mission_event_dedup`, és egyszeri importhoz `mission_migration_ledger`.
Ezek tervezett objektumok, nem a meglevő szezonális MP tábláinak átnevezései.

Egy completion tranzakció CAS-sel ellenőrzi a karakterprogress revisiont,
rögzíti a döntést/státuszt, az esemény dedup kulcsát és a jutalomkönyvelést.
A dedup szemantikát eseménytípusonként kell megadni: sessionbeli értesítés és
tartós craft/kill/reward receipt nem kezelhető ugyanazzal a végtelen halmazzal.

Tárgyjutalomhoz a közös InnoDB `item_award` jó, már létező kézbesítési pont.
Az új Mission ledger és az award insert egy tranzakcióban kell hogy commitoljon,
egyedi `(character, mission, completion_instance, reward)` kulccsal.
A meglévő `ItemAwardManager` delivery/claim állapotai miatt a teljes út
crash- és retry-ellenőrzése is kötelező; puszta outbox insert még nem teljes
bizonyíték az egyszeri inventory-hatásra.

XP, pénz, erőforrásfogyasztás és tartós jogok külön figyelmet igényelnek:
`PointSystem::Change` vagy `RemoveSpecifyItemEcs` önmagában nem osztozik a
Mission SQL-tranzakcióján. Az A01 kardjavításnál például a három vas egyszeri
fogyasztását és a jutalmat közös action receipt/tulajdonosi protokollnak kell
védenie, beleértve a DB commit előtti/utáni leállást. Ezt előbb oldjuk meg,
mint hogy a kampány jutalmai élesbe kerülnének. A szezonális MP meglévő
tranzakciós elve tanulság, de nem vesszük át a season/level/track adatmodelljét.

## 7. A jelenlegi questek és Lua migrációja

### Tényleges tartalom és függőségek

A helyi `data/quest/quest_list` **69** fájlt tölt. A távoli futó rendszer
nem közvetlenül a szerkeszthető `data/quest` könyvtárból, hanem a commitolt
`run/quest-reloads/79c335ac78b1a21a4cf43172ab7c2315/data/quest` snapshotból fut.
Mind a 69 forrásfájl SHA-256 értéke egyezik a helyi példánnyal.
Nyolc további Lua library/bootstrap fájl szerepel a helyi questfában.
Ez nem a teljes szerver összes Lua-használata: például `settings.lua`,
`BlueDragon.lua`, `monkey_dungeon.lua`, `oxquiz.lua`, `restart_city_pos.lua`
is létezik a data gyökerében, külön tulajdonossal és migrációs igénnyel.

A jelenlegi futtató:

- `quest_source.*`: quest DSL parser, közvetlen forrásbetöltés; **nem tiszta
  Lua gráfdefiníció**, a `quest/state/when` saját nyelvi réteg;
- `questmanager.*`, `questnpc.*`: NPC/event dispatch és állapotok;
- `quest_execution.cpp`, `questpc.*`: Lua coroutine, wait/select/input/confirm,
  flag, jutalom és timer ownership;
- `quest_runtime.*`, `quest_snapshot.*`, `quest_hotreload.cpp`,
  `quest_reload_control.cpp`: generáció és fagyasztott fájlok, hot reload;
- `ecs/components/quest_components.hpp`: `QuestPCState::unique_ptr<quest::PC>`
  az entitás egyetlen tulajdonosi állapota; a `QuestContext` generációvédett
  entity handle-öket használ;
- `quest_db.cpp`, `Database/ClientManagerPlayer.cpp`: PID-hez tartozó
  `TQuestTable` betöltés és mentés;
- `questlua*.cpp`: sok meglévő játék API, nem mind biztonságos Mission API.

A statikus keresés **119** szerver/tool/test fájlban talált a régi quest
ownershiphoz vagy managerhez kapcsolódást; ez nem 119 törölhető fájl.
Ezek listája is a melléklet része.

### Migrációs mátrix

A `mission_engine_audit_2026-10-24_quests.json` minden aktív fájlhoz tartalmazza
a quest nevét, SHA-t, állapotneveket, event handlereket, API-hívásokat,
felismert flagneveket és előzetes migrációs osztályt. A külön `_quests.md`
fájl ennek olvasható táblázata.

- **A: 18 jelölt** a közös event/condition/dialogue/action gráfra.
- **B: 2 jelölt** gráf + korlátozott Lua action megoldásra.
- **C: 49 jelölt** előbb natív domainképességet vagy külön átvezetést igényel.

Ez konzervatív **lexikális előosztályozás**, nem 69 kész konverzió vagy
bizonyított viselkedési ekvivalencia. Egy A-jelölt is csak a teljes feltételek,
könyvtárhívások és megszakítási utak vizsgálata után válhat scriptmentessé.
A `sash2.lua` például listázott, de jelenleg nincs aktív handlere.

| Reprezentatív forrás | Migrálandó viselkedés | Kötelező regresszió |
|---|---|---|
| `Quest/login/begin_weapons.lua`, `baule_start.lua` | Job/sex/level feltételek, egyszeri tárgyjutalom, fogyasztás | Újrabelépés, duplakattintás, tele inventory, megszakítás |
| `Quest/item/change_yang.lua` | Item ↔ pénz, nyelvi választás | Pontosan egyszeri fogyasztás és pénzváltozás, crash |
| `Quest/npc/darkforest.lua`, `Quest/item/teleportgyuru.quest` | Interact/use, szintellenőrzés, warp | Más map/core, tiltott helyzet, hibás cél |
| `Quest/system/runesys.lua` | Négy állapot, letter/button, tartós unlock és jutalom | `__status` átvezetése, ismételt gomb és logout |
| `Quest/dungeon/monkey_tower_razor93.lua`, `nemere_zone.lua` | Instance flag/timer, kill, emeletlépés, teleport | Utolsó mob, boss, timeout, party tag távozása, újracsatlakozás |
| `Quest/gilda/*`, `Quest/npc/marriage_manage.lua` | Céhlétrehozás/épület/háború és több játékost érintő házasság | Jogosultság, céljátékos kilépése, több fél döntése |
| `Quest/skill/*`, `passive_skills.lua`, `pet_system.lua`, `sash.lua` | Skill/cooldown/alignment/pet/sash natív műveletek | Meglévő szolgáltatásokkal azonos eredmény, duplázás nélkül |
| `Quest/events/*`, worldboss notice-ok | Globális eventflag, spawn és időzítés | Nem játékosból indítható admin művelet, restart és csatornák |

### A flag-adatok különösen fontosak

A vizsgált `player.quest` **52 464** sort és **153** névteret tartalmaz.
Ezek közül **128 névtér neve nincs az aktív 69 quest nevének halmazában**.
Ez nem jelenti, hogy árva vagy törölhető adat: C++-ból használt dungeon- és
PvP-flagek, képzés, horse név, korábbi questekből megmaradt tartós jogok és
globális eseményadatok is vannak köztük.

A migráció ezért két külön feladat:

1. az aktív questek progress-/choice-/timer állapotának átvezetése;
2. minden régi flag tulajdonosának és jelentésének rendezése a megfelelő
   Mission fact vagy megmaradó domain-szolgáltatás felé.

Ismeretlen névtér nem esik ki és nem kap automatikusan nullát.
A `quest_source.cpp::StateId` esetében `start=0`, más állapotnév `StateCRC`-t
kap; az import nem kezelheti a régi `__status` értéket sorszámként.
Az exact régi név/CRC → új stabil mission/node mappinget a tényleges parserrel
és verziózott importtáblával kell előállítani, majd tesztelni. A coroutine
folytatása nem perzisztálható VM-memóriaként: a cutoverhez checkpoint vagy
előre definiált, jutalmat nem ismétlő folytatási/recovery pont kell.

### A biztonságos Lua réteg

A jelenlegi `LuaExecutionBudget` instruction hookot alkalmaz; a VM a standard
könyvtárak szűkített halmazát nyitja meg, több veszélyes OS függvényt töröl.
Ez hasznos alap, de nem teljes új Mission sandbox. Például a jelenlegi
`questlua_game.cpp` exportál `game.mysql_query`-t, más bindingek közvetlen SQL-t
és széles domainmódosítást kínálnak. `lua_open()` alapból `luaL_newstate`,
nem Missionenkénti kvótás allocator; külön memória-/falióra-korlátot nem
igazolt az audit a teljes kódra.

A tervezett `MissionScriptContext` csak ellenőrzött fact/counter olvasást,
lokalizált dialógus-/choice javaslatot és deklaratív action-kérelmet ad.
Nem exportál registryt, nyers pointert, SQL-t, `game.mysql_query`-t,
tetszőleges `command`/warp/spawn/admin API-t. A mutációkat a natív action
szolgáltatás validálja és deduplikálja. A script nem bírálja felül a jutalmat.
Thread-affinitás, instruction/falióra/memória-budget, megszakítás és contextes
hibanapló tartozik hozzá. Natív C-hívás idejét az instruction hook önmagában
nem korlátozza.

A Mission graph és script-ref verzió együtt publikálódik. A futó instance
verzióhoz kötve marad; váltás csak explicit mappinggel. A folytatás serializált
node/dialogue állapotot tart, nem Lua closure-t vagy VM-címet. A régi
generációpinning jó tapasztalat, de nem kell örökösen a régi `quest::PC`-t
futtatni az új motor mögött.

### A régi futtató nyugdíjazási feltételei

Mind a 69 aktív forrás és library-hívásaik lefedése; minden megőrzendő flag
gazdája rendezett; Python/C++/Unity packet- és UI-függőségek kiváltva;
timer- és megszakított dialógusmigráció tesztelve; két snapshoton végzett
próbakonverzió egyező; sem jutalomvesztés, sem duplázás nincs. Ezután kerülhet
ki a régi dispatch, coroutine-, `quest::PC`-, source DSL- és object-függőség.
A Lua 5.5.1 és a nem questtulajdonú Lua domainlogika szükséges részei maradnak.
A konverter offline eszköz, nem állandó compatibility runtime.

## 8. A kampány és a gráf megvalósításának konkrét hiányai

Programmal ellenőrzött: **80 egyedi ID, 10 fő milestone, 64 A–H oldalküldetés,
6 titok, felvonásonként 16 bejegyzés**. A dokumentumban szereplő ID-hivatkozások
között nincs a katalógusból hiányzó azonosító.

Ez még nem teljesíti az elérhetőségvizsgálatot: nincsenek géppel olvasható
node-élek, teljes choice-/flag-kiosztások és minden failure/recovery út.
A minden legális választás utáni főszál-elérhetőség jelenleg **nem bizonyítható**.
Az NPC/item/map ID-ket és boss-mechanikákat nem találtuk ki a címek alapján.

Szükséges tartalmi munka:

- Act I három külön evidence-source kategóriájának konkrét kiosztása;
- M04 valóban két eltérő szövetség, nem két hívás ugyanarra;
- M06 összes engedélyezett kulcs/informátor/mágia/harc/tunnel alternatívája
  külön valid method factet adjon, és két külön módszer legyen beszerezhető;
- M08 téves vádra teljes recovery ág, három önálló evidence group;
- M09 három szerver által kiérdemelt advantage; nem kliens választotta flag;
- M10 legalább egy ténylegesen végigjátszható nem harci befejezés;
- C05 fázisok és C10 encounter-start fact snapshot csak konkrét harci
  adapterrel, nem élő questflag újraolvasásával minden sebzésnél;
- a 11–23 óra és a részfeladatok percei csak kezdeti célok, playtest kell.

A natív feltételek tiszta, determinisztikus függvények. A SEQUENCE/PARALLEL
állapotát nem lehet egyszerű statikus bool-feltételre redukálni.
A kiértékelés lépésszáma korlátos; publishkor reference-/ciklus-/típusellenőrzés,
majd choice-terek és failure/recovery utak vizsgálata szükséges. Beágyazott
subgraph és negatív flagfeltétel miatt az egyszerű DAG-sorrend önmagában nem
bizonyítja, hogy egy kapu később is elérhető marad.

## 9. Javasolt, fájlszintű megvalósítási sorrend

Az alábbi új fájlnevek **javasolt tervezési célok**, nem már meglévő API-k.

| Fázis | Fájlok és kimenet | Kilépési feltétel |
|---|---|---|
| 0. Audit elfogadása | Jelen leltár, hatókör, jogosultságrendezés, branch-terv | Külön jóváhagyás a régi BP kód eltávolítására |
| 1. Előkészítés | Szerver `codex/mission-engine`, Unity `/main/codex-mission-engine`; baseline, DB backup/export/restore eszközterv | A meglévő helyi módosítások megőrizve, import-/rollback-ellenőrzések |
| 2. Régi BP eltávolítás | A 3. fejezet kizárólagos fájljai és közös integrációs blokkjai | GameServer/Database/client build, no BP endpoint/UI, megmaradó MP tesztjei; nincs DB DROP |
| 3. Natív graphmag | `GameServer/mission/{MissionDefinition,MissionValidator,MissionRuntime,MissionConditionEvaluator,MissionEventRouter}.{hpp,cpp}`, `tests/server/MissionCoreTests.cpp` | Version pinning, bounded evaluation, cycles/refs, choice/recovery reachability; játékadatok nélkül is tesztelhető |
| 4. Tartós store és action receipt | `mission/{MissionPersistence,MissionRewardService,MissionActionService}.*`, külön additív `db/migrations/...mission_engine...sql`; szükség szerint `SQL/AsyncSQL.*` | Prepared SQL, CAS, idempotens fogyasztás/jutalom, timeout/crash/reconnect/race tesztek MariaDB-vel |
| 5. ECS és Lua context | `ecs/components/mission_components.hpp`, `mission/{MissionGameplayAdapters,MissionScriptContext}.*`; a felsorolt tényleges gameplay határok | Logout/VID/PID reuse, party/run, duplázás, forged client input, memória-/futásbudget |
| 6. NPC/quest vertical slice | `mission/{MissionDialogue,MissionTimerService}.*`, offline `SRC/Tools/mission_migrate.py`, verziózott mapping/fixtures | Legalább login reward, itemfogyasztás, warp, dialógus, dungeon és tartós unlock reprezentatív konverziója |
| 7. Studio és publish | Új `SRC/Tools/MissionStudio/`, schema/validation CLI, simulator, diff/publish; meglévő csomagkezelő kontrollált bővítése | Ugyanaz a szervervalidator ellenőrzi az exportot; hibás bundle nem válthat aktív verziót |
| 8. Act I + új Unity UI | Verziózott campaign bundle; `Metin2Mission{Client,Journal,Dialogue}` javasolt modulok; közös packetcodec/routing bővítése | 16 ténylegesen játszható küldetés, Story/Branch/Discovery UI; szezonális MP változatlanul elérhető |
| 9. Teljes migráció | További 64 kampányküldetés, a teljes régi questmátrix, domain fact-átvezetés | 80 ID, minden kötelező kapu, korábbi mentések, economy/balance és end-to-end próba |
| 10. Végső quest-retirement | A 7. fejezet régi futtatójának igazoltan kiváltott részei | Egyetlen authoritative questmotor; Lua 5.5 opcionális marad |
| 11. Adatbázis takarítás | Csak már nem használt BP objektumok, jóváhagyott célzott műveletek | Friss backup + restore-próba + entitlement igazolás + külön DROP-jóváhagyás |

A tartós action/jutalom szerződést szándékosan az éles gameplay adapterek
bekötése elé hoznám. Így az első játszható slice sem kap ideiglenes,
duplázásveszélyes jutalmazást. A régi questmotor az utolsó validált konverzióig
működő kiindulási rendszer marad; nincs kettős jóváírás ugyanarra a questre,
és ez nem végleges kétmotoros architektúra. A régi BP ekkor már nincs jelen.

## 10. UI, protokoll, Studio

Az új UI három nézetet ad: fő történet, oldalfamília-előrehaladás és titkok.
A jelenlegi `Metin2QuestPanel/QuestScript/QuestState/QuestTarget*` és a
`Metin2GamePacketCodec` quest packetjei valós függőségek. A régi quest input
29/30/31 és button 66 kezelése még szükséges a meglévő questekhez; nem
törlendő a BattlePass eltávolításával együtt. A későbbi új transportban csak
mission/node/choice hivatkozás és érvényes session token jön a klienstől;
progress, item/count, jutalom vagy teljesítés nem.

A szezonális MP `/missionpass` parancsai és `amp_begin/chunk/end/complete`
üzenetei megmaradnak. A történeti Mission Engine külön szerződést kap,
protokollverzióval és kiosztásvizsgálattal; nem használjuk újra a törölt
BattlePass üzeneteket vagy a szezonális MP namespace-ét.

A Studio külön Qt6 alkalmazás/modul legyen. A meglévő Server Manager
PySide6-alapja hasznos, de nem áll rendelkezésre a specifikációban feltételezett
kész Quest Studio gráfmag. Az első Studio cél egy validálható node/condition/
action editor és simulator; a runtime teljesítményét a C++ modell határozza
meg. A közös formátum verziózott schema, nem a szerkesztő widgetjeinek mentése.

Publish: ellenőrzött jogosultság, hash-elt immutable bundle, validálás,
staging/prepare, majd commit. Aktív instance verzióhoz kötött; in-flight
dialógus és script action nem vált észrevétlenül új definícióra.

## 11. Ellenőrzések és a továbblépés feltétele

Az audit során friss CMake-konfigurálás után sikeresen lefordított célok:
`QuestRuntimeTests`, `LuaSourceTests`, `QuestDataTests`, `AsyncSqlPreparedTests`,
`MissionPassRulesTests`, `ServerConfigTests`.

A következő célzott tesztcsoportok **6/6 sikeresek**:
`quest_runtime`, `lua_sources`, `quest_data`, `async_sql_prepared`,
`mission_pass_rules`, `server_config`.
Naplók: `build/mission-audit-baseline-build.log`,
`build/mission-audit-baseline-tests.log`. Ez a jelenlegi kód célzott baseline-ja;
nem egy még meg nem írt új Mission Engine vagy teljes kliens-player-build tesztje.

Az első eltávolítási milestone-nál kötelező:

- teljes GameServer/Database és Unity fordítás;
- a megmaradó shared tesztek és a szezonális MP protokoll-/jutalomtesztjei;
- teljes BattlePass referencia- és endpoint keresés, dokumentált történeti
  említési kivételekkel; újraimportálás sem hozza vissza a régi UI-t;
- korábban kiadott itemek/jogok sértetlensége, backup és próbavisszaállítás;
- régi BP packet/command hibás bemenetként kezelése, dangling handler nélkül;
- a többi header/pontkód és közös serialized struktúra ellenőrzése.

Az új motor későbbi kapui: duplikált/out-of-order esemény, reconnect,
definíciópinning/mapping, CAS-verseny két core között, commit előtti/utáni
crash, inventory-full, kézbesítési retry, Lua-budget/cancel, recovery és
összes legális choice utáni főút-elérhetőség. Terhelési budgetet mért aktív
játékos/objective/eseményszám alapján állapítunk meg, nem becsült throughputból.

**Most jóváhagyás szükséges a következő fázishoz.** Az átadott specifikáció
Implementation phases / 1 pontja szó szerint ezt írja:
„Stop and request approval before destructive removal.”
A jóváhagyandó következő művelet a **régi BattlePass kódjának és kizárólagos
UI-jának eltávolítása külön ágakon**, a fenti shared/MP védelmekkel.
Ez a jóváhagyás nem foglal magában DB DROP-ot, élő entitlement törlését vagy
az elkészületlen rendszer éles telepítését.
