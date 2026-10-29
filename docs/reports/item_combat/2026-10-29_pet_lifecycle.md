# Fejlődő petek — összevont életciklus és ellátmányok

2026-10-29, `codex/mission-engine`. Kiinduló HEAD: `e3693f7f`.
[IC-029 szerződés](../../adr/item_combat_029_pet_lifecycle.md).

## Egyben elkészült

- Közös, típusos petpayload: név, születés és monoton megőrzött kor,
  progression, születési tulajdonságok, négy képességhely és rang,
  életidő, szállítóhely, első idézés és visszaidézési szándék.
  Ez az eddigi részállapotok aggregátuma, nem második írható igazságforrás.
- Fejlődő pet külön idézési/visszahívási terve, csatornafoglaltság és
  világpolitika ellenőrzésével. Az újraidézés nem tölti vissza az időt,
  az aktív petre ismételt kérés nem hoz létre újabb követőt.
- Egy karbantartási tervben aktív életidő, valós kor és képességnyitás.
  A kor offline is nő, az aktív idő offline nem fogy. UTC-visszalépés
  nem csökkenti a megőrzött kort és nem zár vissza helyet.
- Evolúció a progressionnel, képességhely-nyitással és rögzített
  traitnövekményekkel együtt. A sima EXP-szintlépés nem dob új bónuszt.
- Kiválasztott helyre biztos tanulás és külön engedélyezhető, már ismert
  skillt fejlesztő könyvolvasás. Utóbbinál sikeresély, owner+skill cooldown
  és a várakozást átugró affect revisionje is a terv része. Sikertelen
  könyv/enchant fogyasztása külön, commitolható eredmény.
- Egy vagy minden skill resetje, megőrzött zárolásokkal; konfigurálható
  skillek és 1..255-ös rangtáblák. Ugyanaz a skill nem tanulható két helyre.
- Korhoz, születési tulajdonsághoz és skillranghoz kötött passzív
  hozzájárulások. Egy forrás teljes csomagját kell cserélni. Lejárt,
  visszahívott vagy tiltott petnél üres csomag távolítja el a bónuszt.
  A régi tizedszázalékos trait egész `/10` csonkolása megmarad.
- Életidő-növelő, explicit korlátlanító és tulajdonságnövelő ellátmány.
  A korlátlanító tartós grantet rögzít; nem pusztán átírja a protót.
- Szállítódoboz: a teljes petállapot és ugyanaz a hívópéldány marad meg.
  Nincs socket/attr szűkítés, újragenerálás vagy idő-visszatöltés.
  Kivételhez foglalt inventory-hely kell. Foglalt doboz nem etethető meg.
- Pet skin: külön forma és négyféle időszámítás, az általános itemóra
  matematikájának újrahasználatával. A megjelenés nem ír petstatot;
  lejáratkor az evolúciós forma tér vissza.
- Verziózott, explicit little-endian petpayload-codec; szigorú mező-,
  kulcs-, UTF-8-, verzió- és EOF-ellenőrzés. Nincs struktúramemória
  szerializálás, régi itemazonosító-feloldás vagy klienspacket-váltás.
- SQL v22, a friss 000..028 láncban 77 natív katalógustábla. A képesség-,
  kor-, helynyitási és ellátmányszabályok típusos táblák. A 17-es kategória
  jelenleg kizárólag az 1-es, névvel regisztrált petellátmány-viselkedést
  engedi; más különleges item nem kerül be feldolgozatlan értéktömbként.
- Qt-szerkesztés: korbónusz, helynyitás, skillek/rangok, mind a nyolc
  ellátmánytípus, sikeresélyek, időkeret, enchant és skin. Másoláskor eltérő
  profil nem írja át az eredetit. Inaktív mező piszkozata nem mentődik.
  A képességrang értékének HP/% egysége célváltáskor is követi a képességet.

## Ellenőrzés és állapot

| Ellenőrzés | Eredmény |
|---|---:|
| Natív életciklus és teljes petút normál / ASAN alatt | 214 / 214 |
| Teljes szerver CTest, egymásutáni végső futás | 131/131 |
| Célzott ASAN CTest, független hash- és payload-encoderrel | 18/18 |
| Teljes Qt/modell/izolált SQL-kör normál helperrel | 70/70 |
| Ugyanez ASAN helperrel | 70/70 |
| Friss SQL-séma belső tesztjei | 38/38 |

