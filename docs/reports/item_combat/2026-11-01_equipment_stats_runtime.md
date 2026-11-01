# Felszerelésből karakterstat és harci bemenet — C11

2026-11-01. Szerverág `codex/mission-engine`, Unity 6.3 `6000.3.27f1`,
klienság `/main/codex-mission-engine`, **Unity changeset 44**.

## Eredmény

A commitált új felszerelés valódi karakterstatot és harci bemenetet ad.
Katalógus, finomítás, példánybónusz, foglalt kő és ellenállásprofil egy
ellenőrzött vetületbe kerül. A felvétel és levétel teljesen újraszámol;
üres új felszerelésnél nem térnek vissza a régi tárgyak bónuszai.
Az alappontok mentése változatlan, a buffok a szorzó előtti forrást módosítják.
A fegyvertípus a szerveren és a helyi Unity-vezérlőben is typed forrású.

Új owner-only GC231 v1 snapshot és Unity **I → Harci statok** ablak mutatja
a tényleges karakterértékeket, valamint a felszerelés külön összegét.
A felszerelés, stat és codec közös golden fixture-rel ellenőrzött.
Nincs új SQL-migráció vagy vnumadapter.
Részletek és pontos korlátok: [IC-039](../../adr/item_combat_039_equipment_stats.md).

## Ellenőrzések

- Release GameServer és célzott tesztprogramok lefordultak.
- CTest **17/17**: `equipment_stats`, `point_calculation`, `combat_state`,
  `persistent_items`, `typed_inventory_wire`, `typed_ground`,
  `item_persistence_completion`, `item_persistence_mariadb`, `npc_shops`,
  `player_trades`, `crafting`, `shop_lifecycle`, `exchange`, `typed_item_grants`.
- Tiszta felszerelésösszegző **22**, statteszt **6650**, harci regresszió
  **1991** ellenőrzés. Többszöri újraszámolás, bónusz/kő/finomítás,
  tulajdonos/slot/ütközés, túlcsordulás, buff hozzáadása és levétele,
  legacy felszerelés kizárása, PvE/PvP/csatorna/penetráció/crit szűrés.
- A késői védelmi affect és az újraszámolt mágikus védelem eltérését az élő
  próba találta meg. A javítást külön natív add/remove regresszió ellenőrzi.
  A rated melee régi lebegőpontos csonkolása változatlan; a meglévő harci
  oracle ezt is ellenőrzi.
- A bővített inventoryteszt **832341**, térbeli/mozgásteszt **58523**,
  GM-parancsteszt **31564** ellenőrzést futtatott. Külön regresszió védi a
  tárolt legacy felszerelés veszteségmentes visszatöltését, új legacy
  felvételének tiltását, a typed fegyvermódot és az üres/hibás gear fallbackjét.
- Nyitott Unity Editor fordítása és célzott validátorok: equipment **708**,
  crafting **3699**, csere **4553**, NPC-bolt **4044**, inventory **4179**.
  A GC231 összes TCP-vágása és csonkolása, hibás mezők, runtime dispatch,
  statablak, fegyverfeltételek, mount spear és karakterváltás ellenőrzött.
- A statablak PNG-je vizuálisan ellenőrzött: `Logs/typed-equipment-preview.png`.
- Valódi helyi TLS Unity-klienssel **29 ellenőrzés**: typed üres alapállapot,
  fegyver és vért felvétele, támadás/mágikus támadás/támadási sebesség,
  CON/HP/védelem, valódi CG_ATTACK és nagyobb sebzés, inventory UI,
  újrabelépéskor azonos példányok/helyek/pontok, teljes levételkor pontos
  alapállapot, ismételt felvételkor nincs halmozás.

Az aszinkron skill/affect betöltés után a live validátor valódi inventory-
mozgatással kér friss tulajdonosi revíziót. A metin tesztcélpont azonosítóját
a normál karakterpacketből veszi, a sebzést GC135-ből olvassa. Nem állít
helyben optimista sebzést vagy statot. A normál találatokat a kritikus és átütő
procoktól a GC135 flagje különíti el. Az élő körben 2 → 18 volt a megfigyelt
fegyvertelen/felfegyverzett normál sebzés.

## Helyi csomag

`D:\AurigaGlobal\Server2-Local` a végső C11 GameServerrel fut. A #1610
dedikált karakterhez tartozó typed állapot és pozíció minden live reset
előtt mentve van a `run/item-equipment-live/` alatt. Az eredeti bináris,
manifest, katalógus és regen a `run/deployments/equipment-*` alatt megmaradt.
A katalógus pin csak az összes tárolt tulajdonos sikeres validálása után lépett tovább.

A kézi fixture ismét a kereskedő/kovács mellett áll, 1000 Yanggal, +1
fegyverrel, +0 vérttel, 3 itallal és 20 anyaggal. Az ideiglenes metin kikerült
a regenből, az eredeti mapfájl visszaállt. A kézi próba: **I → fegyver/vért
kiválasztása → Felszerelés → Harci statok**, majd levétel és összehasonlítás.
A demofegyver 50 fizikai és 20 mágikus alapot, +25% fizikai támadást és
+25% támadási sebességet ad; finomításkor szintenként +10 fizikai alapot kap.
A demovért 40 fizikai, 30 mágikus védelmet és +4 CON-t ad.

Az Editor, jelenetek és a korábbi `ProjectSettings.asset` módosítás megmaradtak.
A munkaközben megjelent csomagmanifest/lock és AI-beállítások nem részei
ennek a módosításnak. Nem futott FreeBSD- vagy standalone Unity-player build,
teljes production tartalomkonverzió, illetve minden kaszt és skill élő tesztje.

Naplók: `build/equipment-final5-build.log`, `build/equipment-ctest-final.log`,
`build/equipment-live-harness-final2.log`, Unity `METIN2_TYPED_EQUIPMENT_*`.
Újrafuttatás: `tests/server/EquipmentUnityLiveSmoke.py` a saját csomaggal,
Unity-projekttel, privát fixture-rel és `ItemBenchTool` elérési úttal.

A következő item-munka a quest/Mission jutalmak, darabszámfeltételek és
fogyasztás közös bekötése. Ezután a további használatok és szolgáltatások,
teljes tartalomkonverzió és kiadási regresszió következik.
