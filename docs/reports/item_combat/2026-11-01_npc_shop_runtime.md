# NPC-bolt: szerver és Unity — C8

2026-11-01. Szerverág `codex/mission-engine`, Unity 6.3
`6000.3.27f1`, klienság `/main/codex-mission-engine`, **Unity changeset 41**.

## Elkészült működés

- ItemKey-alapú, induláskor ellenőrzött NPC-kínálat: `data/typed_shops.json`.
- Vásárlás csomagszámmal, a katalógus darabárán; halomfeltöltés és teljes
  lábnyomos szabadhelykeresés. A pénz és a tárgy egy SQL-tranzakció.
- Teljes vagy részhalom eladása. A maradék az eredeti azonosítón marad;
  teljes eladásnál a teljes példányállapotot őrző tombstone keletkezik.
- Tulajdonos-, munkamenet- és revízióvédelem, túlcsordulás-ellenőrzés,
  NPC-/térkép-/távolságellenőrzés, gazdasági ablakok és Mission-belépésvédelem.
- Új bolttoken nyitáskor, egymás utáni kérésazonosítók. Azonos utolsó kérés
  ismétlése nem ír; más paraméterű vagy régi kérés elavult válasz.
- `CG_TYPED_SHOP` 229, `GC_TYPED_SHOP` 228 v1; 64 bites példányazonosítók,
  szerveres árak és beágyazott katalógusrészlet, szigorú Unity-dekódoló.
- Unity vásárlási és eladási lista, mennyiség, összegzés, megerősítés,
  függőállapot és magyar hibák. Nincs optimista tárgy- vagy pénzmódosítás.
- Normál NPC-boltnyitás és **NPC kijelölése → I → NPC-bolt** útvonal.
  Távolodáskor és karakterváltáskor a bolt bezáródik.

A részletes tartalmi és protokollszabályok az
[IC-036-ban](../../adr/item_combat_036_npc_shops.md) találhatók.

## Ellenőrzés

**Natív Release build sikeres. CTest 6/6:** `npc_shops`, `persistent_items`,
`typed_inventory_wire`, `item_persistence_completion`,
`item_persistence_mariadb`, `shop_lifecycle`.

Az új tiszta boltteszt 27 ellenőrzés: összeg/darabszám, meglévő halom és új
halom, pénzhiány, teljes leltár, részleges és teljes eladás, példányállapot,
felszerelés, tiltás, kivezetés, felső pénzhatár, rövid/hosszú UTF-8 boltnév
és golden csomag. A saját
MariaDB-s kör valós vásárlást/eladást, újratöltést, ismételt nyugtát,
pénztárcaütközéskor atomi visszagörgetést és belépési kerítést ellenőriz.
A meglévő legacy bolti életcikluspróbák is sikeresek.

**Unity fordítás és két célzott validátor sikeres**, a nyitott Editorban:
új NPC-bolt **4044**, meglévő típusos inventory **4179** ellenőrzés.
Lefedés: közös natív golden, minden TCP-vágás, csonkolások, pontos CG-bájtok,
megerősítések, 64 bites eladás, részhalom, függőállapot és karakterváltás.
A boltablak képe: `D:\Auriga-Unity6\SRC\AurigaUnity\Logs\typed-shop-preview.png`.
A teljes korábbi hatos Unity-csomagot ebben a körben nem futtattuk újra.

**Valódi helyi GameServer + Unity TLS-kliens: 79 sikeres ellenőrzés.**

1. Három kétadagos anyagcsomag vásárlása: 18 Yang, 6 darab.
2. Ugyanaz a vételi kérés újraküldve: nincs újabb levonás vagy tárgy.
3. Két anyag eladása: az eredeti példányon 4 marad, +2 Yang. Ismétlésre nincs
   újabb jóváírás. Túl nagy darabszám, idegen azonosító és hibás ajánlat nem ír.
4. Fegyvervásárlás, felszerelés; a felszerelt tárgy eladása elutasított.
   Levétel, teljes eladás és ismételt eladás: pontosan egyszer +50 Yang.
5. Újranyitás új tokent ad; a régi token nem költhet. Újrabelépéskor az
   egy eredeti anyagpéldány, a 4 darab és a 934 Yang megmarad.
6. Nulla Yangból vásárlás: nincs létrehozott tárgy.
7. Valóban tele leltár: 180 teljes halom, 36000 anyag. A vásárlások nem
   vonnak le. Egy egység eladása után a kétadagos csomag továbbra sem fér el;
   nincs részleges halomfeltöltés vagy levonás. Végső SQL: 180 példány,
   35999 egység, 1001 Yang.

A tele leltáras fixture készítésekor a próbapad egy kérésben egy halmot ad;
a harness ezért a visszaadott tényleges leltárméret minden celláját külön
tölti fel. A sikeres teljes kör ezt a javított harness-t használta.

## Helyi csomag és kézi próba

`D:\AurigaGlobal\Server2-Local` a friss GameServerrel fut az 1-es csatornán,
a 41-es főtérkép core-ja a 30078-as porton. A #9003-as kereskedő külön helyi
demo kínálata: fegyver 1/2, ital 5/1, anyag 6/1. A korábbi katalógusról
pillanatkép készült a `run/catalog-editor/typed-items-demo-20261101T154343`
könyvtárban; a korábbi GameServer a `run/deployments/npc-shops-20261101` alatt
található. A tesztkarakterek korábbi állapotai: `run/item-shop-live/*/before.json`.

A #1610-es dedikált karakter a kereskedő közelében áll, `(986600, 269100)`.
A negatív tesztek után kézi próbára előkészítettük: **1000 Yang**, egy fegyver,
3 ital és 4 anyag, sok szabad hellyel. A privát fixture az ignorált szerveres
`build/npc-shop-live.private.json`; belépési adat nem került a repóba.

Az élő próba újrafuttatható a `NpcShopUnityLiveSmoke.py` segítségével.
A nyitott Editorhoz tartozó opt-in bridge ideiglenes jelenetet használ,
megvárja az edit módot, frissíti az asseteket, és nem lép ki az Editorból.
A helyi `ProjectSettings/ProjectSettings.asset` módosítást külön hagytuk;
nem része a bolti changesetnek.

Naplók: `build/npc-shop-ctest-final.log`, `build/npc-shop-live-harness-final.log`,
a Unity `Editor.log` `METIN2_TYPED_SHOP_*` sorai. A részletes utolsó SQL-
ellenőrzés az ignorált `build/npc-shop-final-sql-evidence.json` fájlban van.

Ez a szelet NPC-bolt és Gold pénztárca; játékosbolt, trade, crafting,
quest/Mission tárgyátadás, további pénznem és korlátozott bolti készlet
külön feladat. FreeBSD- és önálló Unity-player build ebben a körben nem futott.
