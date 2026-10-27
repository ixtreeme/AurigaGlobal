# Itemkatalógus — ékszerfoglalat és tartós feltöltés

Történeti v4 lelet. Az újabb fogyasztható kategória és aktuális tesztkör:
[v5 recovery jelentés](2026-10-27_recovery_consumables.md).

2026-10-27. Ág: `codex/mission-engine`; kiinduló HEAD: `ac4a7cf3`.
**Elkészült a foglalatprofil és a tiszta állapotátmenetek helyileg tesztelt
része. A teljes item/combat migráció és a futó szerver átállítása még tart.**

A továbbra is négy támogatott kategória mellé az ékszerek opcionális,
típusos foglalatprofilt kaptak. Egy tárgytípushoz egy prototípus tartozik;
a konkrét példány kapacitása, feltöltött fokozata, ideje és tartós jelzője
nem új prototípus. A normál +N fejlesztéstől külön állapot.

Elkészült:

- A foglalatnyitás és feltöltés explicit, validált profilja: saját maximum,
  esélyek, anyagköltség és ItemKey-alapú kompatibilitás.
- Időkorlátos és tartós feltöltés külön anyaglistával. A tartós mód nem
  különleges időértéktől függ; aktív állapotban a két mód nem keverhető.
- Viselés közben fogyó idő; levett tárgynál szünetel. Több lejárt fokozat
  egy lépésben rendeződik, a foglalatkapacitás megmarad. Az időlépések
  feldarabolása és a UINT64_MAX intervallum is ellenőrzött.
- Sikertelen próbálkozás változatlan állapotot és profil szerinti költséget
  javasol; az API nem dob új véletlent és nem módosít valódi inventoryt.
- Három SQL-tábla és optional accessory FK a friss fejlesztési sémában;
  a `protos` séma most 21 tényleges tábladefiníciót tartalmaz. Az audit
  fixture-táblák ebbe nem számítanak bele.
- A schema/hash v4, a hat prototípusos fixture és a független Python encoder.
  A forrássorrend nem változtatja meg a hash-t, a foglalatprofil tartalma igen.

A forrásaudit igazolta, hogy a tartós feltöltés a jelenlegi buildforrásban
engedélyezett funkció. A kapacitásnyitásnál 50%-os dobás van; a rendes
feltöltési ág közvetlen sikeres emelést használ. Ezeket nem a konstansok
nevéből következtettük ki. A részletes szerződés és forrásszimbólumok:
[IC-008](../../adr/item_combat_008_accessory_socket_profiles.md).

## Friss ellenőrzés

| Helyi ellenőrzés | Eredmény |
|---|---:|
| Normál CTest, a saját MariaDB-példánnyal | 9/9 |
| ASAN-build célzott CTest | 8/8 |
| Külön standalone build célzott CTest | 8/8 |
| Izolált MariaDB 11.4.13 tesztesetei a normál körben | 15/15 |
| Foglalatprofil/állapot/timer C++ ellenőrzések | 208 |
| Katalógus C++ ellenőrzések | 178 |
| Refinement / bonus / kulcs C++ ellenőrzések | 457 / 229 / 197442 |

A natív C++ célok ASAN-instrumentáltak; a Python és MariaDB folyamatokra
ez nem állítás. Az első ASAN-kör a túlméretes listát előállító új teszthelperben
use-after-free hibát jelzett: a vector `assign` saját elemreferenciát kapott
az újraallokáláskor. A másolt kulccsal javított fixture után a teljes célzott
kör ismét sikeres. Az első hibaleletet is megőriztük az ignorált buildmappában.

Build- és tesztnaplók: `build/item-accessory-sockets-20261027/`.
A források, naplók, függőségek, eredeti specifikáció és SQL-környezet
ujjlenyomatai a jelentés melletti
`2026-10-27_accessory_socket_profiles.evidence.json` fájlban vannak.
A korábbi jelentések/evidence fájlok a saját történeti pillanatukat őrzik.

## Ami még szükséges

Ez nem kész foglalatkezelés a futó játékban. Hátravan a konkrét itemállapot
ECS- és SQL-mentése, a viselési idő bekötése, az owner/revision ellenőrzés és
az anyag/item/operation-receipt atomi tranzakciója. A profil bónuszmetaadatait
a későbbi StatBuilder alkalmazza; az azonos BonusId-jű base és aktuális
refinement részeket előbb egyesíteni kell, hogy a minimum ne járjon kétszer.
A checked széles aritmetika, előjel/cap-szabályok és a régi APPLY-k teljes
szemantikai leképezése még nyitott.

A kategóriafelbontást a következő, forrásból ellenőrzött típusos kategóriákkal
és speciális profilokkal folytatjuk; majd a SQL-katalógusbetöltő és a konkrét
tárgypéldányok mentési/fejlesztési tranzakciója következik az
[IC-006 szerinti sorrendben](../../adr/item_combat_006_category_refinement_priority.md).
Teljes P0/P1/P2, offline összevonás, Unity/runtime és az új harcmotor nincs
késznek minősítve.

Csak helyi forrás/build és saját ideiglenes adatbázis változott. Meglévő
játékadatbázishoz, Unityhez vagy távoli géphez ebben a munkában nem nyúltunk.
