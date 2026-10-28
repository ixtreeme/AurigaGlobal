# Helyi tárgykonverzió — állapot, 2026-10-28

**A teljes konverzió és a játékbeli átállás nincs kész.** A GameServer,
Database és Unity még a régi tárgyadatútvonalat használja. Az aktív helyi
adatbázis továbbra is v7, egy korábban létrehozott új katalógustárggyal.
Távoli gépen nem történt művelet.

## Elkészült

- Teljes, konzisztens helyi logikai mentés és a statikus szerveradatok másolata.
- Visszaállítás új MariaDB-be: **145 tábla** darabszáma egyezik;
  **55 konverziós bemeneti tábla** metaadata és minden cellája is azonos.
- Determinisztikus, teljes prototípus-/példány-leltár és családtervezet.
  Azonos snapshotból megismételve az eredmény megegyezik.
- Normál kovácsfejlesztési receptek új kulcsokra/szintekre vezetett tervezete.
- Natív, típusos receptmodell, validátor, SQL-séma/betöltő, kanonikus hash,
  admin-tool mentés és tiszta fejlesztési tervkészítő; részletek: IC-019.
- A meglévő új katalógustárgy v7→v12 konverziójának natív validálása,
  külön adatbázisba importálása, hash és kiadott számlálók visszaellenőrzése.
  Az eredeti tárgy és adatbázis változatlan.

## Teljes helyi forrás

| Adat | Darab |
|---|---:|
| Régi item_proto sor | 10 714 |
| Jelölt szemantikai prototípuscsalád | 5 243 |
| Jelölt összevonással kiváltható ismételt sor | 5 471 |
| Jelenlegi működés szerint besorolt forrássor | 9 260 |
| Karantén/vizsgálandó forrássor | 1 354 |
| Kivezetett tartalom, kizárólag archívumban | 100 |
| player.item példány | 3 084 |
| Account hátastároló példány | 5 |
| Céhraktár példány | 41 |
| Összes példány | **3 130** |
| Összes tárgydarab ezekben | **320 077** |
| Régi refine_proto recept | 1 175 |
| Új normálkovács-lépés tervezete | 4 532 |
| NPC-bolti sor | 922 |
| Itemshop-tartalomsor | 517 |
| Extra proto/ritkaság sor | 312 |

A prototípuscsaládok **jelöltek**, nem elfogadott végleges összevonások.
Minden család felülvizsgálati jelzője false, a teljes terv installable=false.
A már kivezetett DS tartalom nem kerül vissza. A két meglévő talizmánpéldány
karanténjelzést kapott, nem törlődött.

## A forrásban talált problémák

- **46 önhivatkozó refined_vnum**. Ezekből nem készül új fejlesztési lépés.
  A régebbi „0 körbefutó lánc” lelet nem fedte le ezt a külön esetet.
- **295 családjelölt** hiányzó kezdőszintet vagy lépcsőt jelez. Ezek részben
  policy-/név-/méretváltozás miatt szétváló régi láncok; nem állítjuk, hogy
  minden ilyen család forrásadata ténylegesen hiányzik.
- A jelöltként megmaradó normál kovács tartalmában **8 lépéshez nincs recept**:
  5149, 21079, 59020–59025. Nem kapnak kitalált árat vagy esélyt.
- **10 jelölt normál lépés** alapanyagja hiányzik vagy nem használható.
- A teljes nyers recepttáblában három hiányzó alapanyag-hivatkozás és egy
  nulla hely után megadott alapanyag is van. Nem mind használt recept.
- **46 relációs hivatkozás** nem elérhető tárgyra mutat (bolt, árlista stb.).
- **49 példány tulajdonosa hiányzik**: 8 player.item játékos-tulajdonos,
  valamint mind a 41 céhraktári sor céhe. A visszaállított snapshotból
  ellenőrzött owner-listák alapján kaptak karanténjelzést; egyik sem törlődött.
  Az eredeti `D:/AurigaGlobal/Database/player.sql` guild adatszakasza is üres;
  a 41 céhraktári sor már ebben a mentésben létező céh nélkül szerepel.
- A bot/soul/jelmez és további külön fejlesztések nem normálkovács-receptek;
  az átvezetésük saját szabályportot igényel.

