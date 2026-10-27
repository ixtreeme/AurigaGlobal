# Item- és harcrendszer-migráció — P0 audit

Állapot: **folyamatban; nem teljes P0-elfogadás és nem elkészült migráció**.
Vizsgálat napja: 2026-10-27, Europe/Bratislava.

## Hiteles specifikáció és munkafa

- Repository: `D:/AurigaGlobal/LiveWork/AurigaGlobal`.
- Ág: `codex/mission-engine`.
- Auditált HEAD: `ac4a7cf3aa9820745c3901039d7cee16ac0b1ec5`.
- A vizsgálat kezdetén a Git-munkafa tiszta volt. Az audit nem írt át játékforrást, Unity-forrást vagy futó adatbázist.
- Alkalmazandó `AGENTS.md`: a repositoryban és az ellenőrzött szülőkönyvtárakban nem találtunk. Ez nem állítás más munkapéldányok utasításairól.
- Elolvasott útmutatók: `tests/server/README.md`, `tests/client/README.md`, `db/README.md`, a gyökér `CMakeLists.txt` és `cmake/AurigaSystemDeps.cmake` releváns részei.
- Teljes specifikáció: [AURIGAGLOBAL_ITEM_COMBAT_MIGRATION_PLAN.md](../plans/AURIGAGLOBAL_ITEM_COMBAT_MIGRATION_PLAN.md), v1.2, 1561 sor, 142803 bájt, 26 számozott fejezet (0–25).
- Forrásmunkamenet: `01a17a65-f0b2-76d3-a4bb-8256d0aacee7`, címe: **Készíts item combat migrációs tervet**.
- Eredeti fájl: `C:/Users/ixtre/Documents/Codex/2026-10-26/referenced-chatgpt-conversation-this-is-an/outputs/AURIGAGLOBAL_ITEM_COMBAT_MIGRATION_PLAN.md`.
- A bemásolt fájl bájtra azonos az eredetivel. SHA-256: `bb17b3307409f465c5a020c6d1d7c7eb643d4e7d4449c09ba544e0693d2839eb`.
- A specifikációban szereplő régebbi `61855588` HEAD helyett a fenti aktuális HEAD az audit alapja. A közben integrált Spy/Caravan küldetések is a megmaradó funkciók közé tartoznak.

A változtatások és a forrásfájlok ujjlenyomatait az `item_combat_audit_evidence.json` rögzíti. A kezdeti, kézzel ellenőrzött határfelületek az `item_reference_inventory.csv` fájlban találhatók. Ez a leltár **még nem teljes tartalom-/referencia-coverage**.

## Rögzített hatókör

A Mission Engine véglegesítése elé kerül az item/combat migráció. Az új `protos` adatbázisba az itemek mellett a mob- és skill-prototípusok, valamint a kapcsolódó statikus profilok is kerülnek. Az instance-állapot `player`, a Mission saját állapota `mission` tulajdon marad.

A régi BattlePass és offline shop már kivezetett rendszerek. A történeti SQL-táblák jelenléte nem indok a runtime visszaépítésére, új offline-shop modellre vagy migrációs tesztkövetelményre. A szezonális Mission Pass, NPC-bolt, online személyes bolt, trade és közös mentési felületek megmaradnak.

Távoli gépen nem történt művelet. A meglévő játékadatbázis nem migrálódott. Az új runtime bekötése, kliensfrissítés és éles kiadás nem történt meg.

## Build és függőségek — ténylegesen ellenőrzött tények

