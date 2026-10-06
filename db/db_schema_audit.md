# Adatbazis sema audit (kod alapjan)

Ez a dokumentum a `db/schema.sql` melletti tipusaudit: hol ter el a kod
altal elvart adattipus/meret a tipikus (vagy a kodban keverten hasznalt)
MySQL tipusoktol, illetve mely oszlopok hianyozhatnak a valos DB-bol.
Minden allitas mellett forras (`fajl:sor`) szerepel.

A `db/schema.sql` a kodbol kovetkeztetett teljes DDL (98 tabla). Ez az audit
a **kockazatos pontokat** gyujti ossze, prioritás szerint.

---

## 1. Modszer es korlatok

- Oszlopnevek: kizarolag literalis SQL-bol. Pozicionalis INSERT-nel a nev
  `TODO(verify name)` jelolest kapott (log DB egy resze).
- Tipusok: C++ struct mezok (`SRC/Server/common/tables.h`, `length.h`) es
  printf formatumok (`%d/%u/%lld/%s`) alapjan.
- A repo nem tartalmaz `CREATE TABLE`-t (csak a 2026_04_15 migraciot), a valos
  elo sema nincs meg ehhez a repohoz -> az eltereseket ELES DB-n kell
  ellenorizni a 8. pont SQL-jeivel.
- Nem volt cel a default ertekek/NULL-ok pontos rekonstrukcioja; ezek a
  schema.sql-ben javaslatok.

Jelolesek: **P0** = bizonyitottan szuk lehet (tulfolyas/adatvesztes),
**P1** = kevert vagy tobbjelentesu tipus, **P2** = hianyzo oszlop/migracio,
**P3** = bizonytalan nev, **P4** = DB-routing bizonytalansag.

---

## 2. P0 - Meret/tipus kockazatok (bizonyitek a kodban)

| Tabla.Oszlop | Kod elvaras | Javasolt tipus | Bizonyitek | Kockazat |
|---|---|---|---|---|
| `account.coins` | int64 (`%lld`) | `BIGINT` | `PlayerRuntimeSystem.cpp:1584-1587` (int64_t), `:1600` (`%lld`); mas helyeken `%u`: `:2127`, `ClientManager.cpp:4276,4820` | A `%u`-s irasok 4 294 967 295 felett csorbulnak; ha a DB `INT`, minden 4.29Mrd feletti coin elveszik |
| `player.gold` | int64 (`%lld`) | `BIGINT` | `ClientManagerPlayer.cpp` UPDATE `gold = %lld`; `tables.h:457` int64_t | Yang tulfolyas 4.29Mrd felett |
| `guild.gold` | int64 (`%lld`) | `BIGINT` | `GuildManager.cpp:773` | Guild bank tulfolyas |
| `log.money_log.gold` | int64 (`%lld`) | `BIGINT` | `log.cpp:148` | Logolt osszeg csorbul |
| `player.r0`..`r19` | long long (`%lld`) | `BIGINT` | `ClientManagerPlayer.cpp` `r0 = %lld`; `tables.h:136` | Leaderboard ertekek tulfolyasa |
| `mob_proto.max_hp` | uint64 | `BIGINT UNSIGNED` | `tables.h:545` uint64_t `dwMaxHP` | Nagy HP-jú mobok (custom szerverek) csonkulasa |
| `item_proto.gold`, `item_proto.shop_buy_price` | int64 | `BIGINT` | `tables.h:711-712` int64_t | Magas aru itemek tulfolyasa |
| `shop_item.gold` | int64 | `BIGINT` | `tables.h:655` int64_t `price` | Shop arak tulfolyasa |
| `player.exp` | uint32 (`%u`) | `INT UNSIGNED` (nagy level eseten `BIGINT`) | `tables.h:455` uint32_t; `exp_table` ertekek gyorsan nonek | 4.29Mrd XP felett tulfolyas magas levelu szerveren |
| `player.hp`, `player.mp` | int32 | `INT` | `tables.h:471` `int hp` | Ha a harcos max HP > INT (proto `max_hp` uint64), a perzisztalt HP nem fer el -> `BIGINT` javasolt ott, ahol ilyen build van |

