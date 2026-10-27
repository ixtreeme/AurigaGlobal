# ADR IC-009 — Fogyasztható tárgyak és háromféle gyógyítás

2026-10-27. Állapot: **izolált, draft kategória/profil és tiszta azonnali
használati terv**. A cél egy szemantikai típushoz egy prototípus; ez nem
runtime bekötés, teljes fogyasztható-item migráció vagy jóváhagyott balance.

## Ellenőrzött forrás

| Forrás | Megfigyelés | Következmény |
|---|---|---|
| `ItemSystem_LegacyBridge.cpp::UseItemEx`, `USE_POTION_NODELAY` (6834) | Külön flat HP/SP és max-pool százalékos gyógyítás. A potion bonus csak a flat részt növeli. Csak hasznos használatkor fogyaszt. | Explicit négy mező, nem value0/value1/value3/value4 tömb; nincs kliens által küldött végső gyógyítás. |
| Ugyanott, `USE_POTION` (6941) | HP/SP recovery tartalékot növel; a potion bonus felső korlátja ebben az ágban 100%, az azonnali ágban nincs ugyanez a clamp. | A reserve külön típus; az új cap explicit profiladat. Az instant fixture capje új draft adat, nem minden régi italra igazolt konverzió. |
| `AffectSystem.cpp::UpdateAffect` (841–876) | A reserve poolból tickenként legfeljebb max HP/SP 7%-a megy át; a continuous pontokat másik ág adja hozzá. | Külön reserve tickinterval/rate és periodic per-tick mennyiség. Nem egyetlen általános effect értéktömb. |
| `UseItemEx`, `USE_POTION_CONTINUE` (6993) | HP vagy else-if SP affect, value2 időtartammal és felülírással. | Periodic profilban pontosan egy pool; az új ütemezés actor+pool csoportot cserél, nem profile ID alapján halmoz. |
| `UseItemEx` arena/PvP feltételei, potion limit, WarMap UsePotion, seed/moon bottle speciális idő | A közös subtype nem írja le az összes használati feltételt és mellékhatást. | Kontextus/PvP/közös use policy külön ellenőrizhető. Quota, war accounting és tartalomspecifikus cooldown leképezése még import/runtime kapu. |
| `USE_CLEAR`, `USE_INVISIBILITY`, `USE_ABILITY_UP`, `USE_AFFECT`, `ITEM_AUTOUSE/BLEND` | Sok megmaradó aktív viselkedés nem gyógyítás. | Ezek nem kerülnek automatikusan recovery profilba; saját típusos hatás és további audit szükséges. |

A reserve ág SP-módosítása megelőzi a későbbi HP guard `return false` ágát.
Mindkét poolt tartalmazó régi item esetén ezért kódszinten lehetséges részleges
módosítás fogyasztás nélkül. Nem történt ennek élő reprodukciója vagy a régi
runtime javítása ebben a részszállításban. Az új műveleteknél az összes
előfeltételnek és számításnak meg kell előznie az egyetlen közös commitot.

## Típusok és séma

Az új `ItemConsumableRow{ItemKey,recoveryProfile}` az 5-ös kategóriához
tartozik. Egyelőre **csak recovery** viselkedés publikálható. Pozitív uint32
stacklimit, None refinement, nincs equip binding, model appearance vagy passzív
base-bonus. A használati egység nem lehet nagyobb a prototípus stacklimitjénél.
Hiányzó/dupla/árva/wrong-category child és profil blokkolja a snapshotot.

`RecoveryProfile`: id, code, uint16 egység, megengedett kontextusok, PvP-szabály,
optional cooldown és típusos effect-variant. Új delivery kódok: Instant=1,
Reserve=2, Periodic=3. Kontextus bitek: World=1, Duel=2, Arena=4, GuildWar=8.
Ismeretlen/üres kontextusmaszk nem publikálható. Ezek új domainkódok, nem régi
enum-castok. Az első release előtt a tartalommal együtt véglegesítendők.

- Instant: flat HP/SP raw, max HP/SP bp és flat bonus cap bp.
- Reserve: HP/SP raw, flat bonus cap, tickinterval ms, max pool/tick bp.
- Periodic: HP vagy SP raw/tick, tickinterval és duration ms. Az időtartam
  legalább egy teljes tick és az interval egész számú többszöröse. A tervezett
  első tick az interval végén, az utolsó a duration határán következik.
  Az ütemező és tartós effect-csoport még nincs implementálva.

Raw határ 0..10^15, flat bonus cap 0..100000 bp, max-pool százalék 0..10000 bp;
reserve tickarány 1..10000 bp. Üres hatás és periodic kétpoolos konfiguráció
hibás. Pozitív idők legfeljebb INT64_MAX milliszekundumok. Nincs float képlet,
Lua-kifejezés vagy regisztrálatlan behavior fallback.