| Felület | Bizonyíték | Következmény |
|---|---|---|
| C++23 | Gyökér `CMakeLists.txt`, `CMAKE_CXX_STANDARD 23` és REQUIRED | A domain és kernel C++23 marad. |
| Windows build | `build/CMakeCache.txt`: Visual Studio 18 2026, x64-windows, vcpkg; CMake 4.3.2 | A meglévő build konfigurációja használható alapnak. |
| Aktuális fordító | Friss izolált CMake-fordítás: MSVC **19.51.36260.0**, `14.51.36231/bin/Hostx64/x64/cl.exe` | A régebbi build-cache 19.51.36248.0 értéke nem a friss próba fordítóazonosítója. |
| `std::expected` | Izolált `expected_probe`: compile-time és futásidejű siker-/hibaág; `std::unexpected`, `std::to_underlying`; **1/1 pass** | A helyi MSVC standard library ezt a szükséges szerződést támogatja. |
| EnTT | Telepített `entt/config/version.h`: **3.16.0**; GameServer `EnTT::EnTT` | A globális registry főszálhoz kötött szerződése továbbra is alkalmazandó. |
| Boost | Telepített x64 `boost/version.hpp`: **1.91** | A verzió önmagában nem bizonyít minden Boost-modult. |
| Checked széles egész | A tényleges x64 include-ban nincs `boost/multiprecision/cpp_int.hpp`; az izolált CMake compile-check is **sikertelen** | A P3 checked-int128 backend függősége még nincs előkészítve. Az x86-windows-static fejlécsiker nem helyettesíti ezt. |
| Függőségek deklarációja | `CMakeLists.txt::vcpkg_require` Boost-részmodulokat sorol; nincs köztük multiprecision | A megfelelő függőséget később a bizonyított platformgráfba kell felvenni; itt nem telepítettünk csomagot. |
| Helyi MariaDB-csomag | `D:/AurigaGlobal/Server2-Local/tools/mariadb/bin/mariadbd.exe --version`: **11.4.13-MariaDB Win64 AMD64** | Ez a bináris verziója; a futó adatbázis SQL-mode-ja, engine-je és az éles szerver verziója nincs ezzel igazolva. |

FreeBSD/Clang futtatás, keresztplatformos bitazonosság, új kernelbenchmark és új séma cross-database FK-próbája **nem történt**. A specifikáció MySQL 8.4 DDL-mintáit nem tekintjük automatikusan MariaDB-n ellenőrzött telepítőnek.

## Követett harci hívási lánc

1. `network/input_main.cpp`: `CG_ATTACK`, `CG_SHOOT`, `CG_USE_SKILL` diszpécselése.
2. `combat/combat_input.cpp::CInputMain::Attack`: packet/entitás/skill ellenőrzések, combat target és mozgásállapot, majd `CombatSystem::Attack` / `Shoot`. A bejövő támadási és skill-csomagok régi struktúraolvasása is migrációs határ.
3. `ecs/systems/CombatSystem.cpp`: a támadás `CalcMeleeDamage` / `CalcArrowDamage` segédeket használ; `combat/battle.cpp` mob- és fegyver-min/max dobást, float attack ratinget tartalmaz. `SkillSystem.cpp` több saját képlet-/dobási útvonala is érintett.
4. `CombatSystem::Damage`: crit/penetrate/block/dodge dobások, speciális dungeon- és HP-ágak, majd a normál útvonalon `PointSystem::Change(victim, POINT_HP, -dam, false)`. Nem elég egyetlen `number()` hívást cserélni.
5. `PointSystem::Change`, `case POINT_HP` → `PlayerRuntime::SetHP` → `Health.current`. Ez utóbbi `uint64_t`; az új raw int64 szerződés szélességét és előjelét a teljes láncon rendezni kell. Inicializálás, load, gyógyítás és egyedi HP-írások külön osztályozást igényelnek.
6. `CombatSystem::Damage` HP<=0 ág → feature-függően `Dead` vagy `Stun`/késleltetett halál. `Dead` kezeli a Spy/Caravan death hookot, dungeon monstercountot, rewardot, dead packetet és despawnt is.
7. `CombatSystem::Reward` → `DistributeExp`; a jóváírt játékoshoz közvetlen szezonális Mission Pass `Events::Kill`, Lua `CQuestManager::Kill`, valamint a C++ dungeon `OnMobKilled` hívások tartoznak. A ledger runtime ECS-handle-öket használ; nem tartós DeathId szerinti receipt.
8. Itemdrop: `Reward` → `ITEM_MANAGER::CreateDropItem` → `CreateItem` → `EntityFactory::CreateItemEntity` → tárgy elhelyezése/tulajdon. A loot-RNG külön szemantika, nem tiltandó automatikusan a damage-RNG-vel együtt.
9. Perzisztenciahatár: karakter `SessionSystem::Save` / `CreatePlayerProto` → `HEADER_GD_PLAYER_SAVE`, `TPlayerTable` → Database `QUERY_PLAYER_SAVE` és player cache. Item `ITEM_MANAGER::SaveSingleItem` / késleltetett mentés → `TPlayerItem` / Database item cache. A készülő új rendszer ezeket revision/fence és operation/death receipt szerződésre cseréli.
10. Megjelenítés: `SendDamagePacket` → `TPacketGCDamageInfo` (`int damage`); `NetworkSyncSystem` pontszinkron; dead packet. Unity `Metin2GamePacketCodec.ReadDamage` 32 bites damage-et olvas, a target HP viszont külön 64 bites olvasási út. Ezek együtt cserélendők.

