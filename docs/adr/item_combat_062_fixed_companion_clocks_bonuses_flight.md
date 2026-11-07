# ADR IC-062 — Fix társak időkezelése, bónuszai és repülési engedélye

2026-11-07, C27b. Szerverág: `20261103`; Unity:
`/main/unity63-integration-20261103`, Editor `6000.3.27f1`.
Párosított Unity changeset: `cs:88`.

Az állandó, létrehozástól számított, első használattól számított és aktív
idejű pet-/hátaspecsét ugyanazon tartós példány- és kliensműveleteket
használja. A fix pet követés közben, a hátas felülve ad bónuszt.
A hátas repülési engedélyét a típusos katalógus határozza meg.

## Órák és tartós állapot

| Óra | Indulás | Visszahívva és kijelentkezve |
|---|---|---|
| Permanent | Nincs óra | Nem jár le |
| SinceCreation | A szerveres kiosztás UTC ideje | A határidő tovább telik |
| SinceFirstUse | Első sikeres idézés | A határidő tovább telik |
| ActiveTime | Kiosztáskor teljes keret | Követés és felülés fogyasztja; visszahívva és offline megáll |

A `015_companion_lifetime.sql` a példányhoz nullable, 22 bájtos,
verziózott `CPL1` blobot ad: started/expired jelző, UTC-határidő és
fennmaradó aktív keret. A korábbi állandó pecsétek NULL értéke és hash-e
megmarad. A betöltő ellenőrzi a méretet, verziót, jelzőket, óraszabályt és
az állapothasht. A lejárt pecsét tombstone marad a mentésben: nulla
darabszám, nincs leltárhely vagy aktív szándék, megőrzött lejárati adat.

Az aktív idő monoton órából származik. Öt másodpercenként, visszahíváskor,
kijelentkezéskor és lejáráskor nyugtázott tulajdonosi tranzakció rendezi.
A visszahívási commit közben eltelt maradék is levonódik. Az új belépés
megvárja a korábbi munkamenet utolsó tartozásának mentését. A csak UTC
alapú, meg sem idézett pecséteket is figyeli a futó szerver.

A `CMP` v3 nyugta a műveletet, időt, tényleges fázist és szerveres
használati tényeket is rögzíti. Az SQL/domain ellenőrzés ezekből újratervezi
a pontos változást. A régi állandó pecsétek v1/v2 nyugtái továbbra is
érvényesek; általános tárgyművelet nem írhatja át az élettartamot.
A társ műveletével együtt az ugyanazon tulajdonos viselt ékszerének
időlevonása is menthető. A társnyugta ilyenkor is pontosan egy pecsétet
és kizárólag ékszeridő-változásokat enged; egyidejű fejlesztést nem.

## Bónusz és repülés

A fix társ bónuszforrása a katalógus alap- és aktuális fejlesztési
profiljának, valamint a példány attribútumainak összege. Csak tényleges
élő vetület adhat bónuszt: pet követéskor, hátas felülve. Leszállás,
visszahívás, lejárat vagy tiltott világállapot megszünteti a forrást.
A dungeonhoz kötött profil csak dungeonban hat. Ismételt kérés nem
halmozza a forrást. A normál typed statösszegzés ellenőrzi a túlcsordulást.
A lovaglás alapstat-minimuma a bónusz előtti értékre hat: a typed
felszerelés/társ Erő-bónuszát ugyanúgy meg kell tartani, mint a később
alkalmazott legacy tárgybónuszt. Például 6 alap-Erő, 36-os hátasminimum
és +5 társ-Erő mellett a helyes eredmény 41.

A `033_companion_flight.sql` explicit `flying_mount` katalógusmezőt ad.
Csak Mount sor állíthatja igazra. A kanonikus katalógus **v27**;
a régi sorok alapértelmezett jogosultsága hamis. A szerkesztő petre
váltáskor törli és letiltja ezt a mezőt.

A saját típusos hátasnál a repülés a tényleges felült állapotot, az
élettartamot és a világkorlátozásokat ellenőrzi. Tiltott typed jogosultság
nem kaphat engedélyt egy legacy felszerelt tárgyból. A meglévő 221-es
repülési protokoll viszi a képességet, felszállást, leszállást és gyors módot
a tulajdonosnak és a megfigyelőnek. Lejáratkor a szerver elveszi a
hátasalakot és a repülési állapotot.

## Unity és protokoll

A CG249 továbbra is a pecsét példányazonosítóját és a négy felhasználói
művelet egyikét viszi. A karbantartás kizárólag szerveres művelet.

A **GC225 v12** a fix pecsét mellé óratípust, indulási állapotot,
időtartamot, UTC-határidőt, fennmaradó keretet és repülési engedélyt ad.
A v10 petellátmány-oldalmező megmarad; a korábbi változatokat a codec
továbbra is olvassa. A Unity részletezőben megjelenik az óra és a
repülés, UTC-határidőnél másodpercenként frissül a kijelzés.

A **GC231 v5** külön fix társforrást ad a számított statokhoz:
pecsétazonosító, pet/hátas, követés/felülés és a pontos bónuszösszeg.
A fejlődőpet-forrás ebben a változatban üres. A Unity statablak
megnevezi a forrást. A codec szigorúan ellenőrzi a típust, módot,
azonosítókat, darabszámot, valamint a csonkolt és többletbájtokat.

Az elvégzett próbák és a helyi telepítés bizonyítékai a
[C27b jelentésben](../reports/item_combat/2026-11-07_fixed_companions.md) szerepelnek.
