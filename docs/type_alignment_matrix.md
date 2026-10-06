# Tipus-egyeztetesi matrix (packet / DB / rendszer)

Cel: egyseges tipuskonvencio a szerveren, hogy a packet-structok, az uj DB sema
es a rendszer/ECS tipusok ne utkozenek. A wire (kliens fele meno byte-szelesseg)
NEM valtozik; ahol a DB 64-bites, ott a packet-hataron explicit cast/clamp van.

Reszletek es hibak: `docs/source_type_audit.md`; DB sema: `db/schema.sql`.

## Kanonikus szabalyok

| Adatfajta | C++ tipus | Wire (GC/CG) | DB tipus |
|---|---|---|---|
| Penz (gold/yang/coins/guild bank) | `int64_t` | 4 byte ahol mar az volt (cast/clamp), egyebkent 8 | `BIGINT` |
| XP (player/guild/mob) | `uint32_t` (nagy szerveren `uint64_t`) | 4 byte | `INT UNSIGNED` |
| ID-k (player_id, account_id, guild_id, vnum, war_id) | `uint32_t` | 4 byte | `INT UNSIGNED` |
| Darabszam / level / pont / szazalek | `uint32_t`/`uint16_t`/`uint8_t` | valtozatlan | `INT UNSIGNED` / `SMALLINT UNSIGNED` / `TINYINT UNSIGNED` |
| Socket (item) | `int32_t` | 4 byte | `INT` (signed: -1 es unix ido is) |
| Item attribute erték | `int16_t` | 2 byte | `SMALLINT` (signed) |
| `lockedattr` | `int16_t` | 2 byte | `SMALLINT` (signed) |
| Koordinata / map index | `int32_t` | 4 byte | `INT` |
| Idobelyeg | `int64_t` (uj kod), rovid tavon `uint32_t` | 4 byte ahol mar az volt | `BIGINT` vagy `DATETIME` (NOW() alapu) |
| Win/draw/loss, darabszamok | unsigned | valtozatlan | unsigned |

Packet-hatar (wire marad): ahol a DB/rendszer 64-bites, de a kliens packet
mezöje 4 byte, ott `static_cast<uint32_t>(std::min<int64_t>(value, UINT32_MAX))`
(pl. guild money change, guild info gold, item shop dragon coin).

## Entitas-matrix

### 1. Player (`player.player`)
| Mezö | DB (schema.sql) | Rendszer/C++ | Statusz |
|---|---|---|---|
| `id` | INT UNSIGNED | uint32_t | OK |
| `gold` | BIGINT | TPlayerTable.gold int64_t; PointSystem int64_t | OK |
| `exp` | INT UNSIGNED | TPlayerTable.exp uint32_t | OK (nagy XP -> Phase 2: uint64) |
| `level/level_step` | TINYINT UNSIGNED | uint8_t | OK |
| `hp/mp` | BIGINT UNSIGNED | ECS Health/Mana uint64_t; TPlayerTable uint64_t; PointSystem GetMaxHP/GetMaxSP uint64_t | Javitva: HP/MP uint64 (GC points packet mar int64 volt) |
| `r0..r19` | BIGINT (schema) | long long, `%lld` | OK |
| `battle_pass_end` | DATETIME | uint32_t + FROM_UNIXTIME(%d) | Phase 2: 2038, `%u`/int64 |

### 2. Item (`player.item`)
| Mezö | DB | Rendszer/C++ | Statusz |
|---|---|---|---|
| `id`, `owner_id`, `vnum`, `count` | INT UNSIGNED | uint32_t | OK |
| `socket0..2` | INT | int32_t alSockets[3] | OK (signed szandekos) |
| `attrtypeN` | TINYINT UNSIGNED | uint8_t | OK |
| `attrvalueN` | SMALLINT | int16_t sValue | OK |
| `lockedattr` | SMALLINT | int16_t | OK |
| `window` | ENUM (schema) | uint8_t; kevert `%d`/`'NAME'` iras | Phase 2: mindig nev szerinti write |