### Események: bizonyított és hiányzó bekötés

- `ecs/events.hpp::EvEntityDied` csak runtime handle-öket hordoz; `EvEntityDamaged::damage` int32.
- Az auditált `CombatSystem.cpp` nem publikál `EvEntityDied` / `EvEntityDamaged` eseményt. A scoped keresés a Lua NPC/PC halálfunkcióiban talált `EvEntityDied` triggereket, illetve egy stun-eseményútvonalon `EvCharDead` triggert.
- A Mission Engine `EventKind::MobKilled`, `BossDefeated`, `GameplayEvent`, `MissionRuntime::Dispatch` és indexelt router már létezik. A fenti harci láncból ezekhez vezető production bekötést a most követett útvonalon **nem találtunk**. Ez nem bizonyítja más, még nem auditált plugin/funkció hiányát.
- A szezonális `AurigaMissionPass::Events::Kill` külön szolgáltatás. Nem téveszthető össze az új történetalapú Mission Engine-nel.
- Az új event-contract előbb committed HP/death tényt és tartós identitást kíván; a jelenlegi közvetlen dungeon/Lua/seasonal fogyasztók funkcióit ezen kell megőrizni, kettős kill-jóváírás nélkül.

## Item határok és adatvesztési kockázatok

| Felület | Ellenőrzött jelenlegi szerződés | Szükséges új szerződés |
|---|---|---|
| `common/tables.h::TItemTable` | `alValues`, range, refined vnum | Típusos `protos` kategória/profil, immutable catalog. |
| `item_proto_components.hpp::ItemProtoRef` | Min/max, refined vnum, `const TItemTable*` | Katalóguskulcs/revízió; nincs proto-compatibility pointer. |
| `ItemSystem.cpp::MakeItemPrototype` | Régi proto-value-k és `+N` névvég feldolgozása | Explicit instance refinement és profilgörbe. |
| `item_components.hpp::ItemIdentity`, `TPlayerItem` | 32 bites instance ID és több vnum-jellegű azonosító | 64 bites instance ID, ItemKey, külön appearance/transmutation jelentés. |
| `EWindows`, inventory/safebox/trade | Több window, owner-, cell- és grid-szabály | Tényleges owner/container/grid és stack-state invariánsok. |
| `AcceSystem.cpp` | Az elnyelt tárgy original vnumja socketbe kerül | Típusos elnyelt állapot; nem kezelhető minden socket egyszerű foglalatkőként. |
| Mission reward | `RewardBundle` items/consumeItems: `map<uint32_t,uint32_t>` | ItemRewardSpec + refinery/bind/affix/socket; package/hash/receipt együtt cserélendő. |
| Database `Cache.cpp`, award delivery | vnum-alapú sor/csomag és mentési logika | Új explicit codec, cache-revision és tranzakciós idempotencia. |
| Legacy launcher | `Game/ItemManager.cpp` régi item_proto és vnum olvasó; `SRC/CMakeLists.txt` kifejezetten kivezetett kliensként kezeli, nem ad hozzá Launcher/Mysql2Proto buildet | Forrásarchívum, kizárva az új runtime migrációjából; a Unity kliens marad aktív. |
| Unity | `Metin2ItemCatalog` uint vnum; `ReadItemSet`/`ReadItemUpdate`; megjelenési lookupok | Egy szerverből képzett projection, ItemKey, explicit refinement és verziózott delta. |

