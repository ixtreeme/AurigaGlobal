# Későbbi irány: CH-szintű térképek és IxtreemeWorld-féle zónaszimuláció

Dátum: 2026-11-16.
Állapot: a felhasználóval egyeztetett célirány, későbbre eltéve.
Ez nem megvalósított funkció és nem most indítandó fejlesztési feladat.

## A felhasználó által pontosított cél

A régi map–core felosztás helyett a térképek a CH-hoz tartozzanak.
A felhasználó indoklása szerint a korábbi külön core-ok elsődlegesen a régi
32 mapindexes limit megkerülését szolgálták: ugyanahhoz a CH-hoz több
GameServer-core csatlakozott.

Az egyeztetett célfelépítés:

**CH-nként egy GameServer-folyamat → a CH térképei → dinamikus szimulációs
zónák → közös worker-pool.**

- A kezelőben a CH térképei és a rendelkezésre álló feldolgozási kapacitás
  szerepeljen; kézi map–core hozzárendelésre ne legyen szükség.
- Egy térkép több szimulációs zónára osztható, ezért ugyanazon térkép
  különböző részei is párhuzamosan dolgozhatók fel.
- A terhelésmérés vezérelje a zónák felosztását, összevonását és ütemezését.
- Az entitások zónák közötti átadása és a határsáv láthatósága tartsa meg
  a játékos számára a közös CH/térkép folytonosságát.
- A külön GameServer-core-ok maplimit miatti szervezési szerepe így
  megszűnhet. A processzek közötti elosztás nem feltétele ennek az első célmodellnek.

A cél nem pusztán teljes térképek automatikus kiosztása a jelenlegi core-ok
között. A korábbi beszélgetésben ez túl szűk értelmezés volt; a felhasználó
kifejezetten a saját IxtreemeWorld-rendszerét jelölte meg mintának.

## Azonosított referencia és jelenlegi határai

Referencia projekt: `D:/IxtreemeWorld`.

A forrásban áttekintett kapcsolódó részek:

- `gameserver/apps/gameserver/src/world/zone/ZoneWorkerPool.{h,cpp}`:
  zónatickek közös worker-poolja, egy folyamaton belüli szálakkal.
- `world/zone/ZoneScheduler.h`, `world/partition/ZoneLoadMonitor.{h,cpp}`:
  ütemezés, terhelési ablakok, tickidők és felosztási/összevonási feltételek.
- `world/partition/ZonePartition.h`, `world/zone/ZoneManager.h`:
  dinamikus partíciók, split/merge tervezés és véglegesítés.
- `world/zone/ZoneOwnership.h`, `world/OwnerMap.h`, `world/migration/`:
  írási tulajdonjog, entitásirányítás és átadás.
- `world/distributed/MigrationTransport.h`, `WorldMessageRouter.h`:
  a tényleges processzek közötti szállítás még előkészített; a megtekintett
  kódban `RemoteNotSupported` és benchmarkos `EmulatedLoopback` szerepel.
- `docs/adaptive-simulation-fabric.md`, `docs/architecture.md`, valamint
  a map-layer riportok adják a további háttéranyagot. A történeti tervek
  és tesztriportok állításait a folytatáskor a tényleges aktuális kóddal
  és új mérésekkel kell összevetni.

Ez a kör forrásolvasás volt; az IxtreemeWorld tesztjeit ekkor nem futtattuk újra.

## Auriga kiinduló állapota

- A jelenlegi `SRC/Server/common/CommonDefines.h` értéke már
  `MAP_ALLOW_LIMIT = 64`. Ez továbbra is fix protokollkapacitás.
- A `SRC/Server/common/tables.h` `TPacketGDSetup` és `TMapLocation`
  struktúrái, illetve a Database peer és a GameServer dekódolása
  fix méretű maplistára épülnek. Ezeket összehangolt, változó hosszúságú,
  explicit korlátokkal ellenőrzött mapregisztrációra kell majd átállítani.
- A `SRC/Server/GameServer/ecs/Registry.hpp` egy globális EnTT registryt
  deklarál, amelyet csak a fő játékszál érhet el. Az IxtreemeWorld zónánkénti
  Flecs világainak megoldása adaptálást igényel; a worker-pool önmagában
  nem oldja meg a tulajdonkezelést és a párhuzamos hozzáférést.
- A nemrég beépített CH/core-kezelő a jelenlegi architektúra kezelője.
  A zónás átállásig használható; nem jelenti azt, hogy a végleges modellben
  is kötelezően fenn kell tartani a kézi core-szervezést.

## Amikor ismét releváns lesz

Az átállást CH-szintű konfigurációként, térképregisztrációként és zónás
szimulációként együtt kell megtervezni. Része az entitástulajdonjog,
határsáv-láthatóság, üzenetátadás és a gameplay-állapot megőrzése
(például harc, quest-időzítők, petek, tárgyak és dungeonok).

A CH99 későbbi szerepe, a konkrét zónaméretek, terhelési küszöbök,
worker-keretek és átállási sorrend még nincsenek véglegesítve.
A szerverkezelő végleges felülete CH- és kapacitáskezelést mutasson.
Az érintett szerverprotokollokat és Unity-viselkedést együtt kell ellenőrizni,
natív tesztekkel, Unity-fordítással és valódi helyi játékfolyammal.

A felhasználó kérése: ezt az információt most őrizzük meg, és akkor vegyük
elő, amikor a projekt további munkájához relevánssá válik.
