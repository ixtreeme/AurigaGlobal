# Játékoscsere: szerver és Unity — C9

2026-11-01. Szerverág `codex/mission-engine`, Unity 6.3 `6000.3.27f1`,
klienság `/main/codex-mission-engine`, **Unity changeset 42**.

## Működés

- A meghívott játékos külön elfogadja a cserét; mindkét oldalon legfeljebb
  nyolc teljes vagy részhalom és Yang ajánlható fel.
- A teljes ajánlat revízióhoz kötött rögzítést, majd mindkét féltől külön
  végleges megerősítést kap. Módosításkor mindkét fél összes jóváhagyása
  törlődik. Régi jóváhagyás nem írhat.
- Egyetlen SQL-tranzakció adja át a tárgyakat és a nettó Yangot. Egész
  példánynál megmarad az azonosító; részhalomnál az eredeti maradék és a
  friss azonosítót kapó rész is a teljes állapotát őrzi. Nincs beolvasztás.
- Mindkét fél átadott egész tárgyainak helye újra használható az érkező
  tárgyaknak. Nem megfelelő hely vagy pénz esetén nincs részleges csere.
- A tulajdonos-, belépés- és revíziókerítés, a pénztárca ellenőrzése,
  az ismétlésvédelem és a Mission erőforrás-belépésvédelme megmarad.
  Elfogadott csere alatt mindkét fél többi típusos tárgyművelete várakozik.
- Megszakítás és kapcsolatbontás mentés előtt írás nélkül feloldja a
  foglaltságot. Folyamatban lévő COMMIT esetén az eredményt végig rendezzük.
- `CG_TYPED_TRADE` 230, `GC_TYPED_TRADE` 229 v1. A Unity az ajánlatokat és
  a teljes példányállapotot mutatja; másik fél teljes leltárát vagy
  pénztárcáját nem kapja meg. Nincs optimista pénz- vagy tárgymódosítás.

A részletes szabályok és a formátum az
[IC-037-ben](../../adr/item_combat_037_player_trades.md) találhatók.

## Ellenőrzés

**Natív Release build sikeres, CTest 8/8:** `player_trades`, `npc_shops`,
`typed_inventory_wire`, `persistent_items`, `item_persistence_completion`,
`item_persistence_mariadb`, `shop_lifecycle`, `exchange`.

Az új tiszta cserepróba **23 ellenőrzés**: teljes állapotú fegyver,
részhalom és Yang együtt, pénzhiány és felső pénzhatár, kötött és felszerelt
tárgy, ismételt azonosító, érvénytelen mennyiség, hiányzó split-azonosító,
két teljes leltár, helyhiány, nettó nulla Yang, csak pénzes ajándék,
üres ajánlat, mind a 16 tárgy részleges átadása egy 32 változásos commitban,
katalógustiltás és közös golden csomag.

A saját MariaDB-s próba **1158 ellenőrzésre** bővült. A C9-rész mindkét
tulajdonost valóban írja és újratölti: teljes bónusz- és kőállapot, +9
fegyver, új és régi részhalomazonosító, mindkét pénztárca, nyugtaismétlés,
az egyik pénztárca revízióütközésekor teljes visszagörgetés, új belépés
utáni régi csere elutasítása. A legacy csere és bolt tesztjei is sikeresek.

**Unity fordítás és célzott validátorok sikeresek a nyitott Editorban:**

- Új csere: **4553 ellenőrzés**, közös natív golden, minden TCP-vágás,
  csonkolások, pontos 42 bájtos kérésbody, 64 bites példányazonosító,
  ajánlatrögzítés és külön végleges megerősítés, változáskor a megerősítés
  érvénytelenítése, bónuszok/kövek megjelenítése, függőállapot és törlés.
- Meglévő NPC-bolt: **4044**, típusos inventory: **4179 ellenőrzés**.
- A csereablak vizuális ellenőrzése is megtörtént. Kép a Unity
  `Logs/typed-trade-preview.png` fájljában.

**Valódi helyi GameServer, két Unity TLS-kliens: 125 sikeres ellenőrzés.**