Az `ItemSystem_LegacyBridge.cpp` használati/equip/refine funkciói megmaradó domainműveleteket is tartalmaznak. A fájlnév szerinti törlés nem elfogadható migráció.

## Friss alaptesztek

Az alapvonal **a meglévő modellre** futott. Nem bizonyítja az új modell működését vagy teljes szerverbootot.

```powershell
& 'C:/Program Files/CMake/bin/ctest.exe' --test-dir build -C RelWithDebInfo --tests-from-file build/ecs-mission-merge-20261027/native-test-names.txt --output-on-failure --output-log D:/AurigaGlobal/LiveWork/AurigaGlobal/build/item-combat-audit-20261027/native-baseline.log
& 'C:/Program Files/CMake/bin/ctest.exe' --test-dir build-asan -C RelWithDebInfo --tests-from-file build/ecs-mission-merge-20261027/asan-test-names.txt --output-on-failure --output-log D:/AurigaGlobal/LiveWork/AurigaGlobal/build/item-combat-audit-20261027/asan-baseline.log
```

- Normál: **53/53 sikeres**, ebből 5 izolált MariaDB-integráció; 61,06 s.
- ASAN: **41/41 sikeres**, 17,46 s. Ez nem 53 ASAN teszt, és nem új teljes ASAN-build.
- Toolchain-probe: **1/1 sikeres**, 0,07 s. A Boost checked_int128 külön compile-check **sikertelen**, nem elhallgatott tesztkivétel.
- Naplók, input tesztlisták és a probe az ignorált `build/item-combat-audit-20261027/` alatt; az ujjlenyomatok és a futtatott tesztnevek a követhető evidence JSON-ban.

## P0 nyitott munkák — a kódcsere előtt

1. Az összes megmaradó item store, hardcoded tartalomhivatkozás és közvetlen writer szemantikai leltárának befejezése. A kezdeti CSV nem 100%-os migrációs coverage.
2. Minden apply/point/skill formula tételes új mező-, unit-, bucket-, sorrend-, cap- és módleképezése; a nem leképezett tartalom publikálása tiltott. A régi `%` egységből nem lesz ellenőrzés nélkül bp.
3. Soul/DS, timer/expiry, pet/mount, transmutation, elnyelés, foglalatok, wallet/pénzitem és lezárt kereskedelmi állapot veszteségmentes szerződése.
4. Adat-/asset-/toolfájlok és külön Unity-workspace referenciáinak teljes leltára, a launcher tényleges termék/build-szerepének tisztázása.
5. HP/mana/shield/death íróhelyek osztályozása: combat commit, inicializálás/load, gyógyítás, GM, Lua, dungeon phase, recovery. Nem minden HP-változás támadás.
6. A kategóriaalap MariaDB 11.4 izolált mode/engine/constraint, cross-database FK és prepared tranzakció proof-ja az első implementációban elkészült (lásd alább). A teljes típusos P1-séma és CatalogBuilder bound-param integrációja még vizsgálandó; az éles DB-változat továbbra sem ellenőrzött.
7. Új típusok és release-tartalom készültségi kapuinak lezárása a fenti függőségekkel. Az új rendszer nem indítható támogatott release-ként félkész modellel.

