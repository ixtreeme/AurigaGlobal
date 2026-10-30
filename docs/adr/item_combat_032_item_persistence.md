# ADR IC-032 — Tartós tárgypéldány-mentés (F3, első fázis)

2026-10-29. Fázis: F3 ([IC-012](item_combat_012_item_runtime_first.md)).
Állapot: **implementált natív modell és SQL-író, izolált MariaDB-n ellenőrizve**.
A GameServer és a Database még nem használja (F4); a meglévő `player.item`
nem változott.

## Döntés

**Példány (`PersistentItem`).** Mezői:
- 64 bites `ItemInstanceId` és `ItemKey`;
- típusos tulajdonos (karakter / account / céh; az azonos számú karakter és
  account nem keverhető);
- revízió, fejlesztési szint, darabszám és kötés;
- tárolóhely: tároló + horgonycella;
- példánybónuszok, a három kőfoglalat és az ékszerfoglalat-állapot.

A megsemmisült példány sírkőként megőrzi az azonosítóját és az utolsó teljes
állapotát.

**Írás (`ItemCommit` → `CommitItems`).** Egy művelet egyetlen
`REPEATABLE READ` tranzakció, amely együtt viszi:
- az érintett tulajdonosok kerítését (munkamenet-generáció és revízió);
- a tárolók elrendezését;
- a példányok előtte/utána állapotát;
- a pénztárcák előtte/utána állapotát;
- egy nyugtát.

A tranzakció lépései:
1. Minden régi helyfoglalást felszabadít, mielőtt újat foglalna; így a csere
   átmeneti átfedés nélkül működik.
2. A tárgy **összes** inventory-celláját lefoglalja, nem csak a horgonyt.
3. A gyermeksorokat (bónusz, foglalat, cella) explicit cseréli, kaszkád nélkül.

**Idempotencia.**
- Azonos művelet-ID és azonos kéréshash esetén a válasz visszajátszás: nincs
  második levonás vagy jóváírás.
- Azonos ID eltérő tartalommal `ReceiptMismatch`.
- A `CommitUnknown` állapotot friss kapcsolaton a nyugta alapján kell
  egyeztetni; nyugta hiánya nem jogosít vak újraküldésre.

**Betöltés (`LoadItemOwner`).** Egy konzisztens, korlátos pillanatkép.
Elutasítja:
- a sérült vagy hiányzó gyermekállapotot;
- az ellenőrzőösszeg-eltérést;
- a rossz lábnyomot;
- az ismeretlen katalógust;
- az elavult munkamenetet;
- a kikapcsolt FK/CHECK ellenőrzést vagy a nem `REPEATABLE READ` kapcsolatot.

Hiányzó adat helyett sosem ad alapértéket.

**Műveletek → írási halmaz.** A natív `RefinementPlan`, `AttributePlan` és
`SocketServicePlan` közvetlenül írási halmazzá alakul. Nincs vnum-leképezés,
RNG, SQL vagy ECS ebben a rétegben.

**Jogosultság.** A futó író SELECT-et kap a `protos`-ra és a `player`-re, írni
pedig csak a saját mentési tábláit tudja; a katalógust nem módosíthatja.

## Első fázis hatóköre

Támogatott:
- fegyver, páncél, ékszer, alapanyag;
- foglalatkő, pénzjegy, képességkönyv, questtárgy, kulcs;
- a bónusz- és foglalatszolgáltatás tekercsei.

A többi kategória `UnsupportedState` hibát ad, amíg a saját példányállapota
(élettartam, töltet, pet, eszköz stb.) nincs leképezve. Nem nullázódnak.

## Séma

`db/migrations/item_instance/001_item_persistence.sql`. Csak frissen
létrehozott, leállított célra telepíthető, és nem a régi `player.item`
átalakítója. Táblák:
- `item_persistence_format` (katalóguskötés);
- `item_owner_fence`, `item_storage`, `item_wallet`;
- `item_operation_receipt`;
- `item_instance`;
- `item_instance_attribute`, `item_instance_socket`;
- `item_occupied_cell`.

## Következmény

A játékbeli használathoz az F4 kell: GameServer-indításkori katalógusbetöltés,
munkamenet-kerítés, a műveletek bekötése és a régi mentési út lecserélése.
Ezen felül szükség van a jóváhagyott konverziós családtérképre (F6) és a
Unity-protokollra (F5).

2026-10-30: a tulajdonos létrehozása, a munkamenet-foglalás és az aszinkron
betöltés elkészült ([IC-034](item_combat_034_gameserver_runtime.md), F4.2).
