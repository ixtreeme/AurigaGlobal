# Forras-szintu tipusaudit (helyes vs. kevert adattipusok)

Cel: a kodban elofordulo helytelen adattipus-hasznalatok es tipuskeveredesek,
amelyek adatvesztest, tulfolyast vagy hibas szamitast okozhatnak.
A DB-oldali sema-tipusok kulon: `db/db_schema_audit.md`.

Modszer: celzott keresesek a teljes `SRC/Server` fan (SQL formatumok, C++
struct mezok, printf specifikatorok, `str_to_number`/`atoi` parse-ok,
32/64-bites hatarok, idobelyegek, penz-szamitasok). Minden tetel mellett
forras (`fajl:sor`).

Jelolesek:
- **P0** = bizonyitott adatvesztes / tulfolyas
- **P1** = formátum-hiba (UB) vagy pontossagvesztes
- **P2** = 2038 / ido-tipus problema
- **P3** = DB sema oldali (lasd a masik auditot)

---

## P0 - Bizonyitott adatvesztes / tulfolyas

### 1. Guild bank: 64-bites DB vs 32-bites GameServer (a legdurvabb)

- DB oldal: `guild.gold` 64-bites (`UPDATE guild SET gold=%lld`,
  `Database/GuildManager.cpp:773`; a `CGuildManager::MoneyChange(uint32_t, int64_t)`
  parametere int64). A DB tehat `BIGINT`.
- GameServer oldal: `SGuildData::gold` = `int` (`guild/guild.h:116`).
  - Betoltes: `str_to_number(m_data.gold, row[11])` (`guild/guild.cpp:679`)
    -> >2 147 483 647 yang eseten csonkul/atfordul.
  - `RequestWithdrawMoney(entt::entity, int iGold)` (`guild/guild.cpp:2103`),
    `RecvMoneyChange(int iGold)` (`:2137`),
    `RecvWithdrawMoneyGive(int iChangeGold)` (`:2155`).
  - A DB -> GameServer packetek viszont 64-bites mezot hoznak:
    `TPacketDGGuildMoneyChange.iTotalGold` es
    `TPacketDGGuildMoneyWithdraw.iChangeGold` = `int64_t` (`common/tables.h`),
    es a handler implicit szukiti:
    `g->RecvMoneyChange(p->iTotalGold)` (`guild/guild_db.cpp:203`),
    `g->RecvWithdrawMoneyGive(p->iChangeGold)` (`:214`).
- Kovetkezmeny: 2.1 Mrd yang felett a ceh leltar egyes muveleteknel
  eltunik/elojelet valt (a DB-ben megmarad a helyes ertek, de a GameServer
  memoriaban levot irja vissza -> adatvesztes).
- Javaslat: a teljes guild-money uton `int64_t` (struct mezo, paraméterek,
  packet feldolgozas); a `sprintf` marad `%lld`.

### 2. account.coins / Dragon Coin: 64-bites DB vs 32-bites kezelés

- DB: `account.coins` 64-bites hasznalat (`UPDATE ... coins = '%lld'`,
  `GameServer/ecs/systems/PlayerRuntimeSystem.cpp:1600`).
- Ugyanott:
  - `GetDragonCoin()` `uint32_t`-be olvas: `PlayerRuntimeSystem.cpp:2111-2118`
    (`str_to_number(dc, row[0])`, dc uint32) -> >4.29 Mrd coin atfordul.
  - `SetDragonCoin(entt::entity, uint32_t amount)` (`:2123-2128`) es
    `CClientManager::SetDragonCoin(uint32_t id, uint32_t amount)`
    (`Database/ClientManager.cpp:4819`) -> 32-bites keret.
  - `UPDATE account set coins = coins + %d` (`Database/ClientManager.cpp:4276`)
    -> int32-es packet (`SRequestChargeCash.amount`, `tables.h`).
- Kovetkezmeny: ugyanaz a valutapály 4.29 Mrd felett elveszik / hibas lesz.
- Javaslat: `int64_t GetDragonCoin`, `int64_t SetDragonCoin(int64_t amount)`,
  BIGINT; a régi `%u`-s irasok atirasa `%lld`-re.

### 3. Item shop ar-szamitas: uint32 tulfolyas + float penz

- `GameServer/entity/char_manager.cpp:2283`:
  `uint32_t itemPrice = itemData.itemPrice * itemCount;`
  -> a szorzat 32-biten tulfolyik (pl. 100M DC x 50 db = 5 Mrd -> ~0.7 Mrd).
