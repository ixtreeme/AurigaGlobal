# ADR IC-006 — Kategóriabontás és a +9-es plafon megszüntetése az elsődleges cél

2026-10-27. Forrás: a felhasználó pontosítása a jelen munkamenetben.

A módosítás elsődleges termékcélja a régi `item_proto` kategóriákra bontása és
a beépített +0..+9 fejlesztési lánc kiváltása. A következő fejlesztési lépéseknek
ezt kell előrevinniük. A korábbi teljes terv harcrendszeri követelményei
megmaradnak; az önmagukban álló további bónuszbővítések nem előzik meg ezt a két célt.

## Elfogadott viselkedés

- `protos.item_base`: közös adatok; kategóriatáblák: saját, típusos mezők.
- Egy szemantikai tárgytípus egy ItemKey és egy prototípus. A +0, +9, +10,
  +15 vagy +30 állapot ugyanahhoz a prototípushoz tartozik.
- A példány tárolja a fejlesztési szintet, a profil a megengedett szinteket és
  növekményeket. Nincs általános `level >= 9` leállítás vagy következő vnumra váltás.
- A gameplay maximum profilonként állítható: például fegyver +30, páncél +15,
  nem fejleszthető tárgy 0. Ezek példák, nem az összes meglévő tárgy balance-át
  átállító automatikus módosítások.
- A korábbi +9-es fixture egy megengedett profilt tesztel. Az új, +9-en túli
  esetek bizonyítják, hogy ez nem univerzális korlát.
- A játékos tárgyszerzése konkrét példányt hoz létre külön ItemInstanceId-val.
  Két játékos ugyanazon típusú tárgya ugyanazt az ItemKey-t használja, de saját
  fejlesztési szintjük van. Egyik fejlesztése nem módosítja a protót vagy a másik
  példányt. A szintenkénti profiladat nem új tárgyprototípus.
- Az item_base és a hozzá tartozó kategóriasor ugyanazon kulcs két részlete,
  együtt egy logikai prototípus. A régi +0..+9 sorcsaládot az offline migráció
  erre az egy kulcsra vonja össze, a játékosok meglévő szintjeinek megőrzésével.
  Az összevonás szemantikai típus szerint történik; pusztán azonos név nem elég.

A jelenlegi, specifikációból átvett technikai profiltartomány 0..255;
a tárolt típus uint16/SQL SMALLINT UNSIGNED. A tárgy tényleges maximumát ezen
belül saját profilja szabja meg. Ez nem végtelen szinttartomány: a technikai
tartomány további bővítése következetes schema/codec/instance/UI munkát igényel.
A jelen pontosítás a rögzített **+9** játékszabályt váltja ki.

## Következő munka sorrendje

1. Hiányzó kategóriák és a hozzájuk tartozó tényleges profilok; a régi adatok
   veszteségmentes besorolási leltára.
2. SQL-katalógusbetöltő és publikálható fejlesztési profilok teljes 0..max görbével.
3. Item-instance mentés és szerverautoritatív fejlesztési tranzakció: szint,
   revision, alapanyag, pénz, esély/kimenetel, idempotencia és visszatöltés.
4. Inventory/trade/shop/crafting/quest/Mission és Unity közvetlen átállítása.
   A +N kijelzés kezeljen két-/háromjegyű szinteket is; a stat, tooltip, ikon
   és glow explicit profilból jöjjön.
5. Offline konverzió: korábbi +0..+9 protócsalád → egy kulcs és megőrzött
   példányszint. +10-től nem készül új proto. A tágabb terv harcmagja és
   integrációs kapui a koherens kiadás előtt továbbra is teljesítendők;
   nincs runtime vnum-adapter vagy részleges kiadás.

## Ellenőrzési kapu

A `NextRefinementLevel` a profilból oldja fel a következő szintet; ez még nem
gazdasági vagy perzisztens fejlesztési művelet. Kötelező a +9→+10, +15 és +30
próba ugyanazon ItemKey mellett, független maximum két kategórián és hiányzó
+10-es lépcső elutasítása. Ezt C++ domain-, katalógus- és izolált SQL-próba
ellenőrzi. A későbbi instance/Unity integrációban a fejlesztés → mentés →
újrabelépés → megjelenítés azonosságát is bizonyítani kell.

## 2026-10-27 — részleges SQL-kapu teljesítve

Az öt már implementált kategória SQL-betöltője elkészült. Valódi, saját
MariaDB-fixture-ből is ellenőrzött a +9→+10→+30 görbe és a hiányzó +10
elutasítása, szintenként új proto nélkül. A teljes kategórialeltár és runtime
csere ettől még nyitott. [IC-011](item_combat_011_catalog_sql_reader.md)
rögzíti a read-view/ownership/format határt; a következő kapcsolódó kapu
a tartós instance- és fejlesztési művelet.