1. Külön céljátékosi elfogadás; az elérhető tárgyazonosítók csak a sajátok.
   Túl sok Yang és idegen tárgy ajánlása elutasított. Az egyeztetés alatt
   küldött másik tárgymozgatás nem változtatja meg az állapotot.
2. Egész +1 fegyver és 100 Yang az egyik oldalról, a másikról egy 10-es
   anyaghalom 4 darabja és 40 Yang. Az első fél 940, a második 560 Yangot
   kap vissza. Az első saját anyaghalma megmarad, a beérkező 4 darab friss
   azonosítón jelenik meg, a másik félnél az eredeti azonosítón 6 marad.
   A fegyver azonosítója és +1 szintje változatlan.
3. Ugyanaz az ajánlatkérés megismételve nem növeli újra a revíziót.
   Lezárt csere végleges megerősítésének ismétlése nem ír újra.
4. Új cserében mindkét rögzítés és az egyik végleges jóváhagyás után a
   másik fél módosít: minden jóváhagyás törlődik. A régi revízió végleges
   megerősítése elutasított, sem tárgy, sem pénz nem változik.
5. Kifejezett megszakítás, majd új egyeztetésből kapcsolatbontás: a túlélő
   fél foglaltsága feloldódik, és az ajánlott tárgy teljesen megmarad.
   Mindkét újrabelépés pontosan a korábbi sikeres csere állapotát tölti be.
6. A másik fél teljes leltára 180 darab, egyenként 200-as anyaghalom.
   Fegyver és 10 Yang beérkezése helyhiányos: egyik pénztárca vagy tárgy
   sem változik. Ezután az egyik egész anyaghalom helyére az első fél
   egész 1-es anyaghalma kerül: az azonosítók cserélődnek, nincs összevonás,
   a leltár továbbra is 180 példány. Csak a sikeres csere von le 10 Yangot.
   Mindkét újrabelépés megőrzi az eredményt: 990 és 510 Yang.

## Helyi telepítés és kézi próba

`D:\AurigaGlobal\Server2-Local` a friss GameServerrel fut az 1-es csatornán;
a 41-es főtérkép core-ja a 30078-as porton. A korábbi GameServer és a
csomagmanifest mentése: `run/deployments/player-trades-20261101`.
Nincs új adatbázis-migráció vagy katalógusmódosítás.

A #1610 és #1611 dedikált tesztkarakterek egymás mellett, a kereskedő
közelében állnak: `(986600, 269100)` és `(986700, 269100)`. Kézi teszthez
1000/500 Yang, egy-egy fegyver (+1/+0), három-három ital és 4/10 anyag
van náluk. A korábbi és az élő teszt utáni állapotok a
`run/item-trade-live/20261101T180731` könyvtárban vannak. A privát belépési
fixture az ignorált `build/npc-shop-live.private.json` fájlban maradt.

Használat: másik játékos kijelölése → **I → Csere**, a másik oldalon
meghívás elfogadása. Tárgy és darabszám beállítása, Yang-ajánlat, mindkét
oldalon **Ajánlat rögzítése**, végül mindkét oldalon a végleges összegzés
megerősítése. Az ajánlat módosítható vagy megszakítható a mentés indításáig.

Újrafuttatás: `tests/server/PlayerTradeUnityLiveSmoke.py`, a saját csomag,
Unity-projekt, privát fixture és `ItemBenchTool` megadásával. A nyitott
Editor edit módban marad; a bridge ideiglenes jelenetet használ, és nem
zárja be az Editort. A korábbi `ProjectSettings/ProjectSettings.asset`
módosítás bájtra pontosan megmaradt, nem része a 42-es changesetnek.

Naplók: `build/player-trade-ctest.log`, `build/player-trade-final-build.log`,
`build/player-trade-live-harness-2.log`, a Unity `Editor.log`
`METIN2_TYPED_TRADE_*` sorai. Nem futott FreeBSD- vagy önálló Unity-player
build, és nem futott újra a teljes korábbi Unity-validátorcsomag.

A következő nyitott gazdasági út a crafting. A játékosbolt,
quest/Mission tárgyátadás, további pénznem és még nem tartós kategóriák
nem részei ennek a szeletnek.