## Döntési jegyzőkönyvek

- [Hatókör és koherens release](../adr/item_combat_001_scope_and_release.md).
- [Azonosítók, tárolók és katalógus](../adr/item_combat_002_identity_and_catalog.md).
- [Matematika, ownership és események](../adr/item_combat_003_math_ownership_and_events.md).

A következő független munka a teljes referencia-/writer- és bonus/formula-leltár. A P1 domain/schema és P3 kernel csak az érintett P0-határok lezárása után kezdődik. Ebben az auditban még nem készült ItemKey-runtime, új SQL-telepítő vagy új combat engine.

## 2026-10-27 — a domainkulcs implementációjának auditkapuja

Az ItemKey önálló szerződését érintő határok (kategóriaszélesség, local ID,
packed64, packet ABI, prototípus/instance különbsége és C++23 library) ellenőrzése
megtörtént. A specifikáció 21. fejezetében megengedett izolált unit/tool célban
kezdődik a domainkulcs és codec megvalósítása. Ez nem production runtime-csere,
nem vnum-adapter és nem a teljes P0/tartalommigráció elfogadása. A többi nyitott
auditpont az érintett P2/P4/P6 tartalom és integráció kapuja marad.

A build `auriga_collect_server_sources` függvénye rekurzívan gyűjt forrásokat.
Ezért az önálló domain fordítási egységei a `SRC/ItemDomain` modulban készülnek,
a közös fejlécek a terv szerinti `SRC/Server/common/item` alatt. A modul nem
linkel `SERVER`, SQL, Lua, EnTT vagy régi item/combat célt, és a jelenlegi
GameServer/Database sem linkeli. A kategóriaregiszter a specifikáció 18 elemű
kezdeti felosztását követi, **draft**, első kiadás előtt véglegesítendő.

Az implementált rész és a friss tesztek lelete:
[domainkulcs, codec és SQL kategóriaalap](../reports/item_combat/2026-10-27_domain_foundation.md).
Ez a részszállítás nem zárja le a teljes P0/P1 fázist.

## 2026-10-27 — weapon/armor katalógus részmegvalósítás

A `MakeItemPrototype` alValues és név+N olvasása helyett az új, izolált modul
típusos weapon/armor és kumulatív refinement sorokat használ. A slot- és weapon
subtype audit forrása a `common/item_length.h::EWeaponSubTypes/EArmorSubTypes`,
az osztálykészleté `common/length.h::EJobs`. A régi enumértékeket nem castolja át.
Az új profilok még nem fednek le minden régi anti-/timer-/különleges viselkedést;
ezek tételes konverziós auditja továbbra is szükséges.

[Eredmény és fennmaradó kapuk](../reports/item_combat/2026-10-27_equipment_catalog.md).
A teljes P0-tartalomleltár és az új runtime-csere ezzel nem lett késznek jelölve.

## 2026-10-27 — bonus registry és szemantikai leltár

A típusos bonus-profile validátor és katalógusreferenciák izolált P2-része kész.
A szerződés [IC-005](../adr/item_combat_005_bonus_profiles.md), a friss tesztlelet
[bonus-profile jelentés](../reports/item_combat/2026-10-27_bonus_profiles.md).

Az [APPLY-leltár](item_bonus_mapping.csv) a `common/length.h::EApplyTypes`
mind a 122 deklarált nevét tartalmazza, a feltételes feature neveket is. Ez név-
és deklarációs coverage; **nem** 122 végigkövetett call graph vagy runtime mapping.
Nyolc szemantikai jelölt kapott unit/bucket/cap-nevet, 16 konkrét akadály
forrásszimbólumot és magyarázatot; 98 sor még nem vizsgált.