`ConsumableCooldownProfile` közös id/code/duration rekord: több ital ugyanarra
a cooldownra hivatkozhat. Null = nincs cooldown; 0/ismeretlen FK hibás.
A konkrét karakter maradék cooldownja nem katalógusadat.

A friss `010_recovery_consumables.sql` hat táblát hoz: cooldown head, recovery
head, három typed delivery child és item_consumable. Head/child között a
(profile_id,delivery_kind) FK kizárja a más típusú childot. A loadernek a
pontosan egy megfelelő child lefedettségét is ellenőriznie kell. C++ bemenetben
az effect eleve variant, nem három nullable mezőhalmaz.

A `005` kategória-/stack CHECK-je a kiadatlan friss séma részeként bővült.
Ez nem meglévő DB migrációja. Az init-db nem tölti be, SQL→catalog loader nincs.

## Azonnali használati terv

`PlanInstantRecovery` immutable katalógust, ItemKey-t és szerverről származó
állapotpillanatot kap. Ellenőrzi a típust/deliveryt, retire állapotot, poolokat,
halott karaktert, kontextust/PvP-t, közös combat/level/class/sex szabályokat,
cooldownt és felhasználható darabszámot. Reserve/periodic hatást nem hajt végre
azonnali gyógyításként. A nyers állapotban ismeretlen kontextus vagy túlstackelt
darabszám hibás; a nulla SP maximum megengedett.

A potion bonus bemeneti tartománya [-10000,100000] bp. A flat szorzó
`10000 + min(potionBonusBp, flatBonusCapBp)`. A negatív bonus nem okozhat
negatív gyógyítást. Poolonként:

```text
requested = floor(flatRaw * flatMultiplier / 10000)
          + floor(maxPoolRaw * percentBp / 10000)
restored  = min(maxPoolRaw - currentPoolRaw, requested)
```

A két részt külön kerekítjük lefelé; a százalékos rész nem kap potion bonust.
A két pool függetlenül korlátozott. Ha mindkettő nulla hasznot ad, `NoEffect`,
nincs használati terv. Egy teljes pool nem akadályozza a másik gyógyítását.

A számításban a `value/10000*bp + value%10000*bp/10000` pontos, nemnegatív
felbontás kerüli el a közvetlen raw×bp szorzat túlcsordulását. A validált
határoknál az osztott szorzat legfeljebb 11×10^15, a maradék szorzata
1.1×10^9 alatt, az összevont kért érték legfeljebb 12×10^15. Ezt statikus
bound és szélsőértékteszt is ellenőrzi. Ez kizárólag a korlátos recovery
számítása: a CombatKernel előjeles széles aritmetikájának követelménye és
korábbi toolchain-kapuja változatlan.

Az eredmény helyreállítandó HP/SP, elfogyasztandó darab és optional közös
cooldown indítása. **Semmit nem commitol.** Nincs RNG, registry/HP-írás, SQL,
készletfogyasztás vagy quota-módosítás. A későbbi owner thread ellenőrzi az
ownership/lock/revision és map-/quota-feltételeket, majd item, pool, cooldown,
receipt és szükséges esemény együtt kerül commitra. Újrapróbálás nem új
használat; CommitUnknown egyeztetése továbbra is operation ID szerint történik.

## Kanonikus v5

Csak schemaVersion=5 fogadható el, domain `auriga.item_catalog.v5`. Az 1..15
tag szabályai az előző szerződés szerintiek. Új, ID szerint rendezett táblák:

- 16: cooldown identity és u64 duration.
- 17: recovery identity, u16 units, u8 contexts, bool PvP, optional u32 cooldown,
  u8 delivery, majd a konkrét típus mezői. Instant: i64/i64/u16/u16/u32;
  reserve: i64/i64/u32/u64/u16; periodic: i64/i64/u64/u64.
- 18: ItemKey szerint rendezett consumable sor: explicit hatbájtos kulcs, u32
  recovery ID. Számok little-endian; optional presence u8. Nincs struct ABI.

A kilenc prototípusos, öt kategóriás fixture SHA-256-ja:
`ecffcba2e2774c3f67f54c4e95b58075bf2755663f2f8e905836298f29fb0c59`.
Független Python encoder ellenőrzi. A régi sémák runtime fallbackként nem
használhatók; korábbi fixture és evidence csak történeti tesztanyag.

## Következő kapuk

Reserve és periodic futási állapot/timer/refresh és újrapróbálás, map/war/quota
bekötés, hatásnapló és tartós gazdasági commit, majd a többi fogyasztható hatás
típusos portja szükséges. A régi vnum-kivételek és cap/rounding különbségek
ellenőrzött offline megfeleltetést igényelnek. Automata palack, blend, statbuff,
láthatatlanság, tisztítás és aktív tárgyátalakító művelet nincs késznek jelölve.
Teljes P0/P1/P2, ItemFactory/SQL loader/Unity és új combat runtime továbbra is
nyitott. [Friss tesztlelet](../reports/item_combat/2026-10-27_recovery_consumables.md).
