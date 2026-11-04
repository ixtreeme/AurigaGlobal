# ADR IC-057 — Gyűjtőhelyek szerkesztése a szerverkezelőben

2026-11-04, C25d. Előzmény: [IC-056](item_combat_056_free_water_fishing.md).

A halászat és a többi gyűjtőeszköz helyei a szerverkezelőben szerkeszthetők.
A **Tárgykatalógus → Gyűjtőhelyek** gomb az **Adatfájlok** meglévő
szerkesztőjében nyitja meg a `data/typed_gathering.json` fájlt. A generikus
JSON-fa helyett külön űrlap kezeli a helyeket, eszközfajtát, halállományt,
esélyeket és a súlyozott jutalomsorokat. A forráskódnézet továbbra is elérhető.

## Szabály és szerkesztés

NPC-s helyhez valódi VNUM szükséges. A szabadvízi mód botot, első vagy második
halállományt és legalább öt másodpercet állít be, a dokumentumot v2-re emeli,
és `race: 0` értéket használ. Nem horgászbothoz nincs halállomány vagy
csalibónusz. A százalékos és másodperces mezők pontosan bázispontokra és
ezredmásodpercekre váltanak; a 32 bites unsigned azonosítók sem csonkolódnak.

Az új helyhez a tartalomkészítő választ explicit kategória/helyi-ID jutalmat.
Nincs automatikusan kiválasztott tárgy. A hibás, ismeretlen vagy nem
megjeleníthető forrásértékek nem tűnnek el és nem szorulnak csendben az űrlap
határértékei közé: a forráskódnézetben javíthatók. A támogatott v1/v2 forrás
oda-vissza alakítható. A nem mentett változásokat a meglévő szerkesztő őrzi.

## Egyetlen natív ellenőrzés

A `ParseGatheringSites` a core indulásánál használt JSON-parsert közös
item-domain modulba emeli. Ellenőrzi a sémaverziót, mezőket és típusokat,
ismétlődő JSON-kulcsokat, számhatárokat, méretkorlátot, egyedi helyeket és a
jutalmak valódi, aktív, tartós példányként támogatott katalógushivatkozásait.
A szemantika a meglévő `ValidateGatheringSites` függvényen marad.

A `CatalogEditorTool validate_gathering` offline művelet a megadott
katalógus-snapshotot és nyers JSON-t ellenőrzi. Nem nyit SQL-kapcsolatot és
nem ír adatot. A manager mentése friss helyi katalógus-snapshotot olvas,
majd ezzel hívja a natív ellenőrzőt. Nem helyettesíti ezt puszta JSON-szintaxis-
vagy Python-ellenőrzéssel. A távoli támogatás hiányát kifejezetten jelzi.

A meglévő fájlmentési út védi a szervert futás közbeni írástól, ellenőrzi a
megnyitott fájl hash-ét, menti az eredeti bájtokat, és atomi fájlcserét végez.
A játékszervernek leállítva, a csomag saját helyi MariaDB-jének elindítva kell
lennie. A világadat újraindításkor töltődik be; nincs új hotreload.

## Csomagolás

A natív item-segédek MariaDB- és zlib-DLL-je saját végrehajthatóik mellé
kerül a `validators` könyvtárba. A csomagkészítő mindkét segédet ténylegesen
elindítja semleges munkakönyvtárból, SQL nélkül. Ez megfogja a hiányzó
loaderfüggőségeket; nem elegendő csak a fő Qt-ablak indulását ellenőrizni.
A Python és a natív segédek OpenSSL-párjai továbbra is külön maradnak.

Nincs új packet, Unity-kód, SQL-migráció vagy új játékszabály. A C25c-2
cs67 kliens marad a párja; a legacy inventory átállítása továbbra is vár.
Ellenőrzések és korlátok: [C25d jelentés](../reports/item_combat/2026-11-04_gathering_rules_editor.md).
