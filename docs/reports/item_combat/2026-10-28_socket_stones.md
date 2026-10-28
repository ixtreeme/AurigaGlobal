# Foglalatkő-kategória — megvalósítás és helyi ellenőrzés

2026-10-28, `codex/mission-engine`. Kezdő HEAD: `bedd499e`.
A folytatás előtt a teljes korábbi munkafa három commitba került:

| Commit | Befejezett munkafolyamat |
|---|---|
| `0c71bfd4` | Natív katalógus, hatástípusok, könyvek, questek, fejlesztőreceptek, Qt szerkesztő |
| `acdc957b` | Ellenőrzött offline mentés, visszaállítás, leltár és részkonverterek |
| `bedd499e` | Auditok és helyi ellenőrzési bizonyítékok |

A commitok előtt a friss teljes tesztkör 110/110 sikeres volt; a munkafa
a három commit után tiszta volt. Ezután kezdődött a foglalatkő-kategória.

## Új eredmény

- Natív 7-es kategória és típusos SQL-sor: behelyezhető kő vagy kőszilánk,
  fegyver/testpáncél célmaszk, minimum foglalatminőség, két kizárócsoport.
- Típusos zárt, üres, beépített és törött foglalat. A beépített kő kulcsa és
  fejlesztési szintje explicit; nincs nyers item-ID-ként tárolt sentinel.
- Szerveres, mellékhatásmentes behelyezési terv teljes bemeneti/revision-
  előfeltételekkel, egy darab fogyasztásával, rögzített döntéssel és tartalom-
  azonosítókkal. Mind a négy keresztcsoport-egyezés tilt, a törött foglalat
  nem írható felül. A célfelszerelés nem lehet felszerelve vagy cserében.
- A kő új, bónuszalapú fejlesztési profilja ugyanazon prototípust viszi +30-ig
  a tesztben. A közös profil maximuma továbbra is 0..255, nincs +9-korlát.
  A közös gazdasági recept is kezeli az egy darabra különválasztott követ;
  egy többdarabos halom fejlesztése elutasított.
- Natív validátor, hash, SQL-betöltő, tranzakciós mentés és Qt űrlap.
  Másoláskor a közös fejlesztési görbe nem íródik át. Kőszilánknál a
  behelyezési/fejlesztési/bónuszmezők inaktívak, a mentés hatásmentes sort ad.

## Valódi forrásadatok auditja

A hash-ellenőrzött mentésben 189 régi ITEM_METIN prototípus van:
185-nél azonosított a behelyezési cél, 1 a régi kőszilánk (28960), 3-nál
nincs használható célmaszk. 14 sor két kizárócsoportot hordoz. Mindössze
8 sor tartalmaz refined_vnum kapcsolatot és refine_setet, ezért a régi
kövek teljes +N családjait a kapcsolatok automatikus követése nem állítja
helyre. A kizárócsoport önmagában sem családazonosító.

A soronkénti hash, szintfelismerés és csoport/cél-metaadat a kísérő
`2026-10-28_socket_stones.audit.json` fájlban van. Ez audit, nem telepíthető
konverzió vagy runtime lookup. A régi szintenkénti árak, apply-k, PK-drop/
logolási és egyéb policy-adatok átvitele továbbra is a teljes konverter dolga.

## Ellenőrzések

| Ellenőrzés | Eredmény |
|---|---:|
| Új natív foglalatkő-teszt | 85 ellenőrzés sikeres |
| Teljes normál CTest, a kategória/séma/szerkesztő bővítése után | 111/111 |
| Végső gazdasági receptbővítés utáni célzott normál CTest | 7/7 |
| Végső célzott ASAN domain/dekóder/konverter/hash kör | 8/8 |
| Qt + modell + natív SQL mentés saját MariaDB-n | 45/45 |
| Ugyanez ASAN helperrel | 45/45 |

A +30 görbe SQL-visszaolvasása, másolása és módosítása tesztelt. Egy
adatbázis-trigger szándékos íráshibát okoz: a teljes mentés, a profilok és
a kategóriaszámláló léptetése is visszagördül. Külön vizsgált az azonos
instance, idegen tulajdon, zár/csere/felszerelés, revision, halom/szint,
zárt/törött/sérült foglalat, csoportütközés és hiányzó/hibás rögzített döntés.

A v14 kódolást független Python encoder ellenőrzi. Kitöltött kőtábla és
+30 profil fixture hash:
`e3e6f8207e067f31ae3143be9f2e0f9ac96302ccfa67c5f4eae5e486051e9376`.

A megőrzendő valódi felhasználói katalógus v7→v14 offline felkészítése,
új saját MariaDB-be importja és natív visszaolvasása sikerült; a kiadott
tárgykulcs és számlálók megmaradtak. Az aktív katalógus nem lett lecserélve.
Hash: `1505725c26666c5a4d28a3c4a293a34bc15e9e5c05be04b9473088dad46c18fb`.

## Pontos határ és folytatás

Ez F2 domain/katalógus/szerkesztő-bővítés. A működő helyi játék továbbra is
a régi modellből fut; a 189 kő és a játékosok beágyazott kövei még nem
konvertált runtime adatok. Database/GameServer/Unity, a tartós gazdasági
író, a kiemelő/tisztító tárgyak és a statbekötés az IC-012 további lépései.

Az F2-ből még hiányzik többek között a kosztüm, eszköz/halászat, polimorf,
külön ITEM_SOUL, pet/mount és speciális tárgyak teljes lefedése. Az általános
policy-k és a régi bónuszok teljes megfeleltetése is publikálási kapu.
A következő lépés ezek folytatása, majd a teljes offline konverzió és
adatvesztés nélküli helyi játékbekötés. Távoli művelet és Unity-módosítás
ebben a körben nem történt.
