# GameServer: a broadcastok hatótáv-lekérdezése

Dátum: 2026-10-31. Alap: `da94cbce` (`claude/blissful-matsumoto-ef4800`).

## Ok

A `VisibilityService::GetEntitiesInRange` minden broadcastnál fut. Hívói:
- `BroadcastToView`, `Broadcast`;
- `ViewSystem::PacketView`;
- `VisibilitySystem::Remove`.

Hívásonként négy dolgot csinált:
- lemásolta a 9 sector teljes tagságát (`SnapshotAround`), a tárgyakkal együtt;
- minden tagot újraellenőrzött (`SectreeMember`). Ez egy `SectorOf`, benne két `std::map`-keresés, plusz egy hash-keresés;
- tagonként háromszor kérte le a komponens-poolt (`try_get`);
- lineáris kereséssel szűrte a duplikátumot (`PushUnique`, O(k²)).

A mérés `/Od` buildben készült, 1200 karakter és 300 tárgy mellett. A 9 sectorban átlag 951 tag volt, ebből 226 esett hatótávba. Egy hívás 2,35 ms volt, ebből:

| Rész | Idő |
|---|---|
| másolás | 0,21 ms |
| újraellenőrzés | ~0,98 ms |
| szűrés | ~1,0 ms |
| `PushUnique` | ~0,14 ms |

## Javítás

**`world/sectree.h`/`.cpp` – `SECTREE::VisitAround`.** Csak olvasó bejárás:
- ugyanazokat a tagokat adja, ugyanabban a sorrendben, mint a `ForEachAround`;
- nem másol, és nem ellenőriz újra;
- a bejárás a `sectree.cpp`-ben van, tagonként egy sima függvénypointer-hívással. A headerben csak egy vékony template-burok marad, így a `SECTREE`-t dublőrrel helyettesítő tesztek (például az `EntityNetworkDispatchTests`) a bejárást is lecserélhetik.

Miért nincs szükség újraellenőrzésre: egy tag mindig arra a sectorra oldódik fel, amelyik listázza. Az `InsertEntityImpl` a listát és a placementet együtt mozgatja, a kettő között csak egy tárgy `commit` callbackje futhat.

A callback nem módosíthat sector-tagságot. Ezt assert védi: a `sectree.cpp` minden olyan pontján, ahol a tagság változik, ott az `AssertNotWalking`.

**`ecs/services/VisibilityService.hpp` – a `GetEntitiesInRange` most:**
- a `VisitAround`-ot használja;
- a poolokat hívásonként egyszer oldja fel;
- a story-actor szűrőt csak akkor futtatja, ha a forrás actor;
- `push_back`-kel gyűjt. Duplikátum nem keletkezhet, a forrás a végére kerül, ha a bejárás nem adta vissza.

**`GetViewersOfNative` és `GetVisibleEntitiesNative`.** Ezek halmazból gyűjtenek, ezért náluk is kimaradt a `PushUnique`.

## Mérés

`/Od` build, ugyanazon a pályán:

| | Előtte | Utána |
|---|---|---|
| `GetEntitiesInRange` | 2,26 ms | 0,53 ms |
| sector-bejárás, üres callbackkel | 1,14 ms | 0,17 ms |
| `GetViewersOfNative` (drift-check) | 110 µs | 55 µs |

Optimalizált buildben nem mértem.

## Tesztek

A régi algoritmus orákulumként a tesztben maradt. A `SpatialLifecycleTests::RangeQueryMatchesTheSnapshotScan` ezeket ellenőrzi:
- 164 forrás × 3 hatótáv: az eredmény elemenként és sorrendben is egyezik a régivel;
- a story actor csak a gazdáját éri el;
- a sectoron kívüli forrás is megkapja a szomszédait;
- minden sectorra a `VisitAround` ugyanazt adja, mint a `ForEachAround`;
- a natív nézőlista nem ismétel.

Negatív kontroll: elbukik, ha a bejárás nem hagyja ki a saját sectort a szomszédlistából, vagy ha elmarad a forrás hozzáfűzése.

Teljes CTest (RelWithDebInfo, assertekkel): 139/139. A GameServer is fordul.

## Nyitott

- **`VisibilitySystem::Refresh`.** Ugyanezen a pályán 28,8 ms/hívás `/Od`-ben, és minden pozícióváltozásnál fut. Ez most a legdrágább láthatósági út: minden jelöltre mindkét irányban `CanSee` és `Edge`.
- **A `ForEachAround` többi hívója** (harc, skill) továbbra is másol és újraellenőriz. Ott a callback módosíthatja a tagságot, ezért a `VisitAround` csak esetenként vezethető be.
