# Gyógynövénygyűjtés

A szerver tizenkét növényfajt hoz létre a `metin2_map_main_razor93` térképen. A játékos felszereli a Metszőollót, majd egy növényre kattint. Egy növényt egyszerre egy játékos szedhet. A növény kimerülését és újratermelődését a szerver kezeli.

Minden érvényes próbálkozás külön dob a gyógynövényre és a gyakorlásra. A jutalom a földre kerül 30 másodperces tulajdonosi védelemmel. A metszőolló pontja és szintje ugyanúgy a típusos példányon tárolódik, mint a [Favágó baltáé](woodcutting.md).

## Beállítások és jóváhagyás

- Az eszköz profilja a [gyűjtőeszközök adatfájljában](../config/item_combat/field_gathering_tools.json) található.
- A [világbeállítás mintájában](../config/herbalism.json.example) tizenkét típusos jutalom szerepel a növényfajok sorrendjében. Az engedélyezett konfiguráció a szerver adatkönyvtárában `herbalism.json`.
- A [fa- és növény-NPC-k migrációja](../db/migrations/2026-11-23_typed_field_gathering_npcs.sql) kizárólag NPC-prototípusokat rögzít.

A megszerzett pontok és a kovácsnál elérhető fejlesztés a típusos leltár **Eszköz** ablakában jelennek meg. Az előnézethez vedd le az ollót és állj közel a kovácshoz.

A Unity a típusos felszerelési állapotból kapja meg az olló modelljét és a támadás tiltását. Az olló az adott karakterfajhoz beállított kézbe kerül. A növények, az állapotjelzések és a megközelítés a meglévő világmegjelenítésre épülnek.