Megjegyzes: a `player.playtime` (`uint32`, `%u`) ~136 evig eleg, nem kockazat.
A `safebox.gold` szandekosan `uint32` a structban; ha 4.29Mrd feletti raktar
yang kell, ott is `BIGINT` kell.

---

## 3. P1 - Kevert / tobbjelentesu oszlopok

| Oszlop | Problema | Javaslat |
|---|---|---|
| `player.item.window` | A kod KET formaban hasznalja: szovegesen (`window='SAFEBOX'`, `window in ('INVENTORY',...)`; `ClientManagerPlayer.cpp:432,529`, `ClientManager.cpp:811`) es numerikusan (`window=%d`, `window+0`; `Cache.cpp:137,149`, `ClientManagerPlayer.cpp:426`) | `ENUM` az `EWindows` sorrendben (`length.h:669-690`). Ha a valos DB `VARCHAR`, a numerikus irasok/`window+0` latens hiba |
| `player.last_play` | Iras `NOW()`, olvasas `UNIX_TIMESTAMP(last_play)` | `DATETIME` |
| `player.battle_pass_end` | Iras `FROM_UNIXTIME(%d)`, olvasas `UNIX_TIMESTAMP(...)` | `DATETIME` |
| `log.loginlog2.playtime` | `TIMEDIFF(...)` iras | `TIME` (ha INT masodperc a valos DB, a kodnak kell igazodnia) |
| `player.item.socket0..2` | 3 db socket; a socket2 idobelyeges unique-oknal unix idot tarol (`ClientManager.cpp:906,979`) | elojeles `INT`; szukebb (pl. `SMALLINT`) tipus 2038 utan/adatvesztes kockazat |
| `player.item.attrvalue0..7` | `short` -> `SMALLINT` | Ha 32767 feletti bonus kell, `INT` |
| `player.item.lockedattr` | `short`, ertek `-1` is (`input_db.cpp:418`) | elojeles `SMALLINT` |
| `player.affect.bType` | A forkban `uint32_t dwType` (`tables.h:936-944`), sok base-ben `SMALLINT UNSIGNED` | `INT UNSIGNED` |
| `player.player_index.ch` | Build flag (`ENABLE_GENERAL_CH`) jelenleg KI van kapcsolva, de a literalis SQL-ben szerepel | hagyni a tablaban, vagy kapcsolni a definet |
| `player.money_log`, `lotto_list`, `change_empire` | Kulonleges/egyedi oszlopnevek, csak kodbazisbol | Nev-tipus ellenorzes lentebb (P3) |

---

## 4. P2 - Hianyzo oszlopok / migraciok (a kod mar hasznalja!)

1. `player.item.attrtype7`, `attrvalue7` - a SELECT/REPLACE mar tartalmazza
   (`ClientManagerPlayer.cpp:431`, `ClientManager.cpp:1575`), a migracio:
   `SRC/Server/Database/sql/2026_04_15_add_8th_bonus.sql`.
2. `player.item_award.attrtype7`, `attrvalue7` - ugyanaz a migracio
   (`ClientManager.cpp:1006-1031`).
3. `player.offlineshop_shop_items.attr7/attrval7`,
   `offlineshop_safebox_items.attr7/attrval7`, `offlineshop_auctions.attr7/attrval7`
   - ugyanaz a migracio; a mento loop `ITEM_ATTRIBUTE_MAX_NUM=8`-ig jar
   (`ClientManagerOfflineshop.cpp`).
4. `account_mount_inventory.attrtype6/attrvalue6` - a mento loop 0..7-ig jar
   (`MountInventory.cpp:172-195`, `ITEM_ATTRIBUTE_MAX_NUM=8`,
   `item_length.h:28`), de a legacy dumpban es a migracioban csak a 7-es par
   van benne. Ha a 6-os par hianyzik, a mentes SQL hibat dob.
5. Build flag-hez kotott `player` oszlopok - ha az elo DB regi, ezek
   hianyozhatnak (a kod viszont hasznalja oket):
   `gaya`, `part_acce`, `envanter`, `battle_pass_end`, `r0..r19`,
   `skill_victim`, `map1_skillmob`, `change_name`;
   `player_index.hwid`; `player.item.lockedattr`.
