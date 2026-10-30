# GameServer leállás: BROADCAST_DRIFT-flood és lassú CHARACTER_MANAGER-lebontás

Dátum: 2026-10-30. Alap: `63bb29e1` (`codex/mission-engine`). Helyi csomag: `D:\AurigaGlobal\Server2-Local`.

## Tünet

A `ch1-core2` (~100 ezer betöltött NPC) leállása 12 perc 44 mp volt: a `Destroying CHARACTER_MANAGER`
18:10:58-tól 18:23:42-ig tartott. Közben ~72 ezer `[BROADCAST_DRIFT] ... native=0 legacy=k` sor íródott,
destroyolt karakterenként egy. A supervisor core-onként 60 mp-et vár, ezért a stack nem állt le, és a
Database-t külön kellett leállítani. A syslogban lévő mind a 166 357 drift-sor leállításból származott;
normál játék közben egy sem volt.

## Okok

1. **O(N²) kombat-takarítás.** Az `EntityFactory::Destroy` → `CleanupCombatReferences` minden törlésnél
   végigment az összes `AggroTable`-ön. Minden mob kap ilyet, de az `entries`-be és a `stoneOwner`-be
   semmi nem ír, így a bejárás sosem talált semmit. A logban az entitásonkénti idő ~19,5 ms-ról
   (72k hátralévő) lineárisan esett ~0,3 ms-ra; a 764 mp-ből ~615 mp ez volt.
2. **Broadcast a látótér lebontása után.** A `DestroyCharacter` előbb `ViewCleanup`-ot hív (REMOVE minden
   nézőnek, a natív `ViewerMap` kiürül, a `SpatialEntity` lekerül), utána a `DestroyCharacterStatePre` →
   `ClearSync` → `SetSyncOwner` feltétel nélkül `BroadcastToView`-t hív. A hatótáv-ág (a karakter még a
   sectorban van) mindenkit elért, a natív lista üres volt: hamis drift minden karakterre, és csomag olyan
   klienseknek, amelyek a VID-et már törölték. A log aszinkron, ezért ez főleg zaj volt, nem ez vitte az időt.
3. **Karakterenkénti láthatósági lebontás.** Az 1–2. javítás után az idő lineáris lett (~0,9 ms/karakter,
   ~100k-nál 91 mp). Ennek szinte egésze a `VisibilitySystem::Remove`: 9-sectoros snapshot és szűrés,
   REMOVE minden címzettnek, az élek törlése mindkét oldalon. Mérve, `/Od` (1200 NPC, átlag 235 néző):
   `GetEntitiesInRange` 1,8 ms, `ViewCleanup` 1,4 ms/karakter átlagban.

## Javítás

- `ecs/EntityFactory.cpp`: a halott `AggroTable`-söprés kikerült a `CleanupCombatReferences`-ből. A
  `CombatTarget`-söprés marad, az csak a harcolókon megy végig. Az `ai_components.hpp` megjegyzése szerint
  egy jövőbeli író fordított indexszel takarítson.
- `ecs/NetworkService.hpp` (`BroadcastToView`) és `entity/entity.cpp` (`ViewSystem::PacketView`): ha a
  forrásnak nincs `SpatialEntity`-je (a `ViewCleanup` már lefutott), nincs néző és nincs drift-check, csak a
  forrás saját kliense kaphat csomagot. Ez a legacy `PacketView` szemantikája (üres `m_map_view` + self), és
  ugyanaz a jel, amit a `NetworkSyncSystem::UpdatePacket` már használ. Élő entitásnál nincs változás, a valódi
  drift továbbra is logolódik.
- `ecs/systems/VisibilitySystem.cpp`: `RetireCharacters` (`ViewSystem::RetireAllCharacters`) egyszerre törli
  az összes view-élt, és minden karakterről leveszi a `SpatialEntity`/`ViewActiveTag`/`VisibilityDirty`-t;
  csomagot nem küld, a sector-tagság a `StatePost`-ig marad. A `Remove` korán kilép, ha nincs `SpatialEntity`
  (idempotens, mert a `CanSee` mindkét végen megköveteli). Ugyanazon a mérésen: 1,72 s → 62 ms.
- `entity/char_manager.cpp`: a `Destroy` a sweep előtt `RetireAllCharacters`-t hív, és a végén egy sort ír:
  `<shutdown> CHARACTER_MANAGER destroyed N characters in T ms (view retire …; per character: view …,
  state …, teardown …)`. A fázisok csak a legkülső destroyt mérik.

## Tesztek

- `EntityNetworkDispatchTests::RetiredSourceBroadcasts`: élő forrásnál a hatótáv megkapja a csomagot, és a
  drift jelez; kivont forrásnál a `BroadcastToView`, a `BroadcastEffect` és a `PacketView` sem ér nézőt, a saját
  kliens megkapja, és nincs drift-sor.
- `SpatialLifecycleTests::BulkCharacterRetirement`: a kivonás nem küld csomagot, nem hagy élt, az item
  megtartja a `SpatialEntity`-jét, és a későbbi `Remove`/`ViewCleanup` nem küld újra REMOVE-ot.
- `CharacterManagerTests::ShutdownChecks`: a kivonás egyszer, minden `ViewCleanup` előtt fut, üres
  managernél nem.
- Negatív kontroll: a `BroadcastToView`-őr, illetve a `Remove` korai kilépése nélkül a megfelelő teszt elbukik.
- Teljes CTest (RelWithDebInfo, assertekkel): 139/139.

## Élő mérések (helyi csomag, a ch1-core2 teljes betöltése után)

| Változat | `ch1-core2` leállás | drift-sor | supervisor |
|---|---|---|---|
| `63bb29e1` | 12 perc 44 mp | ~72 ezer | 60 mp után feladta, a Database megmaradt |
| 1–2. javítás | 91 mp | 0 | 60 mp után feladta |
| 1–3. javítás | 15 mp (23:16:14 → 23:16:29) | 0 | a teljes stack 24 mp alatt, maradék nélkül leállt |

A 91 mp-es mérésnél látott 18 mp-es indulási késés nem a GameServerből jön: a `manage.py` sorban állít le, a
`ch1-core2` kérése csak a `ch99-core99` (akkor még 17,5 mp-es) leállása után ment ki. A 60 mp-es keret a core
saját kérésétől számít.

## Nyitott

- `VisibilityService::GetEntitiesInRange`: a `PushUnique` lineáris keresés (O(k²)), és a 9-sectoros snapshot
  minden tagjára több hash-keresés jut; normál játékban is minden broadcastnál fut.
- `CleanupCombatReferences` `CombatTarget`-söprése O(harcolók száma) minden destroynál; a teljes megoldás
  fordított index lenne.
- `CombatSystem::ClearStone` a destroyolt metinkő tagjait `Dead()`-del öli meg leállításkor is (legacy).
- A supervisor sorban állítja le a core-okat (`SRC/Tools/server_package/manage.py`).