Közvetlen forrásbizonyíték: `PointSystem.cpp::ApplyPoint` CON/INT esetén több
statot és poolt módosít, MAX_HP/SP esetén double aránnyal az aktuális poolt is
átméretezi; APPLY_SKILL bitcsomagolt skill/sign/amount. `Change` négy grade-bonus
esete a származtatott grade-et is írja. `CombatSystem.cpp::Damage` és a reference
damage ágak eltérő skill/normal szabályokat használnak; az aktuális PvP skill
defense ág 75%-os hatást alkalmaz. A random block/dodge/pierce és rekurzív
reflect az új szerződésben külön újratervezendő, nem mechanikus bp-konverzió.

A jelenlegi regiszter új draft tartalomhoz zárt 25 definíciót fogad; nem
bizonyítja a régi tartalom konverzióját. A factory/StatBuilder/ruleset, további
apply/capability/source/feature audit és offline konverter nyitott kapu marad.

## Felhasználói prioritáspontosítás — 2026-10-27

Az elsődleges cél a kategóriákra bontott item-katalógus és a fix +9 plafon
megszüntetése. A maximumot tárgyanként a fejlesztési profil adja, új protó
nélkül minden szinthez. A fejlesztési sorrend és az ellenőrzési kapu:
[IC-006](../adr/item_combat_006_category_refinement_priority.md).
Ez nem a korábbi harcrendszeri követelmények törlése.

## 2026-10-27 — accessory/material kategóriabővítés

Elkészült az egyszerű wrist/neck/ear ékszerek és öt ordinary material típus
izolált domain/SQL reprezentációja; egy típus egy ItemKey, külön szintprotók
nélkül. Célzott forrásaudit: `EArmorSubTypes/EMaterialSubTypes`, `FindEquipCell`,
`GetItemAttributeSetIndex`, `ModifyPoints`, `ComputeBattlePoints`, accessory
socket/compatibility függvények és a DS-strength material/esély/költség lánc.
A megfigyelések és az importálást továbbra is blokkoló speciális viselkedések:
[IC-007](../adr/item_combat_007_accessory_material_catalog.md).

A schema/hash v3 és az új két SQL-tábla a friss fejlesztési sémában szerepel;
nem élő DB-frissítés. [Friss lelet](../reports/item_combat/2026-10-27_accessory_material_catalog.md).
Nincs késznek jelölt teljes socket/Soul/affix/expiry vagy régi tartalomkonverzió.

## 2026-10-27 — ékszerfoglalat és élettartam célzott auditja

Az ékszer kapacitása és aktív socket-grade külön állapot, nem normál +N szint.
A `UseItemEx` kapacitásnyitási ága 50%-ot dob: az
`ItemSystem_LegacyBridge.cpp:494` maga definiálja az `ENABLE_ADDSTONE_FAILURE`
flaget. A rendes kompatibilis feltöltés ágában közvetlen emelés van; a constants
putPct=70 önmagában nem írja le ennek az ágnak a tényleges működését.

A `common/service.h:85` bekapcsolja az `ENABLE_INFINITE_RAFINES` funkciót.
A `CanPutInto2` és `SetItemAccessorySocketGrade` a tartós feltöltésnél régi
value/vnum és 86410-es időjelölést használ; a timer 86400 fölött nem indul.
A `Start/StopAccessorySocketExpireEvent` a fel-/levételhez kötött; a régi
időzítő a jelen állapotot és megmaradt időt dolgozza fel. Az új izolált modell
explicit lifetime flaget, viselési milliszekundumokat és minden lejárt fokozaton
átvezető, korlátos átmenetet használ, runtime szentinél nélkül.

A `ModifyPoints` proto-apply ágában a százalék/minimum növelés a nem-SKILL
bonust érinti; az affix és extra-proto más forrás. Az új base és refinement
azonos BonusId-jű részeit a későbbi StatBuildernek előbb egyesítenie kell,
különben kétszer járhatna a minimum. Ez még nem implementált statművelet.

