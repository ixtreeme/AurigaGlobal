# Ellenőrzött dungeonforrások

A `quest/Quest/dungeon` a 2026-10-24-i dungeonvizsgálat 15 javított Lua-forrását
tartalmazza. Az eredeti questazonosítókat és állapotneveket megtartja. A natív
Lua 5.5 betöltő használja őket; `qc` és `object` előállítás nem szükséges.

**A questekkel együtt a jelenlegi forrásból fordított GameServer is szükséges.**
A majom dungeon a `d.count_monster(map_index, vnum)` új, prototípus szerinti
szűrését használja. Régi binárison a második argumentum figyelmen kívül maradna,
ezért önmagában a questek feltöltése és újratöltése nem elegendő.

Új csomag készítésekor a `prepare_server.py` a konverzió után automatikusan
alkalmazza ezeket a javításokat. A `dungeon-fixes.json` az ismert eredeti és a
javított változat SHA-256 értékeit rögzíti. A konvertáló csak az ismert eredeti
forrásokat cseréli, az eltérő, egyedileg szerkesztett fájlokat megőrzi és felsorolja
a `conversion-report.json` `paths.dungeon_fixes.custom` mezőjében. A már javított
forrás változatlan, a hiányzó dungeon nem kerül automatikusan aktiválásra.

Meglévő szerver frissítése:

1. A jelenlegi bináris és a 15 érintett questfájl mentése után állítsd le a game
   core-okat. A folyamatban lévő dungeonmeneteket ez lezárja.
2. Fordítsd le a GameServert a célrendszeren, és telepítsd a szerverkezelő
   bináristelepítési lehetőségével.
3. A `quest/Quest/dungeon` fájljai a szerver `data/quest/Quest/dungeon` mappájába
   kerüljenek. A feltöltés előtti eltéréslistán ellenőrizd az esetleges újabb
   egyedi szerkesztéseket.
4. Futtasd a szerverkezelő fájl-/questellenőrzését, majd indítsd a game core-okat.
5. Végezze el egy játékos a dokumentált dungeonpróbákat, különösen a teljes
   majom menetet, partys visszalépést és a jutalmak átvételét.

Adatbázis-migráció és kliensmódosítás ehhez a javításcsomaghoz nem szükséges.
A források önmagukban nem módosítják a futó helyi vagy távoli szervert.
A részletes audit: `docs/dungeon_audit_2026-10-24.md` a repository gyökerében.
