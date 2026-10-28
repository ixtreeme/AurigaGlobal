# ADR IC-016 — Automata helyreállítás és arénakvóta

2026-10-28. IC-014, 3. szelet. Helyi, izolált domain/katalógus/szerkesztő;
nem a futó játékszerverre telepített migráció.

## Ellenőrzött régi működés

- `ItemSystem_LegacyBridge.cpp::UseItemEx`, `ITEM_AUTO_*_RECOVERY_*`:
  HP/SP affecthez tartós item ID tartozik. A palack aktív/használt/teljes
  adatai socket0/1/2 mezőkben vannak. Aktiválás zárol, ugyanannak a tárgynak
  újabb használata kikapcsol; a másik palack cserélhető. Arénában tiltott.
- `AutoRecoveryItemProcess`: halott/kábult karaktert kihagy, a már várakozó
  HP/SP recoveryt levonja a hiányból. A véges palack elfogyáskor törlődik;
  X változatok külön vnum-kivétellel végtelenek. Aktívan +4% max HP/SP
  bónuszt is kaphatnak. Az `AffectSystem::UpdateAffect` a recovery-tartalékot
  tickenként legfeljebb a maximum 7%-ával adja át.
- `USE_POTION_NODELAY`: arénában globális tiltás, vnum-whitelist és feltételes
  személyes kvótavizsgálat van. Siker után a kvótát arénán kívül is csökkenti.
  Az új katalógus nem tartalmaz vnum-whitelistet: a kontextusmaszk a whitelist.
- A régi stackbontás a factory/inventory eredményének igazolása előtt
  fogyaszt, és a speciális logika több, külön affect/socket írásra épül.
  Ezek nem öröklődnek az új, mellékhatásmentes tervekbe.

## Típusos automata profil és állapot

Új `RecoveryDelivery::Automatic=4`: célpool (HP/SP), véges/végtelen ellátás,
véges kapacitás raw egységben, tickintervallum, maximum-pool/tick bp és
aktív max-pool bónusz bp. Végtelen ellátás kapacitása kötelezően 0; végesé
pozitív és legfeljebb `MaxItemStatRaw`. Nincs implicit vnum-kivétel.

Az automata **nem halmozható**, max_stack=1 és units_per_use=1. A bekapcsolás
nem fogyaszt darabot; a példány töltetét a későbbi owner tárolja. A kezdeti
állapot kapacitásig töltött, inaktív. Egy actor egy pooljához egy aktív
`ItemInstanceId` tartozik. A kikapcsolás és másik palackra váltás egy terv,
amelyet a későbbi writer egyetlen ownership/revision/receipt commitban alkalmaz.
Az aktív maximum-bónusz is ehhez az ownershiphez tartozik.

Az owner külön, alapértelmezetten tiltó `recoveryAllowedByRules` döntést ad a
globális/map/párbaj-szabályokból. Ezt a tárgy saját PvP-engedélye nem írhatja
felül; hiányzó döntés nem jelent implicit engedélyt.

Az automata arénában továbbra is tiltott. A kikapcsolás halott/kábult vagy
tiltott környezetbe került karakternek is lehetséges. Tickenként a halál és
kábulat szüneteltet, a retire/hozzáférési tiltás kikapcsolási tervet eredményez.

## Új tick-szerződés

Az új terv közvetlen, korlátozott helyreállítást javasol, és csak a ténylegesen
helyreállítandó mennyiséggel terheli a töltetet. **Nem** foglalja le előre az
egész HP-hiányt egy másik recovery queue-ba. Ez tudatos új szerződés; nem a
régi socket/affect műveletsor bitazonos emulációja.

```text
freeMissing = missing - min(missing, alreadyPendingRecovery)
tickCap     = floor(maxPoolRaw * maxPoolPerTickBp / 10000)
restored    = min(freeMissing, tickCap, finiteRemainingCharge)
```

Végtelen ellátásnál nincs harmadik korlát és nincs töltetcsökkentés. A számtan
egész és túlcsordulásmentes. Nincs potion bonus vagy RNG. Véges 0 töltetnél
kikapcsolás + példánytörlés javaslat keletkezik. A már várakozó másik recovery
nem fogyaszt újra töltetet. A max-pool bemenet már az owner aktuális statja;
a tervező nem adja hozzá ismét az aktív maximum-bónuszt. Kiürülésnél a writer
sorrendje: a jelenlegi maximumhoz számított helyreállítás, majd a példányhoz
kötött maximum-bónusz eltávolítása és a szokásos stat-clamp, egyetlen commitban.
A töltet a helyreállítási lépés mennyiségét méri; a későbbi maximumcsökkenés
nettó HP/SP-különbségét a stat-owner kezeli. Ez a stat-commit még nincs bekötve.

Az állapot `next_tick_in_ms` mezőt is tartalmaz. Egy lépés a szerver által
mért eltelt időt kapja; esedékesség előtt nincs gyógyítás. Hosszú kimaradás
után legfeljebb egy tick hajtható végre, a következő teljes intervallum múlva
esedékes. A kábultság/halál alatt esedékes tick elvész, nem gyűlik burstté.
Egy régi snapshot újraküldése ellen az operation ID/revision védi majd a
commitot; a tiszta tervező nem állít tartós idempotenciát.

## Arénakvóta

A recovery profil `arena_quota_cost` mezője explicit darab/használat költség.
0 = kvótamentes; pozitív csak Arena-kontextust engedő profilon érvényes.
Arénában az admission kötelezően a mérkőzésből kapott `potionsAllowed` és
optional `remainingUses` állapotot tartalmazza. Null kvóta = nem korlátozott;
0 = elfogyott. A tiltás kvótamentes tárgyra is érvényes.

Csak a sikeres, hasznos használati terv tartalmaz kvótacsökkentést, és csak
korlátozott arénában. Világban nincs negatív kvótamódosítás. A későbbi
reserve/periodic writernek ugyanezt a közös arénavizsgálatot kell használnia.

## Határ

Katalógus v9, új típusos `automatic_recovery_profile` tábla. Friss draft
migrációlánc, nem meglévő v7/v8 adatbázis frissítője. `USE_NEW_POTIION`
kapcsolható harmatjai külön, itemhez kötött buffok: nem alakítjuk őket
gyógyító töltetté. Timer/DB/registry/Unity integráció, valódi tartalomimport,
arénamérkőzés- és war-elszámolás a későbbi runtime fázis feladata.