A teljes petút keltetésből indul, idézést, evolúciót, tanulást, etetést,
csomagolást, byte-mentést, visszaolvasást és kicsomagolást ellenőriz.
Minden payload-prefix csonkítását, ismeretlen verziót, kulcsütközést,
dupla skillt, esélyhatárokat, cooldown-túlcsordulást, sikertelen könyvet,
maximális traitet és idegen/zárolt/viselt forrást is vizsgálunk.
Két külön gyermekírási hiba ellenőrzi a teljes SQL-rollbacket,
beleértve a prototípust, profilokat, gyermeksorokat és kategóriaszámlálót.
A Qt-tesztek az el nem kapott signal-kivételeket is hibának tekintik.

A teljes szerver CTest első párhuzamos futásaiban a meglévő
`mission_persistence_mariadb` teszt időszakos hibát adott. Külön futásban
és a katalógus-SQL teszttel együtt is volt sikeres ellenőrzése. A diagnosztika
most az eredeti hívási helyet és az SQL-hibakódot/műveleti eredményt is
kiírja, hitelesítési adat és SQL-payload nélkül. Ez a diagnosztika önmagában
nem a párhuzamos hiba okának javítása. A végső teljes, egymásutáni futás
131/131; a párhuzamos instabilitás behatárolása nyitott. Tesztet nem
kapcsoltunk ki és assertiont nem gyengítettünk.

A katalógus független Python és natív C++ fixture-hashe:
`27c12aad5fc53d42f2a0a61d7187e995e1ee2306964d25dd4e463392e142fc55`.
A teljes petpayload byte-jait külön Python encoder is ellenőrzi.
Az űrlapokat Segoe UI betűvel képen ellenőriztük. A próba Windows/MSVC
alatt futott; FreeBSD- és Unity-játékteszt itt nem történt.

A meglévő egytárgyas v7 felhasználói katalógus v22-be másolása külön
MariaDB-sandboxban, kulcs/cella/számláló-megőrzéssel ellenőrzött:
`c17763bd638d025385324963453f13f3de8de738f5981f1b2f6a6be331016474`.
Ez nem a teljes régi item_proto konverziója. Az aktív helyi adatbázis mind
a 33 táblája változatlan v7, egy felhasználói prototípussal. Telepített appot,
játékot, Unityt és távoli gépet nem módosítottunk.

## Forrásleletek és fennmaradó kapuk

A 12 jelenlegi skill passzív; a kikapcsolt aktív skillágat nem hoztuk vissza.
Az első három hely evolúcióhoz, a negyedik korhoz kötött. Az első idézés
régi `evocation` mezője külön adat; nem következik a pillanatnyi aktív
actorból. A szállító régi szűkített UI-mezőiből nem állítható vissza a pet.

A rögzített mentésben nincs valódi fejlődő petállapot vagy érintett instance.
Ezért a teljes út tesztje szintetikus fixture, nem meglévő petek igazolt
konverziója. Az 5 tojáscél- és 11 quest/native dispatch-ütközés nyitott;
a resetkönyvek tényleges questbekötése külön ellenőrzendő. A leltár minden
forrássorát `converted=false` állapotban hagyja.

**Ez a csomag még nem kapcsolja át a futó játékot az új itemrendszerre.**
A codec sem telepített `player.item` writer vagy Database cache. A tényleges
F3/F4 tulajdonosnak kell az inventoryt, a petaggregátumot, a képességhez
tartozó owner-cooldown/bypass-t, az erőforrásokat és receiptet egyetlen
revisionvédett műveletben kezelnie. A statíró és a követő előkészítése/
publikálása is F4; spawnhiba nem hagyhat fiktív aktív petet vagy zárolt tárgyat.
CommitUnknown esetén tartós operation ID szerinti egyeztetés kell.

A következő nagyobb itemegység a még megmaradó tárgyon végzett műveletek
és különleges viselkedések lezárása, majd az F3 példánymodell és tartós
gazdasági commit. Hátravan az F4 GameServer/Database fogyasztók átállítása,
F5 protokoll/Unity, F6 teljes offline konverzió és F7 helyi játékpróba is.