A v4 profil, SQL és tiszta állapottervek határa:
[IC-008](../adr/item_combat_008_accessory_socket_profiles.md).
A [friss tesztlelet](../reports/item_combat/2026-10-27_accessory_socket_profiles.md)
nem helyettesíti az owner/erőforrás/receipt tranzakciót, timermentést, teljes
APPLY- és régi tartalomauditot. A P0/P1/P2 és a runtime-csere továbbra is nyitott.

## 2026-10-27 — gyógyító fogyasztható tárgyak

A `UseItemEx` instant/reserve/continuous ágai és az `AffectSystem::UpdateAffect`
gyógyítási része célzottan végigkövetett. A három mechanika eltérő adatmodell:
instant flat és max-pool százalék, reserve összeg és 7%/tick, continuous
egy pool és affect-időtartam. A potion bonus cap sem azonos a két első ágban.
Arena/PvP vnum-listák, potion quota, war accounting és seed/moon cooldown
nem vezethető le pusztán a subtype-ból, ezért ezek tartalmi/runtime kapuk.

A reserve ág előbb módosíthat SP recoveryt, majd a HP guardon `return false`
ággal kiléphet fogyasztás nélkül. Ez kódból azonosított részleges állapotírási
kockázat, nem élő reprodukció vagy most elvégzett régi runtime-fix. Az új
végrehajtó minden ellenőrzést a közös commit elé kell helyezzen.

Az új 5-ös kategória recovery-only profiljai és tiszta azonnali terve:
[IC-009](../adr/item_combat_009_recovery_consumables.md).
[Friss helyi tesztlelet](../reports/item_combat/2026-10-27_recovery_consumables.md).
A reserve/continuous ütemező, többi fogyasztható behavior, tényleges
HP/anyag/cooldown/receipt commit és teljes régi konverzió nem készült el.

## 2026-10-27 — új katalógus SQL-betöltése

A `CAsyncSQL::DirectTransactionPrepared`, `SQLMsg` és prepared eredménytárolás
közvetlenül újrahasznált; az SQL-motor implementációja nem változott.
Az új `CatalogSqlReader::PrepareRead/ReadCatalog/ReadCompleted` egyetlen,
REPEATABLE READ / InnoDB, sor- és byte-keretes SELECT tranzakcióból épít
ellenőrzött snapshotot. A tiszta `DecodeSnapshot` ugyanazt a
`BuildItemCatalog` validátort és kanonikus hash-t használja.

A `011_catalog_format.sql` friss draft metadata-tábla; a kanonikus formátum v5
maradt. Kétkapcsolatos, determinisztikus fixture-próba ellenőrzi az egységes
read view-t és a kapcsolatmegszakítás elutasítását. A valódi SQL-fixture
betöltve is egy prototípussal kezeli a +9/+10/+30 szinteket.
A regiszter minden mezője a fordított definícióhoz ellenőrzött.
[Szerződés](../adr/item_combat_011_catalog_sql_reader.md),
[aktuális lelet és határok](../reports/item_combat/2026-10-27_catalog_sql_reader.md).
Az öt implementált kategórián túli tartalom, runtime factory, tartós instance-
és gazdasági commit, offline migráció és Unity továbbra is nyitott.

## 2026-10-27 — tárgyszerkesztő a szerverkezelőben

A szerverkezelő **Tárgyak** oldala a helyi `protos` katalógus öt betölthető
kategóriáját szerkeszti a natív `CatalogEditorTool`-on keresztül, a játék saját
katalógusellenőrzőjével. Megosztott profil csak másolással változik; mentés
egyetlen, hash- és számlálóvédett tranzakcióban. A meglévő sorok frissítése
`ON DUPLICATE KEY UPDATE` helyett sima `UPDATE`.
[Jelentés](../reports/item_combat/2026-10-27_catalog_editor.md).
