# BGFX raidrendszer átvétele a Mission ágba

2026-10-29. Kizárólag helyi forrás-összeolvasztás és ellenőrzés.

## Forrás és cél

- Célág: `codex/mission-engine`.
- Összeolvasztás előtti HEAD: `e8cf76c9c9bcba378bb0dd369ebbfdc775e987aa`.
- Frissen lekért forrás: `origin/ECS_+_BGFX`,
  `5a7a805a9c7a58d7d619e47f1de20d5e3f769cd7`, Razor: **Raid #1 bossterem**.
- Közös ős: `0e0535ffdf08b1b1a4a0aefd61f87c8f9f4c6f7e`.
- A kiinduló munkafa tiszta volt. Az előző állapot helyi referenciája:
  `codex/mission-engine-before-bgfx-raid-20261029`.

A merge szöveges ütközés nélkül egyesítette a két ág változásait. Átkerült
a raidruntime, a bossvezérlők és katalógusok, a térképadatok, a mozgási/harci/
effektushívások, a routingjavítások, a generátorok és az eredeti dokumentáció.
A Mission domain, az új itemdomain, az offline itemmigrációs eszközök és a
DB-sémák nem változtak az összeolvasztás előtti állapothoz képest.
A kivezetett DragonSoul, offline shop és régi BattlePass nem került vissza.

## Az integrációhoz szükséges kiegészítések

- Az öt izolált ECS-teszt explicit, aktív raid nélküli teszthelyettesítőt kapott
  (`NoActiveRaid.cpp`). Ez kizárólag tesztcélokba kerül; a GameServer az eredeti
  raidruntime-ot linkeli. A bossvezérlőket külön ICC domain/runtime tesztek fedik.
- A konfigurációteszt ellenőrzi a teljes 64 mapos listát és a 65. elem elutasítását,
  a korábbi konfiguráció megőrzését és a titkos értékek nélküli diagnosztikát.
- A routingkapacitás túllépése konkrét hibaüzenetet ad. A korábbi dobott kivételt
  az általános típushiba-kezelő elfedte.
- A spell-fejléc elavult forráshashe frissült a mellékelt JSON-ra. A mechanikai
  rekordok változatlanok. A forrás és a generált fejléc LF sorvégű; a generátor
  Windows alatt is LF-et ír. A creature- és spell-adatok újragenerálva egyeznek.
- Egy fölösleges záró üres sor eltávolításával a whitespace-ellenőrzés is tiszta.

## Friss helyi ellenőrzés

Windows x64, C++23, CMake 4.3.2, MSVC 19.51.36248.0,
`RelWithDebInfo` konfiguráció.

| Ellenőrzés | Eredmény |
|---|---|
| GameServer, Database, LoginServer, ServerTests fordítása/linkelése; ServerPackageCheck is felépül | Sikeres |
| Teljes normál CTest, az item-, Mission-, SQL-, lifecycle- és raidtesztekkel | **127/127** |
| Raid/ICC/routing és szerverkonfiguráció AddressSanitizer alatt | **13/13** |
| ServerConfigToolsTests a friss natív binárisokkal | **9/9** |
| Generált creature- és spell-katalógus reprodukálása | **435 / 1555 rekord egyezik** |
| Index whitespace-ellenőrzése | Sikeres |

A konfigurációs diagnosztika javítása után mindkét tesztváltozat újrafordult.
A normál buildben a fordítás közbeni forrásváltoztatásból visszamaradt objektumot
is újrafordítottuk; a táblázat a végső, teljesen sikeres futást mutatja.
Helyi naplók a figyelmen kívül hagyott `build/` mappában:
`bgfx-raid-merge-build-verified.log`, `bgfx-raid-merge-ctest-verified.log`,
`bgfx-raid-merge-asan-ctest-final.log`, `bgfx-raid-merge-native-config.log`.

## Telepítési és kapacitási határ

Nem történt push, Unity-módosítás, telepítés, aktív helyi adatbázis-módosítás vagy
távoli szerverhozzáférés. A mellékelt korábbi telepítési dokumentumok történeti
állításai nem ennek az ellenőrzésnek az eredményei.

A routingcsomag 32 helyett 64 mapot hordoz: későbbi telepítéskor a GameServer,
Database, LoginServer és ServerPackageCheck együtt frissítendő, a régi és új
szerverbinárisok keverése kerülendő. Ezt ez a merge nem telepíti automatikusan.

A raid továbbra is az átvett GM-vezetett, első bossos tesztkonfigurációt használja:
16 példány/folyamat, 10/25 fős módok. A 12–16 fős balance, a példányelosztás/AOI
optimalizálása és a valódi 500–600 klienses FreeBSD-terheléspróba külön munka;
ez a sikeres fordítás és tesztfutás nem kapacitásigazolás.
