# Tárgykészítés: szerver és Unity — C10

2026-11-01. Szerverág `codex/mission-engine`, Unity 6.3 `6000.3.27f1`,
klienság `/main/codex-mission-engine`, **Unity changeset 43**.

## Elkészült működés

- ItemKey-alapú NPC-műhelyek és explicit, induláskor ellenőrzött receptek:
  `data/typed_crafting.json`. Új csomag üres indulófájlt kap; a szerverkezelő
  adatszerkesztőjében szerkeszthető.
- Kérésenként 1–100 próba, szerveres bázispontos eséllyel. A teljes köteg
  alapanyaga és Yangja a tényleges eredménnyel egy tranzakcióban változik.
  A sikertelen próbák is fizetnek. A sorsolások a nyugtába kerülnek.
- A források saját inventoryból determinisztikusan kiválasztott, módosítatlan,
  nem kötött példányok. Több halom is felhasználható; bónuszos, kövezett,
  felszerelt vagy módosított ékszer nem fogy el automatikusan.
- Az eredmény az explicit szinten és alapállapotban készül. Azonos állapotú
  halom feltölthető; az új példány lefoglalt azonosítót kap. Az alapanyag és
  eredmény közös halma egyetlen nettó változásként kerül mentésre.
- Az elfogyó egész alapanyag felszabaduló helye is használható. El nem férő
  eredmény, anyag- vagy pénzhiány esetén nincs részleges fogyasztás vagy jutalom.
  A legfeljebb 32 példányváltozásos korlát külön választ ad a túl nagy kötegre.
- Token, pontosan növekvő kérésazonosító, utolsó kérés visszajátszása új
  sorsolás vagy írás nélkül. A tulajdonos- és belépési kerítés megmarad.
- Az NPC, távolság, állapot és más gazdasági ablak ellenőrzött. A műhely
  az `IsCubeOpen` útján látszik, a normál `Cube_open` is a típusos műhelyre
  vezet. A külön `TypedCraftSession` ECS-komponensnév elkerüli az EnTT
  névhash-ütközését a bolt másik fordítási egységében lévő komponensével.
- A Mission `ItemUse` belépésvédelme és a tárgyfoglaltság a mentés eredményének
  rendezéséig fennmarad. Nincs új SQL-migráció.
- `CG_TYPED_CRAFT` 231, `GC_TYPED_CRAFT` 230 v1. Unity receptlista, eredmény
  és statok, szükséges/elérhető alapanyag, teljes Yang, esély, következmény,
  külön megerősítés és tényleges eredmény. Nincs optimista tárgy- vagy pénzírás.

Részletes szabályok és JSON-minta:
[IC-038](../../adr/item_combat_038_crafting.md).

## Ellenőrzés

**Natív Release build sikeres; végső CTest 9/9:** `crafting`, `player_trades`,
`npc_shops`, `typed_inventory_wire`, `persistent_items`,
`item_persistence_completion`, `item_persistence_mariadb`, `shop_lifecycle`,
`exchange`.

Az új tiszta craftingteszt **26 ellenőrzés**: vegyes siker/kudarc egy
kötegben, teljes költség és sorsolásnapló, szint és 64 bites eredményazonosító,
pénz- és anyaghiány, kötött/módosított/felszerelt források, garantált kudarc,
helyhiány, elfogyó forrás helyének használata, közös input/output egyetlen
revíziója, több halomból fogyasztás, rossz sorsolás és kötegméret,
duplikált alapanyag, túlcsordulás, névkorlát, 31/32/33-as tranzakcióhatár
és közös golden packet.

A saját MariaDB-s készlet **1175 ellenőrzés**. A C10-rész valódi vegyes
köteget ment és tölt vissza: azonosítók, explicit szint, maradék alapanyag és
pénztárca. A nyugtaismétlés nem fizet vagy sorsol újra; eltérő sorsolással
azonos művelet elutasított. Pénztárcaütközéskor minden változás visszagörög.
A sikertelen gyártás díja és alapanyaga együtt fogy, eredmény nélkül. Az új
belépés kerítése megakadályozza a régi gyártás írását.

