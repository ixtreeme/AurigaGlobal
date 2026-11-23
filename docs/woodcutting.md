# Favágás

A szerver önálló fenyőhelyeket hoz létre a `metin2_map_main_razor93` térképen. A játékos felszereli a Favágó baltát, majd egy fenyőre kattint. Egy fán egyszerre egy játékos dolgozhat. A szerver időzíti az ütéseket, követi a fa állapotát és megállítja a munkát elmozduláskor, eszközcserénél vagy más érvénytelen állapotnál.

Az eszköz típusa, szintje és gyakorlópontja a típusos példányból érkezik. A jutalom földi tárgy, 30 másodperces tulajdonosi védelemmel. Ha az ütés gyakorlópontot ad, a szerver a mentés sikeréig visszatartja a földi jutalmat.

## Beállítások és jóváhagyás

- Az eszköz profilja a [gyűjtőeszközök adatfájljában](../config/item_combat/field_gathering_tools.json) található.
- A [világbeállítás mintája](../config/woodcutting.json.example) írja le a fák számát, az ütemezést és a típusos farönk jutalmát. Az engedélyezett konfiguráció a szerver adatkönyvtárában `woodcutting.json`.
- A [fa- és növény-NPC-k migrációja](../db/migrations/2026-11-23_typed_field_gathering_npcs.sql) kizárólag NPC-prototípusokat rögzít.
- A `prepare_field_gathering.py` a két eszközt és a kovács kínálatát jóváhagyásra váró tervezetként tudja előkészíteni. Az NPC-migrációt és az engedélyező adatfájlokat az aktiváláskor kell telepíteni.

A pontok a típusos leltár **Eszköz** ablakában láthatók. Ha a balta elérte a szükséges pontszámot, vedd le, menj a kovácshoz, majd az ablakban nézd át és erősítsd meg a fejlesztést. A jóváhagyásra előkészített profil tíz fokozatot, 20% gyakorlási esélyt, 80% fejlesztési esélyt és kudarc esetére 10% pontvesztést tartalmaz.

A Unity a szervertől kapott eszköztípus alapján kapcsolja fel a balta modelljét. A fa dőlése, a későn érkező nézők állapota és az ütések vizuális jelzése a világállapotból érkezik.
