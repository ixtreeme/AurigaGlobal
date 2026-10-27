# ADR IC-002 — Item-identitás, prototípustárolás és katalógus

Dátum: 2026-10-27. Állapot: **normatív célarchitektúra; izolált domain/SQL és weapon/armor részproof készült, teljes P1/P2 elfogadás még nincs**.

## Kontextus

Az aktuális `ItemIdentity`, `TPlayerItem`, packet, SQL és Mission reward szerződések többnyire 32 bites vnumot használnak. Az ECS item snapshot még TItemTable pointert, min/max értékeket és refined_vnumot hordoz; a factory a név `+N` végéből is szintet értelmez. Források az auditjelentésben.

## Döntés

- Külön `protos` adatbázis tárolja az item-, mob-, skill-prototípusokat és statikus profilokat. A konkrét itemállapot/owner/location/refinement/revision a `player`, a Mission saját állapota a `mission` tulajdona.
- Az item tartalmi kulcsa `ItemKey{category_id:uint16, local_id:uint32}`. A nulla érvénytelen; local_id kategóriánként 1-ről indul, kiadott/törölt ID-t nem használunk újra. Egy puszta local_id nem globális kulcs.
- Az instance külön 64 bites ItemInstanceId-t kap; entt::entity nem kerül tartós ID-ként DB-be vagy wire-ra.
- Egy szemantikai típus egy proto. A fejlesztés instance `refinement_level` és validált, teljes profilgörbe. Nincs refined_vnum, vnum+szint aritmetika vagy névparszolás runtime-on.
- Típusos kategória-/profilstruktúrák készülnek, nem általános value-tömb, JSON/EAV proto. A megosztott catalog immutable snapshot; kizárólag a szerverből készül publikus kliensprojekció.
- SQL természetes összetett kulcsot használ; a wire explicit little-endian mezőket, padding/sizeof/reinterpret_cast szerződés nélkül. A packed64 felső 16 bitje kötelezően nulla, hibáját elutasítjuk.
- A meglévő prepared SQL infrastruktúrát használjuk tovább. A külön DB-k közötti tranzakció ugyanazon szerveren/kapcsolaton ellenőrizendő; teljesen minősített táblanevek és jogosultságtesztek kellenek.

## Nyitott kapuk

A 18 kategória a specifikáció javasolt regisztere, még nem kiadott stabil kódkészlet. A teljes forrás/tartalom besorolás előtt nem véglegesítjük. A MariaDB 11.4 cross-database FK, constraint, SQL-mode és target jogosultságok izolált alappróbája sikeres; a teljes schema/loader és runtime integráció még nyitott. A stack-egyenértékűség, többcellás inventory, soul, timer/expiry, mount/pet, transmutation és elnyelt socket állapot teljes auditja adatvesztési kapu; a generic socket átnevezése ezt nem oldja meg.

A weapon/armor részmegvalósítás és korlátai: [ADR IC-004](item_combat_004_equipment_catalog.md).
A típusos bónuszprofilok és a korábbi hash v2 szerződés: [ADR IC-005](item_combat_005_bonus_profiles.md).
A korábbi négykategóriás katalógus és hash v3: [ADR IC-007](item_combat_007_accessory_material_catalog.md).
A v4 ékszerfoglalat- és élettartamprofil: [ADR IC-008](item_combat_008_accessory_socket_profiles.md).
Az aktuális öt kategória és v5 recovery profil: [ADR IC-009](item_combat_009_recovery_consumables.md).