6. `guild.trophies` (`ADVANCED_GUILD_INFO`, service.h) es a
   `guild_renewal_*` tablak (`ENABLE_GUILD_RENEWAL_BY_RAZOR93`).
7. `common.locale.mValue`: `SKILL_POWER_BY_LEVEL` sorok hosszu listat
   tartalmaznak (`config.cpp` feldolgozas), ezert `VARCHAR(32)` nem eleg:
   `TEXT` (vagy nagyobb VARCHAR) kell.
8. `item_extra_proto`: a betoltes `SELECT *` (`ClientManagerBoot.cpp:1174`),
   az 5 oszlop neve/tipusa a kodbol nem allapithato meg (schema.sql TODO).

---

## 5. P3 - Bizonytalan oszlopnevek (pozicionalis INSERT)

Ezeknel a kod csak ertekeket ad, oszlopnevet nem. A `db/schema.sql`-ben
`TODO(verify name)` jeloles van; elo DB-vel egyeztetni kell:

- `log.money_log` (4 oszlop), `log.shout_log` (4), `log.fish_log` (8),
  `log.quest_reward_log` (7), `log.dragon_slay_log` (4)
- `player.ishop_data` (csak `SELECT *`, egyedul a `sellCount` nevesitett)
- `player.money_log`, `player.lotto_list`, `player.change_empire`
  (kodbeli nevek/ertekek alapjan)
- `player.new_petsystem` evolution/evocation slot-oszlopok
- `log.cheat_log` (a build flag jelenleg KI van kapcsolva)

---

## 6. P4 - DB-routing bizonytalansagok

A kod tobb helyen nem minositi a tablat, ezert a szerver-kapcsolat donti el,
melyik adatbazisba kerul. Ezeket ellenorizni kell:

| Tabla | Kodbeli kapcsolat | Megjegyzes |
|---|---|---|
| `priv_settings` | `SQL_COMMON` (`ClientManager.cpp:193`) | a schema `common` alatt van; ha az elo DB-ben player, at kell tenni |
| `spam_db` | `AccountDB` = common (`network/db.cpp:581`) | `common` alatt |
| `whisper_system_message` | `DBManager` default = player (`whisper_admin.cpp:184`) | **nem** a log DB-ben van |
| `loginlog`, `loginlog2` | `LogManager` = log DB (`log.cpp:136,322`) | log DB |
| `sms_pool` | Database default slot = player (`ClientManager.cpp:1930`), de a regi dumpban is player | a schema `account` alatt van (TODO) |
| `account_mount_inventory` | GameServer `DBManager` (player kapcsolat, `config.cpp:337`), a migracio viszont `account`-ot minosit | ellenorizni kell a valos helyet |
| `item_proto`, `mob_proto`, `object_proto`, `refine_proto`, `skill_proto`, `item_attr*`, `banword`, `land`, `object` | `CDBManager` default = `SQL_PLAYER` (`DBManager.h:43`) | player DB |
| `locale`, `gmlist`, `gmhost`, `helperlist`, `exp_table` | `SQL_COMMON` / `AccountDB` | common DB |

---

## 7. REPLACE / unique key kockazatok

`REPLACE INTO` unique kulcs nelkul duplikal (MySQL INSERT-kent viselkedik) vagy
véletlenszerűen torol. A kodbol kovetkezo kulcsok:

- `player.item` -> `PRIMARY KEY (id)` (minden REPLACE id-t ad)
- `player.affect` -> `UNIQUE (dwPID, bType, bApplyOn)` (REPLACE + DELETE szures)
- `player.quest` -> `UNIQUE (dwPID, szName, szState)` (REPLACE + DELETE szures)
- `player.player_index` -> `PRIMARY KEY (id)`
- `player.safebox` -> `PRIMARY KEY (account_id)`
- `player.skill_color` -> `PRIMARY KEY (player_id)` (REPLACE)
- `player.horse_name` -> `PRIMARY KEY (id)` (REPLACE VALUES(?,?))
- `player.priv_settings` -> unique `(priv_type, id, type)` (REPLACE ... SET)
- `log.levellog` -> `PRIMARY KEY (name)` (REPLACE INTO)
- `player.item` -> javasolt `UNIQUE (owner_id, window, pos)` az inventory
  duplikacio ellen (a kod ezt nem garantalja)

