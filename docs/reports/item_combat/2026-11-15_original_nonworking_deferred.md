# További hibás eredeti sorok félretéve a merge-ig

A felhasználói döntés szerint további **17 forrássor** került a merge után újra ellenőrzendő listára. A katalógus és a konverziós eredmények változatlanok; ez a lista nem kivezetés és nem natív tárgykiadás.

- **50905–50910:** az eredeti receptkezelő az 5. értéket sikeresélynek használja. Mind a hat sorban nulla, öt sorban az eredmény és az összetevők is üresek.
- **50819:** a protó a normál `USE_ABILITY_UP` ágat választja 37-es hatással, amelyhez nincs végrehajtási ág. Az aktív és az archivált szerverforrás is ezt mutatja; a jelenlegi tárgy hatás nélkül fogyna el.
- **22040, 22050:** a 3-as és 4-es talizmánmódhoz nincs kezelő; az eredeti implementáció csak az 1-es és 2-es módot valósítja meg.
- **18900, 18901:** az övfoglalat-kompatibilitás ki van kommentezve, az anyagok egyetlen jelenlegi foglalható kiegészítőhöz sem használhatók a két eredeti kompatibilitási szabály szerint.
- **50821, 50822, 50824–50826, 71157:** a hat blend tárgy hatás-/érték-/időtáblája az aktív adatokból és az ellenőrzött szervermentésekből is hiányzik. A megtalált készletek kizárólag az 51002-es energia-kristály tábláját tartalmazzák.

Minden sor eredeti azonosítója és hash-e megmaradt. A bizonyíték a fájlok útvonalát és SHA-256 hash-ét is tartalmazza. Működő, csak még át nem ültetett funkció nem került ebbe a 17-es csoportba.

Jelenlegi állapot: **559 félretett sor**, további **173 viselkedési/politikai akadály**, összesen továbbra is 732 akadályozott sor. A 9763 konvertált és 491 korábban kivezetett sor, valamint a 6845 natív definíció nem változott. A félretett sorokat az összefésült forrásból kell újraértékelni.

A részletes bizonyíték a mellette található `2026-11-15_original_nonworking_deferred.evidence.json` fájlban, az összesített nyilvántartás a `config/item_combat/full_proto_merge_deferred.json` fájlban található. Natív és Unity kód ebben a nyilvántartási lépésben nem módosult.
