# AurigaGlobal — kategóriákra bontott item-adatmodell és determinisztikus harcrendszer

**Fájlnév:** `AURIGAGLOBAL_ITEM_COMBAT_MIGRATION_PLAN.md`  
**Dokumentumverzió:** 1.2  
**Készítés kezdete:** 2026-10-26; **véglegesítés:** 2026-10-27, Europe/Bratislava  
**Cél:** részletes, magyar nyelvű Codex implementációs specifikáció.  
**Technológiai célállapot:** C++23, EnTT ECS, szerverautoritatív működés, új kategóriás item-katalógus, egyetlen új harcmotor.  
**Státusz:** implementációs terv és korlátozott forráskódaudit; nem elkészült migráció, nem jóváhagyott balance-adatbázis, nem éles telepítési engedély.

**Hatókör:** kizárólag az aktuális céltermékben megmaradó rendszerek integrációja. A kivezetés alatt álló funkciókhoz nem készül új adatmodell, migrációs ág, tesztkövetelmény vagy runtime-integráció; ezeket a végrehajtó Codex nem állíthatja vissza a teljes referencialeltárra vagy funkciómegőrzésre hivatkozva.

## 0. Először repository audit — a jelen dokumentum bizonyítékai és korlátai

### 0.1. Mit sikerült ténylegesen ellenőrizni?

A dokumentum elkészítése előtt csak olvasási célzott audit történt a megtalált `D:\AurigaGlobal\LiveWork\AurigaGlobal` munkapéldányon. Az aktuális chat munkamappája önmagában nem tartalmazott játékforráskódot; a repository helye egy kapcsolódó Codex-chat metaadataiból vált ismertté, majd a fájlok közvetlen olvasása igazolta a releváns megállapításokat.

Az auditkor olvasott Git HEAD: **`618555886ac8a7a2ec7cd2c0864775f3b94df57e`**. A munkafa nem tiszta. A táblázat munkafából olvasott állapotot ír le; nem állítja, hogy minden megfigyelt sor változatlanul ehhez a commitobjektumhoz tartozik. A végrehajtó Codexnek újra kell rögzítenie a commitot, a módosított fájlok hashét és a saját auditjának időpontját.

Az alábbi útvonalak repository-relatívak. A sorszámok auditkori tájékozódási pontok, módosítás előtt újra keresendők.

| Ellenőrzött megfigyelés | Közvetlen bizonyíték | Implementációs következmény |
|---|---|---|
| C++23 van beállítva | `CMakeLists.txt:122–123`, `CMAKE_CXX_STANDARD 23`, kötelező standard | A cél nem C++17-re visszaportolt megoldás |
| A GameServer EnTT-t használ | `SRC/Server/GameServer/CMakeLists.txt:8–11`, `EnTT::EnTT` | A meglévő ECS-szerkezetből kell kiindulni |
| A registry jelenlegi szerződése főszálhoz kötött | `SRC/Server/GameServer/ecs/Registry.hpp`, `g_registry`, „Must only be accessed from the main game thread” | Worker nem olvashatja/írhatja közvetlenül a jelenlegi registryt |
| Az item proto még régi mezőket tartalmaz | `SRC/Server/common/tables.h:689–726`, `dwVnumRange`, `alValues`, `dwRefinedVnum` | A teljes olvasási és szerializációs gráfot cserélni kell |
| Az ECS proto snapshot még min/max és átmeneti pointer alapú | `ecs/components/item_proto_components.hpp`, `weapon_min`, `weapon_max`, `magic_min`, `magic_max`, `refined_vnum`, `const TItemTable* proto` | Nem elég új SQL-táblákat létrehozni; ezek a runtime függések megszűnnek |
| A factory régi proto-ból állít elő komponenst | `ecs/systems/ItemSystem.cpp:310–353`, `MakeItemPrototype` | Az új factory kizárólag `ItemKey` + katalógus + instance adatot fogad |
| Az item identitás több vnum jellegű mezőt hordoz | `ecs/components/item_components.hpp`, `ItemIdentity` | A transzmutáció, megjelenés és tartalomhivatkozások is auditálandók |
| A fizikai/íj sebzés dobással készül | `combat/battle.cpp:603`, `number(iDamMin, iDamMax)`; `:734`, fegyverértékek közötti dobás | Ezeket új képlet váltja, nem fix min=max kitöltés |
| További harci véletlen és float útvonalak is vannak | `ecs/systems/CombatSystem.cpp:4736–4837` környéke; kritikus, penetrate, block, dodge; `combat/battle.cpp:317–325`, float attack rating | Nem csak a fegyver min/max mezőit kell kivenni |
| Mob min/max API is létezik | `ecs/systems/CombatSystem.hpp`, `GetMobDamageMin`, `GetMobDamageMax`, float multiplierek | A mobok is az új fix értékeket használják |
| Létezik legacy nevű item-forrásfájl és régi include-háló | `ecs/systems/ItemSystem_LegacyBridge.cpp` | Fájlnév alapján nem szabad mindent törölni: funkciónként kell az új domainbe átvinni, majd a bridge-et kivenni |
| Már létezik prepared statement infrastruktúra | `SRC/Server/SQL/SqlParams.h`, `PreparedTransaction.h`, `AsyncSQL.h/.cpp`, `AsyncSQLTransaction.cpp` | Ezt auditálva kell továbbhasználni; nem kell második SQL-motor |
| A tranzakciós modell ismeri az ismeretlen commitkimenetelt | `PreparedTransaction.h`, `SqlTransactionOutcome::CommitUnknown` | Újrapróbálás előtt tartós operation ID szerinti egyeztetés kell |
| A sémafájl jelenleg vnumot tárol | `db/schema.sql:92` körül `player.item`; `:265` körül `player.item_proto`, `refined_vnum` | Item instance és az összes kapcsolódó tároló offline konverziót igényel |
| A kliens régi item_proto fájlt olvas | `SRC/Launcher/Game/ItemManager.cpp:284–409`, stride/verzió/vnum kezelés | Új klienskatalógus és protokoll is része a feladatnak |
| A Mission Engine domain létezik | `mission/MissionDefinition.hpp`, `MissionEventRouter.hpp`, `MissionPersistence.cpp` | Az új harci események ide natív, típusos szerződéssel kapcsolódnak |
| A Mission jutalom még 32 bites tárgyazonosító térképet használ | `mission/MissionRewardPayload.hpp`, `std::map<uint32_t,uint32_t> items/consumeItems` | A reward package, codec, hash, DB és award sorok is `ItemKey`-re migrálandók |
| Jelenlegi harci esemény mérete és tartalma korlátozott | `ecs/events.hpp`, `EvEntityDamaged::damage` `int32_t`; `EvEntityDied` ECS handle-ekkel | Új, tartós identitást és fixpontos összegeket hordozó esemény kell |
| Közvetlen dungeon kill-hívások is vannak | `ecs/systems/CombatSystem.cpp:3658–3667` körül | Egyetlen death commitból kell eseményt adni; a fogyasztók frissülnek |
| Már vannak releváns tesztcélok | GameServer CMake; `CombatStateTests`, `PointCalculationTests`, `ItemRuntimeTests`, Mission/Dungeon/SQL tesztek | A regressziós ellenőrzés a meglévő teszteket is lefedi |

Auditkor módosított/új állapot volt többek között az `InventorySystem`, `ItemAttributeSystem`, `ItemSystem.hpp`, `social/shop*`, Mission-dokumentáció és néhány item/shop/quickslot teszt körül. A dokumentumkészítés nem változtatta meg ezeket. A végrehajtás előtt a friss munkafa tulajdonosi és ütközési helyzetét újra fel kell mérni; felhasználói változtatás nem írható felül.

### 0.2. Mi nem ellenőrzött?

- Nem történt teljes call graph audit, repository-build, meglévő játékteszt-futtatás, szerverindítás vagy DB-kapcsolódás. Az elkészült dokumentum C++-mintáinak külön, izolált fordítási és számpélda-ellenőrzését a 25. fejezet rögzíti.
- Nem ismert az éles DB terméke/verziója, storage engine konfigurációja, adatmennyisége, replikációja, SQL-mode-ja vagy backupjának visszaállíthatósága. A MariaDB nevű tesztcélok önmagukban nem bizonyítják az éles DB verzióját.
- Nem ellenőrzött a tényleges EnTT/Boost verzió, a támogatott fordítómátrix, az összes feature flag, a production tickidő és a kliens összes elosztási útvonala.
- Nem ismert az eredeti kategóriafelosztás teljes, korábban elfogadott táblázata, a végleges balance, a különleges itemek minden szemantikája, illetve a Mission/Dungeon események teljes jelenlegi bekötése.
- A `Sebzésrendszer összehasonlítása` (`6adfbca7-8c50-83eb-a76a-ac0a4286cb53`) és az `Item_proto felosztás visszakeresése` (`6ad111e7-79f8-83eb-84b6-9a2028a7fe9f`) beszélgetések olvasása megtörtént. Az olvasható felhasználói üzenetek megerősítik az új felosztás, a fix támadóérték és az átmeneti réteg nélküli terv igényét. A korábbi asszisztensválaszok helyén a lekérdezés csak `chatgpt-content-reference` jelölőket adott; azok hiányzó részleteit ez a dokumentum nem állítja helyre hiteles előzményként.

A további fejezetekben **kötelező követelmény** az aktuális felhasználói kérésből fakadó döntés; **normatív új terv** az itt definiált, implementálandó célviselkedés; **javasolt kiinduló érték** a később balance-audittal rögzítendő szám; **audit-TODO** pedig bizonyíték nélkül nyitott repository/üzemeltetési tény. Egy új terv attól még pontosan implementálható, hogy nem a jelenlegi motor leírása.

### 0.3. Kötelező első Codex-fázis

Minden kódmódosítás előtt:

1. Olvasd el az alkalmazandó `AGENTS.md` és build/teszt útmutatókat. Rögzítsd a repo gyökerét, HEAD-et, munkafaállapotot és toolchain-verziókat. A jelen célzott keresés nem talált alkalmazandó `AGENTS.md` fájlt; ezt az implementáció kezdetén újra ellenőrizd.
2. Kövesd végig a bejövő támadási intenttől a HP-változásig, halálig, dropig, Mission/Dungeon eseményig, DB-mentésig és kliensmegjelenítésig tartó hívási láncot.
3. Leltározd a megmaradó rendszerek összes item-hivatkozását: DB, packet, cache, NPC-shop, safebox, mall, trade, mail/award, crafting/refine, drop, quest, Mission, dungeon, pet/mount, transzmutáció, socket, GM, wiki, launcher/tool, localization és asset.
4. Keresd a `vnum`, `refined_vnum`, `dwRefinedVnum`, `dwVnumRange`, `alValues`, `GetItemValue`, `weapon_min/max`, `magic_min/max`, `GetMobDamageMin/Max`, `number`, RNG, float és közvetlen HP-írásokat. A találat szemantikai vizsgálatot igényel; például loot-RNG nem automatikusan harci sebzésszórás.
5. Minden régi bonus/apply/skill formula esetén írd le: új mező, mértékegység, bucket, sorrend, előjel, cap, érintett módok és teszteset. Nem leképezhető elem blokkolja az érintett tartalom publikálását.
6. Készíts `docs/audits/item_combat_repository_audit.md`, hivatkozási leltárt és `docs/adr/` döntéseket. Minden tényhez fájl+szimbólum vagy izolált mérés tartozzon. Külön legyen „nem vizsgált” és „nem található”.
7. Állapítsd meg a tényleges build/teszt parancsokat és futtasd az érintett meglévő alapteszteket. Korábbi buildlog nem friss sikerbizonyíték.

Az audit után az alábbi specifikációt a tényleges fájlokhoz igazítsd. Eltérő repoállapot esetén ne találj ki nem létező API-t, és ne alakíts vissza már natív ECS-funkciót legacy objektummá.

## 1. Megváltoztathatatlan követelmények

1. **Nincs runtime legacy adapter, kompatibilitási proto, vnum-feloldó vagy párhuzamos régi harcmotor.** Az új indítható release kizárólag az új itemmodellt és új damage pipeline-t használja.
2. **Adatátvétel kizárólag egyszeri offline migrációval történhet.** Az offline eszköz külön executable/tool; a GameServer, Database és kliens runtime nem linkeli és nem tölti be.
3. **A prototípusok külön, `protos` nevű adatbázist kapnak az `account`, `common`, `log`, `player` és `mission` mellé.** A `protos.item_base`, a típusos kategóriatáblák és a kapcsolódó statikus profilok itt találhatók. Nem készül új univerzális `value0..valueN` vagy mindenre használt JSON/EAV item-proto.
4. Kategóriánként a `local_id` **1-ről indul**. A globális tárgyprototípus-azonosító az összetett **`ItemKey{category_id, local_id}`**. A puszta `local_id` nem globális azonosító.
5. Egy szemantikai tárgytípusnak **egy prototípusa** van. A fejlesztettség az instance **`refinement_level`** állapota és egy profil szabálya. Nincs külön +0…+9 proto, `refined_vnum`, vnum+szint aritmetika vagy névvégből kiolvasott runtime szint.
6. Fix fizikai és mágikus alapértékekből, fejlesztési növekményekből és rögzített képletekből számolunk. **Nincs min/max támadóérték és nincs véletlen alapsebzés-szórás.**
7. Harci véletlen kizárólag a névvel, verzióval és szabállyal engedélyezett **kritikus** és **külön proc** döntésekben lehet. Azonos bemenet és azonos rögzített döntések esetén a teljes eredmény bitazonos.
8. A szerver az itemállapot, stat, találat, sebzés, HP, jutalom és esemény kizárólagos autoritása. A kliens intentet küld és megjelenít.
9. Az instance-állapot ECS-tulajdon, az immutable prototípuskatalógus közös olvasási adat. Egy mezőnek egyetlen írható igazságforrása legyen.
10. A matematikai sorrend, kerekítés, overflow-kezelés, eseménysorrend, ownership és release/rollback szerződés része a funkciónak; nem későbbi optimalizálási részlet.

**Tiltott „gyors megoldások”:** régi min/max azonos értékre állítása a régi motor megtartásával; új `ItemKey` régi vnumra fordítása; runtime kétirányú táblázat; `NewCombatEnabled` régi motorra visszakapcsoló ág; két motor árnyékfuttatása éles kéréseken; külön kliens-balance adatbázis; kliens által küldött végső damage; `reinterpret_cast`-olt packet/DB-struktúrák.

## 2. Fogalmak, modulhatárok és identitások

| Fogalom | Jelentés | Nem helyettesítheti |
|---|---|---|
| `ItemKey` | Tartalmi prototípus: kategória + helyi azonosító | Instance ID, ECS entity, régi vnum |
| `ItemInstanceId` | Egy konkrét tárgypéldány tartós 64 bites azonosítója | `ItemKey`, kliens inventory slot |
| `entt::entity` | Egy registryben érvényes, generációs runtime handle | DB/packetben tartós személyazonosság |
| `EntityRef` | Szerveres scope + stabil entity ID + spawn/generáció azonosító | Kliens által szabadon kijelölhető objektum |
| `CatalogRevision` | Immutable tartalomkiadás monoton release-száma | Protokollséma vagy instance revision |
| `RulesetId/hash` | Matematikai és játékszabályok verziózott identitása | Katalógus hash önmagában |
| `state_revision` | Egy item/karakter változásainak optimista konkurenciaverziója | Katalógusverzió |
| `ActionId`, `DamageEventId`, `DeathId` | Szerver által kiosztott idempotencia-/sorrendazonosítók | Kliens sequence önmagában |

Rétegek:

```text
Offline export + explicit mapping + validátor
                   ↓ egyszeri import
Új SQL-séma → CatalogBuilder → immutable CatalogSnapshot + kliensprojekció
                                      ↓
Network intent → owner-thread admission → ECS stat snapshot
                                      ↓
                         tiszta CombatKernel (worker)
                                      ↓
                rendezett owner-thread commit + HP + ledger
                                      ↓
              DamageApplied / EntityDied / item domain events
                    ↓                         ↓
           Mission / Dungeon             kliens szinkron
                    ↓
      tartós inbox/outbox + idempotens reward/resource commit
```

A worker nem beszél SQL-lel, hálózattal, Lua-val, Mission VM-mel vagy a globális registryvel. A normál runtime domainfogyasztókat közvetlenül frissíteni kell az új szerződésekre; nem épül közéjük régi adatmodellt utánzó átmeneti réteg.

## 3. Kategóriás item_proto és ItemKey

### 3.1. Azonosító-szerződés

- `category_id`: előjel nélküli 16 bit; 0 érvénytelen; kiadott érték nem számozható át és nem hasznosítható újra.
- `local_id`: előjel nélküli 32 bit; 0 érvénytelen; kategóriánként indul 1-ről. Az első import determinisztikusan osztja ki; később törölt elemek helye lyuk marad.
- `(1,1)` és `(2,1)` két külön tárgy. Tiltott a globális `AUTO_INCREMENT` local_id-kiosztás.
- Új szerkesztői azonosító-kiosztás kategóriánként sorzárolt számlálóval vagy single-writer catalog builddel történjen; versenyhelyzetben `MAX(local_id)+1` nem elfogadható.
- A `content_code` stabil, egyedi, emberi/szerkesztői kulcs, például `weapon.iron_sword`. Játék runtime betöltés után ne string-parse alapján keressen.
- A 64 bites csomagolás csak az összetett kulcs veszteségmentes reprezentációja, nem új vnum. Alsó 32 bit: local; következő 16: category; felső 16: kötelezően 0. Hibás felső biteket nem szabad levágni.
- SQL-ben a természetes összetett kulcsot tároljuk. Wire-on explicit mezőnkénti little-endian kódolás; sem `sizeof(ItemKey)`, sem struct padding nem protokoll.
- JSON/szkript tartalomnál `{ "category_id": 1, "local_id": 1 }` vagy validált `content_code` használható. Általános JavaScript környezetben 64 bites azonosító decimális string/BigInt; ne veszítsen pontosságot `Number` konverzión.

```cpp
#include <compare>
#include <cstdint>
#include <expected>
#include <utility>

enum class ItemCategory : std::uint16_t { Weapon = 1, Armor = 2 };
enum class KeyError { InvalidCategory, ZeroLocalId, ReservedBits };

struct ItemKey {
    ItemCategory category{};
    std::uint32_t local_id{};
    auto operator<=>(const ItemKey&) const = default;
};

constexpr std::uint64_t pack_item_key(ItemKey key) noexcept {
    return (std::uint64_t{std::to_underlying(key.category)} << 32)
         | std::uint64_t{key.local_id};
}

// A domain-valid kategóriát a teljes, generált kategóriaregiszter ellenőrzi.
constexpr auto unpack_item_key(std::uint64_t value)
    -> std::expected<ItemKey, KeyError> {
    if ((value >> 48) != 0) return std::unexpected(KeyError::ReservedBits);
    const auto category = static_cast<std::uint16_t>(value >> 32);
    const auto local = static_cast<std::uint32_t>(value);
    if (category == 0) return std::unexpected(KeyError::InvalidCategory);
    if (local == 0) return std::unexpected(KeyError::ZeroLocalId);
    return ItemKey{static_cast<ItemCategory>(category), local};
}
static_assert(pack_item_key(ItemKey{ItemCategory::Weapon, 1}) == 0x0000000100000001ULL);
```

