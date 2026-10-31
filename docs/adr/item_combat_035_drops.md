# ADR IC-035 — Drop az új tárgyrendszeren

2026-10-31. Fázis: F4 ([IC-034](item_combat_034_runtime_integration_plan.md),
C csomag, „drop”). Állapot: **döntés elfogadva; a C7a (b92fe955) és a
C7b-1 kész; a C7b-2 és a C7b-3 hátravan**.

## Régi működés (forrásaudit)

- **Sorsolás** (`ITEM_MANAGER::CreateDropItem`), hat forrásból:
  - közös drop a szörny rangja és a gyilkos szintsávja szerint;
  - fajonkénti dropcsoportok;
  - „kill-per-drop” mobcsoportok (egy nyeremény);
  - szintes csoportok;
  - dupla drop (prémium, kesztyű, `APPLY_DOUBLE_DROP_ITEM`);
  - egyéb: a szörny saját tárgya, metinkő, lókönyv, lottó, eseménydropok.
- **Szorzók** (`GetDropPct`): a szintkülönbség, a `PRIV_ITEM_DROP`
  privilégium, a dupla tárgy egyedi felszerelés és az `ITEM_DROP_EVENT`.
- **Kiosztás:** az `ENABLE_DROP_INSTANT_INVENTORY` be van kapcsolva.
  - A zsákmány a jogosult inventoryjába kerül, a meglévő halmokba egyesítve.
    Partyban a jogosult körforgással változik.
  - Ha nem fér el, a földre esik, és 60 másodpercig csak a jogosult veheti
    fel (`__GiveRewardItemToCharacterOrDrop`).
  - Bizonyos dungeonokban mindenki saját példányt kap (HWID-szűrés).

## Döntések (a felhasználó, 2026-10-31)

1. **A droptáblák a `protos` katalógusba kerülnek**, a recept- és a
   zsákmányprofilok mintájára: benne vannak a katalógus-hashben, a Tárgyak
   oldalon szerkeszthetők, és az F6 konverter ide írja a régi táblákat.
2. **Tele inventorynál a zsákmány a földre esik, 60 másodperces joggal**, ahogy
   ma.

## Szeletek

- **C7a — futásidejű azonosítók és tárgyadás.** Ez az alap: a drop, a bolt, a
  crafting és a quest-jutalom is új tárgyat hoz létre.
- **C7b — droptáblák a katalógusban és sorsolás a szörny halálakor.** A régi
  szorzók megmaradnak; a jutalom a jogosult inventoryjába kerül.
- **C7c — földi zsákmány tele inventorynál és felvétel; a játékos saját
  tárgyának eldobása.** A javaslat: a földi zsákmány a régihez hasonlóan nem
  tartós, a tárgypéldány a felvételkor jön létre. Ezt a C7c előtt egyeztetjük.

## C7b — droptáblák a katalógusban (C7b-1 kész)

Három lépésben készül:
- **C7b-1:** katalógusréteg és tiszta sorsoló;
- **C7b-2:** szerkesztőfelület;
- **C7b-3:** bekötés a szörny halálába.

**Modell** (katalógus v25, `031_mob_drops.sql`):
- **`mob_drop_group`:** azonosító, kód, cél és sorsolási mód.
  - A **cél** egy szörny (régi mob-vnum, mert a mobok még nem
    katalógustartalmak) vagy egy szörnyrang (0–5). A rang a régi közös drop,
    és csak „minden bejegyzés” módú lehet.
- **`mob_drop_entry`:** 1…N folytonos sorszám (legfeljebb 256), ItemKey,
  szint, darab, esély vagy súly, ritka bónusz esélye bp-ben, és a gyilkos
  szintablaka (alapból 1–65535).
- **Esélyegység:** milliárdod (ppb) ölésenként, semleges szorzókkal. Ez a régi
  fájlok minden százalékát egészként, veszteség nélkül hordozza.

**Sorsolási módok**, a régi csoportok szerint:

| Mód | Régi forrás | Esély | Metinkőnél |
|---|---|---|---|
| Minden bejegyzés | `drop`, `drop_item_group`, közös drop | `ppb × delta / 25000 ≥ U[1, tartomány]` | rangcsoport nem, szörnycsoport igen |
| Egy a N ölésből | `kill` | `40000 × delta / N ≥ U[1, tartomány]`, majd súlyozott választás: U[1, összsúly] a halmozott súlyokon | igen |
| Szinthez kötött | `limit` | `ppb ≥ U[1, 10⁹]`, szorzók nélkül, ha a gyilkos szintje ≥ a határ | nem |
| Dupla drop | `thiefgloves` | mint a „minden bejegyzés”, de csak prémiummal, kesztyűvel vagy `APPLY_DOUBLE_DROP_ITEM`-mel | nem |

A `delta` és a `tartomány` a régi `GetDropPct` értéke: szintkülönbség,
ritka bónusz, `GetMobItemRate`, `POINT_ITEM_DROP_BONUS`, dupla drop,
prémium, privilégium és esemény. A régi adatból (`dwPct` = fájlszázalék ×
10000) a ppb = `dwPct` × 250, illetve „limit” csoportnál × 1000. Így a régi
egész aritmetika eredménye változatlan marad.

**C7b-1 megvalósítás** (2026-10-31):
- **Tartomány:** `MobDrops.hpp/.cpp`, a validálás és a `PlanMobDrops`. A
  véletlenforrást a hívó adja, ahogy a régi `number(1, határ)`.
- **Katalógus:** `MobDropGroupsFor(mob, rang)`, ranggal és mobbal
  indexelve.
