# Item/combat migráció — weapon/armor katalógus és fejlesztési profilok

Ez a jelentés az első weapon/armor részszállítás állapotát rögzíti.
Az ezt követő bonus-profile és hash v2 fejlesztés friss állapota:
[bónuszprofilok](2026-10-27_bonus_profiles.md).

2026-10-27. Ág: `codex/mission-engine`; kiinduló HEAD: `ac4a7cf3`.
**Állapot: elkészült a két felszereléskategória izolált katalógusa és validátora.
A teljes P0/P1/P2 és a játék runtime migrációja még nincs kész.**

## Elkészült viselkedés

- Új `AurigaItemCatalog` könyvtár: saját adatokat birtokló, változtathatatlan
  snapshot; típusos weapon/armor/base és use/trade/appearance/attack/ammo/resistance
  profilok. Nincs átmeneti proto-pointer, régi enum-/vnum-feloldó vagy evaluator.
- Kötelező a base→egy megfelelő child kapcsolat és minden szükséges profillookup.
  Duplikált/árva/rossz kategóriájú sor, hibás policy vagy túlméretes input hibát ad.
  A snapshot csak a teljes ellenőrzés és sikeres hash után érhető el.
- Teljes, kumulatív 0..max refinement-görbe, nullás kezdősor, max 255,
  monoton és kategóriahelyes statok. A raw alap+növekmény capet nem lehet
  túlcsordulással vagy csendes telítéssel megkerülni.
- A 120/30 pontos fegyver +3-on **150/42**, +9-en **210/66**; ugyanaz az ItemKey.
  A 40/20-as páncél +2-es referenciaértéke **50/24**. Ezek tesztfixture-ek,
  nem jóváhagyott production balance.
- Kanonikus SHA-256 a rendezett, explicit kódolt tartalomból; a sorok forrássorrendje
  és a külön release revision nem változtatja a tartalmi hash-t. Független Python
  encoder igazolja a bináris fixture formátumát is.
- Három új DDL: `003_equipment_profiles.sql`, `004_refinement_profiles.sql`,
  `005_weapon_armor_catalog.sql`. A `protos` 14 újmodelles táblát tartalmaz;
  ebbe nem számítanak bele a privát SQL teszt `ic_audit_*` fixture-táblái.
- A MariaDB-próba már a valódi típusos táblákon is ellenőrzi a profil-FK-kat,
  helyi ID-kat, equip slotokat, statcapet, +0 invariánst és a golden értékeket.
  A runtime-jogosultság a fegyverstat módosítását is elutasítja.
- Az önálló build öröklött MSBuild-autolink/CRT hibája javítva a modul céljain,
  a környező build globális beállításainak átírása nélkül.

## Ellenőrzés

- **11/11 normál** és **10/10 ASAN buildből futtatott** célzott CTest sikeres.
- Külön, fő szerverprojekt nélküli build: **5/5 sikeres**.
- Az új C++ suite-ok: **66 katalógus- és 33 refinement-ellenőrzés**; többek között
  nyolc párhuzamos immutable olvasó, inputtól független snapshot-élettartam,
  negatív/szélsőséges integer bemenet, lyukas/duplikált profil, ismeretlen
  referencia és hibás kategória. A korábbi ItemKey suite 197442 ellenőrzése is futott.
- A közös SQL suite **7/7 esetet** futtat saját MariaDB **11.4.13** példányon.
  Ez a DB-engine nem ASAN-instrumentált; a SQL proof a normál build suite-jából jön.
- A meglévő Mission core/reward/resource, item runtime és combat state tesztek
  ebben a célzott körben változatlanul sikeresek.

A friss naplók az ignorált `build/item-catalog-20261027/` könyvtárban vannak.
A követhető forráshash-/környezetlelet a jelentés melletti
`2026-10-27_equipment_catalog.evidence.json`. A korábbi foundation evidence
az előző munkafás pillanatot rögzíti, nem a most módosult fájlok hashét.

```powershell
cmake --build build --config RelWithDebInfo --target ItemKeyTests ItemCatalogTests RefinementProfileTests --parallel 2
cmake --build build-asan --config RelWithDebInfo --target ItemKeyTests ItemCatalogTests RefinementProfileTests --parallel 2
ctest --test-dir build -C RelWithDebInfo -R "^(item_key|item_catalog|refinement_profiles|item_catalog_digest|item_category_generator|item_combat_schema_foundation|item_runtime|combat_state|mission_core|mission_rewards|mission_resources)$" --output-on-failure
ctest --test-dir build-asan -C RelWithDebInfo -R "^(item_key|item_catalog|refinement_profiles|item_catalog_digest|item_category_generator|item_runtime|combat_state|mission_core|mission_rewards|mission_resources)$" --output-on-failure
```

Önálló Windows konfigurálás a már telepített függőségekkel:

```powershell
cmake -S SRC/ItemDomain -B build/item-catalog-standalone-20261027 -G "Visual Studio 18 2026" -A x64 -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows
cmake --build build/item-catalog-standalone-20261027 --config RelWithDebInfo --target ItemKeyTests ItemCatalogTests RefinementProfileTests
ctest --test-dir build/item-catalog-standalone-20261027 -C RelWithDebInfo --output-on-failure
```

## Korlátok és következő munka

A további 16 kategória, a BonusRegistry és bonus-profile tartalom még nincs
implementálva; a builder ezekre **hibát ad**, nem tölti be őket hiányos statokkal.
Az SQL bonus_profile fejtábla csak az FK előkészítése. Az új use/appearance/
currency profilok sem fedik még le a teljes régi tartalom összes különleges
korlátozását vagy szintfüggő megjelenési szabályát. A részletes szerződés:
[ADR IC-004](../../adr/item_combat_004_equipment_catalog.md).

A builder most típusos, memóriában átadott sortáblából dolgozik. A közvetlen,
meglévő prepared SQL infrastruktúrára épülő loader/publisher és az általa végzett
konzisztens DB snapshot/import még a következő munkacsomag része. Az SQL és
C++ validátor külön igazolt; kész SQL→catalog end-to-end betöltést nem állítunk.

A továbblépés sorrendje: teljes auditált bonus-/policy-leképezés és további
típusos kategóriák; SQL loader/publisher, profil- és public projection build;
majd instance/owner/container/revision perzisztencia, az új CombatKernel,
Mission/Dungeon és Unity közvetlen bekötése. A részletes P0 tartalom/writer audit
és a P6 veszteségmentes konverzió továbbra is nyitott.

A GameServer/Database runtime-ja, a Unity, a csomag init-db-ja és a meglévő
játékadatbázis nem lett átállítva. Nem történt távoli művelet, éles migráció,
teljes új szerverbuild, FreeBSD-próba, klienses smoke, benchmark vagy restore.
Az új modul fejlesztési tesztcél, nem kiadható félkész új játék-runtime.