A bemutatott enum szándékosan rövid típuspélda; a kiadott enum a teljes kategóriaregiszterből generálandó. Az `unpack` szerkezeti ellenőrzés után a catalog lookupnak el kell utasítania az ismeretlen kategóriát és a hiányzó kulcsot. A `pack` csak már validált domainkulcsra hívható. `std::expected` használatához a C++23 standard library támogatását az auditban fordítási próbával ellenőrizni kell; a várható érték/hiba modell a C++ standard munkadokumentumában leírt típusra épül. [C++ expected specifikáció](https://eel.is/c++draft/expected)

### 3.2. Javasolt kategóriaregiszter — nem visszaigazolt történeti táblázat

Az alábbi összefüggő kiinduló felosztás a jelen specifikáció új terve. A meglévő `common/item_length.h::EItemTypes` ténylegesen tartalmaz fegyver, páncél, use/autouse, material, metin, container, fish/rod, unique, könyv, quest, polymorph, costume, DS, ring és belt típusokat. Az összes altípus és feature flag teljes megfeleltetése audit-TODO. A számkódok az első új release kiadásakor válnak véglegessé.

| Új category_id | Típusos tábla | Szemantika / tipikus régi forrás |
|---:|---|---|
| 1 | `item_weapon` | Weapon: kard, tőr, íj, harang stb.; subtype külön enum |
| 2 | `item_armor` | Testpáncél, sisak, pajzs, cipő; slot és armor subtype explicit |
| 3 | `item_accessory` | Nyaklánc, karkötő, fülbevaló, ring, belt, unique felszerelhető tárgy |
| 4 | `item_costume` | Costume, megjelenési hair; külső appearance profile |
| 5 | `item_consumable` | Use/autouse/blend/extract és egyszeri aktiválható effektusok |
| 6 | `item_material` | Material/resource/crafting alapanyag; nem használati effektus |
| 7 | `item_socket_stone` | Metin/foglalatkő, explicit socket compatibility |
| 8 | `item_container` | Treasure box/giftbox/container/lottery; drop/reward profile |
| 9 | `item_skill_book` | Skillbook/skillforget: tanítási/felejtési profil |
| 10 | `item_quest` | Quest token; explicit Mission-tartalomazonosító |
| 11 | `item_tool` | Pick/rod/tool/campfire/totem funkció; validált tool subtype |
| 12 | `item_fishing` | Fish/csalik és feldolgozási profil; a bot tool |
| 13 | `item_polymorph` | Explicit MobKey/forma és transzformációs profil |
| 14 | `item_soul` | DS/special DS; typed grade/stage/effect profile, nem vnum-számjegyek |
| 15 | `item_companion` | Itemként létező pet/mount hívó vagy pecsét, ha az audit ilyet talál |
| 16 | `item_currency_token` | Itemként hordozott pénz/token; valódi wallet nem item-instance |
| 17 | `item_special` | Tényleg különleges, névvel regisztrált behavior; nincs általános értéktömb |
| 18 | `item_container_key` | Treasure key, engedélyezett container/reward profile kapcsolat |

Nem minden régi `ITEM_SPECIAL` kerül automatikusan 17-be: funkció szerint kell besorolni. `ITEM_NONE`, ismeretlen flag vagy altípus publikálási hiba. `item_special` nem lehet menekülőút feldolgozatlan adathoz. Minden regisztrált behavior saját validált struktúrát és tesztet kap; nem regisztrált behavior tiltott.

### 3.3. Közös és kategóriaspecifikus mezők

`item_base`: kulcs, `content_code`, lokalizációs név/leírás kulcsa, ikon/appearance hivatkozás, méret, stacklimit, bind/trade policy, súly, minimális szint, használhatósági profil, refinement profile, alap bonus profile, tiltott/retired állapot. A gazdasági ár és a pénznem explicit mező vagy árprofil, ne rejtett `value` mező legyen.

A kategóriatáblák saját mezői többek között:

- Weapon: weapon subtype, equip slot policy, fix fizikai/mágikus alap, attack timing profile, ammo policy, range/reach, engedélyezett skill family.
- Armor: slot, fix fizikai/mágikus védelem, resistance profile.
- Accessory: slot, base stat profile, socket/refinement compatibility; nem másolja a weapon mezőit.
- Consumable: effect profile, charge/use/cooldown policy, target policy, combat-use engedély. Egy stack használata instance-művelet.
- Material: material family, craft tags; nincs üres attack oszlop.
- Socket stone: elfogadó socket family, stat/proc profile, insertion/removal policy.
- Container/key: reward profile és jogosultság; nem szabad kliens által választott jutalomtáblát használni.
- Skill book: skill ID/family, learning policy; quest: Mission content key és consume policy.
- Tool/fishing/polymorph/soul/companion/special: névvel regisztrált típusos profilok. Soul grade/stage nem kerülhet vissza numerikus ID-interpretációba.

Egy base rekordhoz pontosan a kategóriájának megfelelő egyetlen kiegészítő rekord tartozik. A fordított FK önmagában nem garantálja a base→child teljességet: a CatalogBuilder ezt kötelezően ellenőrzi, és hiány, többes besorolás vagy más kategóriájú child esetén nem publikál snapshotot.

## 4. Konkrét SQL-célséma és integritás

### 4.1. SQL-példa környezete

Az alábbi DDL **új terv és referencia**, MySQL 8.4/InnoDB szemantikára írva, külön új tesztsémában futtatandó. Nem állítás a jelenlegi éles DB-ről, és nem közvetlenül lefuttatandó production patch. MariaDB vagy más igazolt célverzió esetén a migráció SQL-dialektusát, CHECK/FK-kikényszerítését és tranzakciós viselkedését izolált teszttel kell véglegesíteni.

A katalógus release-folyamattal előállított immutable adat a külön `protos` adatbázisban. A példában egyszerre egy aktív katalógus DB-tartalma van; régi release-ek teljes snapshotként archiváltak. A live instance-kulcsok nem változnak katalóguskiadásonként. Hivatkozott prototípus nem törölhető: szükség esetén retired állapotban marad. Inkompatibilis kataloguscsere karbantartási leállással történik.

### 4.1.1. Kötelező adatbázis-felosztás: `protos`

A célrendszer adatbázisai: **`account`, `common`, `log`, `player`, `mission`, `protos`**. A `protos` ezekkel egyenrangú, külön adatbázis; nem táblanév-prefix, nem a `player` egy logikai mappája, és nem önálló fizikai DB-szerver bevezetésének követelménye. A jelen SQL-referencia ugyanazon MySQL/MariaDB példány adatbázisaival számol; az aktuális telepítési topológiát az audit ellenőrzi.

| Adatbázis | A jelen migrációban ide tartozó adatok |
|---|---|
| `protos` | Item-, mob- és skill-prototípusok; `item_category`, `item_base`, minden `item_*` kategóriatábla; refinement-, bonus-, attack-, effect-, resistance-, appearance- és egyéb prototípushoz tartozó statikus profilok; statikus harci ruleset és `content_release` metaadat |
| `player` | Konkrét item instance, refinement_level, affix/socket state, ownership/location, karakterállapot, gazdasági operation receipt és a player/combat domain tartós eseményforrása |
| `mission` | Mission definíció/package, futó állapot, checkpoint, entitlement, valamint a Mission domain inbox/outbox és feldolgozási állapota |
| `account`, `common`, `log` | A saját domainjük adatai; az új prototípuskatalógus nem kerül ezekbe. A diagnosztikai log nem helyettesíti a tranzakciós receiptet vagy outboxot |

Az item instance állapot **nem** kerül át a `protos` adatbázisba. Például a prototípus `protos.item_base`, a példány `player.item_instance`; a példány refinement_level-je a `player` állapota, az értékét meghatározó profil a `protos` adata. A Mission állapot és jutalom-feldolgozás továbbra is a saját domainjéhez tartozik.

Minden DDL, FK és prepared SQL teljesen minősített `adatbazis.tabla` nevet használjon. Így a connection alapértelmezett adatbázisa nem irányíthat katalógusírást a `player`-be vagy instance-írást a `protos`-ba. A példában `player.item_instance(category_id, local_id)` → `protos.item_base(category_id, local_id)` a prototípus-FK; az azonos DB-példány, InnoDB és szükséges jogosultságok feltételeit izolált teszt igazolja. Több adatbázist érintő atomi gazdasági művelet ugyanazon connection egy tranzakcióján belül történjen; külön connectionök tranzakcióit nem szabad egyetlen atomi commitnak tekinteni.

A DB-konfiguráció kapjon explicit `protos` adatbázis-beállítást és ellenőrzött katalogus-hozzáférést. A runtime ezt csak olvassa; a katalóguskiadó és offline migrátor külön írási jogosultságot kap. Az adatbázis-/táblanevek megbízható konfigurációból vagy rögzített SQL-ből származzanak, nem kliensinputból. A CatalogBuilder a `protos` adatbázisból készíti az immutable szerverkatalógust és a kliensprojekciót; a DB nem kérdezhető le hitről hitre.

Az egyszeri offline migráció a régi `player.item_proto` és kapcsolódó régi protóforrások adatait közvetlenül a végleges `protos` modellbe konvertálja. A régi táblák nem maradhatnak runtime másolatként, szinkronizált forrásként vagy kompatibilitási VIEW-ként. Az új mob-/skill-táblák konkrét szerkezetét az auditált prototípusmodellhez kell igazítani, de céladatbázisuk szintén kötelezően `protos`.

```sql
CREATE DATABASE IF NOT EXISTS protos
    CHARACTER SET utf8mb4 COLLATE utf8mb4_bin;

-- A player és mission adatbázis a referencia környezet előfeltétele.
CREATE TABLE protos.item_category (
    category_id SMALLINT UNSIGNED NOT NULL,
    code VARCHAR(48) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
    PRIMARY KEY (category_id), UNIQUE KEY uq_category_code (code),
    CHECK (category_id > 0)
) ENGINE=InnoDB;

CREATE TABLE protos.bonus_profile (
    profile_id INT UNSIGNED NOT NULL,
    code VARCHAR(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
    PRIMARY KEY (profile_id), UNIQUE KEY uq_bonus_code (code)
) ENGINE=InnoDB;

CREATE TABLE protos.refinement_profile (
    profile_id INT UNSIGNED NOT NULL,
    code VARCHAR(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
    max_level SMALLINT UNSIGNED NOT NULL,
    PRIMARY KEY (profile_id), UNIQUE KEY uq_refinement_code (code),
    CHECK (max_level <= 255)
) ENGINE=InnoDB;

-- A step N a 0-hoz képesti ÖSSZESÍTETT növekmény, nem az előző szinthez képesti.
-- *_raw: 1 megjelenített statpont = 1000 raw. A 0. szint növekménye nulla.
CREATE TABLE protos.refinement_level (
    profile_id INT UNSIGNED NOT NULL,
    level SMALLINT UNSIGNED NOT NULL,
    physical_attack_gain_raw BIGINT NOT NULL DEFAULT 0,
    magical_attack_gain_raw BIGINT NOT NULL DEFAULT 0,
    physical_defense_gain_raw BIGINT NOT NULL DEFAULT 0,
    magical_defense_gain_raw BIGINT NOT NULL DEFAULT 0,
    extra_bonus_profile_id INT UNSIGNED NULL,
    PRIMARY KEY (profile_id, level),
    FOREIGN KEY (profile_id) REFERENCES protos.refinement_profile(profile_id),
    FOREIGN KEY (extra_bonus_profile_id) REFERENCES protos.bonus_profile(profile_id),
    CHECK (level <= 255),
    CHECK (physical_attack_gain_raw BETWEEN 0 AND 1000000000000000),
    CHECK (magical_attack_gain_raw BETWEEN 0 AND 1000000000000000),
    CHECK (physical_defense_gain_raw BETWEEN 0 AND 1000000000000000),
    CHECK (magical_defense_gain_raw BETWEEN 0 AND 1000000000000000),
    CHECK (level <> 0 OR (physical_attack_gain_raw = 0
      AND magical_attack_gain_raw = 0 AND physical_defense_gain_raw = 0
      AND magical_defense_gain_raw = 0 AND extra_bonus_profile_id IS NULL))
) ENGINE=InnoDB;

CREATE TABLE protos.item_base (
    category_id SMALLINT UNSIGNED NOT NULL,
    local_id INT UNSIGNED NOT NULL,
    content_code VARCHAR(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
    name_key VARCHAR(128) NOT NULL,
    description_key VARCHAR(128) NOT NULL,
    icon_asset VARCHAR(192) NOT NULL,
    appearance_profile_id INT UNSIGNED NULL,
    inventory_width TINYINT UNSIGNED NOT NULL DEFAULT 1,
    inventory_height TINYINT UNSIGNED NOT NULL DEFAULT 1,
    max_stack INT UNSIGNED NOT NULL DEFAULT 1,
    min_level SMALLINT UNSIGNED NOT NULL DEFAULT 0,
    use_policy_id INT UNSIGNED NOT NULL,
    trade_policy_id INT UNSIGNED NOT NULL,
    refinement_profile_id INT UNSIGNED NOT NULL,
    base_bonus_profile_id INT UNSIGNED NULL,
    retired BOOLEAN NOT NULL DEFAULT FALSE,
    PRIMARY KEY (category_id, local_id),
    UNIQUE KEY uq_item_content_code (content_code),
    FOREIGN KEY (category_id) REFERENCES protos.item_category(category_id),
    FOREIGN KEY (refinement_profile_id) REFERENCES protos.refinement_profile(profile_id),
    FOREIGN KEY (base_bonus_profile_id) REFERENCES protos.bonus_profile(profile_id),
    CHECK (local_id > 0), CHECK (max_stack > 0),
    CHECK (inventory_width > 0 AND inventory_height > 0)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin;

CREATE TABLE protos.item_weapon (
    category_id SMALLINT UNSIGNED NOT NULL DEFAULT 1,
    local_id INT UNSIGNED NOT NULL,
    weapon_subtype SMALLINT UNSIGNED NOT NULL,
    physical_attack_base_raw BIGINT NOT NULL,
    magical_attack_base_raw BIGINT NOT NULL,
    attack_profile_id INT UNSIGNED NOT NULL,
    ammo_policy_id INT UNSIGNED NOT NULL,
    PRIMARY KEY (category_id, local_id),
    FOREIGN KEY (category_id, local_id) REFERENCES protos.item_base(category_id, local_id),
    CHECK (category_id = 1),
    CHECK (physical_attack_base_raw BETWEEN 0 AND 1000000000000000),
    CHECK (magical_attack_base_raw BETWEEN 0 AND 1000000000000000)
) ENGINE=InnoDB;

CREATE TABLE protos.item_armor (
    category_id SMALLINT UNSIGNED NOT NULL DEFAULT 2,
    local_id INT UNSIGNED NOT NULL,
    equip_slot SMALLINT UNSIGNED NOT NULL,
    physical_defense_base_raw BIGINT NOT NULL,
    magical_defense_base_raw BIGINT NOT NULL,
    resistance_profile_id INT UNSIGNED NOT NULL,
    PRIMARY KEY (category_id, local_id),
    FOREIGN KEY (category_id, local_id) REFERENCES protos.item_base(category_id, local_id),
    CHECK (category_id = 2),
    CHECK (physical_defense_base_raw BETWEEN 0 AND 1000000000000000),
    CHECK (magical_defense_base_raw BETWEEN 0 AND 1000000000000000)
) ENGINE=InnoDB;

CREATE TABLE protos.item_consumable (
    category_id SMALLINT UNSIGNED NOT NULL DEFAULT 5,
    local_id INT UNSIGNED NOT NULL,
    effect_profile_id INT UNSIGNED NOT NULL,
    cooldown_group_id INT UNSIGNED NOT NULL,
    use_during_combat BOOLEAN NOT NULL,
    PRIMARY KEY (category_id, local_id),
    FOREIGN KEY (category_id, local_id) REFERENCES protos.item_base(category_id, local_id),
    CHECK (category_id = 5)
) ENGINE=InnoDB;

CREATE TABLE protos.item_material (
    category_id SMALLINT UNSIGNED NOT NULL DEFAULT 6,
    local_id INT UNSIGNED NOT NULL,
    material_family SMALLINT UNSIGNED NOT NULL,
    craft_tag_profile_id INT UNSIGNED NOT NULL,
    PRIMARY KEY (category_id, local_id),
    FOREIGN KEY (category_id, local_id) REFERENCES protos.item_base(category_id, local_id),
    CHECK (category_id = 6)
) ENGINE=InnoDB;

CREATE TABLE player.item_instance (
    instance_id BIGINT UNSIGNED NOT NULL,
    category_id SMALLINT UNSIGNED NOT NULL,
    local_id INT UNSIGNED NOT NULL,
    refinement_level SMALLINT UNSIGNED NOT NULL DEFAULT 0,
    stack_count INT UNSIGNED NOT NULL DEFAULT 1,
    durability_raw BIGINT NOT NULL DEFAULT 0,
    binding_state SMALLINT UNSIGNED NOT NULL DEFAULT 0,
    expires_at_utc_ms BIGINT NULL,
    state_revision BIGINT UNSIGNED NOT NULL DEFAULT 1,
    PRIMARY KEY (instance_id),
    KEY ix_instance_proto (category_id, local_id),
    FOREIGN KEY (category_id, local_id) REFERENCES protos.item_base(category_id, local_id),
    CHECK (instance_id > 0), CHECK (refinement_level <= 255),
    CHECK (stack_count > 0), CHECK (durability_raw >= 0), CHECK (state_revision > 0)
) ENGINE=InnoDB;

-- Az owner/container domének végleges kódjai ADR-ben rögzítendők.
-- 1=karakter inventory, 2=karakter equipment; a többi owner fajta külön leltározandó.
CREATE TABLE player.item_location (
    instance_id BIGINT UNSIGNED NOT NULL,
    owner_kind SMALLINT UNSIGNED NOT NULL,
    owner_id BIGINT UNSIGNED NOT NULL,
    container_kind SMALLINT UNSIGNED NOT NULL,
    slot_index INT UNSIGNED NOT NULL,
    PRIMARY KEY (instance_id),
    UNIQUE KEY uq_owner_slot (owner_kind, owner_id, container_kind, slot_index),
    FOREIGN KEY (instance_id) REFERENCES player.item_instance(instance_id),
    CHECK (owner_kind > 0), CHECK (owner_id > 0)
) ENGINE=InnoDB;

CREATE TABLE player.item_instance_attribute (
    instance_id BIGINT UNSIGNED NOT NULL,
    slot_index SMALLINT UNSIGNED NOT NULL,
    bonus_id INT UNSIGNED NOT NULL,
    value_scaled BIGINT NOT NULL,
    PRIMARY KEY (instance_id, slot_index),
    FOREIGN KEY (instance_id) REFERENCES player.item_instance(instance_id)
) ENGINE=InnoDB;

CREATE TABLE player.item_instance_socket (
    instance_id BIGINT UNSIGNED NOT NULL,
    slot_index SMALLINT UNSIGNED NOT NULL,
    stone_category_id SMALLINT UNSIGNED NOT NULL DEFAULT 7,
    stone_local_id INT UNSIGNED NOT NULL,
    stone_refinement_level SMALLINT UNSIGNED NOT NULL DEFAULT 0,
    PRIMARY KEY (instance_id, slot_index),
    FOREIGN KEY (instance_id) REFERENCES player.item_instance(instance_id),
    FOREIGN KEY (stone_category_id, stone_local_id)
      REFERENCES protos.item_base(category_id, local_id),
    CHECK (stone_category_id = 7), CHECK (stone_refinement_level <= 255)
) ENGINE=InnoDB;

CREATE TABLE protos.content_release (
    release_id BIGINT UNSIGNED NOT NULL,
    catalog_schema_version INT UNSIGNED NOT NULL,
    protocol_schema_version INT UNSIGNED NOT NULL,
    ruleset_version INT UNSIGNED NOT NULL,
    catalog_sha256 BINARY(32) NOT NULL,
    ruleset_sha256 BINARY(32) NOT NULL,
    client_projection_sha256 BINARY(32) NOT NULL,
    PRIMARY KEY (release_id), CHECK (release_id > 0)
) ENGINE=InnoDB;

-- Idempotens gazdasági műveletek végleges commitbizonylata.
CREATE TABLE player.item_operation_receipt (
    operation_id BINARY(16) NOT NULL,
    owner_id BIGINT UNSIGNED NOT NULL,
    request_sha256 BINARY(32) NOT NULL,
    outcome_code SMALLINT UNSIGNED NOT NULL,
    result_payload VARBINARY(4096) NOT NULL,
    PRIMARY KEY (operation_id)
) ENGINE=InnoDB;
```

A példa a kulcstáblák létrehozását mutatja; nem teljes, önálló production schema. Az összes további kategóriatáblát a 3. fejezet mezőszerződésével kell megírni. Az `*_policy_id`, effect/appearance/attack/resistance/craft profile és `bonus_id` hivatkozásokhoz tényleges táblák, enumregiszterek és FK-k/validátorok kellenek a végleges migrációban; névtelen, feloldatlan számazonosító nem maradhat. DB-constraint és catalog/domain-validátor együtt ad integritást.

A CHECK-ek lokális sorfeltételek; például `refinement_level <= item_base.profile.max_level` nem fejezhető ki itt másik táblára hivatkozó CHECK-ként. Ezt a katalógusvalidátor és a tranzakcióban dolgozó domainellenőrzés végzi. A kompozit FK-knál a típus, előjel és oszlopsorrend egyezzen. [MySQL FK-szabályok](https://dev.mysql.com/doc/refman/8.4/en/create-table-foreign-keys.html), [MySQL CHECK-szabályok](https://dev.mysql.com/doc/refman/8.4/en/create-table-check-constraints.html)

### 4.2. Kötelező, a rövid DDL-en túli integritás

- Pontosan egy domainhely minden aktív instance-hoz. Létrehozás/törlés/move egy tranzakció; nem commitolható location nélküli félkész tárgy. Törléshez explicit tombstone/audit policy kell, nem árva rekord.
- Többcellás inventory: a fenti unique csak az anchor slotot védi. Kell `item_occupied_cell(owner_kind,owner_id,container_kind,cell,instance_id)` unique cell foglalással, vagy bizonyítottan atomikus container writer az összes érintett cella tranzakciós ellenőrzésével. Az anchor unique önmagában elégtelen.
- Ground: map/instance/pozíció és drop ownership külön típusos tárolás. Trade escrow, mail, account safebox és Mission pending award saját container/owner fajtát kap. Nem hamis karakterownerrel ábrázolandó.
- Az ownership és location fogalmak különböznek a földön lévő tárgy ideiglenes loot-jogosultságától. Az utóbbi időzített access policy, nem második tulajdonos.
- Stack merge csak azonos ItemKey, refinement, bind, expiry, affix, socket és egyéb szemantikailag releváns state mellett. Nem stackelhető fejleszthető felszerelésnél `max_stack=1`.
- Socketben megőrzendő külön stone instance-state esetén a rövid fenti socketmodell helyett escrowba helyezett `embedded_instance_id` kapcsolat kell, ciklustiltással és egyetlen locationnel. Migráció előtt audit dönt erről; értékhordozó socket nem alakítható veszteségesen puszta ItemKey-vé.
- A runtime DB-felhasználó a katalógust csak olvassa; a CatalogBuilder/release publisher külön jogosultság. Játékszerver nem oszthat `local_id`-t és nem módosíthat prototípust.
- Katalógusimportnál parent→child sorrend, aktív FK-k és utólagos teljességvizsgálat. FK kikapcsolása utáni visszakapcsolás nem helyettesít adatellenőrzést. [MySQL FK-ellenőrzés](https://dev.mysql.com/doc/refman/8.4/en/create-table-foreign-keys.html)
- DDL-t külön migrációs lépésben kezelj. Egy `START TRANSACTION` nem teszi automatikusan visszagörgethetővé a MySQL DDL-sorozatot. [MySQL implicit commit](https://dev.mysql.com/doc/refman/8.4/en/implicit-commit.html)

### 4.3. Lokális ID és fejlesztési profil seed-példa

```sql
INSERT INTO protos.item_category(category_id, code) VALUES
  (1, 'weapon'), (2, 'armor'), (5, 'consumable'), (6, 'material');
INSERT INTO protos.refinement_profile(profile_id, code, max_level) VALUES
  (1, 'none', 0), (2, 'weapon_linear_10_4_v1', 9);
INSERT INTO protos.refinement_level(profile_id, level) VALUES (1, 0), (2, 0);
INSERT INTO protos.refinement_level
  (profile_id, level, physical_attack_gain_raw, magical_attack_gain_raw)
VALUES
  (2,1,10000,4000),(2,2,20000,8000),(2,3,30000,12000),
  (2,4,40000,16000),(2,5,50000,20000),(2,6,60000,24000),
  (2,7,70000,28000),(2,8,80000,32000),(2,9,90000,36000);

-- Értelmezési példa, nem balance seed: weapon:1 és armor:1 legális pár.
-- A tényleges base insert előtt a szükséges policy/profil rekordokat is létre kell hozni.
SELECT category_id, MIN(local_id) AS first_id, COUNT(*) AS prototype_count
FROM protos.item_base GROUP BY category_id ORDER BY category_id;
```

Az első kiadás minden nem üres kategóriájának első kiosztott local_id-je 1. Később a retired/törölt elem miatt a live `MIN(local_id)` lehet nagyobb; ilyenkor az ID-allokációs napló igazolja a kezdést. A helyi számozás nem jelent folyamatos, újratömörített listát.

## 5. Egy prototípus, instance refinement és profilok

### 5.1. Kötelező számítás

Egy fegyvertípus egyetlen `item_weapon` sora tartalmazza a 0. szint fix alapját. A base a refinement profile-ra mutat, az instance csak a szintet tárolja.

```text
WeaponPhysicalRaw(key,L) = weapon.physical_attack_base_raw + profile[L].physical_attack_gain_raw
WeaponMagicalRaw(key,L)  = weapon.magical_attack_base_raw  + profile[L].magical_attack_gain_raw
ArmorPhysicalRaw(key,L)  = armor.physical_defense_base_raw + profile[L].physical_defense_gain_raw
ArmorMagicalRaw(key,L)   = armor.magical_defense_base_raw  + profile[L].magical_defense_gain_raw
```

Lineáris profil szerkesztési alakja lehet `gain(L)=L*increment`, de a CatalogBuilder ezt validált, kumulatív level rekordokká materializálja. Runtime egyetlen reprezentációt olvas; nem keveri az „előző szinthez képesti delta” és „0-hoz képesti gain” szemantikát. Nemlineáris profil ugyanezt a táblát használja. A profil level sorai **nem tárgyprototípusok**, nincs saját ItemKey-jük.

Példa: alap fizikai 120, alap mágikus 30; szintenként +10/+4. +0: 120/30; +3: 150/42; +9: 210/66. Mind ugyanaz az ItemKey. A névhez a kliens az instance-szintet formázza, az ikon/glow/render effect az appearance profile `level→appearance` szabályából jön.

### 5.2. Refinement tranzakció

1. Kliens: `RefineIntent(instance_id, expected_state_revision, request_sequence)`; nem küldhet új kulcsot, szintet, árat vagy sikerjelzőt.
2. Owner thread: ownership, session, zárolás, hely, NPC/recipe, jelenlegi szint, profilkövetkező szint, pénz/anyag és stackfeltételek ellenőrzése.
3. Gazdasági writer/fence lefoglalja a teljes write setet. A DB-tranzakcióban az item, pénz, anyag és operation receipt együtt változik. A sikerhez minden elvárt revision/affected-row feltétel teljesül.
4. Tartós commit után az owner frissíti az ECS-t vagy snapshotból helyreállítja a commitált állapotot, növeli a stat revisiont és újraszámoltatja az érintett statokat. Commit közben az item nem mozgatható másik művelettel.
5. `ItemRefined` esemény: ugyanaz az instance ID és ItemKey, régi/új refinement szint, operation ID, új revision, release ID.
6. Hiba/rollback esetén nincs részleges anyagvesztés vagy félkész felszerelés. `CommitUnknown` esetén előbb receipt-ellenőrzés, nem vak újrafuttatás.

A fejlesztés sikerességének gazdasági szabálya nem következik a determinisztikus sebzésből. Az auditban rögzítsd a kívánt refinement outcome policy-t; az induló referencia-fixture biztos sikerű. Ha a termék szándékosan sikerdobást használ, az külön gazdasági döntési napló és tranzakció, nem része a CombatKernelnek, és nem szivároghat át alapsebzés-RNG-ként.

### 5.3. Profilvalidálás

Teljes `0..max_level` lefedettség; level 0 minden gainje 0; nem fejleszthető profilnál csak 0; weapon/armor számára csak értelmes statgain; cap és monotonitás az adott profiltípus szerint; nincsenek lyukak, túlcsorduló összegek vagy duplikált step-ek. A példa csak nemnegatív növekményt enged; ettől eltérő debuffos fejlesztés új, explicit ruleset-döntés és teszt nélkül tiltott.

Valódi tárgyátalakítás (például más tárgytípusra crafting) új ItemKey-t adhat explicit recepttel és instance-életciklusdöntéssel. Ez külön domainművelet; nem rejtett refinement-lánc és nem `refined_vnum` új neve.

## 6. Fixpontos matematika és determinisztikusság

### 6.1. Kötelező mértékegységek

| Mennyiség | Tárolás | Jelentés |
|---|---|---|
| HP, shield, sebzés, támadás, védelem, flat stat | `int64_t`, `*_raw` | 1000 raw = 1 megjelenített pont |
| Százalék, esély, skill coefficient, multiplier | `int32_t`, `*_bp` | 10 000 bp = 100%; 1 bp = 0,01 százalékpont |
| Tick, cooldown, duration | explicit integer tick / UTC ms | Nem float másodperc, nem kliensóra |
| Pozíció és harci távolság | explicit fixed world unit | Az auditban rögzített skála; összehasonlítás négyzetes távolsággal |
| Szint, stack, darabszám | integer | Nem szorzó és nem raw stat |

Ezek a skálák a specifikáció normatív új formátumai. Jelenlegi integer HP átvételekor `old_hp*1000` ellenőrzött offline/runtime boundary konverzió kell; tilos egy új raw értéket régi HP-pontként értelmezni. Az új runtime perzisztencia és wire mezők egységét a sémaverzió rögzíti. Nincs unit-heurisztika.

**Kerekítés:** minden dokumentált `R(a*b/d)` művelet a legközelebbi egészre kerekít; pontos fél esetén nullától távolodva. Nemnegatív mennyiségnél ez half-up. Minden pipeline-szakasz külön kerekít, nem vonhatók össze a szorzók más sorrendben. Például `R(R(x*a/BP)*b/BP)` szerződését nem helyettesítheti `R(x*a*b/BP²)`.

**Ábrázolási korlát:** minden stat/damage raw eredmény abszolút értéke legfeljebb `1'000'000'000'000'000` a referencia-rulesetben; normál amount nemnegatív. A tartalom ettől jóval kisebb balance-capot is előírhat. Szorzás széles, ellenőrzött integer köztes típusban történik; natív signed overflow, unsigned wrap és float konverzió tiltott.

Ha érvényes tartalom mellett matematikai/domainhiba történik: a hit eredménye hibásként elutasítandó, nincs HP-írás, legyen korlátozott rátájú diagnosztika és riasztás. Hibás katalógus esetén a szerver nem indul. Csendes numerikus szaturáció nem mentheti el a hibát. Ettől különbözik a szabályos, névvel leírt gameplay clamp, például a nemnegatív sebzés vagy a resistance cap.

### 6.2. Konkrét C++23 matematikai példa

A hordozható referencia Boost.Multiprecision ellenőrzött széles integer típust használ; `__int128` nem C++23 standard típus, ezért nem feltételezhető minden támogatott fordítón. Az alkalmazott Boost-verziót lockolni és ellenőrizni kell. A checked típusok használata a Boost dokumentált lehetősége; az alábbi aritmetikai sorrend saját Auriga-szerződés. [Boost integer típusok](https://www.boost.org/doc/libs/latest/libs/multiprecision/doc/html/boost_multiprecision/tut/ints/cpp_int.html)

```cpp
#include <boost/multiprecision/cpp_int.hpp>
#include <cstdint>
#include <expected>

namespace auriga::combat {
using Wide = boost::multiprecision::checked_int128_t;
inline constexpr std::int64_t BP = 10'000;
inline constexpr std::int64_t MaxRaw = 1'000'000'000'000'000LL;
enum class MathError { OperandOutOfRange, BadDivisor, ResultOutOfRange };
using MathResult = std::expected<std::int64_t, MathError>;

// Minden operandus ellenőrzése a szorzás ELŐTT történik.
// |a*b| <= 10^30, így a köztes érték az alkalmazott Wide tartományában marad.
inline MathResult mul_div_round(std::int64_t a, std::int64_t b, std::int64_t d) {
    if (a < -MaxRaw || a > MaxRaw || b < -MaxRaw || b > MaxRaw)
        return std::unexpected(MathError::OperandOutOfRange);
    if (d <= 0 || d > MaxRaw) return std::unexpected(MathError::BadDivisor);
    Wide product = Wide(a) * Wide(b);
    const bool negative = product < 0;
    if (negative) product = -product;
    Wide result = (product + Wide(d / 2)) / Wide(d);
    if (negative) result = -result;
    if (result < -Wide(MaxRaw) || result > Wide(MaxRaw))
        return std::unexpected(MathError::ResultOutOfRange);
    return result.convert_to<std::int64_t>();
}

inline MathResult add_raw(std::int64_t a, std::int64_t b) {
    if (a < -MaxRaw || a > MaxRaw || b < -MaxRaw || b > MaxRaw)
        return std::unexpected(MathError::OperandOutOfRange);
    const Wide result = Wide(a) + Wide(b);
    if (result < -Wide(MaxRaw) || result > Wide(MaxRaw))
        return std::unexpected(MathError::ResultOutOfRange);
    return result.convert_to<std::int64_t>();
}

// Szorzó, nem bonusdelta: 12500 → 1,25x. A 25% bónusz delta 2500 bp.
inline MathResult scale_amount(std::int64_t amount, std::int32_t factor_bp) {
    if (amount < 0 || factor_bp < 0 || factor_bp > 100'000)
        return std::unexpected(MathError::OperandOutOfRange);
    return mul_div_round(amount, factor_bp, BP);
}
} // namespace auriga::combat
```

Az integer-only gyorsított backend később használhat fordítóspecifikus széles szorzást, ha a referencia és a cross-platform golden tesztek bitazonosságot igazolnak. Ez ugyanannak az új képletnek matematikai backendje, nem második harcmotor. A C++-minták interfész-/referenciapéldák; a végrehajtáskor saját buildcélban fordítandók, nem a jelen dokumentumkészítés során igazolt production kódok.

### 6.3. Egyéb determinisztikussági szabályok

- A damage hot path nem használ `float`, `double`, `std::pow`, platformfüggő trigonometrikus eredményt, nem rögzített hash-iterációs sorrendet vagy globális RNG-t.
- A skill görbék integer táblák vagy korlátozott, típusos fixed-point expression bytecode. A régi szabad formula/string evaluator nem hívható az új motorból.
- A célpontlista stabil entity/spawn ID szerint rendezett; a stack, aura, proc és bonus források rendezése explicit.
- Kliensidő helyett szervertick. Időzített buff, projectile, DoT és cooldown lejáratának ugyanazon tickre esése esetén rögzített fázissorrend dönt.
- Mozgás/geometria igazolása is meghatározza, keletkezik-e hit. Amíg ez nem integer/fixed-point és stabil, csak a már validált hit numerikus kimenete tekinthető teljesen determinisztikusnak. Az end-to-end elfogadás része a range/LOS/target selection auditja is.
- Katalógus, ruleset, stat snapshot, eseményazonosító és rögzített crit/proc döntés nélkül a damage log nem teljes replay-bemenet.

## 7. Statépítés és bónuszok

### 7.1. Egyetlen stat-igazságforrás

Az ECS karakteren legyen külön base stat, equipment/affix contribution, buff contribution és az ezekből épített immutable `CombatStatsSnapshot`. A derived stat nem külön kézzel növelgetett perzisztens truth. Betöltés/equip/unequip/refine/socket/buff/level változás után a tulajdonos növeli a `stat_revision`-t; a StatBuilder rendezett forráslistából számol új snapshotot.

Az auditált `PointSystem::Compute/ComputeBattlePoints/ApplyPoint` útvonalak jelenlegi szemantikáját le kell képezni. Nem maradhat ugyanarra a mezőre egyszer régi point módosítás és egyszer új contribution összegzés. Az üres/placeholder jellegű `StatSystem.hpp` megléte nem bizonyít kész statmotort.

Referencia statképlet:

```text
PhysicalCoreRaw = class_level_profile.physical_base_raw
                + R(STR_raw * str_to_physical_bp / BP)
                + R(DEX_raw * dex_to_physical_bp / BP)
                + equipped_weapon_physical_raw + allowed_ammo_flat_raw
                + Σ physical_attack_flat_raw
PhysicalAttackRaw = R(max(0, PhysicalCoreRaw) * (BP + Σ physical_attack_bonus_bp) / BP)

MagicalCoreRaw = class_level_profile.magical_base_raw
               + R(INT_raw * int_to_magical_bp / BP)
               + equipped_weapon_magical_raw + Σ magical_attack_flat_raw
MagicalAttackRaw = R(max(0, MagicalCoreRaw) * (BP + Σ magical_attack_bonus_bp) / BP)

PhysicalDefenseRaw = max(0, class_defense_raw + Σ armor_physical_raw + Σ defense_flat_raw)
MagicalDefenseRaw  = max(0, class_magic_defense_raw + Σ armor_magical_raw + Σ magic_defense_flat_raw)
```

Az osztályonkénti coefficient és a Defense %-os bónuszok, ha vannak, verziózott profilban rögzítendők. Nem lehet implicit „régen a kódban még +level*2 volt” hozzáadás. Kétkezes/kétfegyveres/íj/ammo/mount/pet/polymorph kivétel explicit attack source policy. A fegyver alapja egyszer kerül be; a későbbi normal/skill bónusz nem ugyanannak a statnak újraalkalmazása.

### 7.2. BonusRegistry szerződés

Minden bonus definíciója tartalmazza: `BonusId`, név, egység (`raw`/`bp`/integer), célstat vagy pipeline-stage, filter (damage channel, normal/skill, target family/rank, PvP/PvE), stacking policy, cap policy, forrástípus és kliensmegjelenítési kulcs. A DB-s `value_scaled` értelmezését mindig a bonus definíció adja; egység nélküli „int bonus” tiltott.

| Bucket | Összevonás | Alkalmazás |
|---|---|---|
| Attack/defense flat | rendezett integer összeg | StatBuilder, %-os statbónusz előtt |
| Attack stat % | ugyanazon staton additív bp | Egyszer a StatBuilderben |
| Normal / skill outgoing | az aktuális hithez pontosan egyik típus | Damage pipeline offense szakasz |
| Target family/rank | egyedi forrás-azonosító szerint deduplikált matching összegek | Family és rank két külön szakasz |
| PvP/PvE outgoing/incoming | egyetlen, szerver által számolt mode | Rögzített szakasz; nincs kettős PvE+PvP |
| Flat/percent penetration | csatornánként; százalék cap 0..10000 bp | Védelemre, nem extra sebzésként |
| Physical/magic/element resist | névvel rögzített, csatornához illő bp | Védelem után |
| Crit chance / crit resistance | különbség, clamp 0..10000 | Kizárólag CritDecision |
| Crit multiplier | ruleset szerint összegzett extra bp | Crit esetén egyszer a bundle csatornáira |
| Block / evasion | determinisztikus csökkentő/faktor | Nincs rejtett proc-dobás |
| Proc chance/effect | explicit ProcId, trigger, ICD, budget | Külön kivételként regisztrált döntés |

**Stack policy** minden buffnál: `Add`, `Max`, `ReplaceByPriority`, `UniqueSource` vagy előre rögzített stack limit. Holtverseny: előbb explicit priority, aztán stabil `SourceId`; nem pointercím, nem beérkezési verseny.

**Javasolt referencia capek, nem hitelesített régi balance:** offense factor 0..100000 bp; channel/element resistance összeg -5000..9000 bp; incoming/block reduction 0..9000 bp; accuracy factor 5000..15000 bp; penetration 0..10000 bp; crit multiplier 10000..50000 bp. A loader a definíciókat és egyedi rekordokat validálja. Az összevont statoknál kizárólag az adott cap policy szerinti clamp legális; váratlan túlcsordulás nem clamp.

Minden régi apply/POINT/immune flag mapping kapjon táblázatsort: régi név és mértékegység → új BonusId vagy typed capability → források → teszt. Ismeretlen bonus nem 0-ra csendesítendő. A nem támogatott protó/skill nem kerül kiadható katalógusba.

## 8. Teljes damage pipeline — normatív új sorrend

### 8.1. Input és output

`AttackIntent` kliensoldali kérés. A szerver ebből validálás után `CombatAction`-t készít: ActionId, támadó és célpont EntityRef, skill/attack profile, hit index, szervertick, server sequence, catalog/ruleset identitás, stat snapshot revisionök. A skill/proc/DoT/reflect/mob/normal útvonal mind ugyanennek az új pipeline-nak típusos inputja.

`DamageBundle`: maximum a rulesetben engedélyezett számú csatorna, minden csatorna `Physical`, `Magical` vagy `True`, opcionális element taggel. `True` csatornát kliens nem kérhet; trusted content capability kell hozzá. Hybrid skill nem mossa össze a fizikait és mágikust a védelem levonása előtt.

`DamageResult`: stage trace/debug hash, csatornánként pre-mitigation/post-mitigation raw, crit/proc döntések, shield absorption, mana absorption, tényleges HP-vesztés, overkill, result reason, végső state revision és eseményazonosítók. A kliens ebből csak a szükséges projekciót kapja.

### 8.2. Stage-sorrend

1. **Admission és jogosultság.** Létező/generációhelyes entity, session-ownership, életállapot, map/dungeon instance, PvP engedély, target policy, cooldown, range/LOS, felszerelés, lőszer, erőforrás, skill-szint, rate limit. Érvénytelen intentből nem lesz sebzés, költség vagy proc.
2. **Action commit / költségfoglalás.** Szerver ActionId és stabil target/hit sorrend. Egy cast költsége egyszer terhelhető, nem célpontonként vagy retry-nként. A már elfogadott cast halál miatti megszakításának költség-visszatérítését a skill policy rögzíti; a referencia nem téríti automatikusan vissza.
3. **Snapshot.** Tickhatáron rögzített attack/defense stats, skillrank, buffok, target tagek, mode, map/dungeon módosítók, katalógus és ruleset. A mana/shield/HP elhasználható pool végső értékét a rendezett commit kezeli.
4. **Fix alap csatornánként.** `B_c = max(0, R(Attack_c * skill_coefficient_c_bp / BP) + skill_flat_c_raw + allowed_outgoing_flat_c_raw)`. Normal attack coefficientje 10000, flatje 0; attack profile adhat explicit unarmed/ammo szabályt. Igazsebzés alapja explicit fixed effect vagy engedélyezett formula, nem tetszőleges attacker attack.
5. **Offense multiplierek, ebben a sorrendben**, külön R-rel: normal VAGY skill; target family; target rank (például boss/stone); mode outgoing; level-difference factor; accuracy/evasion factor; dungeon/environment outgoing. Minden factor a validált snapshotból jön. Nincs rejtett attack rating float vagy damage roll.
6. **Crit döntés.** Célpont/hit bundle szinten egy döntés; az aktivált crit multiplier minden critképes csatornára egyszer alkalmazódik. `chance=clamp(attacker_crit_bp-target_crit_resist_bp,0,10000)`. Crit-tiltott DoT/proc nem dob. A kritikus ebben az új rulesetben **védelem előtti** szorzó; ezt nem szabad implementáció közben védelem utánra áttenni.
7. **Penetration döntések.** Alapesetben determinisztikus flat+% armor penetration. Ha külön engedélyezett pierce proc van, az névvel regisztrált proc-kivételként módosítja a penetration inputot. A régi védelemhozzáadó „penetrate damage” formula nem marad meg.
8. **Védelem csatornánként.** `D_eff = R(max(0, Defense_c - FlatPen_c) * (BP-Pen_c_bp) / BP)`. `A_c=max(0, B_after_crit_c-D_eff)`. A `True` csatorna ezt kihagyja.
9. **Resistek.** Előbb csatorna+matching weapon resistance egy definiált bucketben; aztán külön element resistance. `R(A_c*(BP-resist_bp)/BP)`, a cap policy szerint. `True` mindkettőt kihagyja. Element nélküli csatornára nincs element szakasz.
10. **Incoming multiplierek**, ebben a sorrendben: normal VAGY skill reduction; mode incoming reduction; dungeon/environment mitigation; deterministic block reduction. Minden stage külön R. A `True` megkapja a normal/skill, mode és dungeon szabályt, de a blockot kihagyja. A „true” ezért nem jelent minden játékszabályt megkerülő adminsebzést.
11. **Bundle cap és állapotszűrők.** Teljes invulnerability/untargetable és duel nonlethal szabály explicit. Alapsebzés minimuma 0; nincs véletlen 1..5 vagy titkos minimum 1. A maximum hit cap profil és HP-floor, ha létezik, névvel szerepeljen a rulesetben. A cap miatti csatornacsökkentés arányos: floor kvóták, maradék a legnagyobb törtrész szerint, holtversenyben Physical→Magical→True.
12. **Shield, majd mana shield, majd HP** a végső owner-thread commitban, stabil sorrendben. A shield a már mitigált sebzést nyeli el; shield order: priority, majd shield instance ID. Mana shield explicit `mana_cost_bp` szerint, integer math és rendelkezésre álló mana alapján. Nincs dupla HP-védelem.
13. **HP commit és ledger.** `actual_hp_loss=min(remaining_damage,max(0,current_hp-hp_floor))`. Lethal módban `overkill=remaining_damage-actual_hp_loss`; nonlethal HP-floor esetén ez a többlet külön `prevented_by_hp_floor_raw`, nem killre jogosító overkill. Shield/mana absorption, actual damage és megakadályozott többlet külön mező. HP nem megy 0 alá. Csak a tényleges pozitív HP-vesztés számít life steal/exp-damage credit alapnak, ha a profil másként kifejezetten nem rendelkezik.
14. **Halálátmenet.** Egy élő→halott átmenethez egy `DeathId`; despawn/resurrection új life generation. Reward/killer/assist/aggro policy a commitált ledgerből számol. Nem a legkésőbb beérkező worker nyer.
15. **Committed események, kliens packet és utóhatások.** `DamageApplied`, szükség esetén `EntityDied`, majd a regisztrált proc, life steal, reflect és secondary effect parancsok. Ezek a következő determinisztikus microstep/tick inputjai, nem rekurzív HP-hívások.

**Mana shield pontosítása:** a mana is 1000 raw/pont skálájú. A profil `mana_cost_bp` értéke 1..100000, 10000 jelent 1 mana raw / 1 damage raw költséget. `absorb_limit=floor(available_mana_raw*BP/mana_cost_bp)`, `absorbed=min(remaining_damage,absorb_limit,profile_remaining_capacity_raw)`, `spent_mana=ceil(absorbed*mana_cost_bp/BP)`. Nulla absorption költsége nulla. Ez az explicit floor/ceil pár kivétel a normál R-kerekítés alól, és garantálja, hogy nem használunk több manát a rendelkezésre állónál. Minden köztes szorzat ellenőrzött széles integerben fut.

**Összegzés és pool-elszámolás:** a csatornaösszeg széles integerben képződik; a végső hit cap legfeljebb MaxRaw. Cap nélküli MaxRaw-túllépés domainhiba, nem wrap. A cap és minden shield/mana absorption csatornafelosztása a 11. stage rögzített kvóta/maradék módszerével történik. A végső összeg invariánsa: `capped_damage = shield_absorbed + mana_absorbed + actual_hp_loss + overkill + prevented_by_hp_floor`. Utóbbi kettő közül az adott hitben legfeljebb az egyik pozitív. OnHit egy elfogadott kontaktusra értelmezett, akkor is, ha a védelem 0-ra csökkent; érvénytelen vagy invulnerable célpontra nem triggerel. OnActualHpDamage csak pozitív tényleges HP-vesztéskor, OnKill kizárólag a death commitnál fut.

### 8.3. Rövid C++23 csatorna-referencia

Az alábbi példa a **numerikus** csatornautat mutatja a már validált snapshoton. Admission, crit/proc kiválasztás, cap, shield és HP commit külön rendszer; ezek nem hiányzó implicit lépések. A `std::span` pontosan a 8.2. fejezetben rögzített sorrendű, már összesített faktorokat kapja.

```cpp
#include <algorithm>
#include <array>
#include <span>

namespace auriga::combat {
enum class Channel { Physical, Magical, True };
struct ChannelInput {
    Channel channel;
    std::int64_t attack_raw, skill_flat_raw, outgoing_flat_raw;
    std::int32_t skill_coefficient_bp;
    std::int64_t defense_raw, flat_penetration_raw;
    std::int32_t penetration_bp, resistance_bp, element_resistance_bp;
    std::int32_t critical_multiplier_bp; // 10000, ha nem kritikus
    std::int32_t block_reduction_bp;
    bool has_element;
};

inline MathResult resolve_channel(const ChannelInput& in,
    std::span<const std::int32_t, 7> offense_factors,
    std::span<const std::int32_t, 3> incoming_factors) {
    // A domainvalidátor a többi capet/filtert is ellenőrzi, nem csak e rövid függvény.
    if (in.defense_raw < 0 || in.defense_raw > MaxRaw ||
        in.flat_penetration_raw < 0 || in.flat_penetration_raw > MaxRaw ||
        in.penetration_bp < 0 || in.penetration_bp > BP)
        return std::unexpected(MathError::OperandOutOfRange);
    auto value = scale_amount(in.attack_raw, in.skill_coefficient_bp);
    if (!value) return value;
    value = add_raw(*value, in.skill_flat_raw);
    if (!value) return value;
    value = add_raw(*value, in.outgoing_flat_raw);
    if (!value) return value;
    *value = std::max<std::int64_t>(0, *value);
    for (const auto factor : offense_factors) {
        value = scale_amount(*value, factor);
        if (!value) return value;
    }
    value = scale_amount(*value, in.critical_multiplier_bp);
    if (!value) return value;
    if (in.channel != Channel::True) {
        auto defense = scale_amount(std::max<std::int64_t>(0,
            in.defense_raw - in.flat_penetration_raw),
            static_cast<std::int32_t>(BP - in.penetration_bp));
        if (!defense) return defense;
        *value = std::max<std::int64_t>(0, *value - *defense);
        value = scale_amount(*value, static_cast<std::int32_t>(BP - in.resistance_bp));
        if (!value) return value;
        if (in.has_element) {
            value = scale_amount(*value,
                static_cast<std::int32_t>(BP - in.element_resistance_bp));
            if (!value) return value;
        }
    }
    for (const auto factor : incoming_factors) {
        value = scale_amount(*value, factor);
        if (!value) return value;
    }
    if (in.channel != Channel::True)
        value = scale_amount(*value, static_cast<std::int32_t>(BP - in.block_reduction_bp));
    return value;
}
} // namespace auriga::combat
```

Éles API-ban a szabad integer mezők helyett validált, erős `AmountRaw`, `MultiplierBp`, `ChanceBp`, `ResistanceBp` típusokat adj. A fenti példában a `BP-resistance` castot domainellenőrzésnek kell megelőznie; nem fogadhat tetszőleges hálózati integer inputot. A hot pathban nem a kliens által összeállított `ChannelInput` fut.

### 8.4. Számolási és golden példák

**Egyszerű normal PvE hit:** weapon alap 120, refinement +3 gain 30, egyéb class/stat hozzájárulás 50 → PhysicalAttack 200. Nincs attack stat %. Normal outgoing +20% → 240. Target family +10% → 264. Minden további offense faktor 1. Crit nincs. Defense 40, flat penetration 10, percent penetration 20% → effektív defense `(40-10)*0,8=24`. Armor után 240. Resistance 25% → 180. Mode incoming reduction 10% → **162**. Shield 12 → **150 HP-vesztés**. Minden adat a raw skálán fut.

**Ugyanez kritikus 2x esetén:** crit előtti 264 → 528; defense után 504; resistance után 378; mode után 340,2; shield 12 → **328,2 HP-vesztés**. Nem `162*2`, mert a crit helye védelem előtt rögzített.

**Mágikus skill:** MagicalAttack=100, coefficient=15000, skill flat=10, skill outgoing factor=11000 → 176. MagicalDefense=20, nincs penetration → 156. Magic resist=2000 → **124,8**, minden más 1. Fizikai defense ezt nem módosítja.

**Hybrid:** a két fenti csatorna külön mitigálódik; csak ezután összegezhető, cap/shield/HP commit előtt. Ugyanazt a védelmet nem lehet a már összevont összegből egyszer levonni.

**Kerekítés:** `R(1*5000/10000)=1 raw`, `R(-1*5000/10000)=-1 raw`; `R(1*4999/10000)=0`; `R(1*5001/10000)=1`. `1 raw` megjelenítéskor 0,001 pont, nem 1 egész HP.

## 9. Kritikus és külön proc — pontosan körülhatárolt véletlen

### 9.1. Ami nem maradhat véletlen

Alap fegyver/mob damage, magic damage, attack rating, skill coefficient, level correction, defense/resistance, alacsony damage fallback, block és dodge nem dobhat. A régi dodge/block mezők determinisztikus evasion/mitigation bónusszá migrálódnak explicit balance mappinggel. Ha a termék tényleges véletlen dodge/blockot akar, azt külön engedélyezett, névvel regisztrált proc-kivételként kell újraspecifikálni; nem maradhat észrevétlen régi ágként.

### 9.2. Reprodukálható, szerveroldali döntések

- A véletlen döntés a tiszta kernel számára már adat: `DecisionFacts{critical, proc outcomes}`. Így a normál numerikus kernel RNG nélkül tesztelhető.
- Élesben egy külön `DecisionService` állítja elő a döntést. Javasolt megoldás auditált kriptográfiai könyvtár HMAC-SHA-256 művelete szervertitokkal; saját crypto implementáció tiltott.
- Domain-separated kulcsanyag: seed epoch, realm, life/spawn generation, ActionId, target stable ID, hit index, ProcId/crit tag és draw index, kanonikus integer kódolással. Worker ID, falióra, pointer vagy futási ütemezés nem része.
- Az első 32 bitből unbiased `0..9999` mintavétel: csak a `floor(2^32/10000)*10000` alatti érték fogadható el, utána modulo 10000; elutasításkor következő domain-separated draw index. Nem `rng()%10000` torzítással.
- Aktiválás: `draw < chance_bp`. 0 sosem, 10000 mindig. Az RNG algoritmus, kódolás, seed epoch és döntésverzió pinelt.
- A szervertitok/seed nem mehet klienshez és normál logba. Biztonságos replayhez rögzített döntési tények elégségesek; a titkos epoch külön jogosultságú archívum.
- Retry ugyanarra az ActionId-ra azonos döntést ad. A kliens nem választhat szerver ActionId-t és nem kérhet újradobást packetismétléssel.

### 9.3. ProcRegistry

Minden proc: ProcId, trigger (`OnHit`, `OnActualHpDamage`, `OnKill`, `OnCrit`), feltételek, chance bp, effect profile, cooldown/ICD scope, stack policy, crit eligibility, target policy, recursion tag, damage mode, snapshot policy és verzió.

Referencia korlátok: legfeljebb 8 aktivált proc egy gyökérhitből, legfeljebb 2 secondary generáció, legfeljebb 32 secondary parancs egy root ActionId alatt. Ezek javasolt induló terhelési limitek; véglegesítés benchmarkkal. A tartalom nem hagyatkozhat arra, hogy a runtime csendben eldob extra procokat: a statikus konfiguráció validálása és a runtime budget-hit diagnosztikája kötelező.

Reflect alapértelmezés szerint nem critel, nem reflectel tovább és nem triggerel `OnHit` procot. Life steal a tényleges HP-vesztésből számol, overkill/shield absorption nem alapja; életben lévő célzott heal commandként a következő microstepben fut. Poison/burn/stun külön hatás; az alkalmazási proc dobhat, a már aktív DoT tickje fix képletű. DoT újraalkalmazásnál `refresh/stack/replace` szabály és snapshot/dynamic stat policy explicit. Semmi nem hívhatja magát szinkron rekurzívan végtelen damage-lánccá.

## 10. Skill, mob, PvP és PvE

### 10.1. Skill modell

`SkillDefinition`: SkillId, engedélyezett fegyver/capability, rank curve, cast/cooldown/resource policy, target selection, hit count/timing, csatornák és coefficient/flat értékek, damage tags, crit/proc permission, element, defense-ignore capability, snapshot policy.

Az új skill evaluator csak típusos bemeneteket és engedélyezett operátorokat fogad: integer konstans, validált statref, checked add/sub, `mul_div_round`, explicit clamp és lookup curve. Nincs tetszőleges Lua/SQL eval vagy float formula a damage számításban. Scriptek parancsot küldhetnek a skill/domain API-nak, közvetlen HP-írást vagy végső damage integer injektálást nem.

AoE célpontok stabil ID szerint rendezve, max target limit; egy cast resource költsége egyszer, egy hit/target saját idempotenciakulcs. Multi-hit total flat nem adódhat hozzá minden hithez véletlenül: definíció jelöli, hogy a flat per cast vagy per hit, és per cast esetén a CatalogBuilder determinisztikus hitelosztást készít. Projectile kilövéskor rögzített támadói snapshot, becsapódáskor a tick policy szerinti target defense; mindkettő verziója a replayben. Csatornázott skill megszakítása, halál és target invalidation explicit.

### 10.2. Mob/NPC

- `MobCombatProfile`: fix physical/magical attack raw, defense raw, channel/element resistance, attack profile, skill set, family/rank tags és level profile. Nincs damage min/max.
- Spawn level/difficulty modifier egyszer, checked fixed-point képlettel épít statot. Dungeon scaling nem épülhet be egyszer spawnkor és még egyszer névtelen outgoing szorzóként.
- Boss phase módosítás a következő stat epochban publikált buff/profile; az aktuális batch közben nem ír worker által olvasott snapshotot.
- Pet/summon támadó a tulajdonos alapján kap PvP/PvE és jutalom-credit kapcsolatot; a kliens nem adhat neki kedvezőbb mode-ot.
- Mob AI célpontválasztása és mozgási RNG külön AI-szabály lehet, de azonos validált hit inputra a damage fix. A teljes replayhez AI-döntések/szerverparancsok is rögzítendők.

### 10.3. CombatMode és prioritás

A mode szerveroldali függvény: `ResolveCombatMode(attacker ownership chain, target ownership chain, map/duel/arena/guild-war policy)`. Player-controlled pet/summon játékos elleni támadása PvP, ha a map policy engedi. NPC→PC, PC→hostile mob, mob→mob támogatása explicit. Environment/trap/DoT megőrzi az eredeti forrás módját vagy a környezeti policy-t; nem választ újra kedvezőbb módot önkényesen.

Prioritás: tilalom/invulnerability → instance/map szabály → duel/arena/guild-war explicit szabály → általános PvP/PvE preset. Egy hit egy mode-ot kap. Friendly fire, party immunity, level protection, safe zone és nonlethal duel HP-floor a jogosultság/commit része, nem 0 damage utólagos kozmetika.

Nincs játékos és mob számára két külön harcmotor. Az eltérő statforrások, coefficientek, tagek és mode-profilok ugyanabba a kernelbe mennek. A balance-fixture lefedi legalább: PC→mob, mob→PC, PC→PC duel/arena/world, pet→PC, pet→mob, boss→party, trap/DoT, hybrid skill és immunis célpont.

## 11. ECS ownership, életciklus és párhuzamos feldolgozás

### 11.1. Tulajdonosi szerződés

Az induló új implementáció **megtartja a jelenlegi főszálas `g_registry` tulajdont**. A párhuzamosság immutable snapshotokon végzett számítás, nem a registry párhuzamos használata. Későbbi registry-sharding külön ADR és migráció; nem előfeltétele az item/combat munkának.

| Adat | Egyetlen író | Olvasók / élettartam |
|---|---|---|
| Karakter/item ECS state és entity lifecycle | Game owner thread | Owner; worker csak másolat/snapshot |
| Derived stat cache és dirty revision | StatBuilder az owner threaden | Tick alatt immutable snapshot |
| CatalogSnapshot/RulesetSnapshot | Builder validál, owner publikál | `shared_ptr<const ...>` release-élettartammal |
| Combat job output | Egy worker saját előre kiosztott slotja | Barrier után owner |
| HP, shield, mana, ledger, halál | Rendezett CombatCommit az owneren | Következő snapshot/commit |
| SQL connection és prepared handles | A hozzájuk rendelt DB worker | Nem megosztott handle párhuzamos execute-ra |
| Tartós item/gazdasági állapot | Domain reservation + tranzakciós writer | ECS csak commit után aktualizál |
| Mission/Dungeon állapot | Saját rendezett domain owner / commitfázis | Eseményekből, közvetlen combat worker nélkül |

Az EnTT registry általánosan nem automatikusan thread-safe; a storage-ok és iterációk felhasználói szinkronizálást igényelnek. A storage előkészítése vagy `ENTT_USE_ATOMIC` önmagában nem teszi biztonságossá a globális registry tetszőleges párhuzamos írását. Az itt választott immutable snapshot-határ saját architekturális döntés. [EnTT hivatalos ECS/multithreading dokumentáció](https://github.com/skypjack/entt/wiki/Entity-Component-System)

### 11.2. Új komponensszerződések

```cpp
#include <array>
#include <memory>
#include <vector>
#include <entt/entt.hpp>

using ItemInstanceId = std::uint64_t;
struct ItemIdentityComponent { ItemInstanceId instance_id; ItemKey prototype; };
struct ItemRefinementComponent { std::uint16_t level; };
struct ItemStateRevision { std::uint64_t value; };
struct ItemOwnerComponent {
    std::uint16_t owner_kind;
    std::uint64_t durable_owner_id;
    entt::entity runtime_owner{entt::null}; // csak ugyanebben a registryben
};
struct ItemAffix { std::uint32_t bonus_id; std::int64_t value_scaled; };
struct ItemAffixesComponent { std::vector<ItemAffix> values; };
struct CatalogSnapshot; // immutable, nincs TItemTable kompatibilitási nézet
struct CombatSnapshot;  // önálló értékek, nincs ECS komponenspointer

struct CombatJob {
    std::uint64_t action_sequence;
    std::uint32_t hit_index;
    std::shared_ptr<const CatalogSnapshot> catalog;
    std::shared_ptr<const CombatSnapshot> snapshot;
    // EntityRef, SkillId, DecisionFacts és output index a végleges típusban.
};
```

Ez típusváz, nem az összes komponens teljes implementációja. Az instance count/location/bind/expiry/socket külön komponens; nincs proto pointer az instance-ban. Az immutable katalógust a job vagy tick context tartja életben; a registry nyers belső pointere nem kerül jobba. A hot pathban előre kiosztott compact/SOA stat tömb használható; shared ownership a batch szintjén elég, nem kötelező minden hitnél refcountot növelni.

### 11.3. Tickfázis és determinisztikus commit

1. Bejövő csomagok dekódolása és bounded intent queue; SQL completionök értékadatként owner queue-ba kerülnek.
2. Owner thread: lifecycle/movement, az adott tick elején lejárt buffok, gazdasági commitcompletionök, equip/refine és stat dirty feldolgozás. Lejáró buff **nem** él még a tick támadásainál.
3. Owner: intent admission, erőforrásfoglalás, target selection, immutable snapshot, ActionId és stabil job index kiosztás.
4. Workers: csak numerikus CombatKernel és előre rögzített DecisionFacts. Minden output slotnak egy írója van. Worker completion sorrendjének nincs játékszemantikai jelentése.
5. Barrier után owner rendez: `(simulation_tick, accepted_action_sequence, hit_index, target_stable_id, channel_order)`. A channel bundle HP-commitja egyben történik.
6. Owner: generáció/instance/mode érvényesség végső ellenőrzése; cap/shield/mana/HP és ledger; halál; committed event queue. Ezen a fázison belül buff/statváltozást nem alkalmazunk visszamenőleg egy későbbi job stat snapshotjára.
7. Secondary commandok determinisztikusan rendezve a következő microstep/tick bemenetei. Az ebből keletkező buff a következő stat epochban él. Instant immunitás külön, explicit commit-state gate lehet, de sorrendje tesztelendő.
8. Network projekció és tartós outbox/WAL flush policy; csak ezután új snapshot epoch.

Ha két támadás ugyanarra a célpontra halálos: a fenti sorrend szerinti első élő→halott átmenet kap kill creditet; a másik `TargetAlreadyDead`, nincs új DeathId vagy új kill reward. Ha a támadó korábban meghal a commitrendben, még nem elindított melee/cast parancsa megszakad. Már kilőtt projectile és aktív DoT a saját source snapshot/lifetime policy szerint futhat tovább; ezt a profile kifejezetten engedélyezi.

Egy célpont shield/HP állapotát nem lehet egymástól független worker-eredményekben előre levonni. A sorrendfüggő részt az owner végzi. Ha később több commit shard kell, egy entity egy tickben egy shard tulajdona; cross-shard hit parancs és epoch-határ nélkül közös írás tiltott.

### 11.4. Élettartam és backpressure

- Entity destroy/component add/remove kizárólag owner phase-ben. Job entity handle-t nem dereferál; commit a stabil ID + generáció alapján újra validál.
- DB callbackbe ne kerüljön `Item*`, `CHARACTER*`, komponensreferencia vagy `this` élettartam-feltételezés. OperationId, owner/session generation és saját tulajdonú completion token kell.
- Egy logout/disconnect nem semmisítheti meg az in-flight tartós gazdasági művelet eredményét. A receipt után a következő login hiteles snapshotot tölt.
- Minden queue mérete korlátos. Túlterheléskor admission rate limit / új intent visszautasítás / explicit tick-budget policy; nincs végtelen pending job, nincs csendes committed reward eldobás.
- Szerverleállás: admission stop → job barrier → owner commit → tartós írások/outbox drain → visszajelzett shutdown. Timeout esetén a tartós receipt/WAL alapján recovery; hamis „minden mentve” státusz tiltott.

## 12. Mission és Dungeon natív eseményszerződés

### 12.1. A domain esemény committed tényt jelent

A jelenlegi `EvEntityDamaged`/`EvEntityDied` payload nem elegendő az új tartós feldolgozáshoz. Az új belső szerződés például:

```cpp
struct EventId { std::array<std::uint8_t, 16> bytes; auto operator<=>(const EventId&) const = default; };
struct EntityRef {
    std::uint32_t realm_id;
    std::uint64_t stable_id;
    std::uint64_t life_generation;
    auto operator<=>(const EntityRef&) const = default;
};
struct DamageApplied {
    EventId event_id;
    EventId root_action_id;
    EntityRef attacker, target;
    std::uint64_t server_tick, accepted_sequence;
    std::uint64_t catalog_revision, ruleset_version;
    std::uint64_t dungeon_instance_id;
    std::int64_t hp_loss_raw, shield_absorbed_raw, mana_absorbed_raw, overkill_raw;
    std::int64_t prevented_by_hp_floor_raw;
    bool critical;
    // Mode, skill/proc ID, channel breakdown és source-owner credit a végleges típusban.
};
struct EntityDied {
    EventId event_id, death_id, caused_by_damage_event;
    EntityRef victim, killer;
    std::uint64_t dungeon_instance_id;
    std::uint64_t credit_character_id;
    // MobKey, family/rank, party contribution snapshot, death tick és release identitás.
};
```

Az ActionId a tartós eseménysémában teljes EventId/128 bites namespace-elt azonosító. A korábbi példák `action_sequence` mezője csak realm-sessionen belüli rendezési kulcs, nem helyettesíti a globális idempotenciaazonosítót.

További item-események: `ItemCreated`, `ItemCollected`, `ItemConsumed`, `ItemCrafted`, `ItemRefined`, `ItemEquipped`, `ItemTransferred`; instance ID, ItemKey, refinement, darabszám, operation ID, owner és új state revision kíséri őket. Mission számláló csak tényleges committed ownership/count növekményt fogadhat el; packet-ismétlés vagy failed pickup nem új gyűjtés.

### 12.2. Bekötés a meglévő Mission Engine-be

- A jelenlegi `MissionDefinition::EventKind` tartalmaz `MobKilled`, `ItemCollected`, `ItemCrafted`, `BossDefeated` eseményfajtákat. Új `DamageDealt`, `DamageReceived`, `ItemRefined` szükség esetén sémaverzióval bővítendő; nem állítjuk, hogy ezek már implementáltak.
- A CombatCommit/Dungeon domain új eseményeit a natív Mission ingestion API fogyasztja. Ez rendes domainintegráció, nem régi item/harcmotor adapter.
- `MobKilled` és `BossDefeated` ugyanahhoz a DeathId-hoz tartozhat, de külön typed esemény. Egy általános kill objective nem számolhatja meg mindkettőt. Dedup kulcsban event semantic kind + DeathId, objective-routing pedig explicit.
- Mission ItemKey filter lehet pontos kulcs, kategória, content tag, refinement tartomány vagy instance-szintű feltétel; nincs vnum range vagy `vnum%10`.
- A jelenlegi `RewardBundle.items/consumeItems` 32 bites kulcstípusa lecserélendő típusos `ItemRewardSpec` listára: `{ItemKey, refinement_level, count, bind policy, affix/socket preset}`. Azonos specifikációk kanonikusan aggregálhatók. A refinement nem fér elvesztés nélkül pusztán ItemKey szerinti darabszám-mapbe.
- A Mission package codec, hash, validator, persistence, checkpoint, resource plan, pending award, kliens projekció és offline kampánykonverter mind ugyanazt az új rewardsémát használja. Aktív régi reward/package byteformátumot a live runtime nem értelmez tovább; egyszeri offline konverzió kell.
- Jutalmak pénz/item/resource változása a meglévő prepared transaction/fence infrastruktúrához illeszkedik. Nem kap a harcmotor önálló jutalom-SQL-t.

### 12.3. Dungeon integráció

Dungeon instance ID és spawn/life generation kötelező a boss/kill credithez. Ugyanannak a MobKey-nek egy másik instance-ban bekövetkező halála nem teljesítheti ezt a dungeon-fázist. Kill credit, party assist és jogosult résztvevők listája commitált snapshot; később csatlakozó karakter nem kap visszamenőleges entitlementet.

A jelenlegi közvetlen `COrcsDungeon::OnMobKilled` és hasonló hívásokat egyszeri `EntityDied/BossDefeated` feldolgozásra kell átírni az érintett dungeon modulokban. Az egyéb dungeon szabályok átírása csak a szükséges integrációs felületig része ennek a munkának, de a régi kill útvonal párhuzamosan nem maradhat aktív. Phase transition, boss death, ajtónyitás és reward entitlement deduplikált domain command.

### 12.4. Tartósság és „pontosan egyszer”

Az eseménykézbesítés lehet at-least-once; **pontosan egyszeri üzleti hatást** inbox unique kulcs + állapot/jutalom tranzakció együtt ad. Memóriabeli `std::set` nem crashbiztos dedup.

```sql
CREATE TABLE player.domain_event_outbox (
    event_id BINARY(16) NOT NULL,
    partition_id BIGINT UNSIGNED NOT NULL,
    sequence_no BIGINT UNSIGNED NOT NULL,
    event_kind SMALLINT UNSIGNED NOT NULL,
    schema_version INT UNSIGNED NOT NULL,
    payload MEDIUMBLOB NOT NULL,
    published_at_utc_ms BIGINT NULL,
    PRIMARY KEY (event_id), UNIQUE KEY uq_event_order (partition_id, sequence_no)
) ENGINE=InnoDB;

CREATE TABLE mission.domain_event_inbox (
    consumer_code VARCHAR(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
    event_id BINARY(16) NOT NULL,
    payload_sha256 BINARY(32) NOT NULL,
    PRIMARY KEY (consumer_code, event_id)
) ENGINE=InnoDB;

CREATE TABLE player.combat_death_receipt (
    realm_id INT UNSIGNED NOT NULL,
    victim_stable_id BIGINT UNSIGNED NOT NULL,
    life_generation BIGINT UNSIGNED NOT NULL,
    death_id BINARY(16) NOT NULL,
    killer_credit_id BIGINT UNSIGNED NULL,
    PRIMARY KEY (realm_id, victim_stable_id, life_generation),
    UNIQUE KEY uq_death_id (death_id)
) ENGINE=InnoDB;
```

A halálhoz kapcsolódó tartós receipt/outbox és a spawn/dungeon állapot olyan egyetlen tranzakcióhoz vagy replayelhető tartós journalhoz kötendő, amely crash után is ugyanazt a DeathId-t állítja vissza. Ha a mob HP csak memóriabeli, attól a kill reward még nem lehet tranzakció nélkül tartósan többször kiosztható. Recoverykor az adott life generation death tombstone-ját figyelembe kell venni; új respawn új life generationt kap.

Az eseménytáblák domainállapotok, nem prototípusok: a fenti forrás-outbox és death receipt a `player`, a Mission fogyasztói inbox a `mission` adatbázisé. A Mission saját kimenő eseményeihez a végleges séma `mission.domain_event_outbox` táblát is definiáljon; más fogyasztói domain a saját inboxát birtokolja. Ezek egyike sem kerül a `protos` adatbázisba. A domain tranzakcióban érintett táblákat teljes adatbázisnévvel kell megcímezni.

Mission/Dungeon fogyasztó: inbox insert → state/checkpoint módosítás → entitlement/reward outbox → közös commit. Duplikált eventnél azonos payload hash idempotens no-op; azonos EventId eltérő payload hash biztonsági/konzisztenciahiba. Kézbesítési ACK csak tartós commit után. Sorrendhiánynál bounded buffer/resync, nem csendes továbblépés.

Minden damage HP-íráshoz külön szinkron SQL tranzakció nem követelmény. A tartóssági osztályok különböznek: vizuális damage stream lehet nem tartós; gazdasági jutalom, mission progress és dungeon completion tartós; replay/WAL retention és crash esetén elfogadható HP-visszaállási pont ADR-ben rögzítendő. Az implementáció nem ígérhet nulla eseményvesztést addig, amíg a journal/receipt határt crash-teszt nem igazolja.

## 13. Prepared statement DB, cache és tranzakciós biztonság

### 13.1. Meglévő infrastruktúra használata

Az audit igazolta `SqlParam`, `SqlParams`, `SqlTransactionStep`, `SqlTransactionOutcome` és `CAsyncSQL` prepared API-k létezését. A következőket ezek köré kell illeszteni, a tényleges include/link elrendezés ellenőrzésével. Nem épül második SQL-réteg és nem kell kézi escape/SQL-string konkatenálás.

- Minden változó érték bind paraméter: ItemKey két oszlopa, instance ID, owner, refinement, darabszám, revision, event/operation ID, hash, szöveg és blob.
- Tábla/oszlop név nem paraméterezhető `?`-tel. Kategória lekérdezéséhez előre definiált SQL és zárt enum dispatch; user által adott táblanév tiltott.
- Prepared handle cache connection-local, bounded; reconnect esetén az új connection új handle-öket kap. Lejárt handle másik szálon nem használható.
- A numeric bind signedness/width pontosan egyezzen. Raw statra `SqlParam::Int`, azonosítóra `UInt`, hashre `Blob`; combat adat nem megy `Real(double)` típusba.
- Boot/catalog loadnál megengedett a korlátozott blocking olvasás; normál tickben nincs DB roundtrip. Runtime write aszinkron command/completion.
- Timeout, deadlock, lock wait, reconnect és `CommitUnknown` külön hibakód/állapot. Csak bizonyítottan nem commitált, idempotens művelet próbálható automatikusan újra bounded backoffal.

A paraméterezés és a prepared statement connection-scope tulajdonságai a DB dokumentált működésén alapulnak. [MySQL prepared statements](https://dev.mysql.com/doc/refman/8.4/en/sql-prepared-statements.html), [paramétermarkerek és session scope](https://dev.mysql.com/doc/refman/8.4/en/prepare.html)

### 13.2. Konkrét C++23 prepared példa

Ez egy **item revision update részlépés**, nem teljes refinement tranzakció. A végleges művelethez ugyanabban a `SqlTransaction`-ben kell hozzáadni az owner/location lockot, pénz-/anyagköltséget, receiptet és outboxot.

```cpp
#include <SQL/SqlParams.h>
#include <SQL/PreparedTransaction.h>

SqlTransactionStep make_refinement_step(std::uint64_t instance_id,
    std::uint64_t expected_revision, std::uint16_t old_level,
    std::uint16_t new_level) {
    SqlTransactionStep step;
    step.query =
        "UPDATE player.item_instance SET refinement_level=?, state_revision=state_revision+1 "
        "WHERE instance_id=? AND state_revision=? AND refinement_level=?";
    step.params = {
        SqlParam::UInt(new_level), SqlParam::UInt(instance_id),
        SqlParam::UInt(expected_revision), SqlParam::UInt(old_level)
    };
    step.expectedAffectedRows = 1;
    return step;
}

SqlTransactionStep make_owned_item_lock(std::uint64_t instance_id,
    std::uint16_t owner_kind, std::uint64_t owner_id) {
    SqlTransactionStep step;
    step.query =
        "SELECT instance_id FROM player.item_location "
        "WHERE instance_id=? AND owner_kind=? AND owner_id=? FOR UPDATE";
    step.params = {SqlParam::UInt(instance_id), SqlParam::UInt(owner_kind),
                   SqlParam::UInt(owner_id)};
    step.expectedResultRows = 1;
    return step;
}
```

Az old/new level értéket a domain a pinned refinement profile-ból készíti; nem a kliensből. `state_revision` maximumánál újabb írás tiltott/operációs hiba, nem uint wrap. `expectedAffectedRows=0` konfliktus, nem siker. Több item és wallet érintésekor lock sorrend: owner/wallet kulcs, majd instance ID növekvően; minden writer ugyanazt a sorrendet követi.

Tartós operation receipt elején unique insert, vagy meglévő receipt ellenőrzés. A request hash tartalmazza az owner, action, itemrevision, recept és release identitást. Duplikált OperationId eltérő kérésre hiba. A receipt és minden gazdasági változás közös commit; a receipt nem kerülhet előbb külön tranzakcióba. `CommitUnknown` után új connectionön receipt lookup: megtalált → eredmény visszaadása/reconcile; bizonyítottan hiányzó, lezárt tranzakció → szabályos retry; bizonytalan kapcsolatállapot → várakozó reconciliation.

### 13.3. Cache write ordering

A jelenlegi Database/player cache és GameServer közötti útvonalakat auditálni kell. Egy writer commitálja a végleges itemállapotot. Régi késleltetett flush nem írhatja felül újabb revisionnel rendelkező DB/ECS állapotot. Cache delta és snapshot is tartalmaz `state_revision`-t; kisebb revision elutasítandó, rés esetén resync. A flush sor megszüntetésekor drain és tartós checkpoint szükséges, nem memóriabeli queue törlés.

## 14. Szerverautoritatív kliensadat-szinkron, verzió és hash cache

### 14.1. Egy tartalmi igazságforrás

A CatalogBuilder az új szerveroldali validált katalógusból készíti a **ClientCatalogProjection**-t. A kliens nem önálló item_proto adatbázisból számol. A projekció tartalmazza a megjelenítéshez szükséges ItemKey-t, neveket/lokalizációs kulcsokat, kategóriát, ikont/appearance-t, látható fix alapokat és refinement görbéket, tooltip-bónuszokat és használati feltételeket. Szervertitok, rejtett drop esély, anti-abuse küszöb vagy RNG seed nem kerül bele.

Az asset elérési útvonal és az ItemKey külön fogalom. Régi, számozott assetfájlnév változatlanul használható explicit új appearance rekordban; a fájlnév nem runtime legacy azonosító-feloldás. A kliens nem vezethet le ikont `vnum-vnum%10` vagy levelből számazonosító-aritmetikával.

### 14.2. Manifest

```text
ContentManifest {
  protocol_schema_version,
  catalog_schema_version,
  release_id,
  ruleset_version,
  math_schema_version,
  amount_scale = 1000,
  rate_scale = 10000,
  catalog_sha256,             // teljes szerver-katalógus identitása
  ruleset_sha256,
  client_projection_sha256,   // a tényleges klienspayload byte-jai
  client_projection_size,
  chunk_count,
  compression_codec_version,
  minimum_client_build,
  manifest_signature/key_id  // kiadási/autentikációs terv szerint
}
```

A hash **integritás és cache-azonosító**, nem önmagában hitelesítés. A manifestet a már hitelesített szerverkapcsolatból vagy ellenőrzött release-aláírással kell elfogadni. A pontos könyvtár/crypto/transport az auditban rögzítendő; ne találj ki meglévő TLS/launcher-szolgáltatást ellenőrzés nélkül. Kliens cache állítása nem anti-cheat bizonyíték, a szerver nem bízza rá a számítást.

Kanonikus hash input: sémaverzió, fix endian integer mezők, elemszámok, stabil ItemKey/level/BonusId sorrend, UTF-8 NFC szöveg, meghatározott null/üres reprezentáció. Tiltott raw C++ struct memória, pointer, unordered iteráció, DB természetes sorrend vagy platformfüggő string encoding hash-elése. A szerver-katalógus és public projekció eltérő payload, ezért külön hash szükséges.

### 14.3. Kapcsolódási és cache folyamat

1. Kliens kapcsolatfelvételkor elküldi a támogatott protokollsémát/buildet és a lokális projection hash/release információját. Ez cache-optimalizálási állítás, nem autoritás.
2. Szerver meghatározza a realm/session kötelező manifestjét. Nem támogatott protokoll/build esetén érthető frissítési hiba; nincs régi packet fallback.
3. Egyező `(schema, projection hash)` esetén a kliens a hitelesített manifesthez tartozó cache-t használhatja. Nem egyezőnél teljes csomag vagy az adott schema új, ellenőrizhető deltaformátuma töltődik. A delta opcionális optimalizálás, első release-ben teljes bundle elég.
4. Letöltés bounded chunkokban, chunk index/size/hash, retry limit. Tömörítés előtt/után méretlimit és decompression-ratio limit. Temp fájl → teljes hash-ellenőrzés → atomikus rename; részleges cache soha nem aktív.
5. Kliens ACK a projekció hashével. A world/gameplay belépési gate csak a kötelező release-re sikeres szinkron után nyílik meg; hibás cache nem indokol régi katalógussal játszást.
6. Instance snapshot: `instance_id`, kétmezős ItemKey, `refinement_level`, count, bind/expiry, látható socket/affix state, `state_revision`, owner/container/slot és `catalog_revision`.
7. Inventory delta az instance revíziójához és container transaction sequence-hez kötött. Hiányzó/out-of-order delta esetén szerveres snapshot resync, nem kliens találgatás.
8. Harci packet: szerver Action/DamageEvent ID, tick, source/target network identity, actual HP damage/shield flags, crit/proc megjelenítési jelző és hiteles HP/revision delta. A kliens nem írhatja vissza „szerintem ennyi damage történt” értékként.

Cache kulcs: `(catalog_schema_version, client_projection_sha256)`; release manifest megmondja, melyik cache aktív. `release_id` monotónia és minimum build megakadályozza a nem engedélyezett downgrade-et. Authorizált operációs rollbacknél a szerver új deployment epoch/manifestben hirdeti a kiválasztott kompatibilis release-t; a kliens nem önállóan dönt a visszalépésről.

### 14.4. Katalógusváltás futó szerveren

Első migráció: koordinált leállás és új szerver+kliens release, nincs mixed legacy. Későbbi, schema-kompatibilis balance/catalog reload csak validált új immutable snapshot tickhatári publikálásával; in-flight action és projectile megőrzi a pinned verziót, aktív realm játékosai ugyanazon következő epochra váltanak. Snapshot erőforrása csak az utolsó hivatkozás után szabadul.

Kulcstörlés, refinement szűkítés, instance-séma vagy matematikai sorrend változtatás nem sima hot reload. Ehhez új release és karbantartás/migráció kell. A szerver nem tarthat két eltérő számítási rulesetet kliensenként tetszőlegesen; a pillanatnyi in-flight snapshot-élettartam nem jelent párhuzamos régi motort.

## 15. Biztonsági és visszaélés elleni követelmények

- Attack/refine/use/equip/trade intentet a session karakteréhez és ownership chainhez kell kötni. Objektum ID kitalálása nem jogosultság.
- Attack sequence, tick window és ActionId dedup védi a replayt; a kliens sequence wrap/újracsatlakozás új session generationnel értelmezett.
- Range/LOS, cooldown, cast time, attack speed, ammo és resource a szerver állapotából jön. A kliens animation completion nem engedély újabb hitre.
- Packet méret, elemszám, kategória, subtype, refinement, count és stringhossz dekódoláskor ellenőrzött. Ismeretlen enum, fenntartott bit és nem kanonikus kódolás hiba.
- Affix/socket/refine bonus csak szerver által generált és validált állapotból eredhet. Kliens item tooltip vagy cache módosítása nem hathat a statokra.
- GM/szkript/admin művelet is domain command és auditált jogosultság; nem kerülheti meg az inventory/economy/HP writer szabályát.
- Gazdasági művelet nem hagyatkozhat a „főszál biztosan nem versenyez” feltevésre DB/cache/egyéb process writer mellett: revision/fence/transaction kötelező.
- Prepared bind mellett a dinamikus SQL identifier, nyers filter, sort expression és script query továbbra is tiltott.
- Content/mission/skill csomag nem végrehajtható kód. Profile ID, expression bytecode, proc recursion és target count validált; nincs path traversal az asset/cache fájlnevekben.
- Diagnosztikai trace nem tartalmazhat session token, DB-jelszó, RNG titok vagy teljes személyes adatot. Méret- és rate limit kell a logokra is.
- A magas szintű cache/hash egyezés integritást szolgál; nem bizonyítja, hogy a kliens tisztességes. Minden pénz, damage és jutalom döntés szerveren marad.

## 16. Egyszeri offline migráció — nincs runtime átmenet

### 16.1. Önálló migrációs termék

`AurigaItemCombatMigrate` külön tool. Régi formátumot csak ez a tool olvas; output kizárólag új sémák, új katalógus, új Mission/Dungeon package és ellenőrzési jelentés. A tool nem a GameServer processben fut és nem épül annak linkgráfjába. A régi→új map a migráció audit-archívuma, nem runtime resource.

„Egyszeri” az éles adatátvételre vonatkozik: az offline konverter próbamentéseken többször futtatható determinisztikusan, de nincs folyamatos szinkron/CDC/dual write az új és régi modell között. Azonos input hash + mapping verzió + tool verzió ugyanazt az outputot adja. Részleges batch retry staging célon operation manifesttel idempotens; nem az élő régi DB mellett futó átmeneti szolgáltatás.

### 16.2. Kötelező input-manifest és explicit mapping

Manifest: source DB export hash, schema dump hash, asset/content export hash, repo/tool commit, mapping version, balance policy hash, charset, time/expiry egységek és teljes forrástáblalista. A forrás csak olvasható backup/snapshot.

Offline mapping rekord:

```json
{
  "source_legacy_item_id": 12340,
  "target": {"category_id": 1, "local_id": 1},
  "refinement_level": 0,
  "semantic_group": "weapon.iron_sword",
  "mapping_version": 1,
  "basis": "reviewed explicit mapping; illustrative identifier"
}
```

Az azonosítók példák, nem a repository valódi tárgylistája. A régi +1…+9 rekordjai ugyanarra a kulcsra, eltérő instance refinementre mutatnak. Automatikus jelöltcsoportosítás használhat nevet/refine chain-t, de a végleges csoportot felülvizsgált mapping dönti el. Nem lehet általános `old_id/10` vagy `old_id%10` képletből igazságot csinálni.

A csoporton belül minden eltérő mezőt vizsgálni kell: subtype, slot, limit, effect, affinity, socket, flag, ára, model, magic/physical érték és proc. Csak fejlesztési eltérés kerül profile gainbe; valódi más tárgytípus külön proto marad. Hiányzó +szint nem jogosít fel tartalom kitalálására; a célprofil teljes definícióját explicit policy adja.

### 16.3. Min/max és régi képletek átvétele

Az új fix értékek balance-adatok, nem a régi motor automatikus ekvivalens másolatai. Induló **javasolt** konverzió `floor((min+max)/2)` széles integerben, de kizárólag offline policyként és tételes ellenőrzéssel. A régi `*2`, attack grade, level és attack rating összefonódása miatt egy midpoint önmagában nem garantál azonos várható végső sebzést. Kritikus és defense-sorrend is eltérhet.

Minden weapon/mob/skill csoporthoz legyen előtte–utána statjelentés és balance-fixture. A régi harci RNG kimenetét nem „játsszuk vissza” új alapsebzésként. A régi engine adatai/korábbi mérési exportjai offline elemzésre használhatók; új runtime-ba nem csomagolható régi evaluator.

Eltérő régi refinement szintekből a kumulatív gain explicit számolandó és validálandó. Ha egy profil nem fejezi ki veszteség nélkül az összes szintkülönbséget, külön profil vagy tudatos új balance szükséges. Azonosíthatatlan adat karantén, nem silent default.

### 16.4. Konvertálandó referenciák — nem csak player.item

| Felület | Új reprezentáció |
|---|---|
| Inventory/equipment/safebox/mall | InstanceId + ItemKey + refinement + normalized state/location |
| Trade/escrow/mail | Ugyanez, explicit owner/container lifecycle |
| Pending item award/reward entitlement | ItemRewardSpec és OperationId, nem puszta vnum |
| NPC-shop/drop/loot/crafting/refine recipe | ItemKey + kívánt refinement/count + profile/filter |
| Mission/quest/dungeon definition és checkpoint | Új schemas, typed item filter/reward spec; symbolic content key resolution buildkor |
| Socket/stone/transmutation/mask/signature | Explicit ItemKey/appearance/behavior kapcsolat; jelentésenként külön mező |
| Pet/mount/soul/unique time effect | Típusos profil és megőrzött instance-state |
| Kliens tooltip/wiki/quickslot | ItemKey vagy instance ID a funkció szerint, új projection |
| GM command/config/tool/export/import | Új kulcs vagy content_code; régi azonosító-feloldás csak offline toolban |
| Kliens/server/DB packet/cache | Új schema és szélességek; régi struct ABI megszűnik |

A szkriptekben lévő számliterálok, vnum-intervallumok és számtani tárgyfelismerések AST-/szemantikai auditot igényelnek. Nem elég globális szövegcsere. Opaque blob, aktív pending művelet vagy ismeretlen custom rendszer konvertálhatatlansága cutover-blokkoló.

### 16.5. Migrációs lépések

1. A külön `protos` adatbázis, az új `protos` katalógusséma és az érintett `player`/`mission` állapotsémák, validátor és runtime build kész stagingben. Régi éles runtime még nem kap új adaptert.
2. Read-only forrássnapshot és integritásmanifest. Determinisztikus dry-run: nem ír éles táblát.
3. Tételes legacy ID→ItemKey/refinement mapping, profilépítés, bonus/skill/mob konverzió, kategória-allokáció.
4. Teljes referenciakonverzió külön üres staging DB-be. FK és semantic validation; karanténlista üres a kiadható teljes adathalmazra.
5. Katalógus/client projection build, hash és release bundle. Új server+client staging tesztek és reprezentatív replay/balance ellenőrzés.
6. Legalább egy teljes visszaállítási próba és cutover időmérés production méretű izolált másolaton.
7. Karbantartás: login és gazdasági writer stop, aktív műveletek drain/reconcile, minden régi process és background writer leállítása. Írásfagyasztási checkpoint igazolása.
8. Végső konzisztens backup/export, hash, offline konverzió, ellenőrzés. Korábbi staging snapshotot nem szabad friss forrásként feltüntetni.
9. A `protos` katalógus és az érintett `player`/`mission` állapotadatok összehangolt checkpointjával az új DB/katalógus/server/client release együtt aktiválása, régi DB runtime credentialek visszavonása, új writer kizárólagos működésének igazolása. Backup/restore során a katalógusverzió és a rá hivatkozó instance-/Mission-állapot nem választható szét.
10. Zárt smoke és gazdasági ellenőrzés → játékosok visszaengedése → meghatározott megfigyelési ablak. Hibánál a 19. fejezet szerinti leállítás/helyreállítás.

### 16.6. Kötelező adatmegőrzési bizonyítékok

- Minden forráspéldány pontosan egy célpéldányhoz vagy indokolt, jóváhagyott merge/split művelethez tartozik. Egy protócsalád összevonása **nem** egyesíti a játékosok tárgypéldányait.
- Összes count és owner/container szerinti count megőrzött; InstanceId-k megőrizhetők 64 bitre bővítve, új ID esetén teljes instance mapping szükséges.
- Gold/egyéb wallet egyenlegek, bind, attr, socket, expiry, durability, refine szint és pending reward megőrzött vagy tételesen dokumentált új balance/state policy szerint változott.
- Referencia-coverage 100%; nincs árva, 0 local_id, ismeretlen kategória, refinement túllépés, duplikált instance, ütköző inventory foglalás vagy hiányzó asset/profile.
- Kategóriánként és ownerfajtánként sor/darabszám, rendezett kanonikus rekordhash és pénzügyi összegzés készüljön. Véletlen mintavétel nem helyettesíti a teljes számlálást.
- Régi ID csak offline mappingben/audit archívumban maradhat; új runtime séma, csomag és betöltött tartalom nem tartalmazhat feloldáshoz szükséges legacy mapet.

## 17. Tesztstratégia és konkrét elfogadási esetek

### 17.1. Tesztelési elv

A feladat adatvesztést és gazdasági hibát is okozhat, ezért egység-, integrációs, migrációs, concurrency- és crash-tesztek szükségesek. A meglévő repository tesztstílusát/CMake-céljait használd; új frameworköt csak indokolt esetben vezess be. Teszt ne ugyanannak a függvénynek a másolatával számolja a várt eredményt. A numerikus oracle legyen külön egyszerű, tetszőleges pontosságú integer modell vagy felülvizsgált golden fixture.

Régi motorral való runtime összehasonlítás nem tesztarchitektúra. Régi statisztikai export offline balance-kontroll lehet; a normatív helyességet az új szabályok, független oracle és invariánsok adják. Az új kernel egyetlen szabályimplementáció marad.

### 17.2. Kötelező tesztmátrix

| Terület | Konkrét esetek | Pass-feltétel |
|---|---|---|
| ItemKey | weapon:1 és armor:1; 0; reserved bitek; max local; unknown category; endian roundtrip | Nincs ütközés vagy csendes csonkolás |
| Kategória-integritás | base child nélkül; rossz child; többes subtype; missing profile | Publikálás elutasítva, pontos diagnosztika |
| Adatbázis-felosztás | Katalógus a `protos`-ban, instance a `player`-ben; eltérő default DB; readonly runtime; cross-database FK | A minősített SQL mindig a kijelölt adatbázist éri el; runtime katalógusírás elutasítva; árva instance nem írható be |
| Refinement | 0/3/9; profillyuk; max+1; nem fejleszthető tárgy; kumulatív vs delta | Egy proto, helyes fixed gain, hibás állapot nem tölthető be |
| Matek | 0; ±fél; negatív buff; odd divisor; 64 bites szélsők; kerekítéssorrend | Oracle-rel egyező eredmény/hiba; nincs UB/wrap |
| StatBuilder | equip/unequip/refine/socket/buff/expiry/level; duplikált forrás | Egyetlen contribution, stabil snapshot hash/revision |
| Normal hit | 8.4. fejezet 162 és shield után 150 | Pontos raw eredmények |
| Crit | 0/10000 chance; 2x; crit resist; crit-tiltott effect | 340200 raw shield előtt, 328200 utána a példában |
| Magic/hybrid | 124800 raw magic példa; két külön defense; true channel | Helyes csatornák, nincs dupla/hiányzó védelem |
| Defense/penetration | flat>D; 0/100% penetration; negatív resist; cap | Nemnegatív amount, rögzített pipeline-sorrend |
| Block/evasion | azonos input 10000 ismétlés; nincs crit/proc | Bitazonos kimenet, nincs véletlen miss/block |
| Shield/mana/overkill | több shield, azonos priority, mana elfogy, HP-floor, lethal | Poolok nem negatívak; absorption+HP+overkill megmaradási invariáns |
| Skill | per-cast/per-hit flat, AoE target limit, cooldown, resource retry | Castköltség egyszer, stabil target/hit sorrend |
| Mob és mode | PC/mob/pet/environment összes engedélyezett irány; safezone/duel | Egy mode, helyes tiltás és HP-floor |
| Proc | 0/100%, ICD, retry, scheduling, reflect, secondary budget | Azonos EventId nem újradobás; nincs rekurzív ciklus |
| ECS lifecycle | destroy/logout/warp/másik registry; új entity ugyanazon indexen | Stale handle nem talál másik célpontot |
| Párhuzamosság | 1/2/4/8 worker, eltérő job sorrend, random scheduling delay | Azonos commit/event/world-state hash |
| Race | két lethal hit; két refine; refine+trade; expiry+equip | Egyetlen jogosult commit/DeathId, konzisztens owner |
| Prepared SQL | signed/unsigned 64bit; blob/hash; injection string; reconnect | Paraméter nem SQL; helyes típus és eredmény |
| Tranzakció | expected row mismatch; deadlock; timeout; rollback; CommitUnknown | Nincs félkész költség/item/reward, receipt szerinti reconcile |
| Mission | duplicate/out-of-order kill; boss+mob esemény; reward replay | Pontosan egyszeri progress és gazdasági hatás |
| Dungeon | másik instance; respawn generation; phase retry; disconnect | Nincs cross-instance credit vagy duplikált completion |
| Sync/cache | cache hit/miss; wrong schema/hash; chunk reorder; félbehagyott download | Csak teljes, hiteles projection aktív; bounded recovery |
| Kliens autoritás | hamis damage/crit/refine/ItemKey/owner; régi packet | Szerver állapota nem változik jogosulatlanul |
| Offline migráció | összes tárolófajta, hiányzó map, opaque blob, megismételt run | Determinisztikus output, 100% coverage, nincs silent loss |
| Runtime legacy tilalom | link/asset/config/protocol/source call gráf | Nincs betölthető régi proto/motor/feloldó út |

**Matek propertyk:** 0 támadás és 0 flat esetén a sebzés 0; azonos csatornán nagyobb defense nem növel damage-et; növekvő resist nem növel damage-et a capek között; nagyobb alaptámadás nem csökkenti a sebzést azonos egyéb állapot mellett; refinement monoton profilnál nem csökkenti az érintett alapstatot; 100% penetration mellett a defense értéke nem befolyásolja a találatot; stat/bónuszforrások bemeneti permutációja nem változtat eredményt.

A komplex stateful teljes harcra ne állíts hibás általános monotonitást: például nagyobb damage előbb megölheti a célt, így későbbi hitek elmaradnak. A property hatókörét pontosan a tiszta stage-re kell szabni.

### 17.3. Arany fixture-ek és replay

`tests/fixtures/item_combat/v1/` tartalmazza a katalógus részhalmazát, math schema/ruleset hashét, stat inputokat, DecisionFacts-t, action/event sorrendet és minden stage várt raw értékét. A fixture-változás balance/semantic diffként reviewzandó; teszthiba eltüntetésére vak „golden update” tiltott.

Kötelező minimum: normal melee, ranged ammo, magic, hybrid, true, unarmed, all-defense, resistance vulnerability, crit, pierce proc, DoT tick, reflect, life steal, shield+mana shield, duel HP-floor, mob boss phase, pet PvP, repeated event és stale instance. Mindegyik azonos eredményt ad a támogatott platform/fordító/release build mátrixon.

### 17.4. Fuzz, sanitizer és fault injection

- Fuzz: ItemKey/packet decoder, catalog/skill/Mission package, compressed cache chunk és offline mapping parser. Bounded memória és CPU, nincs OOB/UB/infinite loop.
- ASan/UBSan ahol támogatott; TSan támogatott platformon külön concurrency harness. Egy sanitizer hiánya adott Windows toolchainen nem írható le sikeres futásként.
- Fault injection: process stop az offline batch közepén, DB-kapcsolatvesztés COMMIT előtt/után, ACK-vesztés, outbox publish után consumer crash, Mission reward commit után crash, shutdown drain timeout.
- Helyreállítás után ugyanaz az operation/death nem ad második tárgyat/pénzt/progresszt, és a queue-k nem tűnnek el bizonyítatlanul.

### 17.5. Meglévő regressziós célok

Auditáltan léteznek a GameServer CMake-ben többek között `CombatStateTests`, `PointCalculationTests`, `ItemRuntimeTests`, `ItemManagerLifecycleTests`, `ItemAttributeTests`, `QuickslotTests`, `RefineWindowTests`, `ShopLifecycleTests`, `ExchangeTests`, `SafeboxLifecycleTests`, `DungeonLifecycleTests`, `MissionCoreTests`, `MissionPersistenceTests`, `MissionResourceTests`, `AsyncSqlPreparedTests` és izolált MariaDB tesztkapcsolatok. Pontos neveket és függőségeket a friss CMake-ből olvasd ki; minden érintett tesztet frissíts az új modellre. A régi modellel sikeres teszt nem igazolja az új viselkedést.

Izolált DB-tesztek kizárólag külön tesztadatbázison és nem production credentiallel. A végrehajtó a tesztjelentésben a tényleges parancsot, buildkonfigurációt, eredményt és kimaradt ellenőrzést rögzítse.

## 18. Benchmark, kapacitás és megfigyelhetőség

### 18.1. Benchmark-terv — mért eredményt nem állítunk

A dokumentumkészítés során benchmark nem futott. A végleges SLO-t az auditban kell workloadhoz és referenciagéphez kötni, **a performance-optimalizálás előtt**. A régi build mérése izolált, offline alapvonal lehet; nem indokol runtime kétmotoros feldolgozást.

Minden mérés manifestje: CPU/memória/OS, compiler/flags, dependency verziók, repo commit, build type, worker count, dataset hash, hit/channel/proc eloszlás, warmup, seed/DecisionFacts és ismétlések. Debug és sanitizer build nem production throughput-mérés.

| Benchmark | Bemenet | Kötelező mérőszám |
|---|---|---|
| Key lookup / catalog | Kicsi és teljes katalógus; random/stabil lookup | ns/lookup, cache miss, RSS, betöltési idő |
| StatBuilder | 1/12/32 felszerelési/bonus forrás, buff expiry hullám | ns/rebuild, alloc/rebuild, dirty batch p95/p99 |
| Tiszta CombatKernel | Normal/magic/hybrid, crit facts, penetration/resists | hit/s, ns/hit, alloc/hit, bitazonosság |
| Batch + commit | Sok külön target és egy hot target | p50/p95/p99 compute/commit, queue wait, worker scaling |
| Proc/DoT stress | Max legális budget és sok timer | tick p99, secondary queue high-water |
| ECS életciklus | Spawn/despawn/logout/warp, in-flight jobok | use-after-free/race nulla, snapshot retention RSS |
| Katalógusszinkron | Cache hit/miss, chunk loss, nagykliens-szám | byte/session, latency, peak memory, retry |
| DB/gazdaság | Refine/use/trade/Mission award konkurensen | tx latency p95/p99, lock wait, retry/conflict arány |
| Offline migráció | Teljes anonymizált prodméretű snapshot | sor/s, idő, peak RSS, export/import méret |

**Javasolt kiinduló, még nem igazolt SLO:** egy rögzített, legalább 8 fizikai magos staging referenciagépen 50 ms-os szimulációs tick, 10 000 aktív entity és 1000 validált root hit/tick, a fixture szerinti skill/proc keverékkel. Compute+combat commit p99 legfeljebb 10 ms; teljes tick p99 legfeljebb 40 ms; tartós backlog nem nő 30 perces soak alatt. Ez nem állítás a jelenlegi Auriga tickkonfigurációjáról. Az audit módosíthatja a referencia-workloadot, de a végleges kaput a futások előtt ADR-ben rögzíteni kell.

Tiszta kernel hot pathban **0 heap allocation/hit** a batch előkészítése után; nincs SQL, network send vagy mutexelt globális RNG. A normál stat-cache hit nem generál új snapshotot. Más workerszám eredményhash-e kötelezően azonos, akkor is, ha a gyorsulás nem lineáris.

Mintatervezett CLI, csak a tool megvalósítása után használható:

```text
AurigaCombatBench --fixture tests/fixtures/item_combat/v1/stress.json --workers 1 --warmup 5s --duration 30s
AurigaCombatBench --fixture tests/fixtures/item_combat/v1/stress.json --workers 8 --warmup 5s --duration 30s
AurigaItemCombatMigrate --source-export <read-only-export> --mapping <reviewed-map> --dry-run --report <report-dir>
```

Ezek nem ellenőrzött, már létező repository parancsok. A Codex készítse el hozzájuk az argumentumvalidálást, determinisztikus inputkezelést és géppel olvasható riportot.

### 18.2. Production megfigyelhetőség

Metrikák: combat intent accepted/rejected reason szerint; math/domain error; crit/proc activation; snapshot revision mismatch; hit/secondary count; queue depth/age; compute/commit tick p99; stale target; death duplicate; outbox lag; inbox dedup; Mission award retry; DB CommitUnknown; item revision conflict; catalog/hash mismatch; cache letöltési hiba.

Teljes trace csak mintavételezve vagy célzott debug kapcsolóval, rate limit mellett. Trace mezők: release/ruleset hash, ActionId, hit/target generation, source stat snapshot hash, stage raw eredmények, DecisionFacts, absorption és commit reason. Ez lehetővé teszi egy hibás számítás replayét anélkül, hogy productionon újra futna a régi motor.

Alarm küszöbök véglegesítendők az SLO-val. Numerikus overflow, eltérő payloadú EventId, duplikált tartós instance ID és schema/hash split brain már egyetlen előfordulásnál kivizsgálandó invariánssértés; normál gameplay elutasítás nem riasztásvihar.

## 19. Release, cutover és rollback régi runtime adapter nélkül

### 19.1. Release-egység

Egy egységként archiválandó és aktiválandó: új server/Database binárisok, dependency/build manifest, új DB-séma/migráció, teljes katalógus és ruleset, client projection és manifest, frissített kliens/launcher assetcsomag, Mission/Dungeon package, migrációs mapping/tool/report, backup checkpoint és release runbook.

Item- és combat séma nem kapcsolható szét tetszőleges „félig új” éles állapotra. Régi kliens nem maradhat online új item protocol mellett. Régi server process, cron/tool/DB writer és cache-flush process nem férhet hozzá az új táblákhoz. A package/install ellenőrzés kizárja a régi item_proto resource és legacy combat module runtime betöltését.

### 19.2. Aktiválási kapuk

1. Minden P0–P7 elfogadási feltétel teljesül, unresolved data mapping nincs.
2. Végső backup hash és visszaállítási próba rendelkezésre áll, RTO és megengedett RPO rögzített.
3. Új release immutable manifest/hash egyezik a server/client/DB oldalakon.
4. Régi writerek bizonyítottan leálltak és a végső source checkpoint után nincs új régi írás.
5. Új zárt smoke sikeres; pénz/item/award/ownership összesítések egyeznek.
6. A tényleges éles telepítés az adott munkamenetben kifejezetten engedélyezett üzemeltetési művelet legyen. Ez a fájl specifikációt ad, nem hajt végre deployt.

### 19.3. Rollback-szintek

**Kód/balance rollback kompatibilis új sémán:** az előző igazolt, már **új modellű** release binárisát és hozzátartozó katalógus/ruleset/projekció csomagját karbantartásban aktiváld. Instance-állapotot csak akkor tarthat meg, ha a schema/profile/refinement kompatibilitás validált. Nem fut párhuzamos régi motor és nincs kulcs-visszafordító adapter.

**Első cutover hibája még játékosnyitás előtt:** új writerek stop, hibás állapot és logok megőrzése, staging/cél DB visszaállítása a cutover checkpointból, új offline konverzió/javított új release. A belépési kapu zárva marad, amíg a teljes új rendszer ellenőrzött. Ez az elsődleges rollback/repair út.

**Inkompatibilis hiba játékosnyitás után:** azonnal karbantartás és writer freeze; legutolsó igazolt **új formátumú** checkpoint + domain journal/receipt replay; gazdasági reconciliation és majd új release. Ha nincs elégséges journal, az RPO miatti előrehaladás-vesztést konkrétan fel kell mérni és az üzemeltetőnek döntést kell hoznia. Automatikus adateldobás vagy veszteségmentesnek nevezett vak DB restore tiltott.

**Teljes migráció-visszavonás:** csak külön üzemeltetési döntéssel teljes pre-cutover backup és teljes akkori release hideg visszaállítása jöhet szóba, az új rendszer összes processének leállítása után. Ez nem része az új runtime-nak, nem párhuzamos üzem és nem folyamatos reverse migration. Az új rendszerben keletkezett adatok visszavezetése nincs automatikusan megoldva; nem szabad ezt veszteségmentes rollbackként ígérni. Ha a termék semmilyen régi release újraindítását nem engedi, ezt az ágat a runbookból ki kell venni, és csak új modellű restore/repair marad.

### 19.4. Rollback trigger és gyakorlás

Kötelező stop/rollback vagy repair trigger: tulajdon/stack/pénz invariánssértés, duplikált reward, reprodukálható determinisztikussági eltérés, schema/ruleset split brain, tartósan növekvő outbox vagy tick backlog, kritikus sync/belépési hiba. A pontos idő/küszöb SLO-függő; integritássértésnél nem várunk throughput-romlási küszöbre.

Gyakorlat: backup restore → új bináris + manifest egyeztetés → journal/receipt recovery → duplicate event/reward próbák → újranyitási smoke. Mérni kell a valós helyreállítási időt és a visszaállási pontot; egy backupfájl megléte nem restore-bizonyíték.

## 20. Javasolt fájlstruktúra és konkrét módosítási felületek

Az alábbi fa **tervezett**, az auditált repositoryhoz illesztett elrendezés; nem állítja, hogy az új fájlok már léteznek. A tényleges modulhatárokat a build audit alapján alakítsd ki.

```text
SRC/
  Server/
    common/
      item/ItemKey.hpp
      item/ItemCategory.hpp              # generált enum + schema regiszter
      item/ItemWireCodec.hpp
      combat/CombatWireTypes.hpp
    GameServer/
      item/catalog/
        ItemCatalog.hpp/.cpp
        ItemCategoryTables.hpp
        ItemCatalogValidator.hpp/.cpp
        RefinementProfiles.hpp/.cpp
        BonusRegistry.hpp/.cpp
      combat/kernel/
        FixedMath.hpp
        CombatTypes.hpp
        DamagePipeline.hpp/.cpp
        DecisionFacts.hpp
        ProcRules.hpp/.cpp
        SkillEvaluator.hpp/.cpp
      combat/runtime/
        CombatAdmission.hpp/.cpp
        CombatSnapshotBuilder.hpp/.cpp
        CombatBatch.hpp/.cpp
        CombatCommit.hpp/.cpp
        DecisionService.hpp/.cpp
        CombatEventJournal.hpp/.cpp
      ecs/components/
        item_components.hpp              # új instance identity/state
        item_proto_components.hpp        # régi snapshot/pointer megszűnik vagy natív refre cserélődik
        combat_components.hpp            # új typed állapot
      ecs/systems/
        ItemSystem.hpp/.cpp              # natív domain API
        InventorySystem.hpp/.cpp
        ItemAttributeSystem.cpp
        PointSystem.hpp/.cpp             # contribution/StatBuilder integráció
        CombatSystem.hpp/.cpp            # új runtime orchestration; régi formula kivéve
        SkillSystem.hpp/.cpp             # új skill pipeline
        NetworkSyncSystem.hpp/.cpp
      mission/
        MissionRewardPayload.hpp         # ItemRewardSpec
        MissionEventIngestion.hpp/.cpp    # új natív consumer, ha szükséges
        ... codec/hash/checkpoint/resource/persistence módosítások
      dungeon/
        ... committed death/progress event fogyasztók
    Database/
      ItemCatalogRepository.hpp/.cpp
      ItemInstanceRepository.hpp/.cpp
      ... cache/award/packet frissítések
    SQL/
      ... meglévő SqlParams/PreparedTransaction/AsyncSQL, auditáltan továbbhasználva
  Launcher/
    Game/
      ItemCatalogClient.hpp/.cpp
      ItemManager.cpp                    # új projection betöltés
    UserInterface/
      ... tooltip/wiki/inventory/refine/protocol frissítések
  Tools/
    ItemCatalogBuilder/
    ItemCombatMigrate/                   # egyetlen régi formátumot ismerő offline modul
    CombatBench/
db/
  migrations/item_combat/
    000_protos_database.sql
    001_protos_catalog_schema.sql
    002_player_instance_schema.sql
    003_player_mission_domain_events.sql
    004_constraints_and_indexes.sql
config/
  item_combat/category_registry.json
  item_combat/bonus_registry.json
  item_combat/ruleset_v1.json
tests/
  server/ItemKeyTests.cpp
  server/ItemCatalogTests.cpp
  server/RefinementProfileTests.cpp
  server/FixedMathTests.cpp
  server/DamagePipelineTests.cpp
  server/CombatDeterminismTests.cpp
  server/CombatCommitTests.cpp
  server/ItemCombatSqlTests.cpp
  server/CombatMissionDungeonTests.cpp
  server/ItemCombatMigrationTests.*
  fixtures/item_combat/v1/
benchmarks/item_combat/
docs/
  audits/item_combat_repository_audit.md
  audits/item_reference_inventory.csv
  adr/item_combat_*.md
  runbooks/item_combat_cutover.md
  runbooks/item_combat_restore.md
  reports/item_combat/
```

Régi útvonalak kezelése:

- `common/tables.h::TItemTable`, `Database/ProtoReader.cpp` item része, régi proto packet és client loader: az új runtime-ból kiveendők, az offline olvasó saját külön típust kaphat.
- `combat/battle.cpp` damage/attack-rating formulái és `CombatSystem::Damage` régi módosítósorozata: új kernel és commit váltja. A nem damage jellegű hasznos domainlogika külön megőrzendő, de nem delegálhat vissza a régi számításra.
- `ItemSystem_LegacyBridge.cpp`: funkciónként natív új domainműveletekre bontandó; a bridge és a szükségtelen include-ok kiveendők a release linkből. Egy fájl puszta átnevezése nem elfogadás.
- `root/data/items/protos/item_proto`, extra proto, régi numeric lookupok a wiki/tooltip/asset betöltőben: új projection és typed profile váltja. Megmaradó kép/model fájlnév lehet explicit assethivatkozás, régi protofájl nem betölthető fallback.
- Database/Gameserver cache, `ItemAwardDelivery` és Mission resource/award útvonalak: új ItemKey/refinement/instance revision payload. Nem elég csak a GameServer header módosítása.

## 21. Fázisonkénti megvalósítás és elfogadási kapuk

A fázisok implementációs/review lépések. Nem jelentenek egymással párhuzamosan futó régi és új production motort. Félkész targetek izolált unit/tool buildként készülhetnek; az új runtime release csak a teljes konverzió és call-site csere után indítható támogatott termékként. Sem ideiglenes runtime vnum adapter, sem kétmotoros feature flag nem megengedett fejlesztési rövidítésként.

| Fázis | Feladat és kézzelfogható eredmény | Elfogadási kritérium / bizonyíték |
|---|---|---|
| **P0 — Audit** | Friss repo/build/DB/protokoll és referencialeltár; munkafaállapot; ADR és nyitott kérdések | Minden módosítandó határ fájl+szimbólummal azonosított; tesztalapvonal rögzített; nincs ellenőrzöttként feltüntetett ismeretlen; régi writerek és összes item store leltára kész |
| **P1 — Domainkulcs és schema** | Külön `protos` adatbázis, ItemKey, kategóriaregiszter, typed schema, SQL constraints, codec | A protók/profilok csak `protos`-ban, az instance-state `player`-ben van; teljesen minősített SQL és cross-database FK izolált DB-n igazolt; 0/ütközés/endian tesztek pass; kategóriánként 1-ről allokálás igazolt; minden base-hez pontosan megfelelő child |
| **P2 — Catalog/refinement/bonus** | Immutable catalog, profile loader/validator, public projection builder | Egy proto/tárgytípus; teljes refinement step coverage; fix alap+gain golden; ismeretlen bonus/profil blokkol; kanonikus hash reprodukálható |
| **P3 — CombatKernel** | FixedMath, új pipeline, skill evaluator, mob/mode, crit/proc facts | Stage golden és property/oracle tesztek pass; nincs alap damage RNG/float; crit/proc pontosan regisztrált; cross-platform bitazonosság a támogatott mátrixon |
| **P4 — ECS és domainintegráció** | StatBuilder, snapshot/batch/commit, item lifecycle, Mission/Dungeon natív esemény, cache/persistence | 1/2/4/8 worker azonos state hash; egy HP/death writer; death/reward dedup crash után; equip/refine/trade lifecycle konzisztens; nincs registry worker hozzáférés |
| **P5 — Kliens és protokoll** | Új wire schema, catalog manifest/cache, instance/tooltip/wiki/refine sync | Cache hit/miss/corrupt/out-of-order próbák pass; régi kliens elutasítva; kliens nem dönt damage/refine adatról; server/client projection hash egyezik |
| **P6 — Offline migráció** | Read-only export→új schema/content/package; mapping/report; teljes dry-run | 100% referencia-coverage; 0 megmagyarázatlan karantén/árva/duplikáció; owner/count/gold/state reconcile; determinisztikus ismételt output; production méretű próba és idők |
| **P7 — Hardening és release** | Build/package legacy-mentesítés, regresszió, fuzz/sanitizer, bench/soak, restore runbook | Minden releváns teszt pass vagy konkrét release-blokkolóként jelölt; SLO teljesül; restore próba igazolt; runtime link/resource/call gráfban nincs régi motor/adapter |
| **P8 — Koordinált cutover** | Writer freeze, final backup/migration, egységes új release, zárt smoke, kontrollált nyitás | A final snapshot hash és importreport egyezik; régi process nincs; new schema/catalog/client mindenütt helyes; inventory/economy/Mission/Dungeon smoke és megfigyelési ablak sikeres |
| **P9 — Utóellenőrzés** | Integritás- és teljesítményriport, legacy runtime csomagok kizárása, archívum | Megfigyelési ablakban nincs nyitott invariánssértés/backlog; új modellű restore pont kész; operációs dokumentáció és benchmark/replay lelet átadva |

Minden fázis végén külön jelentés: ténylegesen elkészült fájlok, megváltozott viselkedés, futtatott ellenőrzés és eredménye, kimaradt ellenőrzés oka, maradó kockázat és következő kapu. Nem elég „kész” státusz vagy puszta compilation success.

## 22. Codex végrehajtási utasítások

Az alábbi blokk önálló munkautasításként használható a repositoryban végzett implementációhoz. A jelen fájl kérésének eredménye a specifikáció; a teljes játék migrációja külön végrehajtási munka.

> **Implementáld az AurigaGlobal új kategóriás itemmodelljét és determinisztikus harcrendszerét a jelen specifikáció alapján. Először auditálj; a kód módosítása előtt térképezd fel a tényleges repositoryt, buildet, SQL/cache/protokoll és Mission/Dungeon kapcsolatokat. Az auditkor rögzített HEAD és megfigyelések nem helyettesítik a friss ellenőrzést.**
>
> A C++23 és EnTT cél megmarad. Hozz létre külön `protos` adatbázist az `account`, `common`, `log`, `player`, `mission` mellé. Az item-, mob- és skill-prototípusok, a kategóriatáblák és kapcsolódó statikus profilok ebbe kerüljenek; a konkrét item instance és refinement_level maradjon a `player` állapota. `protos.item_base` és kategóriatáblák, kategóriánként 1-ről induló local_id, összetett ItemKey, egy proto szemantikai tárgytípusonként. Használj teljesen minősített SQL-neveket és ellenőrzött adatbázis-jogosultságokat. A fejlesztés instance refinement_level és profil. Nincs +0…+9 proto, refined_vnum, runtime vnum-feloldás, min/max támadó vagy véletlen alapsebzés. Csak explicit kritikus/proc kivétel használhat harci véletlent. A fixpontos pipeline sorrendje és kerekítése normatív.
>
> Az új runtime nem hívhat régi item/harci evaluatort, és nem kaphat átmeneti adaptert vagy párhuzamos régi motort. A régi adatok átvétele külön egyszeri offline eszközben történjen. Az összes call site, DB/cache, kliens, Mission reward/package és Dungeon esemény frissüljön közvetlenül az új szerződésre.
>
> Egyetlen írható ECS/domain igazságforrást használj. A registry owner-thread marad; a workers csak immutable snapshotból számolnak. Stabil commitrend, szerverautoritatív packet, prepared statement, revision/fence, tartós operation/death idempotencia és esemény inbox/outbox kell. A meglévő SQL/Mission infrastruktúrát auditálva használd tovább; ne hozz létre második párhuzamos infrastruktúrát.
>
> Az előírt SQL/C++ mintákat illeszd a bizonyított DB-dialektushoz és include/build felülethez. Ne hivatkozz nem létező API-ra kész tényként. Minden audit-TODO-t oldj fel vagy konkrét blokkolóként dokumentálj. Üzleti/balance döntést ne állíts korábbi jóváhagyásnak. Rutinszerű implementációs döntésben a jelen normatív szabályt kövesd.
>
> A felhasználói munkafát őrizd meg. Ne törölj hasznos funkciót pusztán legacy nevű fájl miatt: portold a szükséges domainműveletet, majd távolítsd el a régi runtime utat. Ne kapcsolj ki tesztet vagy validátort az implementáció kedvéért. Production adatot ne írj fejlesztési tesztből.
>
> Csak a céltermékben megmaradó funkciókat portold. A folyamatban lévő rendszerkivezetéseket tartsd tiszteletben: eltávolításra kijelölt funkciót, táblát, cache-t, protokollt vagy tesztcélt ne állíts vissza, és ne tegyél a migráció elfogadási feltételévé.
>
> Fázisonként haladj az elfogadási kapukkal. A régi runtime törlése és az új teljes bekötés egyetlen koherens release eredménye legyen. Az offline migráció, staging rehearsal, benchmark és restore ellenőrzés nélkül a feladat nem kész. Éles telepítést csak az adott munkamenetben megadott üzemeltetési felhatalmazással hajts végre.
>
> A végső átadás tartalmazzon friss auditot, új sémát/katalógust, új kernel/runtime/client integrációt, offline konvertert és coverage riportot, teszt/benchmark eredményt, release manifestet, cutover/restore runbookot és a kimaradt vagy nem igazolható ellenőrzések tételes listáját.

### 22.1. Javasolt első keresések

Ezek olvasási/audit minták, nem vakon végrehajtandó módosítások. A repo gyökerében, a helyi shell szintaxisával használandók; az output ne tartalmazzon credentialt.

```text
git status --short
git rev-parse HEAD
rg --files -g AGENTS.md -g CMakeLists.txt -g "*.sql"
rg -n "dwRefinedVnum|refined_vnum|dwVnumRange|TItemTable|alValues|GetItemValue" SRC db root tests
rg -n "weapon_min|weapon_max|magic_min|magic_max|GetMobDamageMin|GetMobDamageMax" SRC tests
rg -n "number\(|random|mt19937|uniform_|POINT_CRITICAL|POINT_PENETRATE|POINT_BLOCK|POINT_DODGE" SRC/Server
rg -n "EvEntityDamaged|EvEntityDied|MobKilled|BossDefeated|RewardBundle|ItemAward" SRC tests
rg -n "DirectQueryPrepared|ReturnTransactionPrepared|CommitUnknown|state_revision" SRC tests
```

A keresés után kódszinten ellenőrizd a találatokat. A `number()` minden találatának eltávolítása például helytelenül lootot/AI-t is átírhatna; a min/max minden találatának tiltása pedig jogos range/HP limitet is érintene. A tilalom a damage-alap random tartományára és a régi item/harci runtime szemantikára vonatkozik.

### 22.2. Új buildcélok szerződése

Javasolt modulcélok: `AurigaItemDomain`, `AurigaCombatKernel`, `AurigaItemCatalogBuild`, `AurigaItemCombatMigrate`, `AurigaCombatBench`, valamint célzott tesztexecutable-ek. Az offline migráció legacy reader könyvtárát **csak** a migrációs tool linkelheti. A GameServer/Database/kliens függőségi gráfján automatizált guard ellenőrizze, hogy ez a könyvtár nem elérhető.

CMake részlet új, önálló kernelhez, a tényleges dependency targetek ellenőrzésével:

```cmake
add_library(AurigaCombatKernel STATIC
    combat/kernel/DamagePipeline.cpp
    combat/kernel/SkillEvaluator.cpp
    combat/kernel/ProcRules.cpp)
target_compile_features(AurigaCombatKernel PUBLIC cxx_std_23)
set_target_properties(AurigaCombatKernel PROPERTIES CXX_EXTENSIONS OFF)
target_include_directories(AurigaCombatKernel PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
# Boost header/include target a repository igazolt csomagkezeléséből.
# A kernelhez nem linkelhető SQL, network, Lua, legacy item/combat vagy g_registry modul.
```

Tesztfuttatás előtt olvasd ki a tényleges configure presetet/generátort és buildkonfigurációt. Ne állítsd, hogy egy CMake target létezik, amíg nem hoztad létre vagy nem láttad az aktuális buildben. A végső buildet dependency/package szinten is vizsgáld, nem csak forrásnév-kereséssel.

## 23. Nyitott döntések és blokkoló jellegük

| Kérdés | Jelenlegi státusz / alapértelmezés | Mikor kötelező lezárni? |
|---|---|---|
| Pontos DB-termék/verzió/engine/SQL mode | Nem ellenőrzött; DDL referencia MySQL 8.4/InnoDB | P1 SQL merge és minden adatírás előtt |
| Eredetileg elfogadott teljes kategórialista | Nem olvasható az előzményből; 3.2. új javaslat | Első új kulcsregiszter/release előtt |
| Összes feature flag és custom item behavior | Csak részben feltárt | P0/P2, érintett content publikálás előtt |
| Fix alapok és refinement balance | Példák, nem production számok | P2/P3 balance fixture és P6 migráció előtt |
| Dodge/block/pierce régi bonus mapping | Új default determinisztikus; explicit proc kivétel lehetséges | BonusRegistry/skill validálás előtt |
| Soul/socket/expiry/timer teljes jelentés | Forrásfüggő, tételes audit szükséges | P2/P6; adatvesztési blokkoló |
| Damage/world geometry fixed skála | Audit-TODO | P3 admission és end-to-end determinism előtt |
| Támogatott OS/compiler/EnTT/Boost/crypto | Audit-TODO | P3 cross-platform és P7 release előtt |
| HP/death journal tartóssági határa | Receipt/outbox kötelező; részletes RPO/WAL policy nyitott | P4 crashbiztossági kapu előtt |
| Mission/Dungeon aktuális writer/fence és event routing | Csak felületvizsgálat történt | P4 integráció előtt |
| Production workload, tick és SLO | 18. fejezet számai javaslatok | Benchmark futás/optimalizálás előtt |
| Backup restore, RTO/RPO, post-open recovery | Nem igazolt | P7/P8 cutover előtt |

Ezek nem indokolják a jelen terv helyett egy runtime átmeneti réteg beépítését. A végrehajtó végezze el a független munkát, és csak a valóban hiányzó, nem kikövetkeztethető termék/üzemeltetési döntést kérje be konkrét alternatívákkal és hatással.

## 24. Végső készültségi ellenőrzőlista

- [ ] Friss repository audit és ellenőrzött hivatkozási leltár készült, ismeretlenek elkülönítve.
- [ ] Külön `protos` adatbázis készült az `account`, `common`, `log`, `player`, `mission` mellé; `protos.item_base` + minden tényleges kategóriatábla, prototípus/profil és FK/validátor kész. Az instance-state `player`-ben maradt, a jogosultságok és a teljesen minősített SQL-hivatkozások ellenőrzöttek.
- [ ] Kategóriánként 1-ről induló, stabil `local_id`; összetett ItemKey minden szükséges rétegen.
- [ ] Egy protó tárgytípusonként; refinement instance-state + teljes profilgörbe.
- [ ] Nincs runtime `refined_vnum`, vnum-szint aritmetika, min/max damage, proto compatibility pointer vagy generic value olvasás.
- [ ] Fix physical/magical alap+gain, fixed-point unitok és kerekítés minden matematikai határon.
- [ ] Teljes normal/skill/mob/PvP/PvE/hybrid/DoT/proc pipeline, stage goldenekkel.
- [ ] Kizárólag explicit kritikus/proc harci véletlen; rögzített döntésekkel bitazonos replay.
- [ ] Owner-thread ECS commit, immutable worker snapshot, stabil esemény- és HP-sorrend.
- [ ] Mission/Dungeon új typed esemény és ItemRewardSpec, tartós inbox/outbox/receipt.
- [ ] Prepared DB, revision/fence, reconnect/CommitUnknown recovery és cache ordering igazolt.
- [ ] Új kliensschema, autoritatív instance sync, verzió/hash cache és hiteles manifest.
- [ ] Offline migráció 100% coverage, count/owner/gold/state reconciliation, nincs silent loss.
- [ ] Regresszió, oracle/property, fuzz/sanitizer/fault injection és cross-worker/platform ellenőrzés teljesült.
- [ ] Benchmark/soak a rögzített SLO-n megfelelt; mért eredmények archiváltak.
- [ ] Runtime build/link/package/resource ellenőrzés kizár minden legacy adaptert és párhuzamos régi motort.
- [ ] Végső backup, release manifest, cutover és restore próba rendelkezésre áll.
- [ ] Éles nyitás után új modellű checkpoint és utóellenőrzési jelentés készült.

## 25. Forráskezelés és a dokumentum ellenőrzési státusza

Elsődleges projektbizonyítékok a 0.1. fejezetben felsorolt, közvetlenül olvasott repositoryfájlok. A két korábbi beszélgetés csak a ténylegesen olvasható szövegrésszel szolgált kontextusként; a hiányzó asszisztensválaszokból sem séma, sem korábbi jóváhagyás nem lett ellenőrzött tényként átvéve.

A külső linkek kizárólag a hivatkozott technikai működést támasztják alá: EnTT thread-safety, Boost széles checked integer lehetőség, C++ expected, SQL prepared/FK/CHECK/DDL viselkedés. Nem bizonyítják az AurigaGlobal telepített verzióit vagy az itt javasolt új balance helyességét. Az új adatmodell, pipeline, capek, ID-kódok és fázisok a jelen specifikáció tervdöntései.

Ez a fájl a ténylegesen elkészített átadandó dokumentum. Nem történik benne automatikus production deploy, forráskód-átírás, offline adatimport vagy adatbázis-módosítás. Az implementáció készültségét kizárólag a 21–24. fejezetekben kért konkrét bizonyítékok igazolhatják.

### 25.1. Az átadott MD ellenőrzése

A dokumentum hat C++ kódblokkját egy külön workspace-ellenőrző fordítási egységbe összegyűjtve a helyi MSVC x64 fordító `/std:c++latest`, `/EHsc`, `/W4` beállításokkal sikeresen lefordította; az ellenőrzés Boost 1.91 headereket, telepített EnTT headereket és a repository tényleges SQL-típusheadereit használta. Futó assertionök igazolták a félértékek kerekítését, néhány overflow/hibás kulcs/divisor eset elutasítását, ItemKey roundtripet és a normal/crit példák 162000/340200 raw eredményét. Független BigInt-számolás a shield utáni 150000/328200 és a magic 124800 raw példát is visszaigazolta.

Ez szűk példakód-ellenőrzés, nem production link/build, nem DB-végrehajtás, nem cross-platform bizonyítás és nem performance mérés. A `/std:c++latest` siker önmagában nem helyettesíti a repository támogatott C++23 toolchain-mátrixának tesztjét. Az SQL-blokkok referencia-sémák: tényleges DB-n még nem futottak. A dokumentum számpéldái és kódblokkjai így ellenőrzöttebbek, de a végrehajtási elfogadási kapuk továbbra is elvégzendő munkák.