- `:2286`: `itemPrice = static_cast<uint32_t>(float(...) / 100.0f * ...)`
  -> float pontossag (2^24 felett 1 helyettes pontatlansag) penzre.
- Ugyanaz a Database oldalon: `Database/ClientManager.cpp:4960`
  `(uint32_t)(float(itemPrice) / 100.0 * float(100 - discount))`.
- Kovetkezmeny: hibas ar / olcsobb vasarlas bizonyos araknal.
- Javaslat: `int64_t` szorzas, egesz szamu szazalek
  (`price * (100 - discount) / 100`).

### 4. guild_war_bet betoltese 32-biten

- Iras 64-bitten: `INSERT INTO guild_war_bet ... SqlParam::Int(dwGold)`
  ahol `dwGold` `int64_t` (`Database/GuildManager.cpp:1305-1306`).
- Betoltes 32-bitten: `uint32_t dwGold; str_to_number(dwGold, row[2]);`
  (`Database/GuildManager.cpp:1240-1247`), es a `mapBet` is uint32-ot tarol.
- Kovetkezmeny: 4.29 Mrd feletti bet elveszik a jutalom-szamitasnal
  (`WarReward` `dwTotalBet` szinten uint32, `:1432`).
- Javaslat: `int64_t` vegig (DB, mapBet, WarReward), `%lld` formatum.

### 5. int64 -> `%d` hibas formatum (nem csak kozmetika)

- `GameServer/world/world_input.cpp:149`:
  `ChatSystem::SendNew(..., 773, "%d", rConf.price)`, ahol
  `TMapConfig::price` = `int64_t` (`GameServer/core/config.h`,
  TMapConfig). Varargs `%d` a 64-bites ertek also 32 bitjet olvassa
  (x86-64) -> hibas ar a kliens uzenetben.
- Ugyanez a minta tobb helyen (`PointSystem::Get` = `int64_t`,
  `PointSystem.hpp:9-11`):
  `cmd_gm.cpp:1104,1168`, `CombatSystem.cpp:4859,4885-4900`
  (`"%d"`/`"%3d"` int64-tel). Ezeknel a gyakorlati ertek kicsi, de a
  formatum-egyezes hianyzik (UB), es 64-bites borra barmikor elszall.
- Javaslat: `%lld` (vagy explicit `(int)` cast, ha valoban 32-bites a
  tartomany); hosszu tavon `std::format`/fmt-re atterni.

### 6. `str_to_number` 32-bites overload 64-bites DB ertekre

- `Database/ClientManager.cpp:778`: `pSafebox->dwGold = atoi(row[3])` —
  ha a `safebox.gold` DB oszlop BIGINT (a kod `%u`-val ir, de a
  konfiguracios audit BIGINT-et javasol), a 2.1 Mrd feletti ertek
  `LONG_MAX`-ra satulal (Windows `atoi`) -> rossz egyenleg.
- `GuildManager.cpp:1247` (lasd 4. pont), `guild.cpp:679` (lasd 1. pont).
- Javaslat: `atoll`/`str_to_number(int64_t&)`, majd tartomany-ellenorzes.

### 7. Log buffer tulfolyas-kozeli (truncation)

- `GameServer/network/input_db.cpp:570`:
  `char buf[51]; snprintf(buf, 51, "%s %lld %d %d %u", ip, gold, ...)`.
  A `%lld` gold max 20 karakter + IP 15 + a tobbi -> a log sor hosszabb,
  mint 51 -> a hasznos adat csendben csonkul.
- Javaslat: `char buf[128]` (vagy `std::string` + fmt).

---

## P0b - Hordozhatosagi `%ld`/`%lu` (Windows 32-bites long vs FreeBSD/Linux 64-bites long)

A szerver FreeBSD/Linux alatt is fordul (lasd start.sh, FreeBSD fixek a
git logban). A `long` Windows-on 32 bit, LP64 rendszeren 64 bit, ezert a
`%ld`/`%lu` Windows-on 32 bitet, FreeBSD/Linux-on 64 bitet olvas/ir.
Ahol a C++ valtozo `int32_t`/`uint32_t`, ott LP64 alatt **garbage ertek
vagy memoria-feluliras** tortenik. Az elo szerver FreeBSD-n fut, ezert
ezek gyakorlati P0 hibak:

### B1. `sscanf("%ld")` int32 valtozoba -> memoria-feluliras

