# ADR IC-037 — Játékosok közötti csere az új tárgyrendszeren (C9)

2026-11-01. A szerver és a Unity 6.3 kliens közös szelete.

## Ajánlat és elfogadás

Két, az új tárgyrendszeren kiépített karakter ugyanazon a core-on, ugyanazon
a térképen, 1000-nél kisebb közelítő távolságban cserélhet. A legacy csere
indítása is erre az útra vezet, ha a résztvevők típusosak; vegyes csere
elutasított. Az új Unity-út: másik játékos kijelölése → **I → Csere**.

A céljátékosnak külön el kell fogadnia a meghívást. A meghívó 30 másodpercig
él, az elfogadott egyeztetés legfeljebb öt percig. Mindkét oldalon legfeljebb
nyolc saját, inventoryban lévő példány vagy részhalom ajánlható fel, és
külön Yang-összeg. A kötött, felszerelt vagy `allow_trade` által tiltott
tárgy nem ajánlható fel. Másik fél teljes leltára és pénztárcája nem látható.

Minden ajánlatmódosítás növeli a szerveres revíziót, és törli **mindkét fél**
rögzítését és végleges jóváhagyását. Először mindketten rögzítik az ajánlatot,
utána mindketten külön megerősítik a változatlan revíziót. A Unity az utolsó
lépés előtt összegzést mutat; az egyes tárgyaknál a teljes bónusz-, szint-
és foglalatállapot megvizsgálható. Közben érkező módosítás bezárja a korábbi
megerősítést. Elavult jóváhagyás nem hajthat végre cserét.

## Tartós állapot

Az ajánlott tárgyak az utolsó jóváhagyásig eredeti helyükön maradnak. Az
elfogadott egyeztetés mindkét karakter többi típusos tárgyműveletét és a
várakozó tárgyadásokat foglaltsággal védi. A Mission erőforrás-belépésvédelme
is mindkét résztvevőre fennmarad a mentés eredményének rendezéséig.

Az atomi `Trade` írási halmaz mindkét tulajdonos kerítését, összes érintett
példányát és a nettó pénztárcaváltozásokat tartalmazza. Egész példánynál az
azonosító és a teljes állapot megmarad. Részhalomnál a maradék megtartja az
eredeti azonosítót, az átadott rész előre lefoglalt új azonosítót és a teljes
payload másolatát kapja. Az érkező példányok nem olvadnak meglévő halomba.
Mindkét oldal teljesen átadott tárgyainak lábnyoma felszabadul az érkező
tárgyak helykeresése előtt, ezért megtelt leltárok között is lehet cserélni.
Legfeljebb 16 részhalom a meglévő 32 példányváltozásos tranzakcióba belefér.

A felajánlott Yangnak már a csere előtt rendelkezésre kell állnia. A végső
egyenlegek nem haladhatják meg az `INT64_MAX` értéket. Azonos Yang-ajánlatok
nettó nulla pénzváltozását nem írjuk újra. Üres, gazdasági változás nélküli
csere nem érvényes. Pénzhiány, helyhiány, revízió- vagy munkamenetütközés
esetén egyik fél tárgya vagy pénze sem változik részlegesen.

Nincs új SQL-séma vagy tartós csere-escrow. Megszakítás, távolodás, tiltott
állapot vagy kapcsolatbontás a végső mentés előtt egyszerűen feloldja a
foglaltságot. Folyamatban lévő COMMIT-ot nem szakítunk meg, és nem indítunk
újra; az író a meglévő nyugtaegyeztetéssel rendezi az eredményt. Az új
belépés a saját aktuális kerítésével tölti vissza az állapotot.

## Protokoll

- `CG_TYPED_TRADE` **230**, 42 bájtos LE body a meglévő szekvenciás úton:
  művelet, partner VID, token, kérésazonosító, ajánlatrevízió, tárgy/összeg,
  darabszám. A Unity író 43. bájtját a session szekvenciakezelése dolgozza fel.
- Műveletek: meghívás, elfogadás, tárgybeállítás, tárgykivétel, Yang,
  rögzítés, végleges megerősítés, megszakítás, rögzítés feloldása.
- `GC_TYPED_TRADE` **229 v1**, 32 bites méret, státusz, token, oldalankénti
  kérésazonosító, közös revízió és jóváhagyásmaszkok. Aktív cserében két
  partner neve/azonosítója és ajánlott Yangja, csak a saját átadható
  példányazonosítók, két GC225 ajánlatpillanatkép és közös GC227 részlet.
  Lezáráskor csak a 35 bájtos fejléc érkezik, privát payload nélkül.
- A kérésazonosító oldalanként pontosan egyesével nő. Az utolsó kérés
  azonos paraméterű ismétlése nem módosít, csak visszaküldi az állapotot.
  Régi token, eltérő ismétlés vagy revízió elutasított. Megszakítás kérés
  nélküli is lehet. Új csere új tokent kap.
- Minden válasz előtt irányadó saját inventory érkezik. A Unity nem
  módosít előre tárgyat vagy pénzt, és karakterváltáskor törli a cserét.

## Ellenőrzés és határ

A natív `player_trades` tesztet a közös golden fixture köti a Unity
dekódolójához. Az SQL-teszt mindkét tulajdonos valódi írását és újratöltését,
a nyugtaismétlést, pénztárcaütközéses visszagörgetést és belépési kerítést
ellenőrzi. A `PlayerTradeUnityLiveSmoke.py` két dedikált fixture-fiókkal
valódi TLS-munkameneteket és a nyitott Unity Editort használja. A harness
csak a pontosan ellenőrzött fixture-karaktereket reseteli karakterválasztás
alatt, előtte és utána menti az állapotukat a `run/item-trade-live` alá.

A jelenlegi támogatott tartós kategóriák cserélhetők. Játékosbolt,
cross-core csere, további pénznem, crafting, quest/Mission tárgyátadás és
a még nem tartós kategóriák külön munka. Konkrét ellenőrzések és telepítés:
[jelentés](../reports/item_combat/2026-11-01_player_trade_runtime.md).
