# ADR IC-033 — Fejlesztési receptek szerkesztése és a Tárgyteszt próbapad

2026-10-29. Fázis: F3 → F4 előtti próba ([IC-012](item_combat_012_item_runtime_first.md)).
Állapot: **implementált; saját MariaDB-n és a helyi csomagon ellenőrizve**.
Előzmény: [IC-032](item_combat_032_item_persistence.md) (tartós példánymentés).

## Probléma

- A tartós mentés (IC-032) kész, de a GameServer még nem használja (F4).
  Az új tárgyrendszert így játékon kívül sem lehetett kipróbálni.
- A tárgyszerkesztőben nem lehetett fejlesztési receptet megadni. Recept
  nélkül nincs fejlesztés, mert a hiányzó recept nem jelent ingyenes
  fejlesztést.

## Döntés

### Receptek a tárgy dokumentumában

- A recept a **forrástárgyához** tartozik. A tárgy megnyitásakor a saját
  receptjei is betöltődnek, mentéskor együtt íródnak ki.
- **Fejlesztés lap, „Recept” oszlop.** A +N sor a +N-1 → +N lépés feltétele:
  Yang-díj, sikeresély, kudarcszabály (megmarad / megsemmisül / visszaesik
  egy alacsonyabb szintre) és hozzávalók.
- **Egyenletes kitöltés.** A Yang-díj és a sikeresély is kitölthető vele.
  A hiányzó szintekre a legközelebbi recept másolata kerül, ugyanazokkal a
  hozzávalókkal.
- **A „+” gomb** a felső két recept különbségével (vagy a megjegyzett
  növekménnyel) folytatja a sort. A visszaesés távolsága megmarad.
- **Átalakítások** (például kard +9 → hosszú kard +0): külön ablakban
  szerkeszthetők. A példány azonosítója, bónuszai és kövei megmaradnak.
- **Azonosítók.**
  - Mentett recept megtartja az azonosítóját és a kódját.
  - Másolt vagy új tárgy mindig új azonosítókat kap. Az önmagára mutató lánc
    a másolat saját kulcsára kerül.
  - Kivezetett tárgy nem kínál receptet.
  - Ha egy régi dokumentumból hiányzik a receptszakasz, a receptek
    változatlanok maradnak.
- **Ellenőrzés.** A végső ellenőrzést a játék natív katalógusépítője végzi
  (`ValidateRefinementRecipe`). A felület csak a gyakori hibákat jelzi előre,
  magyarul.

### Tárgyteszt (szerverkezelő, új lap)

- **Natív eszköz.** `ItemBenchTool`. A játék saját tervezőit
  (`PrepareRefinement`, `PlanAttributeService`, `PlanSocketService`) és a
  tranzakciós írót (`CommitItems`, `LoadItemOwner`) hívja. A felület SQL-t
  nem épít, kimenetelt nem dönt el.
- **Egy dedikált tesztkarakter**, a #990001 (munkamenet 1; inventory 5×9,
  4 oldal). A régi `player.item`-hez és a karakterekhez nem nyúl.
- **Műveletek:**
  - adás (darab, szint);
  - áthelyezés;
  - megsemmisítés;
  - Yang beállítása;
  - fejlesztés: véletlen, kényszerített sikeres vagy kényszerített sikertelen
    dobással;
  - bónusz- és foglalatszolgáltatás.
- **Telepítés.** A 9 additív mentési tábla a helyi `player` adatbázisba kerül.
  - Feltételek: a játékszerver le van állítva, és egyik tábla sem létezik még.
  - Előtte teljes `player` mentés készül
    (`run/item-bench/<stamp>/player-before.sql`).
  - Ha a telepítés nem sikerül, pontosan a létrehozott táblák törlődnek.
- **Katalógusváltás.** A Tárgyak lapon végzett mentés után a próbapad átáll
  az új katalógusra, a leállított kiadó szerepében. Ennek feltételei:
  - a próbapad az egyetlen tulajdonos a mentési táblákban;
  - **minden** tárolt példány hibátlanul betölthető és érvényes az új
    katalógussal.

  Ha bármelyik feltétel nem teljesül, a régi katalóguskötés visszaáll, és
  a felület visszaállítást kér.
- **Kapcsolat.** Csak a kiválasztott csomag saját, ellenőrzött helyi
  adatbázisa, a csomag admin fájljával. Távoli gépen a lap nem működik.

## Határ

- **Nem játékbeli működés.** Nincs ECS, kliens, harc vagy bolt. Ez az F4.
- **A próbapadon még nem elérhető:**
  - felszerelés;
  - kő berakása és kiemelése (visszaadott kő);
  - a mentés első fázisában nem támogatott kategóriák (IC-032).
- **A katalóguskötés átállítása** csak a helyi, egytulajdonos próbapad
  kényelmi funkciója. Éles környezetben továbbra is a leállított kiadó
  feladata.