- `GameServer/ecs/systems/MovementSystem.cpp:881`:
  `sscanf(..., " %s %ld %ld ", szTmp, &m_lTargetX, &m_lTargetY)`, ahol
  `int32_t m_lTargetX;` (`:948`) es `m_lTargetY` is int32.
  LP64-on a `%ld` 8 byte-ot ir a 4 byte-os mezobe -> a szomszedos
  tag(ok) felulirasa minden warp NPC betoltesnel.
  Windows-on mukodik -> rejtett, platformfuggo memoria-corruption.
- Ugyanez a minta: `guild_renewal.cpp:419` (`%lld` + `long long yang`,
  ott rendben), de barmely `%ld` + int32 kombinacio veszelyes.
- Javaslat: `%d` `int`-hez, vagy `%ld` `long`-hoz; a mezot explicit
  `long`-ra valtani ahol a `%ld` marad.

### B2. SQL generalas `%ld`/`%lu`-val int32/uint32 ertekbol

- `Database/ClientManager.cpp:1592`: `"%ld, %ld, %ld"` +
  `p->alSockets[i]` (int32) -> FreeBSD-on garbage socket ertekek a DB-ben.
- `Database/OfflineshopCache.cpp:1305,1448,1669`: `%ld` + `alSockets[i]`.
- `GameServer/pet/MountInventory.cpp:181`: `%ld` + socket ertekek.
- `GameServer/ecs/systems/ItemSystem_LegacyBridge.cpp:4102,4119`:
  `%ld` + `GetItemSocket()` (`uint32_t`, `ItemSystem.hpp:204`), es
  `:7720,7796`: `%lu` + `GetItemID()` (`uint32_t`, `:93`) ->
  `WHERE id = <garbage>` a pet rendszer UPDATE-jeiben FreeBSD-on
  (rossz sor modosulhat).
- `Database/GuildManager.cpp:1083`: `%ld` + `p->lWarPrice` stb.
  (`int32_t`, `tables.h:1042-1043`) -> guild war reservation ertekek.
- Javaslat: `%d`/`%u` (int32/uint32), vagy explicit cast + `%lld`.

### B3. `%ld` int64 ertekre Windows-on csonkul

- `GameServer/combat/battle.h:59,64`: `"%s %s(%ld%%) ..."` +
  `PointSystem::Get(...)` (int64, `PointSystem.hpp:9-11`) ->
  Windows-on 32 bitre csonkul, LP64-on jo. Forditva ugyanaz a hibaosztaly.

### B4. Egyeb `%lu`/`%ld` int32/uint32 ertekre (kliens command / log)

- `GameServer/item/wheel_of_destiny.cpp:43,102`: `%lu` +
  `std::get<0>(m_Data[i])` / `GetGiftVnum()` (uint32 tuple,
  `wheel_of_destiny.cpp:13-20`) -> FreeBSD-on garbage kliens command.
- `GameServer/core/profiler.h:169`: `%lu` + `iEndTime - iStartTime`
  (`int32_t`, `:19-20`) -> garbage profiler sor.
- `GameServer/ecs/systems/MovementSystem.cpp:806`,
  `PlayerRuntimeSystem.cpp:3572`: `%ld` + int32 koordinatak (log/chat).
- Javaslat: `%u`/`%d`, illetve uj kodnal `std::format`/fmt.

---

## P1 - UB / pontossag / kevert minta

### 8. `size_t` -> `%u` (32-bites formatum 64-bites ertekre)

- `Database/ClientManagerOfflineshop.cpp:1369`,
  `GameServer/social/new_offlineshop_safebox.cpp:189`:
  `OFFSHOP_DEBUG("... %u ...", items.size())` — `size()` = `size_t`.
- Nem adatvesztes (log), de UB; javaslat `%zu` vagy cast.

### 9. `str_to_number(long long&)` unsigned parse

- `common/utils.h`:
  `out = (long long) strtoull(in, NULL, 10);`
  Negativ ertekek (`-1`) az implementacio-fuggo cast miatt mukodnek, de
  nem definialt; `LLONG_MIN`/tulcsordulas nem kezelt.
- Javaslat: `strtoll` hasznalata, explicit tartomany-kezeléssel.

### 10. Kevert int/float penz- es XP-szamitas

- `Database/ClientManager.cpp:4960`, `char_manager.cpp:2286`: float arfolyam
  (lasd P0-3).
