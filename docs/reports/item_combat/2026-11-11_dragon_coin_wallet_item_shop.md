# C45 — Sárkányérme a natív fiókpénztárcában, tárgybolt natív kiadással

Csak helyi módosítás: szerver `20261103` (HEAD `a577d68e`, commit nélkül), Unity UVCS
`/main/unity63-integration-20261103` (checkin nélkül). A VPS és más gép változatlan.

## Pénztárca

A Sárkányérme egyetlen helye a natív fiókpénztárca (`player.item_wallet`, owner_kind 2,
currency 2). Az `account.account.coins` mostantól csak beérkező „postafiók” a régi és külső
feltöltésekhez: a játék olvasáskor egy tranzakcióban áttölti a pénztárcába, és
`player.item_wallet_legacy_sweep` sort ír (`SRC/Server/common/DragonCoinWallet.h`).
Ha a mező közben változott, a tranzakció visszagörget, érték nem vész el és nem duplázódik.

Minden írás a pénztárca és a fiók owner-fence revízióját is lépteti, így egy régebbi
fiókállapotot tartó típusos művelet (bank, Mall, pénzjegy) CAS-hibát kap és újratölt,
nem írja felül az egyenleget (IC-068).

Átkötött utak: tárgybolti egyenleg és vásárlás, `pc.charge_cash` (Database `ChargeCash`),
szavazási érme (`pc.get/set_vote_coin`), régi érmeutalvány-tárgyak, 80008-as eladás.
A régi abszolút „set” hívások különbségként, atomikusan hatnak.

Az 1213 régi egyenlegű fiók érméi így az első olvasáskor kerülnek át; tömeges
adatmódosítás nem kellett.

## Tárgybolt

Natív tárgyrendszerű karakternél egyetlen store-commit vonja le az érmét és teszi a natív
tárgyat a karakter leltárába (`TypedItemShop.cpp`); hiba, helyhiány, hiányzó konverzió,
korlát vagy fedezethiány esetén semmi nem változik. A heti/havi korlátot az
`ishop_log` adja; a sikeres vásárlást a Database naplózza (`ITEMSHOP_RECORD`).

A régi út (nem natív karakter) is a natív pénztárcából von le; kikerült belőle a soha fel
nem épített SQL-szövegű tárgybeszúrás, amely eddig levonás után hibát adott.

A bolti sorok egyszeri konverziója: `convert_item_shop.py` → `player.ishop_native_item`
(migráció `item_instance/024`). Forrás csak ellenőrzött bizonyíték lehet (tényleges import,
review, telepített kódú javaslat), ellentmondás esetén a sor konvertálatlan marad.
Helyben 517 sorból **12** konvertálható (alapanyagok); 505 sor (hátas, kisállat, jelmez stb.)
natív definíciója még nincs — ezek a tartalomkonverzió után válnak megvásárolhatóvá.

## Teleport gyűrű

A gyűrű menüje a belépéshez és a gyűrűpéldányhoz kötött, nem a leltárrevízióhoz: egy
elixírtick vagy tárgymozgatás nem zárja be. Futó régi quest vagy lejárt menü esetén a válasz
a quest-rendszeré.

## Ellenőrzés

- Élő TLS-teszt saját helyi tesztfiókkal (`ItemShopUnityLiveFixture.py` +
  `Metin2ItemShopLiveValidator`): 22 + 12 ellenőrzés, két belépés. 5000 régi érme áttöltve,
  nem konvertált és fedezetlen vásárlás elutasítva változás nélkül, 2 db natív tárgy 560
  érméért, újrabelépés után is megvan; gyűrűmenü tárgymozgatás után is él, „Mégse” megtartja.
- SQL-végállapot: pénztárca 4440, egy átviteli sor, egy bolti napló, nincs régi tárgysor,
  más tulajdonos változatlan.
- 3200 item persistence SQL-ellenőrzés (friss MariaDB, 024-gyel), 41 sémateszt,
  10 CTest, 3 új + régi konverziós Python-teszt, 6 Nexoria Unity-validátor.

## Telepítési feltétel

Új környezetben a binárisok előtt a `024_dragon_coin_wallet.sql` migrációt kell
alkalmazni; enélkül az áttöltés nem fut le (az érme a postafiókban marad, nem vész el).
