# ADR IC-044 — Foglalatkövek behelyezése szerveren és Unityben

2026-11-02, C16, F4/F5. Előzmény:
[IC-021](item_combat_021_socket_stones.md),
[C15 karbantartás](item_combat_043_socket_maintenance.md).

## Szerződés

Az új 7-es kategória behelyezhető köve saját, leltárban lévő fegyver vagy
testpáncél első kompatibilis üres foglalatába kerülhet. A szerver ellenőrzi
a céltípust, minimális foglalatminőséget és mindkét kizárócsoportot. Zárt,
törött és foglalt helyet nem ír felül. A cél előbb leveendő; az eszköz
szint-/kaszt-/nem-/harcpolitikája érvényes, a cél viselhetősége nem feltétel.

Egy kő fogy siker és sikertelenség esetén is. Sikerkor az ItemKey és a
konkrét példányszint kerül a foglalatba; kudarckor az ugyanilyen minőségű
hely törött lesz. A többi foglalat, a céltárgy ID-je, szintje, kötése és
teljes bónuszállapota megmarad. Egy részhalom eredeti ID-je/helye megmarad;
az utolsó darab tombstone. Nincs új output, ezért teli leltárban is működik.
Szilánk, retire-olt forrás, hatástalan cél és csoportütközés nem fogyaszt.

A beépített modell ItemKey-t és szintet tárol. Egyedi példánybónuszt vagy
egyéb nem ábrázolható kőállapotot nem dobunk el. Bound forrás csak saját,
már bound célba helyezhető, és csak OnAcquire prototípus esetén: annak
kötését a C15 kiemelés visszaállítja. Egyedi, protótól eltérő kötés
elutasított. A cél kötését nem oldjuk fel és nem írjuk át.

## Döntés és gazdasági commit

A `MakeSocketInsertionCommit` a tiszta `PrepareSocketStoneInsert` tervét
használja. A meglévő `stone_chance` szerverbeállítás az igazságforrás,
százalékból bp-re alakítva. Nincs kliens által megadott esély vagy dobás,
új katalógusmező vagy hallgatólagos 30%-os fallback a tervezőben.
A helyi csomag meglévő **85%** beállítása megmarad.

Previewkor mindkét teljes kimenet előáll, RNG nélkül. A quote rögzíti a
forráshalom darabját, szintjét, célt, slotot, minőséget és esélyt. Confirmkor
friss admission, policy, owner session/revision és az egész quote egyezése
kell. Csak ezután történik egy szerveres, OpenSSL `RAND_bytes` alapú,
torzítatlan 1..10000 dobás. A tiszta domain 0/100% végpontjai nem kérnek
dobást; a futó konfiguráció a meglévő 1..100 százalékos tartományt használja.

Forrásfogyás, foglalatcsere és receipt egy SQL itemtranzakció. Wallet nem
változik. Outcome `0x1601` siker és `0x1602` szabályos, fogyasztó kudarc;
`INS\x01` payload: esély, rögzített dobás, slot és eredmény. A teljes
before/after képek a commit digest részei. CAS-hiba mindent visszagörget.
CommitUnknown a meglévő store-egyeztetésen fut.

Kriptográfiai nonce/operation ID, ItemUse permit és TypedItemBusy védi a
műveletet. Azonos confirm a korábbi eredményt adja, új dobás nélkül. Régi
nonce, módosított cél, inventoryváltozás és régi belépés nem írhat. A C15 és
C16 közös munkamenetet használ: új előnézet az előzőt lecseréli.

## Unity és protokoll

**I → behelyezhető kő → Behelyezés**. A közös foglalatablak szerveres
céllistát, esélyt, sikeres és törött kimenetet mutat. Külön megerősítés
mondja el, hogy kudarc esetén is egy darab fogy. Pending alatt nincs helyi
fogyasztás vagy döntés. A szerver sikerét és a fogyasztó kudarcot külön
eredményként jelzi. A tényleges leltár továbbra is kizárólag GC225-ből frissül.

- GC227 `Socket=128` az 5-ös karbantartó és a 7-es behelyezhető kategóriára;
  szilánkra nem. A mezőméretek és a GC227 v1 megmaradnak.
- CG235 változatlan, 26+1 bájt. Nem tartalmaz esélyt vagy dobást.
- GC235 **v2**, 162 bájtos terminal envelope. A C15 mezőit az esély u16,
  slot u8 és három teljes kudarcsocket egészíti ki. `kind=4` a behelyezés;
  `Broken=13` a commitolt kudarc, külön NoSocket/Conflict/Incompatible.
- Az új kliens a korábbi v1 karbantartó golden-t is olvassa. A szerver v2-t
  küld. Az átmenet két decoderformátum, nem két item-writer vagy vnum-adapter.
- Sikeres quote/eredmény esetén a kliens minden érintetlen slot egyezését,
  a kiválasztott hely korábbi ürességét, megmaradó minőségét, a sikeres
  ItemKey/szintet és a teljes törött kimenetet is ellenőrzi.

A felszerelés már létező C11 értékelője használja a beépített követ; a
leltárban maradó kövek nem adnak felszerelésstatot. Kiemelés és tisztítás
a C15 útján folytatható. Új séma és legacy item-írás nincs.
