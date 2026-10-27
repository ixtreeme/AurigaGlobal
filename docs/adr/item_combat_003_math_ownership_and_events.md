# ADR IC-003 — Determinisztikus számítás, owner commit és tartós esemény

Dátum: 2026-10-27. Állapot: **normatív szerződés; kernel/runtime proof még nincs**.

## Kontextus

A jelenlegi combat fix és min/max/float/random útvonalakat vegyít; a HP, gyógyítás, mana és dungeon phase külön írható. A registry főszálhoz kötött. A damage packet 32 bites, a runtime HP uint64. A Mission Engine és a szezonális Mission Pass két külön szolgáltatás.

## Döntés

- Minden damage raw int64, 1000 raw = 1 pont, 10000 bp = 100%. A stage-határok a specifikáció 8. fejezete szerinti sorrendben és külön kerekítéssel futnak; ties away from zero.
- A széles köztes szorzás ellenőrzött; nincs wrap, csendes telítés vagy platformfüggő float-eredmény. A raw tartomány és faktorhatárok explicit validálás alá esnek.
- Harci véletlen csak regisztrált, verziózott crit/proc döntés. A kernel döntési tényeket kap; nincs saját RNG, registry-, SQL-, Lua- vagy hálózathozzáférése. A loot és AI véletlenét külön szemantika alapján auditáljuk.
- A globális registry tulajdonosa a game thread. A worker immutable snapshotot olvas és tiszta eredményt ad vissza. HP/shield/mana és halál egyetlen owner commitban, stabil server action/hit/target sorrendben változik.
- A kliens intentet küld, és az elfogadott tényt jeleníti meg. A sebzés és a jutalom szerverautoritatív.
- Egy életgenerációhoz legfeljebb egy DeathId és committed halálesemény tartozik. Mission/Dungeon/seasonal/Lua fogyasztók közvetlenül az új típusos szerződésre migrálnak, szemantikai deduppal. Az entt-handle nem tartós identitás.
- A pénzügyi/jutalmi hatások meglévő prepared tranzakciós infrastruktúrán, tartós operation/death receipt és inbox/outbox mellett történnek. CommitUnknown után tartós operation ID szerinti egyeztetés kell, nem vak újrapróbálás.

## Bizonyíték és nyitott kapuk

A helyi MSVC C++23 expected-probe 1/1 pass. A tényleges x64 Boost include-ban a szükséges multiprecision fejléc hiányzik; checked_int128 compile-check fail. Ez függőségi előfeltétel, nem indok floatra vagy unchecked64-re visszatérni.

A HP/death journal pontos tartóssági/RPO határa, platformmátrix, geometriai fix skála, BonusRegistry összes régi apply-leképezése és production balance nyitott. A specifikáció golden számai referenciafixture-ek, nem jóváhagyott production balance. Cross-worker/platform bitazonosság és crash-recovery még nincs igazolva.
