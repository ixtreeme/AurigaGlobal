# ADR IC-001 — Hatókör és koherens item/combat release

Dátum: 2026-10-27. Állapot: **elfogadott felhasználói/specifikációs korlátok; a megvalósítás nem kész**.

## Kontextus

A felhasználó a kategóriás itemmodellt és determinisztikus harcrendszert a Mission Engine kompletizálása elé sorolta. A teljes v1.2 specifikáció forrása a `01a17a65-f0b2-76d3-a4bb-8256d0aacee7` munkamenet; változatlan repositorymásolata a `docs/plans/` alatt van. A kiinduló HEAD `ac4a7cf3`.

## Döntés

- A migráció P0 auditból indul. A régi BattlePass és offline shop kivezetése érvényben marad; nem készül hozzájuk új modell vagy runtime-integráció.
- A megmaradó online boltok, NPC shop, trade, szezonális Mission Pass, Lua questek, C++ dungeonok és Spy/Caravan küldetések funkcióit az új szerződésekre kell portolni.
- Nincs runtime vnum adapter, kompatibilitási proto, dual write, két harcmotor, régi motorra visszakapcsoló feature flag vagy éles shadow-evaluation.
- Az új domain/kernel/tool tesztcélok izoláltan építhetők fejlesztés közben. Félkész modellből nem készül támogatott játék-runtime release. Ez nem engedély a régi és új modell production összekötésére.
- A régi formátumot csak a külön offline migrációs termék ismerheti; GameServer, Database és kliens nem linkelheti.
- Ebben a munkában helyi teszt és fejlesztés történik. Távoli módosítás és éles cutover nincs engedélyezve. A kiadás önálló, későbbi művelet.

## Következmény és nyitott pont

A régi adat-/code-path törlése csak a megmaradó művelet közvetlen új implementációjával és megfelelő release-bizonyítékkal együtt elfogadható. Az offline-shop archívumok esetleges későbbi adatkezelése nem része az új itemmodellnek. A végleges balance, kategóriaregiszter, teljes forrásleltár és release manifest még nincs elfogadva/elkészítve.