Az item_extra_proto, award, bolt és árlista sorok tételes forráshasht és
új kulcs/szint hivatkozási tervezetet kapnak. Ez **nem** az összes payload,
ritkaság, affix, socket, lejárat vagy jutalomnyugta befejezett konverziója.
Az épület-alapanyagok, quest flag-ek, affect-ek és petállapot külön szemantikai
konverziós kapu; nem végzünk globális számcserét.

## Még szükséges az átálláshoz

1. A teljes typed kategória-/viselkedésport és a családok összes eltérő
   mezőjének kezelése. A meglévő kilenc kategória még nem fed le minden
   lőszert, jelmezt, övet, foglalatkövet, quest/társ/horgász/eszköz/soul tárgyat
   és használati műveletet. A szintfüggő ár/limit/ritkaság sem dobható el.
2. Elfogadott családtérkép; hiányzó, hibás vagy inaktív tartalom kezelése
   a korábbi IC-012 adatmegőrzési döntés szerint. +0 adatok nem találhatók ki.
3. A példányok teljes típusos állapota, tartós mentése és a fejlesztési
   operation receipt + wallet + alapanyag + source egyetlen commitja.
4. A GameServer/Database gyártás, stat, használat, inventory, tároló, trade,
   bolt/itemshop, drop, crafting, quest és Mission közvetlen átállítása.
5. Unity/protokoll/klienskatalógus átállítása ItemKey-ra és példányszintre;
   teljes asset-/lokalizáció-/szkript- és egyéb tartalomhivatkozás-konverzió.
6. Teljes új katalógus natív validálása és adatmegőrzési ellenőrzése; végső
   helyi checkpointból átállás, majd bejelentkezés/inventory/fejlesztés/
   újrabelépés/bolt/trade/drop/quest próba.

Ezek a feladatok már a kért helyi átállítás részei, nem új jóváhagyásra várnak.
A régi runtime mellé nem készült vnum-adapter vagy kettős item-írás.

## Ellenőrzések

- ServerTests build sikeres. A 108 cél teljes futásában 107 sikeres volt;
  a fennmaradó SQL-loader teszt régi verzió-visszaállító fixture-ét javítottuk,
  célzott újrafuttatása sikeres. A módosított célok végső futása is sikeres.
- **180 C++ fejlesztésirecept-ellenőrzés**: +9→+10, max, átalakítás,
  megtartás/megsemmisítés/visszalépés, tulajdon és revision, részhalmok,
  zárolás, hiányzó pénz/alapanyag, forrás önfogyasztásának tiltása,
  garantált/rögzített dobás és hash/SQL snapshot.
- **44 Python offline konverziós/upgrade teszt** sikeres.
- **39/39** natív helper + model + Qt + izolált SQL teszt sikeres,
  normál és ASAN helperrel is.
- **19/19** kiválasztott ASAN item/domain/loader-completion/generator teszt
  sikeres. A receptet is tartalmazó új hash-fixture külön, független Python
  bájtkódolóval ellenőrzött.

Az új alap-fixture hash:
`6d386c17abdde5c9cd5f1d605ba72b65b82b6d117252fb5a8dc15dff62a62ecb`.
Ez fejlesztési fixture, nem a teljes régi tartalom új hashje.

## Privát helyi eredmények

Nem kerültek Gitbe játékosadatok, mentés vagy hitelesítési adatok.

- Snapshot: `D:/AurigaGlobal/Server2-Local/run/item-migration/20261028-source-v2/`.
- Teljes tervezet: `.../run/item-migration/20261028-full-proposal-final.json`.
- User-katalógus próbafrissítés: `.../run/item-migration/20261028-user-catalog-v12.json`.
- Visszaállítási eredmény: `build/item-migration-final-restore-20261028/restore-verification.json`.
- Tulajdonoskulcsok: ugyanott `ownership-supplement.json`, a mentés SHA-256-jához kötve.
- Próbaimport: `build/item-migration-user-catalog-stage/catalog-stage-result.json`.
- Az ellenőrzött fájlok és eredmények ujjlenyomatai a mellette levő evidence JSON-ban.
