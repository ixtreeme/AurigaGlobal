# DragonSoul szerveroldali kivezetés — 2026-10-27

Munkapéldány: `D:/AurigaGlobal/LiveWork/AurigaGlobal`, ág:
`codex/mission-engine`. Az előző itemkatalógus-munka külön commitja:
`b0e87969` — `Add isolated typed item catalog and configurable refinement foundation`.
A kivezetés ehhez képest külön változtatáscsomag.

## Elkészült módosítás

A DS manager, a táblabetöltő, az ECS rendszer és komponensek, a quest API,
a parancsok, a packetkezelők és a készlet-/item-/affect-/stat-/mentési bekötések
kikerültek. A DS kövek, kinyerők, töltők és fejlesztőanyagok protóit mind a
Database, mind a GameServer kiszűri. A factory közvetlenül sem készíthet ilyet.
A régi deck/set affectek betöltése, csatolása és hozzáadása tiltott. A DS
készlethelyek és a korábbi felszerelési tartomány olvasási és írási aliasai
lezárva. A Mission snapshot és recovery sem fogadja el a régi DS window-t.

A többi funkció SQL/wire számozása megmaradt; a régi helyek fenntartott értékek.
Az elavult, kizárólag DS-t végrehajtó pozitív tesztek és a RefineWindowTests
target kikerültek, a közös regressziós tesztek megmaradtak és bővültek.

A külön Soul tárgyak, kosztümrúnák, vállkendő és dungeonok megmaradnak.
Két korábbi kapcsoló-összefonódás rendezve:

- A vállkendő enchantja a saját `ENABLE_STOLE_COSTUME` kapcsolóját használja.
- Az `ENABLE_DS_RUNE` valójában a rúnadungeon harci szabályait kapcsolta.
  Ezek változatlan működéssel, `ENABLE_RUNE_DUNGEON` alatt maradtak meg:
  bossfázisok, HP-visszatöltés, RUNE race bit és a 121-es apply bónusz.

A döntés és az új itemmodellre gyakorolt hatása: [IC-010](../adr/item_combat_010_dragonsoul_retirement.md).
A módosított és törölt források, séma és tesztek eredeti blobhashét és helyi
fájlhashét a [manifest](dragonsoul_retirement_manifest_2026-10-27.csv) rögzíti.

## Ellenőrzés

Toolchain: Windows x64, Visual Studio 18 / MSVC 19.51, CMake 4.3.2,
C++23, `RelWithDebInfo`; külön `ENABLE_ASAN=ON` build.

Végső ellenőrzés: 2026-10-27, Europe/Bratislava, 16:49.

| Ellenőrzés | Eredmény |
|---|---|
| `GameServer Database LoginServer ServerTests` teljes build | Sikeres; `verified-build.log` |
| Teljes CTest, `RelWithDebInfo`, két párhuzamos teszt | **95/95 sikeres**, 68,28 másodperc |
| Ebből izolált MariaDB integráció | **6/6 sikeres**, saját tesztadatkönyvtárakkal |
| Érintett és javított fixture-ek külön ASAN buildje | Sikeres |
| Célzott ASAN regresszió | **16/16 sikeres**, 5,51 másodperc |
| `RetiredContentTests` | **328 981 ellenőrzés sikeres** |
| `ItemRuntimeTests` ASAN | **392 ellenőrzés sikeres** |
| `PointCalculationTests` ASAN | **6 222 ellenőrzés sikeres**, a rúnadungeon bónuszával együtt |
| Diff whitespace ellenőrzés | Sikeres, Windows CRLF-et engedő `cr-at-eol` beállítással |
| Launcher / Python / Unity / távoli gép | Változatlan |

A végső forrásból újraépült a szerver; a korábbi, szerkesztés közben futó
buildnaplók nem végső sikerbizonyítékok. A fordító korábban is meglévő
figyelmeztetéseket jelez, például a NAN makró, printf-formátum és régi pointer
cast körül; ez a munka nem általános warning-takarítás.

Helyi diagnosztikai naplók: `build/dragonsoul-retirement-20261027/`.
A végső tesztnaplók: `all-tests.log`, `asan-verified-tests.log`.

Reprodukálás a repository gyökeréből:

```powershell
cmake --build build --config RelWithDebInfo --target GameServer Database LoginServer ServerTests --parallel 4 -- /p:CL_MPCount=4
ctest --test-dir build -C RelWithDebInfo --output-on-failure --parallel 2
cmake --build build-asan --config RelWithDebInfo --target RetiredContentTests QuickslotTests ItemRuntimeTests ItemAttributeTests ItemManagerLifecycleTests AffectLifecycleTests PointCalculationTests ExchangeTests GmCommandTests PlayerCacheSnapshotTests MissionResourceTests CombatStateTests ShopLifecycleTests MotionTimingTests OXEventTests QuestRuntimeTests --parallel 3 -- /p:CL_MPCount=3
ctest --test-dir build-asan -C RelWithDebInfo -R '^(retired_content|item_runtime|quickslots|item_attributes|item_manager_lifecycle|affect_lifecycle|point_calculation|exchange|gm_commands|player_cache_snapshot|mission_resources|combat_state|shop_lifecycle|motion_timing|ox_event|quest_runtime)$' --output-on-failure
```

A teljes `ServerTests` újrafordítás három, a korábbi SQL queue / story
változásoktól elmaradt tesztbekötést is felszínre hozott. A MotionTimingTests
és az OXEventTests megkapta a többi headless SQL-seam által is használt
QueueDrain implementációt; az OX teszt SQL-helyettesítője inicializálja a
kötelező queue-lane mezőt; a QuestRuntimeTests explicit, inaktív story
helyettesítőket ad a Spy/Caravan click belépési pontokhoz. Ezek a javítások
a tesztkörnyezetet igazítják a már meglévő API-khoz, játékszabályt nem módosítanak.

## Adatok és gyakorlati korlát

Meglévő játékadatbázisban nem történt ALTER vagy DELETE. A korábbi DS sorok
inaktívak maradnak, nem állítódnak vissza a karakterbe. A friss séma enumjában
a korábbi window helye `RETIRED_WINDOW_6`. A fizikai adattakarítás külön,
mentéshez kötött művelet lehet.

Régi adatcsomag telepítése előtt a DS tárgyakra mutató drop-, bolti és quest
jutalomhivatkozásokat is ki kell vezetni. A kiszűrt tárgyakat a program nem
helyettesíti más tárggyal. A régi drop-beolvasók továbbra is elutasítják az
ismeretlen prototípust; ez a forrásmódosítás nem egy régi adatcsomag automatikus
konverziója. A helyi szervercsomag és a meglévő játékadatbázis nem lett telepítve
vagy átírva ebben a körben.

Távoli gép és futó szerver nem változott. Launcher-, Python- és Unity-fájl
nem része a módosításnak. Teljes bejelentkezéses/játékbeli kliens-szerver
teszt ebben a körben nem történt; a fordítás és a helyi natív regressziók
bizonyítják az itt ellenőrzött viselkedést.

Az eredeti item/combat specifikáció változatlan történeti forrás. A DS nem
kap új katalógusimplementációt vagy offline migrációt, akkor sem, ha a korábbi
tervben szerepelt. A hivatkozási leltár DS sorai kizárt/történeti állapotúak.