- `Database/GuildManager.cpp:1432`: `(uint32_t)(dwTotalBet * ratio * 0.9)`
  -> double + 32-bites cast; a 0.9 nem egesz, kerekitesi hiba.
- `CombatSystem.cpp:1648-1652`: `rate_t` (long double) XP-szamlalas;
  `%Lf` formatummal. A fogado valtozo `int iExp` (`GiveExp(..., int iExp)`,
  `CombatSystem.cpp`), a `iExp *= lvFactor;` / `iExp *= (rateFactor/100.0L)`
  int-be konvertal vissza -> 32-bites tulfolyas/trunkacio magas XP-ratenal.
- Javaslat: penz/XP mindenhol `int64_t`, szazalek egesz szammal.

### 11. `int` vs `size_t` ciklus-osszehasonlitasok

- Pl. `Database/ClientManager.cpp:4482,4515,4785-4802`,
  `ClientManagerOfflineshop.cpp:445,524,755,1163,1375,1568`,
  `OfflineshopCache.cpp:487,1134`: `uint32_t i < vec.size()`.
- Nem bug kis elemszamnal, de 4 Mrd felett/tipusvaltasnal
  fordito-figyelmeztetes/atfordulas; `size_t`-re atirni.

### 11b. `uint8_t`/`uint16_t` oszlopszam/size a kontener meretebol

- `Database/ClientManager.cpp:4639,4647`, `char_manager.cpp:1789,1797,1806`:
  `const uint8_t dayCount = m_EventManager.size();` -> 255+ event eseten
  csendben csonkul a packet/count.
- `Database/ClientManager.cpp:5296,5306`, `char_manager.cpp:2041,2050,2075,2083`:
  `uint8_t categorySize = it->second.size();` -> 255+ kategoria elem.
- `GameServer/ecs/systems/SocialSystem.cpp:487`:
  `header.byCount = itemkind.size();`
- `GameServer/ecs/systems/MountSystem.cpp:464`:
  `header.wCount = static_cast<uint16_t>(items.size());`
- `Database/ClientManagerPlayer.cpp:1037,1135`: `uint32_t` count vector
  meretbol (itt OK, de ellenorizni kell a kliens oldali fogadast).
- Javaslat: `uint32_t`/`size_t` + explicit meret-korlat.

### 11c. Egyeb eszrevetelek

- `Database/ClientManager.cpp:1592` es a `ClientManager.cpp:1575` koruli
  item-award REPLACE: a `window` mezo szoveges es numerikus irasa
  (lasd P0-5 es a DB audit) ugyanebbol a blokkolbol ered.
- `GameServer/network/input_db.cpp:570` `char buf[51]` + `%lld` (lasd P0-7).
- `common/utils.h` `str_to_number(long long&)` `strtoull` (lasd P1-9).

---

## P2 - Ido-tipusok (2038 / int32)

### 12. `time_t` -> `int`

- `GameServer/entity/char_manager.cpp:1791`: `const int cur_Time = time(nullptr);`
  `:1933`: `const int now = time(nullptr);`
- `GameServer/guild/guild_renewal.cpp:837,929,1029,1122`:
  `cc.lastPayTime = (int)get_global_time();` (a DB `last_pay_time` int).
- `guild_renewal.h`: `Tax::deadline` `int` (unix idobelyeg).
- `LoginServer/login_auth.cpp:175-183`: `premiumTimes[i] = atoi(row[col])`
  `UNIX_TIMESTAMP(...)` ertekre; `int` tomb.
- `Database/ClientManagerPlayer.cpp:160`:
  `battle_pass_end = FROM_UNIXTIME(%d)` — `dwBattlePassEndTime` uint32;
  2038 utan negativ `%d` -> `FROM_UNIXTIME` NULL (a `FROM_UNIXTIME` negativ
  bemenetre NULL-t ad).
- Javaslat: idobelyeghez `int64_t`/`time_t` + BIGINT oszlop; rövid tavon
  `%u`/`uint32_t` a `%d` helyett es dokumentalt 2038-as hatarido.

### 13. `TPlayerSkill.tNextRead` uint64 a skill-blobban

- `common/tables.h:429`: `uint64_t tNextRead` — a skill blobba kerul
  (`CREATE ... skill_level`), a DB-ben `BLOB`/`TEXT`; a blob merete a
  struct packingtol fugg. Ha valaha valtozik a `TPlayerSkill` layout,
  a regi blobok ertelmezhetetlenne valnak (verziozas/ migracio kell).