**Unity fordítás és célzott validátorok sikeresek a nyitott Editorban:**
crafting **3699**, csere **4553**, NPC-bolt **4044**, inventory **4179**
ellenőrzés. Közös natív golden, minden TCP-vágás és csonkolás, pontos 30
bájtos kérésbody, kötegkorlát, megerősítés, függőállapot, bónuszos forrás
kizárása, inventoryfrissülés és karakterváltás. A felület képe vizuálisan
ellenőrzött: Unity `Logs/typed-craft-preview.png`.

**Végső GameServerrel, valódi Unity TLS-klienssel: 84 sikeres ellenőrzés.**

1. A nyitott műhely mellett az NPC-bolt elutasított; bezárás után újra
   megnyitható. A műhely tokenje újranyitáskor változik.
2. A Unity megerősítőgombjával három biztos sikerű gyártás: három külön +1
   fegyver, 20 anyagból 14 marad, 1000 Yangból 925. A megismételt kérés nem
   hoz létre újabb eredményt és nem fizet másodszor.
3. Két biztos sikertelen próba: két alapanyag és 20 Yang elfogy, tárgy nem
   készül. Ismétlésre nincs újabb fogyasztás. Ezután azonos anyagból történő
   visszagyártás az eredeti halmon nettó +1 egységet ad: 13 anyag, 900 Yang.
4. Ismeretlen recept és régi műhelytoken nem fogyaszt. Újrabelépéskor a
   három eredeti eredményazonosító, a +1 szintek, az anyaghalom azonosítója,
   13 darabja és a 900 Yang pontosan visszatöltődik.
5. Nulla Yang, de 20 anyag: nincs anyagfogyasztás vagy ingyenes eredmény.
   Üres leltár, 1000 Yang: nincs levonás vagy eredmény.
6. Teljes leltár, 180 teljes anyaghalom, 36000 egység: a fegyver nem fér el.
   A másik recept részleges anyaglevonása és eredmény-halomfeltöltése is
   visszagörög, mert a maradék eredményhez nincs új hely. Egy teljes 200-as
   alapanyaghalom elfogyasztása viszont helyet ad a három italnak: 179
   anyaghalom és egy új italhalom marad, 993 Yanggal. Az új italazonosító,
   a három darab, a megszűnt forrás és a pénz újrabelépés után is helyes.

## Helyi csomag és kézi próba

`D:\AurigaGlobal\Server2-Local` a végső builddel fut az 1-es csatornán,
a 41-es főtérkép core-portja 30078. A #20016 normál kovács négy külön helyi
demoreceptet kapott: biztos +1 fegyver, biztos kudarc, anyag-visszagyártás,
és teljes anyaghalomból italcsomag. Ezek célzott tesztadatok; a korábbi
katalógus, a többi NPC és a második tesztkarakter nem változott.

A #1610 dedikált karakter a kereskedő és a kovács közelében áll,
`(986600,269100)`, kézi próbához **1000 Yang, egy +1 fegyver, 3 ital és
20 anyag** van nála. Használat: kovács kijelölése → **I → Készítés** →
recept és próbák száma → áttekintés → megerősítés.

Korábbi bináris és csomagmanifest: `run/deployments/crafting-20261101`.
A végső teszt előtti és utáni karakterállapotok:
`run/item-craft-live/20261101T185110`. A privát fixture az ignorált
`build/crafting-live.private.json`; belépési adat nem került verziókezelésbe.

Újrafuttatás: `tests/server/CraftingUnityLiveSmoke.py`, a saját csomag,
Unity-projekt, privát fixture és `ItemBenchTool` megadásával. A harness
kizárólag a pontosan ellenőrzött fixture-karaktert reseteli karakterválasztás
alatt. A bridge ideiglenes jelenetet használ, és nem zárja be az Editort.
A korábbi `ProjectSettings/ProjectSettings.asset` módosítás bájtra pontosan
megmaradt, nem része a 43-as changesetnek.

Naplók: `build/crafting-final-build.log`, `build/crafting-ctest-final.log`,
`build/crafting-live-harness-final.log`, a Unity `Editor.log`
`METIN2_TYPED_CRAFT_*` sorai. Nem futott FreeBSD- vagy önálló Unity-player
build, és nem futott újra a teljes korábbi Unity-validátorcsomag.

A következő nyitott út a quest/Mission tárgyműveleteinek közös bekötése.
Speciális másoló és segédanyag, többféle eredmény, más pénznem és a teljes
legacy receptkonverzió külön feladat.