- **Hash:** v25, 54-es tábla.
- **SQL:** táblaleírás, pillanatkép, a szerkesztőeszköz commit-sorrendje és az
  export-staging lefedettsége.
- **Független ellenőrzés:** a Python-orákulumban egy négycsoportos
  droptáblás fixture. Minden korábbi golden újraszámolva (v25).
- **Tesztek:**
  - `MobDropTests` (45 ellenőrzés): validálás, katalógus-keresztellenőrzés,
    lekérdezés, SQL-sorok oda-vissza, és minden mód rögzített dobásokkal a régi
    képletekhez mérve;
  - a sémateszt a 031-es SQL-megkötéseket ellenőrzi.
- A teljes CTest zöld. Egy GUI-teszt egyszer időzítés miatt elbukott, majd
  háromszor egymás után átment.

**C7b-1 határa:**
- A szörny saját dropja, a metinkő, a lókönyv, a lottó és az eseménydropok
  szerverszabályok, nem droptáblák; ezek külön szeletek.
- A ritka bónusz esélye tárolódik, de az új tárgy még nem kap véletlen
  bónuszt.

## C7a — futásidejű azonosítók és tárgyadás

**Azonosító-kiosztás.**
- **Tábla:** `player.item_instance_id_allocator` (migráció:
  `db/migrations/item_instance/002_instance_id_allocator.sql`), egyetlen
  számlálóval.
  - Az alsó határ 2^32, így a futás közben kiosztott azonosítók nem
    ütközhetnek a konverzióval áthozott régi 32 bites tárgyazonosítókkal.
  - A migráció a már kiadott legnagyobb azonosító fölé állítja.
- **`ReserveItemInstanceIds`:** egy tranzakcióban (`FOR UPDATE`) foglal le
  egy blokkot. Az el nem használt azonosítókat senki más nem kapja meg.
  Elutasítja az üres, a túl nagy (1 000 000 feletti) és a túlcsorduló kérést.
- **Ki használja:** minden író, amely tárgyat hoz létre. A `MAX(id)+1` egy futó
  játékszerver mellett soha nem helyes. A próbapad is átállt, a régi
  `0x00BE…` saját tartománya megszűnt.

**Tárgytároló** (`GameServer/item/ItemStore.*`).
- **Első blokk:** indításkor 4096 azonosítót foglal le.
  - Ha a tábla hiányzik, a szerver ettől még elindul: a meglévő tárgyakkal
    minden működik, csak új tárgy nem jön létre. Ezt naplózza.
- **Kiadás:** a játékszál várakozás nélkül veszi az azonosítókat
  (`PeekInstanceIds`, `SpendInstanceIds`). Az elköltöttek a commit
  sorba állítása előtt kikerülnek a készletből, így egy másik adás sem
  kaphatja meg őket.
- **Utántöltés:** a negyed alá csökkent készletet a tárolószál a háttérben
  egy új blokkal tölti fel.

**`MakeGrantCommit`** (`PersistentItems`). Egy tárgyállapot (kulcs, szint,
darab) új egységei a tulajdonos inventoryjába kerülnek.
1. A halmozható egységek előbb a tulajdonos egyező halmait töltik fel. Egyező
   az a halom, amelynek azonos a kulcsa, a szintje és a kötése, és nincs
   példányállapota. A sorrend tároló, majd cella szerint halad.
2. A maradék új tárgyként az első szabad cellákba kerül. Halmonként egy-egy
   azonosítót kap, a kapott lista elejéről.
3. **Mindent vagy semmit:**
   - `InvalidPlacement`, ha nem fér el;
   - `LimitExceeded`, ha kevés az azonosító;
   - a már használt azonosító `InvalidChange`.
4. Az `OnAcquire` kötésű tárgy a tulajdonoshoz kötődik.

**Szerver.**
- **`RequestTypedItemGrant`:** a jövőbeli drop, bolt és jutalom útja. A
  következő okokkal utasít el:
  - nincs bekapcsolva;
  - foglalt;
  - érvénytelen;
  - nincs hely;
  - nincs azonosító.

  Elutasításkor semmi nem kerül sorba.
- **GM-tesztparancs:**
  `/typed_give <kategória> <helyi azonosító> [darab] [szint]`
  (`GM_HIGH_WIZARD`, mint a régi `/item`).

**Próbapad.**
- **Telepítés:** a 001 után a 002 is lefut.
- **Frissítés:** egy korábban telepített csomagban (a 9 régi tábla megvan, a
  kiosztó nincs) az „előkészítés” csak a 002-t futtatja, előtte a `player`
  adatbázisról mentés készül.
- **Csomagolás:** a `build_manager` mindkét fájlt becsomagolja.

**Ellenőrzés.**
- `PersistentItemTests`: 152 ellenőrzés (+14). Lefedi:
  - a halom feltöltését és túlcsordulását;
  - az új halmokat az első szabad cellákban;
  - a szintet;
  - a kevés azonosítót, a helyhiányt, a hiányzó inventoryt, a nulla darabot,
    az ismeretlen tárgyat és a foglalt azonosítót;
  - az `OnAcquire` kötést.
- `item_persistence_mariadb`: diszjunkt blokkok egy és két kapcsolaton, 2^32
  feletti kezdet, üres és túl nagy blokk, nincs átfordulás.
- `item_catalog_runtime_mariadb`:
  - teljes blokk induláskor;
  - adás a tárolón át a kiosztott azonosítókkal;
  - háttérbeli utántöltés új blokkal.
- `ItemBenchToolTests`:
  - a próbapad azonosítói a kiosztóból jönnek;
  - egy 002 nélküli adatbázis frissítése a már kiadott legnagyobb azonosító
    fölé állítja a számlálót.