---

## P3 - DB sema oldali következmények

- `player.gold`, `guild.gold`, `account.coins`, `log.money_log.gold`
  -> `BIGINT` (lasd: `db/db_schema_audit.md` P0).
- `player.item.window` kevert szoveges/numerikus hasznalat
  (`window+0` vs `'INVENTORY'`) -> csak `ENUM` fedi le.
- `player.hp`/`mp` -> `uint64_t` (ECS Health/Mana, TPlayerTable), DB
  `BIGINT UNSIGNED`; javitva (2026-10-06 migracio).
- Hianyzo migraciok: `attrtype7/attrvalue7`, `account_mount_inventory`
  `attrtype6/attrvalue6` (lasd a DB auditot).

---

## Javasolt javitasi sorrend

0. `%ld`/`%lu` -> `%d`/`%u`/`%lld` javitas ott, ahol int32/uint32/uint64
   az argumentum (P0b) - FreeBSD/Linux alatt memoria-feluliras es
   rossz SQL ertekek; az elo szerver FreeBSD-n fut.
1. Guild money ut (`int64_t` a `SGuildData`-ban es a packet handlerben) -
   adatvesztest okoz, gyorsan javithato.
2. account.coins / Dragon Coin (`int64_t` get/set).
3. Item shop ar (`int64_t` szorzas, egesz szazalek).
4. `%lld`/`%d` formatumok javitasa (world_input, PointSystem-hivasok).
5. `atoi` -> `atoll` minden 64-bites DB mezore (safebox.gold, guild gold).
6. 2038-as idobelyeg-oszlopok (`int64_t`/BIGINT) tervezese.
7. DB oldali `ALTER`-ek a `db/db_schema_audit.md` ellenorzo SQL-jei utan.

Konvencio-javaslat a jovore:
- Penz (gold/yang/coins) es XP: `int64_t` + `%lld` + `SqlParam::Int`
  (ami int64-et vesz, `SQL/AsyncSQL.h:72`), DB `BIGINT`.
- ID-k (`player_id`, `account_id`, item id): `uint32_t` (ahogy most).
- Idobelyeg: `int64_t`/`time_t`, DB `BIGINT` (vagy `DATETIME` ahol NOW()
  alapu).
- Minden `snprintf`-nel a specifikator egyezzen a C++ tipussal;
  uj kod inkabb `fmt::format`/`std::format`.
- `long`/`%ld` kerulese (platformfuggo meret); hasznalj `int32_t`+`%d`,
  `uint32_t`+`%u`, `int64_t`+`%lld`-t.

---

## Lehatarolasok (mi NINCS a listaban)

Ez az audit celzott mintakeresesekre epul, nem formalis statikus analizis
es nem teljes koru. Ami nincs benne / amit kulon kell megnezni:

- **signed/unsigned osszehasonlitasok** teljes kodbazisra: itt csak
  mintavetel tortent (lasd P1-11). Erdemes `/W4` (MSVC) vagy `-Wall
  -Wextra -Wconversion` (clang/gcc) buildet futtatni es a warningokat
  osszegyujteni.
- **packet struct packing/layout egyezes** a klienssel (`SRC/Launcher`):
  `#pragma pack`, `sizeof(struct)` valtozasok, `wSize`/`wCount` kliensoldali
  ertelmezese - kulon audit.
- **Lua hid** (`lua_tointeger`/`lua_pushnumber`, `lua_Number` pontossag,
  `lua_Integer` meret) - a questlua_* fajlokban szamos konverzio van,
  ezeket kulon kell atnezni.
- **memcpy/reinterpret_cast buffer-méretek**, alignment, type punning.
- **Core/SQL/Poly/Tools modulok** reszletes tipusauditja (itt fokent a
  GameServer/Database/LoginServer volt a cel).
- **DB oszlop-tipusok**: teljes lista a `db/schema.sql`-ben es
  `db/db_schema_audit.md`-ben; az elo semaval valo gepi osszevetes
  meg hianyzik (nincs dump).
- **Forditoi bizonyitas**: javasolt MSVC `/W4` + `clang-tidy`/`cppcheck`
  + ASan build futtatasa; ezek tovabbi teteleket fognak hozni.

A fenti korok lefuttatasa utan a lista bővíthető; a jelenlegi 4 P0-blokk
es a P0b hordozhatosagi blokk a legnagyobb ismert kockazat.
