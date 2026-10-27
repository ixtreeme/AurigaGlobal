# Itemkatalógus — tárgyszerkesztő a szerverkezelőben

2026-10-27. Ág: `codex/mission-engine`. Kiinduló HEAD: `9dfb1def`.

A Qt szerverkezelő új **Tárgyak** oldala a kiválasztott helyi csomag saját
`protos` adatbázisát kezeli. Az öt betölthető kategóriában (fegyver, páncél,
ékszer, gyógyító ital, alapanyag) tárgy listázható, szűrhető, létrehozható,
szerkeszthető és kivezethető. Fizikai törlés nincs; a kategória és a helyi
azonosító létrehozás után nem változik, és nem kerül újrahasznosításra.

## Működés

- `item_page.py` — a felület. Lapok: alapadatok, használat és kereskedelem,
  kategóriaadatok, fejlesztés (+0…+N halmozott görbe, szintbónuszok),
  alap bónuszok. Nyers értékek pontban (1000 raw/pont), bp százalékban,
  ms másodpercben jelennek meg; a tárolt érték pontos egész marad.
  A legmagasabb szint növelésekor az új szint a felső szintet folytatja: a
  kitöltésnél megadott (megjegyzett) lépéssel, ennek hiányában az utolsó két
  szint különbségével; egyforma értéknél másol. Az egyenletes kitöltés statot
  és szintbónuszt is tölt (kezdőszint, kezdőérték, lépés), élőben a táblázatba;
  bezáráskor megmarad, csak a „Visszavonás” állítja vissza.
- `item_catalog.py` — dokumentum ↔ tábla modell. Megosztott profilt sosem
  szerkeszt helyben: másolat készül (copy-on-write), a szerkesztés által
  használaton kívül került profil a gyermeksoraival együtt törlődik.
  Új dokumentum nem írhat felül kiadott tárgykulcsot.
- `CatalogEditorTool` — natív segédprogram, a játék saját
  `DecodeSnapshot`/`BuildItemCatalog` ellenőrzőjével. Műveletek: `read`,
  `commit`, valamint adatbázis nélküli `validate` és `blank`. A hibát
  szerkezetesen adja vissza; a kezelő ebből magyar üzenetet készít.

Mentéskor a teljes új katalógus validálódik, majd egy tranzakcióban,
`GET_LOCK` és a várt tartalomhash mellett íródik. Új tárgynál a
`item_local_id_sequence` sor `FOR UPDATE` ellenőrzése és léptetése ugyanabban
a tranzakcióban történik. A meglévő sorokat sima `UPDATE` frissíti; a korábbi
`INSERT … ON DUPLICATE KEY UPDATE` egy egyedi kód ütközésekor egy másik sort
írhatott volna felül. A mentés a visszaolvasott hash-sel zárul. Minden mentés
nyugtája: `run/catalog-editor/<időbélyeg>/`.

Mentéshez a helyi játékszervernek állnia kell, az adatbázisnak futnia; az oldal
**Adatbázis indítása** gombja csak a MariaDB-t indítja. Távoli célgépen az oldal
nem szerkeszt.

## Ellenőrzés

| Helyi ellenőrzés | Eredmény |
|---|---:|
| Teljes normál CTest (`catalog_editor`, `catalog_editor_mariadb` új) | 100/100 sikeres |
| `CatalogEditorTests.py`: modell, natív validálás, Qt űrlap, saját MariaDB | 13/13 sikeres |
| Szerverkezelő GUI-tesztek eldobható csomagmásolaton | 14/14 sikeres |
| Becsomagolt exe: worker `status`, beépített `CatalogEditorTool` | sikeres |

A tesztek lefedik: mind az öt kategória kezdő dokumentuma érvényes; minden
űrlapmező pontosan visszaolvasható; megosztott használati, támadási,
fejlesztési és ital-várakozási profil szerkesztése nem érint más tárgyat;
+30-as görbe egy prototípussal; szintbónusz saját profilt kap és törléskor
eltűnik; elavult olvasásra épülő mentés, kihagyott vagy már kiadott helyi
azonosító és fizikai törlés elutasítva; hibás dokumentumból semmi nem íródik.

A `quest_api_catalog.py` újragenerálva (a DragonSoul `ds` modul két sora
kikerült, a `9dfb1def` commitnak megfelelően), mert az exe-build enélkül leállt.

## Határ

A GameServer még nem a `protos` katalógusból tölt; a szerkesztett tárgyak a
játékban addig nem jelennek meg. A foglalatprofilok (ékszer) csak választhatók,
tartalmuk itt nem szerkeszthető. A többi kategória, a példányállapot és a
Unity-bekötés a korábbi jelentésben leírt sorrendben hátravan.