### 3. Guild (`player.guild`, `guild_war_bet`, `player_index`)
| Mezö | DB | Rendszer/C++ | Statusz |
|---|---|---|---|
| `guild.gold` | BIGINT | `SGuildData.gold` int64_t (javítva), `GetGuildMoney` int64 | Javítva: 4a9b1446 |
| `guild.win/draw/loss` | INT UNSIGNED | `SGuildData.win/draw/loss` uint32_t (javítva) | Javítva: 4a9b1446 |
| `guild.exp` | INT UNSIGNED | uint32_t | OK |
| `guild.skill_point` | INT | uint8_t (tables.h) | Phase 2: DB INT vs uint8 (packet-hatar) |
| `guild_war_bet.gold` | BIGINT (frissítve) | mapBet int64_t, `str_to_number(int64)` | Javítva: 4a9b1446 + migracio |
| `player_index.pid1..5` | INT UNSIGNED | uint32_t | OK |
| `player_index.hwid` | VARCHAR(64) | char[65] | OK |

### 4. Account / Dragon Coin (`account.account`)
| Mezö | DB | Rendszer/C++ | Statusz |
|---|---|---|---|
| `coins` | BIGINT | `GetDragonCoin`/`SetDragonCoin` int64_t (javítva); item shop ar int64 | Javítva: f54af1d5 |
| charge packet `amount` | - | `SRequestChargeCash.amount` int32 (wire) -> SQL `%lld` cast | Javítva: f54af1d5 |
| `account_mount_inventory.attrtype6` | hianyzik | mento loop 0..7 | Phase 2 migracio (DB audit P2) |

### 5. Safebox (`player.safebox`)
| Mezö | DB | Rendszer/C++ | Statusz |
|---|---|---|---|
| `account_id` | INT UNSIGNED | uint32_t | OK |
| `size` | TINYINT UNSIGNED | uint8_t | OK |
| `gold` | INT UNSIGNED | TSafeboxTable.dwGold uint32_t; GC packet 4 byte | Szandekosan 4 byte (wire); DB BIGINT csak kliens-protokollal egyutt |

### 6. Item shop / Offlineshop
| Mezö | DB | Rendszer/C++ | Statusz |
|---|---|---|---|
| `ishop_data.itemPrice` | INT UNSIGNED | TIShopData.itemPrice uint32_t; szamitas int64 (javítva) | Javítva: f54af1d5 |
| `ishop_log.itemPrice` | INT UNSIGNED | TIShopLogData.itemPrice uint32_t | OK (log) |
| offlineshop `yang`/`illYang` | BIGINT | int64_t, `%lld` | OK |
| `myshop_pricelist.price` | INT UNSIGNED | TItemPriceInfo.dwPrice uint32_t | OK (4 Mrd-ig) |

### 7. Log (`log.money_log`)
| Mezö | DB | Rendszer/C++ | Statusz |
|---|---|---|---|
| `gold` | BIGINT (migracio) | `int64_t`, `%lld` | Javítva (log.cpp mar jo volt) |

## Elvegzett fazisok
- Phase 1a (4a9b1446): guild money int64 + win/draw/loss unsigned + war reward int64.
- Phase 1b (f54af1d5): DragonCoin/coins int64 + item shop ar int64 (integer szazalek).
- Phase 1c (27e50185): `%ld`/`%lu` hordozhatosagi javitasok (FreeBSD/Linux).
- DB: `SRC/Server/Database/sql/2026_10_06_type_alignment.sql`, `db/schema.sql` frissitve.

## Phase 2 (kovetkezo kor)
- `guild.skill_point`, `trophies`, `part_*`, `count` tipusok veglegesitese.
- Packet-structok signedness sweep-je (a kliens `Packet.h`-val parban).
- Idobelyeg 2038: `battle_pass_end`, `premiumTimes`, `last_pay_time`, `deadline`.
- `safebox/myshop` 64-bitesitese csak kliens-protokoll frissitessel.
