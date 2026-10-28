# Item proto — automata palack, töltet és arénakvóta

2026-10-28. Ág: `codex/mission-engine`, kiinduló HEAD:
`45e76b230f83a9adc2a244881f488786679ef7e7`.
A korábbi review/timed-consumable munkafa megmaradt; nem készült commit.

## Elkészült a forrásban

- `RecoveryDelivery::Automatic`: HP/SP, véges/végtelen ellátás, kapacitás,
  tickidő, maximum-poolhoz viszonyított helyreállítási korlát és aktív maximum-bónusz.
  Nincs socket-index, vnum-kivétel vagy általános value-tömb az új modellben.
- Külön példányállapot: megmaradt töltet, aktív állapot, következő tick ideje.
  Az új példány induló állapota a prototípusból készül; a további használat
  nem változtatja és nem sokszorozza a prototípust. Az állapot még nem ECS/DB-bekötés.
- Tiszta kapcsolási terv: be/ki, másik palack leváltása, poolonként egy aktív
  tartós `ItemInstanceId`, aktiválási cooldown. A kikapcsolás halál/kábulat,
  tiltott környezet vagy retire után is működik igazolt, konzisztens állapotból.
- Tiszta tick-terv: csak esedékességkor tölt, figyelembe veszi a már várakozó
  recoveryt, véges esetben pontosan a javasolt gyógyítással csökkenti a töltetet.
  Kiürüléskor ugyanabban a tervben jelzi a kikapcsolást és példánytörlést.
  Hosszú kimaradás nem ad felhalmozott gyógyítási burstöt.
- A szerver/map/párbaj szabálya külön, alapból tiltó admission-döntés.
  A tárgy saját PvP-engedélye nem írhatja felül. Az automata arénában tiltott,
  a halál/kábulat alatti tick nem gyógyít és nem terheli a töltetet.
- Explicit `arena_quota_cost` a recovery profilban. Arénában kötelező a
  mérkőzés engedélye és nullable kvótaállapota. Null = korlátlan, 0 = elfogyott.
  A sikeres instant terv tartalmazza a kvótacsökkentést; világban nincs ilyen
  csökkentés. A közös vizsgálat a későbbi reserve/periodic writer számára is elérhető.
- Katalógus, pontos kanonikus hash, SQL-snapshot és atomi szerkesztői mentés.
  Automata csak max_stack=1 és units_per_use=1 adatokkal publikálható.
- Qt szerkesztő: automata típus, supply/kapacitás/időköz/arány/bónusz,
  zárolt halmozás és aréna, kontextusfüggő kvótamező. Végtelen kapacitás 0;
  a nem mentett véges érték oda-vissza kapcsoláskor megmarad.
  Másolat és profilváltás nem ír át más tárgyat, és eltávolítja a fölös privát profilt.

## Számtan és szemantika

```text
freeMissing = missing - min(missing, pendingRecovery)
tickCap = floor(maxPoolRaw * maxPoolPerTickBp / 10000)
restored = min(freeMissing, tickCap, finiteRemainingCharge)
nextRemaining = finiteRemainingCharge - restored
```

A skálázás osztott szorzással kerül el signed overflow-t. A tesztek 1000
determinista mintában független unsigned aritmetikával ellenőrzik a számítást
és a töltet megmaradását. Végtelen ellátás nem hordoz fiktív pozitív töltetet.

Ez új szerződés: a régi rendszer előre lefoglalja a teljes hiányt recovery
queue-ba, az új a tényleges gyógyítási lépést számolja el. A tick maximumbemenetét
az owner számítja, a tervező nem adja hozzá ismét a maximum-bónuszt.
Kiürüléskor a helyreállítás után következő bónuszelvétel/stat-clamp szintén
a későbbi egyetlen owner-commit része; az nincs ebben a modulban végrehajtva.

## Formátum és adatok

- **v9**, `auriga.item_catalog.v9`; új típusos `automatic_recovery_profile`.
- A 015-ös migráció és a friss draft lánc együtt szükséges. **Nem** meglévő
  v7/v8 adatbázisra futtatható frissítő; a recovery-diszkriminátor alapja is bővült.
- Független SQL/C++ fixture és Python hash-oracle. Golden:
  `b02a500eca103be1d37fc1c16155e34f36103753af353d968a05f9f1677a9889`.
- Csak új, saját PID/datadir/port szerint igazolt teszt-MariaDB-példányok
  módosultak. A telepített app, a meglévő helyi adatbázis és a távoli gép nem.

## Ellenőrzés

| Ellenőrzés | Eredmény | Log |
|---|---|---|
| ServerTests build + teljes normál CTest | 104/104 | `build/automatic-recovery-final-ctest.log` |
| Qt/model/native writer + izolált MariaDB | 32/32 | `build/automatic-editor-final.log` |
| ASAN natív katalógus és kapcsolódó tesztcélok | 15/15 | `build-asan/automatic-recovery-final-tests.log` |
| Ugyanaz a 32 szerkesztőteszt ASAN helperrel | 32/32 | `build-asan/automatic-editor-final.log` |
| Teljes manager GUI, eldobható helyi csomagmásolat | 14/14 | `build/automatic-manager-gui-tests.log` |

A tesztek a hibás profil, halmozás, töltet, ownership/timer, kvóta és kontextus
elutasítását, az időbeli részlépéseket, kábulatot/halált, kiürülést, korlátlan SP-t,
profilmegosztást, típusváltást, mentési rollbacket és form round-tripet is lefedik.
A bővített fixture miatti régi `.back()` teszthivatkozások és egy magyar hibaszöveg
kis-/nagybetű-elvárása a kezdeti körben javításra szorult; a végső körök sikeresek.

Offscreen vizuális ellenőrzés: 1400×950 és 1100×760, memória-fixture alapján.
A mezők oldalra nem lógnak ki; képek: `build/automatic-recovery-editor.png`,
`build/automatic-recovery-editor-small.png`.

## Ami még nem készült el

1. Tárgyhoz kötött, kapcsolható `USE_NEW_POTIION` harmatok. Ezek stat-buffok,
   egyedi házasság/party-vezető szabállyal; nem gyógyító palackként portoltuk őket.
   A képességkönyvek és tárgyműveletek szintén következő tartalmi részek.
2. A meglévő helyi katalógushoz adatmegőrző, visszaállítással tesztelt frissítő.
   A v9 app csak ezután kerülhet a valódi helyi csomagba.
3. F3/F4: példánymentés és visszatöltés, capacity/profilváltozás validálása a
   meglévő példányokra, valódi ECS-tulajdon, timer, inventory-lock, HP/stat,
   kvóta/cooldown, replacement/depletion és operation-receipt egyetlen commitja.
4. F6: régi stack/socket/affect mentések hiteles offline leképezése.
   A prototípusból inicializáló függvény nem meglévő játékostárgy feltöltője.
5. Unity/protokoll-integráció és tényleges játékbeli próba.

Részletes szerződés: `docs/adr/item_combat_016_automatic_recovery.md`.
Audit: `2026-10-28_automatic_recovery.audit.json`;
teszt- és forráshash-ek: `2026-10-28_automatic_recovery.evidence.json`.
