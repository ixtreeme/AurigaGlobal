# ADR IC-038 — Tárgykészítés az új rendszeren (C10)

2026-11-01. A szerveres műhely, az atomi gyártás és a Unity 6.3 ablak együtt
készült. A meglévő vnum-alapú cube tartalmát nem konvertáljuk automatikusan.

## Receptek

A `server.data_path` alatt lévő **typed_crafting.json** explicit NPC-race →
ItemKey recepteket tartalmaz. Hiányozhat vagy lehet üres; hibás, túl nagy
vagy a katalógussal összeegyeztethetetlen fájlnál a katalógust használó core
nem indul. Új csomag az üres `config/typed_crafting.example.json` fájlt kapja.
A szerverkezelő adatszerkesztőjében a JSON szerkeszthető; változtatás után
új core-indítás szükséges.

```json
{
  "version": 1,
  "workshops": [{
    "npc_race": 20016,
    "name": "Kovácsműhely",
    "recipes": [{
      "id": 1,
      "name": "Fegyverkészítés",
      "result": {"category": 1, "local": 2, "level": 1, "count": 1},
      "gold": 25,
      "chance_bp": 7500,
      "ingredients": [{"category": 6, "local": 1, "level": 0, "count": 2}]
    }]
  }]
}
```

A példában szereplő kulcsoknak és szinteknek létezniük kell a saját
katalógusban. Egy NPC-hez egy műhely, műhelyenként legfeljebb 128 recept,
receptenként 1–16 különböző kulcs/szint alapanyag tartozhat; legfeljebb 256
műhely lehet. A receptazonosító műhelyen belül egyedi. A `count` és `gold`
egy próbára vonatkozik. A `chance_bp` 0–10000 bázispont, próbánkénti esély.
Az eredmény csak aktív, már tartósan támogatott tárgykategória lehet.
Kivezetett, de továbbra is tárolható alapanyag explicit receptben elfogyhat.

## Alapanyag és eredmény

A kliens 1–100 próbát kérhet egyszerre. Nem küld árat, alapanyagpéldányt,
eredményt vagy sorsolást. A szerver csak saját inventoryból válogat, tároló,
cella és azonosító szerint determinisztikusan. Több halmot is felhasználhat.
Felszerelt, escrowban vagy földön lévő, kötött, példánybónuszos, zárolt,
nyitott, törött vagy kövezett foglalatú, illetve módosított ékszer nem automatikus
alapanyag. Az alapértelmezett, még üres ékszerállapot használható.

Egy próbára egy szerveres, egyenletes sorsolás jut a [0,9999] tartományból;
az esélynél kisebb érték siker. Minden kért próba elfogyasztja az előírt
alapanyagot és Yangot, a sikertelenek is. Sikeres próbánként a recept szerinti
darabszám és explicit szint készül. Az eredmény új példány alapállapota;
a katalógus megszerzéskori kötési szabálya érvényes. Nem örökli az
alapanyag bónuszait, ezért ezek a példányok ki vannak zárva a válogatásból.

Azonos állapotú eredmény meglévő halmot feltölthet, a többi lefoglalt új
azonosítót kap. Az elfogyasztott egész alapanyagok helye felszabadul a
teljes lábnyomos helykeresés előtt. Ha az alapanyag és eredmény ugyanaz a
halom, a végső nettó állapot egyetlen írás lesz az eredeti pillanatkép ellen.

## Mentés és védelem

Az összes próba alapanyaga, Yangja és tényleges eredménye **egy SQL-
tranzakció**. Anyaghiány, pénzhiány vagy el nem férő eredmény esetén az
egész kérés változatlanul elutasított. Legfeljebb 32 példány változhat;
ennél nagyobb köteg kevesebb próbával kérhető. A pénz és a darabszámok
szorzásai, a tulajdonos és a kerítések ellenőrzöttek. A nyugta a recept
azonosítóját, a kért és sikeres próbák számát, a költséget és minden
sorsolást rögzít. Új SQL-séma nélkül a meglévő `Service` műveletet használjuk.

Nyitáskor véletlen munkamenettoken készül. A gyártási kérésazonosítók
pontos egyesével nőnek. Az utolsó azonos paraméterű kérés ismétlése csak
visszaadja az eredményt; nem ír és nem sorsol újra. Más paraméter, régi kérés
vagy régi token elavult. Újranyitás új tokent ad. A SQL-nyugta önállóan is
ismétlésvédett, és eltérő sorsolással nem írható át ugyanaz a művelet.

Az NPC, a térkép és az 1000-nél kisebb közelítő távolság ellenőrzött.
Halott, kábult, megfigyelő vagy másik gazdasági ablakban lévő karakter nem
gyárthat. A műhely az `IsCubeOpen` útján is látszik a többi rendszernek.
A normál `Cube_open` a konfigurált típusos műhelyre vezet; a régi gyártási
packet nem hajthat végre mellette legacy műveletet. Az ECS-komponens neve
`TypedCraftSession`, hogy a többi fordítási egység komponensével ne ütközzön
az EnTT típusnév-hash alapú tárolóazonosítója.

A Mission meglévő `ItemUse` erőforrás-belépésvédelmét a COMMIT eredményének
rendezéséig megtartjuk. Ugyaneddig a karakter típusos tárgyműveletei foglaltak,
a tárgyadások várakoznak. Folyamatban lévő mentést a bezárás nem szakít meg.
Kapcsolatbontás után is a tároló rendezi az eredményt; új belépéskor a
kerítéssel védett aktuális állapot töltődik vissza. Távolodáskor az üresjárati
műhely bezárul.

## Protokoll és Unity

- `CG_TYPED_CRAFT` **231**: 30 bájtos LE body, művelet (nyitás/gyártás/
  bezárás), NPC VID, token, kérésazonosító, receptazonosító és a próbák száma.
  A Unity 31. bájtját a meglévő session szekvenciakezelése dolgozza fel.
- `GC_TYPED_CRAFT` **230 v1**: 32 bites méret, státusz, NPC, token, kérés,
  az utolsó gyártás kért/sikeres darabszáma, műhelynév, receptek és GC227
  katalógusrészlet. A lezárt, üres válasz 47 bájt. A pontos mezők a
  `Crafting.hpp` fejlécben vannak.
- A szerver a válasz előtt saját inventorypillanatképet küld. A Unity
  nem fogyaszt előre tárgyat vagy Yangot. Külön készítési megerősítés van;
  frissülő inventory érvényteleníti a nyitott megerősítést. A feldolgozás
  idején a dupla küldés tiltott. Karakterváltáskor minden műhelyállapot törlődik.
- Belépés: NPC kijelölése → **I → Készítés**. A receptlista, az explicit
  eredményszint és statok, a szükséges/elérhető alapanyag, teljes költség,
  sikeresély és sikertelenségi következmény a megerősítés előtt látszik.

Ellenőrzés és helyi használat:
[jelentés](../reports/item_combat/2026-11-01_crafting_runtime.md).
Speciális másoló/fejlesztést segítő tárgy, többféle választható eredmény,
Gaya és más pénznem, valamint a teljes legacy receptkonverzió külön munka.
