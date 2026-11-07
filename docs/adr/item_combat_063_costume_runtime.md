# ADR IC-063 — Típusos test-, haj- és fegyverjelmez runtime

2026-11-07. C28, az [IC-022](item_combat_022_costumes_and_lifetime.md)
domainjének szerver–Unity bekötése. Aktív ágak: `20261103` és
`/main/unity63-integration-20261103`, Unity `6000.3.27f1`.

## Felszerelés és bónusz

A 4-es kategória tartós példánya külön 9/10/11 felszerelési helyre kerül
(test/haj/fegyver). A normál fegyver és páncél alatta megmarad. A viselés,
levétel és az azonos helyre történő csere ugyanazon tulajdonosi/session-
kerítéssel és atomi itemcommittal működik, mint a többi típusos felszerelés.
A csere a levett jelmezt az új jelmez felszabadult leltárhelyére teszi.
Sikeres felvétel alkalmazza az OnEquip kötést; sikertelen próbálkozás nem.

Az osztály, nem, minimumszint, kivezetés és harci használati policy a
befogadás része. Fegyverjelmezhez a ténylegesen viselt, megfelelő altípusú
fegyver kell. A később lecserélt valódi fegyver ideiglenesen megszünteti a
nem kompatibilis jelmez modelljét és bónuszát; a jelmez a helyén marad.
A valódi fegyver határozza meg a harci fegyvertípust és mozgást.

Az alap-, aktuális fejlesztési és példánybónusz a közös stataggregátoron
keresztül pontosan egyszer adódik hozzá. Levételkor vagy lejáratkor eltűnik.
Ez a fejlesztési szint hatását beköti; új jelmezfejlesztési műveletet nem vezet be.

## Tartós órák

A `016_costume_lifetime.sql` a nullable `costume_lifetime_blob` mezőt és
a régi felszerelési tárolók legalább 11 helyre bővítését hozza. A régi
példányrekordok változatlanok, más kategória nem kaphat jelmezórát.
Új jelmeznek kötelező a 22 bájtos CTL1 állapot:
`CTL,1,started:u8,expired:u8,deadline:u64LE,remaining:u64LE`.
A hibás vagy hiányzó állapot betöltési hiba, nem új órát gyártó fallback.

| Óra | Indítás és fogyás |
|---|---|
| Állandó | Nincs határidő és fogyó keret |
| Létrehozástól | Kiosztáskor induló abszolút UTC-határidő |
| Első használattól | Az első befogadott felszerelési kísérlet UTC-határideje |
| Viselési idő | Csak online és ténylegesen felszerelve fogyó monoton keret |

Az első használati kísérlet hiányzó vagy rossz altípusú fegyvernél is
elindítja az órát az IC-022 szabálya szerint; az általános befogadásnál
elutasított használat nem indítja el. A későbbi felvétel és újrabelépés
nem indít új órát. A katalógus időtartamának átírása nem tölti fel a már
kiadott példányt; óratípusváltáshoz külön migráció kell.

A viselési keret öt másodperces checkpointokkal, levételkor, cserénél és
kijelentkezéskor rendeződik. A commit közben felgyűlt utolsó időtartozás
is levonódik. Az új belépés megvárja az előző munkamenet tartozásának
rendezését. Leltárban és offline nem fogy. A lejárat tombstone-t ír,
felszabadítja a helyet; a modell és stat projekció már a határidőnél
megszűnik, az SQL-válaszra várva sem marad aktív.

A CTM1 műveleti bizonylat rögzíti az idő-, szereplő- és fegyverkontextust
és az érintett jelmezek időlevonását. A tartós író újratervezi a pontos
állapotátmenetet; önkényes óraváltoztatás, helycsere vagy bónuszmódosítás
nem fogadható el e bizonylat alatt. A korábbi tárgyak kanonikus hash-e nem változik.

## Hálózat és tényleges modellek

A GC225 v13 jelmezenként küldi a fajtát, fegyveraltípust, óratípust,
definíciós időtartamot és a CTL1 állapotot. A korábbi verziók olvashatók.
A részletező csomag a három új felszerelési helyet ismeri. A Unity leltár
Felszerel/Levesz gombja valós hálózati műveletet kér, helyi jóslás nélkül;
a részletező és az órakijelzés a szerver állapotát követi.

A külön GC241 v1 csomag a VID-t, projekciórevíziót, valódi fegyver- és
eszköztípust és öt explicit assethivatkozást küld: alapfegyver, alappáncél,
testjelmez, hajjelmez, fegyverjelmez. A változat az osztály/nem szerinti
katalógussorból jön. A csomag mérete 30..2590 bájt, modellenként legfeljebb
512 UTF-8 bájt; a kliens útvonalbejárást és hibás formátumot elutasít.
**A 237-es GC_TIME_SYNC azonosító és feldolgozása megmarad.** A validator
a két csomag és a ping összefűzött, minden pozícióban darabolt folyamát is vizsgálja.

A mount-flight képességkézfogás 2-es bitje jelzi a GC241 támogatását;
csak ezt kérő kliensnek küldjük. A gyors repülés 1-es bitje megmarad.
A megjelenés a tulajdonos és a látókörben levő megfigyelő számára is
publikus, belépésnél/látókörbe kerülésnél visszajátszható. A kliens
az actor létrejöttéig tárolja a legfrissebb megjelenést, törléskor elengedi.
A később érkező legacy parts nem írja felül a typed modellt.

A Unity runtime binder explicit equipment asseteket tölt; a jelmez
felülírja az adott alapparti modellt, levétel visszaállítja az eredeti
felszerelést és hajat. Az aszinkron töltés generációja kizárja az előző
öltözet késői visszaírását. A megjelenítés továbbra is a meglévő
Equipment/Addressables rendszert használja.

## Ellenőrzés és határ

A futtatott tesztek, helyi SQL- és kétklienses újrabelépési eredmények a
[C28 jelentésben](../reports/item_combat/2026-11-07_costumes.md) szerepelnek.
A teljes legacy tartalom konverziója, a stóla/rúna, a normál leltár
átállítása és a kiadási grafikai/standalone ellenőrzés külön munkacsomag.
