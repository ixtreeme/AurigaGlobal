# Saját tárgy eldobása — C7d-1 natív és SQL alap

2026-11-01. F4, [IC-035](../../adr/item_combat_035_drops.md).
Történeti C7d-1 állapot: **a natív/SQL alap kész**. Az erre épülő játékbeli
és Unity-bekötés azóta elkészült; lásd a [teljes integráció és helyi
próba beszámolóját](2026-11-01_unity_item_runtime_integration.md).

## Megvalósítás

Az `OwnedGround` tervezők a meglévő tranzakciós írót használják:

- Teljes halom: inventory → előre kiépített `TradeEscrow`, ugyanazzal a
  példányazonosítóval és teljes állapottal.
- Részhalom: a maradék az eredeti példányon, az eldobott rész külön,
  lefoglalt azonosítón; a két változás egy műveletben történik.
- Felvétel: az escrow-példány kerül vissza inventoryba. Két tulajdonosnál
  mindkét kerítés ugyanabban a tranzakcióban vesz részt.
- Lejárat: csak escrow-példányból lehet sírkő; az inventoryhoz és a
  felszereléshez ez a művelet nem nyúlhat.

A fejlesztési szint, a signed példánybónuszok és a zárolt hely, a kövek,
valamint az ékszerfoglalat kapacitása, fokozata és hátralévő ideje megmarad.
Nincs újragenerálás `ItemGrant`-ból és nincs halomösszevonás.

Kötött és felszerelt tárgy nem dobható el. A DROP/GIVE tiltást a katalógus
`allowDrop` és `allowTrade` mezői érvényesítik. Az új `TransferDenied` hiba
magyar szöveget kapott a szerverben és a próbapadon.

## Ellenőrzés

A build elkészült a módosított natív/szerveres tesztcélokra és az
`ItemBenchTool`-ra. A célzott CTest-kör **7/7 sikeres**:

```powershell
ctest --test-dir build/codex-item-fixes -C Release -R '^(persistent_items|typed_item_grants|typed_ground|mob_drops|item_persistence_completion|item_persistence_mariadb|item_catalog_runtime_mariadb)$' --output-on-failure
```

- `PersistentItemTests`: 188 ellenőrzés. Új esetek: azonosság, teljes
  payload, split és darabmegmaradás, saját és idegen felvétel, tilalmak,
  tároló- és kerítéshibák, teljes lábnyom szerinti ütközés, lejárat.
- `item_persistence_mariadb`: 1093 ellenőrzés saját MariaDB-n. Valódi
  fegyver- és ékszerátadás két karakter között, visszatöltés, idempotens
  nyugta, elavult felvétel, részhalom és lejárat.
- Az előző logout- és elveszített COMMIT-regressziók továbbra is zöldek.

## Következő szelet

Az [IC-035 C7d-2 terve](../../adr/item_combat_035_drops.md) rögzíti a
földi entitás lefoglalását és publikálását, a core-/indulási munkamenet
felügyeletét és az újraindulás utáni takarítást, a két online karakter
snapshotjának frissítését, valamint a Unity-csomagot és inventoryakciót.
Ezek előtt a C7d-1 tervezőket játékbeli kérés nem hívja. A felhasználó
2026-11-01-i kérése alapján a szerver és a Unity-bekötés együtt készül;
a következő szelet helyi szerver–kliens próbával zárható.