---

## 8. Ellenorzo SQL az elo DB-n

Futtasd a szerveren, es hasonlitsd a `db/schema.sql`-hez:

```sql
-- oszlopok es tipusok
SELECT TABLE_SCHEMA, TABLE_NAME, COLUMN_NAME, COLUMN_TYPE,
       IS_NULLABLE, COLUMN_DEFAULT, COLUMN_KEY, EXTRA
FROM information_schema.COLUMNS
WHERE TABLE_SCHEMA IN ('player','common','account','log')
ORDER BY TABLE_SCHEMA, TABLE_NAME, ORDINAL_POSITION;

-- indexek / unique kulcsok
SELECT TABLE_SCHEMA, TABLE_NAME, INDEX_NAME, NON_UNIQUE,
       GROUP_CONCAT(COLUMN_NAME ORDER BY SEQ_IN_INDEX) AS cols
FROM information_schema.STATISTICS
WHERE TABLE_SCHEMA IN ('player','common','account','log')
GROUP BY TABLE_SCHEMA, TABLE_NAME, INDEX_NAME, NON_UNIQUE;

-- a P0 tablak ellenorzese kulon
SELECT TABLE_SCHEMA, TABLE_NAME, COLUMN_NAME, COLUMN_TYPE
FROM information_schema.COLUMNS
WHERE (TABLE_SCHEMA='account'   AND TABLE_NAME='account'   AND COLUMN_NAME='coins')
   OR (TABLE_SCHEMA='player'    AND TABLE_NAME='player'    AND COLUMN_NAME IN ('gold','exp','hp','mp'))
   OR (TABLE_SCHEMA='player'    AND TABLE_NAME='item'      AND COLUMN_NAME IN ('window','socket0','socket1','socket2'))
   OR (TABLE_SCHEMA='player'    AND TABLE_NAME='guild'     AND COLUMN_NAME='gold')
   OR (TABLE_SCHEMA='player'    AND TABLE_NAME IN ('item_proto','mob_proto'))
   OR (TABLE_SCHEMA='log'       AND TABLE_NAME='money_log' AND COLUMN_NAME='gold');
```

Alter mintak (csak a fenti ellenorzes utan futtatni!):

```sql
ALTER TABLE account.account MODIFY coins BIGINT NOT NULL DEFAULT 0;
ALTER TABLE player.player      MODIFY gold BIGINT NOT NULL DEFAULT 0;
ALTER TABLE player.guild       MODIFY gold BIGINT NOT NULL DEFAULT 0;
ALTER TABLE log.money_log      MODIFY gold BIGINT NOT NULL;
ALTER TABLE player.mob_proto   MODIFY max_hp BIGINT UNSIGNED NOT NULL DEFAULT 0;
ALTER TABLE player.item        MODIFY window ENUM('RESERVED_WINDOW','INVENTORY','EQUIPMENT','SAFEBOX','MALL','MOUNT_INVENTORY','DRAGON_SOUL_INVENTORY','BELT_INVENTORY','EXTRA_INVENTORY','SWITCHBOT','GROUND','NEW_OFFSHOP','SHOP_SAFEBOX') NOT NULL DEFAULT 'INVENTORY';
```

---

## 9. Fontosabb forrasok

- `SRC/Server/common/tables.h` - SPlayerTable, TPlayerItem, SMobTable,
  SItemTable, TPacketAffectElement, TSafeboxTable
- `SRC/Server/common/length.h`, `common/item_length.h` - meretek
- `SRC/Server/Database/ClientManagerPlayer.cpp` - player/item/affect/quest
- `SRC/Server/Database/Cache.cpp`, `ClientManager.cpp` - item, safebox, ishop
- `SRC/Server/Database/ClientManagerBoot.cpp` - proto tablak
- `SRC/Server/Database/ClientManagerGuild.cpp`, `GuildManager.cpp` - guild
- `SRC/Server/Database/ClientManagerOfflineshop.cpp` - offlineshop
- `SRC/Server/GameServer/core/log.cpp` - log DB
- `SRC/Server/LoginServer/login_auth.cpp`, `login_hwid.cpp` - account
- `SRC/Server/Database/sql/2026_04_15_add_8th_bonus.sql` - ismert migracio
