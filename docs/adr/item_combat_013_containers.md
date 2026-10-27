# ADR IC-013 — Tárolók, kulcsok és tartalomprofilok

2026-10-27. Fázis: F2 ([IC-012](item_combat_012_item_runtime_first.md)).
Állapot: **implementált katalógus-, SQL-, szerkesztő- és tervező réteg**. A
GameServer még nem használja (F4).

## Régi működés (forrásaudit)

- **Ajándékdoboz (`ITEM_GIFTBOX`):**
  - `UseItemEx` elfogyaszt egy darabot, és a doboz vnumjával a
    `special_item_group.json` csoportjából ad.
  - Opcionálisan globális nyitási időkorlát érvényes (`g_BoxUseTimeLimitValue`),
    és nő a ranglistapont.
- **Kincsesláda (`ITEM_TREASURE_BOX`):**
  - Önmagában nem nyitható. Kulcsot (`ITEM_TREASURE_KEY`) kell ráhúzni.
  - Akkor nyílik, ha a két tárgy `value0` értéke egyezik; ekkor a kulcs és a
    láda is elfogy.
- **Csoporttípusok:**
  - „normál”: súlyozott, pontosan egy nyeremény;
  - „pct”: minden sor 1–100% eséllyel külön dob.
- **Jutalmak:** tárgy (darab, ritka bónusz esély), valamint álvnumok: arany,
  tapasztalat, szörny, szörnycsoport, lassítás, HP-szívás és mérgezés.

Régi hibák, amelyeket az új modell nem örököl:
- A `GiveItemFromSpecialItemGroup` sorban osztja ki a jutalmakat. Ha egy
  későbbi jutalom elbukik, például mert tele a táska, a korábbiak már kint
  vannak, a doboz viszont nem fogy el. Így újranyitható, ez duplikációs rés.
- „pct” csoportnál, ha egy sor sem nyer, a függvény inicializálatlan értékkel tér vissza.

A helyi adatok:
- 362 doboz, ebből 89-nek van csoportja (mind „normál”).
- 3 857 tárgyjutalom, 16 szörnycsoport és 1 szörny.
- A 4 kulcsos ládának és a 273 többi doboznak nincs csoportja. Ezek ma csak
  quest által nyithatók, vagy egyáltalán nem.

## Döntés

- **8 Tároló** három fajtával:
  - használatkor nyílik;
  - kulccsal nyílik (zárazonosító ≥ 1);
  - quest nyitja (beépített tartalom nélkül).
  A használatkor nyíló és a kulcsos fajta kötelezően tartalomprofilra mutat,
  a questes fajta nem.
- **18 Kulcs** zárazonosítóval. A kulcs és a láda egyezése tárgytípus szerinti,
  nem `value0`.
- **Tartalomprofil** (`loot_profile` + `loot_entry`):
  - sorsolási mód: súlyozott egy nyeremény, vagy soronként független esély bp-ben;
  - 1…N folytonos sorszámú jutalomsorok, legfeljebb 256.
  - Jutalomfajták: tárgy (ItemKey, darab, ritka bónusz esély bp-ben), Yang,
    tapasztalat, szörny, szörnycsoport, lassítás, HP-szívás (a max. HP bp-je,
    legfeljebb 1 HP-ig), mérgezés.
  - A szörnyhivatkozás egyelőre régi mob-vnum, mert a mobok még nem
    katalógustartalmak.
- **Ellenőrzés:**
  - Tárgyjutalom csak élő (nem kivezetett) tárgy lehet, legfeljebb egy
    halomnyi mennyiségben.
  - A súlyok összege elfér 32 biten.
  - A tárolók és kulcsok halmozhatók. Felszereléskori kötés, megjelenés és
    bónusz nem engedett nekik.
- **Tervező (`PlanContainerOpen`):**
  - A sorsolás egy hívó által adott egyenletes forrásból jön, így egy rögzített
    sorozat pontosan visszajátszható.
  - A terv minden jutalmat birtokolt másolatként sorol fel, és egy egységnyi
    doboz- és kulcsfogyasztást ír elő.
  - A jövőbeli író (F4) előbb ellenőrzi az összes tárgyjutalom helyét, és csak
    utána véglegesít, egyetlen tranzakcióban. Részleges kiosztás nincs.
- **Formátum:**
  - A kanonikus formátum v6 (`auriga.item_catalog.v6`), a 19–21. táblákkal.
  - A `012_containers.sql` az `item_base` CHECK-jeit is bővíti a 8-as és 18-as
    kategóriára.

A globális nyitási időkorlát és a ranglistapont szerverbeállítás, illetve
runtime-feladat (F4); nem katalógusadat.

## Ellenőrzés

- **C++ tartomány** (`ContainerTests`, 69 ellenőrzés):
  - validátor, sorsolási határok, nyitási terv, katalógus-keresztszabályok;
  - kanonikus hash, amelyet a független Python-orákulum ugyanazzal az értékkel számol:
    `aebb86e8bc7c37a241aaa177c1055534a04de1221c1f3615a12949cfcc96164d`.
- **Saját MariaDB-példány:**
  - SQL-korlátok;
  - az SQL-ből betöltött katalógus hash-e azonos a C++ fixture-ével;
  - szerkesztő-commit tárolóval és kulccsal.
- **Szerkesztő:** modell- és Qt-tesztek; minden jutalomfajta pontos
  visszaolvasása.
