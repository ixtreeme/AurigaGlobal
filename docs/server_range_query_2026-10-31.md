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

## 2. kör: `VisibilitySystem::Refresh` (2026-10-31)

A `Refresh` minden pozícióváltozásnál fut. A 9 sector minden tagjára, átlag 951-re, mindkét irányban `CanSee`-t,
`Edge`-et és `Matches`-t futtatott, pedig csak átlag 226 volt hatótávon belül. Hívásonként 7 666 `SectorOf` jött ki,
vagyis két-két `std::map`-keresés.

**Javítás** (`ecs/systems/VisibilitySystem.cpp`):
- A távolsági szabály külön függvénybe került (`Distance`, `PairReach`). A `CanSee` és az új szűrő ugyanezt használja.
  Az alapeset `VIEW_RANGE + VIEW_BONUS_RANGE`, a nyomozós és a karavános pár 10 000.
- `OutOfReach` kihagy egy jelöltet, ha egyszerre igaz rá, hogy
  - egyik irányban sincs látótér-éle (mindkét fél `ViewMap`/`ViewerMap`-jét nézi);
  - nem épület;
  - messzebb van, mint amit a `CanSee` bármelyik irányban megenged.

  Ilyenkor mindkét `CanSee` hamis, és mindkét `Edge(false)` üres művelet. A `ViewAgeMap.ageByEntity`-be sehol nem
  kerül semmi, így annak törlése sem marad el.
- A gyűjtés csak olvas, ezért `VisitAround`-dal megy, másolat nélkül.

**Mérés** (`/Od`, ugyanaz az 1200 karakteres pálya):

| | Előtte | Utána |
|---|---|---|
| `Refresh` | 28,9 ms | 13,1 ms |
| `SectorOf` egy `Refresh`-ben | 7 666 | 2 692 |

**Teszt:** `SpatialLifecycleTests::RefreshKeepsEveryPairExact`.
- 150 entitás, három mozgatási kör után minden pár pontosan a láthatósági szabály szerint áll.
- Egy távoli (11 900), csak a túloldalon nyilvántartott fél élt a `Refresh` lebont, és küld rá REMOVE-ot.
- Negatív kontroll:
  - a túloldali él-ellenőrzés nélkül a második rész elbukik;
  - rossz küszöbbel az első.

## Nyitott

- **`VisibilitySystem::Refresh` maradék költsége.** A hatótávon belüli jelöltenkénti munka (kétszer `CanSee`, `Edge`
  és `Matches`) maradt; ebből 2,2 ms a `SectorOf` két `std::map`-keresése. Gyorsabb `SECTREE_MANAGER::Get`
  (például `unordered_map` vagy sűrű rács) minden láthatósági és harci útnak segítene.
- **A `ForEachAround` többi hívója** (harc, skill) továbbra is másol és újraellenőriz. Ott a callback módosíthatja a tagságot, ezért a `VisitAround` csak esetenként vezethető be.
