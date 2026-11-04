# ADR IC-051 — Könyvsegédek a szerveren és Unityben

2026-11-04, C23. Előzmény: [IC-050](item_combat_050_skill_books_runtime.md).

A könyvbónuszt és a várakozás elhagyását adó segédtárgy két külön töltetkészletet
növel. Fogyasztható tárgyak, saját `BookAidProfile` profillal; nincs vnumhoz vagy
legacy affect-azonosítóhoz kötött kivétel. A profil a fajtát, felhasznált darabot,
jóváírt töltetet és tárolási korlátot tartalmazza. A teljes jóváírásnak el kell
férnie: részleges jóváírás, csonkolás és túlcsordulás nem megengedett.

## Katalógus és szerkesztő

A `032_book_aids.sql` a katalógus formátumát v26-ra emeli. Új
`book_aid_profile` tábla és nullable `item_consumable.book_aid_profile_id`
hivatkozás; a fogyaszthatónak továbbra is pontosan egy viselkedése lehet.
A profilok és a hivatkozás a kanonikus hash, SQL snapshot, validátor és natív
katalógusszerkesztő részei. A kiadott v25 katalógushoz mentés utáni, leállított
szerveres migráció szükséges; a régebbi szerver nem olvassa v26-ként.

Szerkesztő: **Fogyasztható típusa → Könyvbónusz / várakozáselhagyás**.
A szerkesztés/másolás megőrzi a közös profilok másoláskori leválasztását.
Típusváltáskor az inaktív mezők nem kerülnek a mentendő profilba.

## Használat és mentés

Szerveroldali előnézet és külön megerősítés. A szint, kaszt, nem, életállapot,
átváltozás, harci szabály és aktuális műveleti zárolások ellenőrzöttek.
Csak saját, normál inventoryban levő, érvényes példány használható.

Egy ItemCommit fogyasztja a forrást és írja a BOK1 learning állapotot,
owner-fence-et és receiptet. A töltetekhez nem szükséges új player-sidecar.
A karakter-checkpoint az aktuális élő XP-/pont-/skillállapotból készül;
a régi sidecarban tárolt checkpoint nem írhatja vissza az időközi fejlődést.
A C22 függő műveleti és login-sorompója a segédtárgyra is érvényes.
Megváltozott forrás, karakter vagy learning-revízió érvényteleníti az előnézetet.
Ugyanaz a megerősítés vagy receipt nem írhat jóvá még egyszer.

A könyv profilja határozza meg a bónuszesélyt és a felhasználást. A segédtárgy
nem tesz minden könyvet garantálttá. A bónusztöltetet csak az ezt engedő könyv,
a várakozáselhagyó töltetet csak meglévő várakozás és engedő könyv használja fel.
Elfogadott sikertelen olvasás is fogyaszthat töltetet; elutasított használat nem.
Kilépéskor a töltetek megmaradnak. A legacy könyv-affectek konverziója külön,
továbbra is nyitott migrációs munka.

## Protokoll és Unity

CG244 változatlan. GC244 v1 továbbra is a könyvformátum. Segédtárgynál v2:
5-ös kategória, 4/5 művelet, 6/7 eredmény, 21-es új hiba (töltetkorlát),
majd a korábbi fix rész után két u32: jóváírás és tárolási korlát.
A skill-/olvasás-/XP-költség-/dobásmezők a segédtárgy idézetében nullák.
GC227 v2 Book action a megfelelő fogyaszthatókra is érvényes.

Unity: **külön típusos leltár → segédtárgy → Könyvsegéd → Olvasás áttekintése
→ Használat megerősítése**. A felület pontosan mutatja a jóváírást, aktuális
készletet, korlátot és eredményt; a forrás helyi levonását nem szimulálja.
Az utolsó darab elfogyása után az elmentett eredmény olvasható marad.
A normál legacy leltár átállítása a felhasználó kérésére továbbra is vár.

Ellenőrzések és ismert kiadási korlátok:
[C23 jelentés](../reports/item_combat/2026-11-04_book_aids_runtime.md).
