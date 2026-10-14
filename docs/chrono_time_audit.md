# Ido- es timer-kezeles -> std::chrono (audit es terv)

Statusz: TERV (2026-10-13). A projekt C++23; a Core mar hasznal chrono-t
(peer_session, peer_auth). Cel: minden idokezeles chrono-alapu, a jelenlegi
adattipus-elteresekbol szarmazo bugok megszuntetese.

## Jelenlegi allapot (adattipusok es domainek)

- `get_global_time()` -> `time_t` (64-bit): `time(nullptr) + global_time_gap`.
  A `global_time_gap` viszont **`int`** (GameServer/core/utils.cpp:9) ->
  `set_global_time` tuneten trunkolhat (elso konverzio puttyolo).
- `get_dword_time()` -> `uint32_t` ms, de **falora alapul**
  (`gettimeofday - get_boot_sec`, Core/utils.cpp:330): NTP/DST/kezi
  oraallitas megzavarja a timer-eket. Domain-keveredes.
- `get_float_time()` -> float, fix offsetes falora (Core/utils.cpp:312);
  NetworkSyncSystem/EntityFactory 3 masodperces sync-ablakhoz hasznalja ->
  monotonnak kell lennie.
- `heart.h`: `struct timeval before_sleep/opt_time/last_time`, `int pulse`
  (32-bites tullfolyas), `int passes_per_sec`.
- `common/cache.h`: TTL `time_t`-vel, faloraval (a cache-nek monoton kell).
- DB sémában vegyes: `DATETIME` (UNIX_TIMESTAMP konverziokkal), `INT
  UNSIGNED` unix-masodperc (marriage.time, horse_hp_droptime,
  upgrade_limit_time), mar migralt BIGINT-ek (guild_renewal last_pay_time).
- Struct mezok vegyesen: `uint32_t dwBattlePassEndTime`,
  `int aiPremiumTimes[]`, `int iPremiumTimes[]`, `uint32_t dwCooltime`,
  `time_t time` (char.h), `time_t duration_sec` (tables.h).
- Quest flag idobelyegek `int`-ben (`QuestSystem::SetFlag(ch, x + cooldown)`).

## Konkret tipus-eltelesek / bugok (talalt helyek)

1. **`%d` + `time_t` DB queryben** (64-bites vararg, `%d` 32 bitet olvas):
   - GameServer/guild/guild.cpp:321
   - GameServer/guild/guild_input.cpp:272
   (`REPLACE INTO guild_invite_limit VALUES(%d, %d)`, get_global_time()).
2. **`time_t` -> `int32_t` szukites** (2038/overflow):
   - 22x `const int32_t now = get_global_time();` (EasterDungeon,
     Halloween2022, LostCastle, Nightmare, Orcs, Pyramid, Rune, Triton,
     Valentine, Viking dungeon-ok).
   - `DragonSoul.cpp:1054`: `const int32_t remainingDelay = ... -
     get_global_time();` (+ `"%d"` log).
   - `questlua_dungeon.cpp:1382,1427`: `int32_t time = get_global_time() -
     enter_time;`.
3. **`time_t` -> `uint32_t`**:
   - `SkillSystem.cpp:1160`: `uint32_t nextTime = get_global_time() + ...`
   - `char_manager.cpp:702,704`: `static uint32_t s_nextTime`/`curTime`
   - `ItemSystem_LegacyBridge.cpp:8075`: `uint32_t dwNowSec`
4. **Wire format**: `input_db.cpp:943,1239`: `set_global_time(*(time_t*)
   data)` - platform-fuggo `time_t` a packetben; int64 kell.
5. **`global_time_gap` int** (fenti 1-2. pont forrasa).
6. **Domain-keveredes**: 14 fajlban van egyszerre `get_dword_time` es
   `get_global_time`/`time(nullptr)`; a `get_dword_time` falo-fuggo.

## Terv

- **Phase 1 (KESZ lesz ez a commit)**: `Core/Time.h` chrono helperek;
  `get_dword_time` monoton (`steady_clock`), `get_float_time` monoton,
  `get_global_time`/`set_global_time` chrono + `int64_t` gap; a fenti
  1., 2., 3., 5. pont javitasa; `common/cache.h` monoton chrono TTL-re.
- **Phase 2**: `heart.h/.cpp` chrono (steady_clock + duration a timeval
  helyett, int64 pulse); event/`PASSES_PER_SEC` atvezetes chrono
  helperekre; `input_db` time wire int64 + protokoll verzio.
- **Phase 3**: DB hatar: ido-oszlopok egysegesitese (minden idobelyeg
  BIGINT unix-sec vagy DATETIME, de konzisztensen), `tables.h`/`char.h`
  ido mezok int64/time_point; quest flag idok int64.
- **Phase 4**: lokalizalt idoformatum helperek (chrono + std::format),
  `localtime`/`strftime` helyett; teszt/audit, hogy ne maradjon
  `time_t`->szukitó cast.

## Phase 1 reszletei

- `Core/Time.h`: `auriga::time` - `WallClock` (system_clock),
  `SteadyClock`, `UnixSeconds()`, `FromUnixSeconds()`, `SteadyMillis()`,
  `LocalTime(time_t)` (thread-safe, ertek szerint).
- `get_dword_time()`: `steady_clock`-alapu ms (a timer-ek tovabbra is
  uint32-t hasznalnak a wire/strukturakban; a monotonitas a lenyeg).
- `get_float_time()`: `steady_clock` masodperc float-kent (sync ablakok).
- `get_global_time()`: `system_clock` masodperc + `int64_t` gap;
  `set_global_time` ugyanigy (nincs tobb `int` trunkolas).
- cache: `steady_clock::time_point/duration`.
- A szukito helyek `time_t`/`int64_t`-re; `%d` -> `%lld` + cast a
  guild querykben; `"...%d"` -> `"%lld"` a DragonSoul logban.


## Phase 1 statusz (KESZ, 2026-10-13)

- Uj: `Core/Time.h` (`auriga::time`): `WallClock`/`SteadyClock`,
  `UnixSeconds()`, `FromUnixSeconds()`, `ProcessMillis()`,
  `ProcessSeconds()`, `LocalTime(time_t)` (thread-safe, ertek szerint).
- `get_dword_time()` mar **monoton** (`steady_clock` ms a process
  indulasa ota) - a falora ugralasa (NTP/DST/kezi oraallitas) nem
  zavarja tobb a timer-eket; `get_boot_sec()`/gettimeofday kivezetve.
- `get_float_time()` mar monoton masodperc (sync ablakok).
- `get_global_time()`/`set_global_time()`/`init_gayaTime()` chrono
  alapon; a `global_time_gap` **int64_t** (elotte `int` - trunkolas
  forrasa volt).
- `common/cache.h`: TTL `steady_clock` duration/time_point; uj
  `GetLastUpdateAgeSeconds()`; a Database `CPlayerTableCache`
  `GetLastUpdateTime()` (uint32 falora) helyett az age szamitast
  hasznalja (`logoff_interval`), a Cache.cpp `m_expireTime`/`time(nullptr)`
  atirva chrono-ra.
- Javított tipus-eltelesek:
  - 22x `const int32_t now = get_global_time();` -> `time_t` (dungeonok),
  - `DragonSoul.cpp` remainingDelay int32 -> int64 (+ `"%lld"` log),
  - `questlua_dungeon.cpp` ket `int32_t time` diff -> int64,
  - `SkillSystem.cpp` `uint32_t nextTime` -> `time_t` (a
    `SetSkillNextReadTimeCapped` time_t parametert var!),
  - `char_manager.cpp` `s_nextTime`/`curTime` -> time_t,
  - `ItemSystem_LegacyBridge.cpp` dwNowSec -> int64 (+ `"%lld"` format),
  - `guild.cpp`/`guild_input.cpp`: `%d` + time_t -> `%lld` + cast (a
    guild_invite_limit query 64-bites erteket kap),
  - `cmd_general2.cpp` es `New_PetSystem.cpp`: `%d` vararg 64-bites
    idokifejezessel -> explicit `(int)` cast.
- Build: GameServer Debug + Release, Database Debug + Release, LoginServer
  Release - zold.

## Hatralevo (Phase 2-4)

- heart (timeval -> chrono, int64 pulse), event/`PASSES_PER_SEC`.
- `input_db.cpp` ido-wire (time_t a packetben) -> int64 + protokoll bump.
- DB ido-oszlopok egysegesitese (INT UNSIGNED unix-sec -> BIGINT), a
  `tables.h`/`char.h` ido mezok (uint32/int/time_t vegyes) int64-re.
- Quest flag idobelyegek int32 -> int64.
- LoginServer `time_t` hasznalatok atnezese; `localtime`/`strftime`
  helperek chrono-ra (Time.h `LocalTime` mar ad alapot).


## Phase 2 statusz (KESZ, 2026-10-13)

- **heart (Core/heart.h/.cpp)**: a `struct timeval` mezok helyett
  `std::chrono::steady_clock::time_point` / `microseconds`; a
  `heart_idle` mar chrono durationokkel szamol, a varakozas
  `std::this_thread::sleep_for` (a `thecore_sleep(timeval*)` helper
  torolve - mar nem volt hivas). A tick-utemezes igy monoton.
- **Ido-wire (BOOT packet)**: a Database `time_t` helyett explicit
  `int64_t`-et kuld (`ClientManager.cpp` packet-meret + Encode), a
  GameServer `memcpy`-vel olvassa (`input_db.cpp` BOOT es `CInputDB::Time`).
  A korabbi `*(time_t*)data` platformfuggo meret + nem igazitott
  pointer-dereference volt.
- **Takaritas**: `thecore_sleep` deklaracio + ket definicio torolve;
  `get_boot_sec`/`gettimeofday` alapu `get_dword_time` mar Phase 1-ben
  kivezetve.
- Build: GameServer/Database/LoginServer Debug + Release zold.

### Megjegyzes a pulse-okhoz

A `heart.pulse` es a `thecore_pulse()` tovabbra is `int` (a hivashelyek
szama es a struktura-mezok miatt); a hasznalat wrap-turo (`diff` es
`% passes_per_sec`), a ~2.7 eves tullfolyas elmeleti (signed overflow).
A Phase 3-ban a pulse-szamlalok es az ECS pulse-mezok int64-re
egysegesithetok.


## Phase 3A statusz (KESZ, 2026-10-13)

- **localtime/gmtime/asctime teljes kivezetese**: minden hivas
  `auriga::time::LocalTime` / `UtcTime` (thread-safe, ertek szerint) +
  `strftime` a tm ertekre. Erintett: Core/log.cpp (2x), Core/utils.cpp,
  Database/ClientManager.cpp (8 blokk + UsageLog asctime), battle_pass.cpp,
  PlayerRuntimeSystem.cpp, char_manager.cpp, desc.cpp, questlua_global.cpp
  (`time_to_str` mar gmtime helyett UtcTime + strftime buffer),
  LoginServer/login_auth.cpp, debug_allocator_adapter.h.
- **shutdown_manager**: a `struct tm* m_pTime` (localtime statikus bufferere
  mutato pointer!) helyett `std::tm m_time` ertek; az `UpdateTime()` chrono
  alapu LocalTime-nal tolti.
- A `Core/Time.h` uj `UtcTime(time_t)` helpert kapott.
- Build: GameServer/Database/LoginServer Debug + Release zold.

### Hatralevo (Phase 3B)

- **Premium idok**: `TPlayerTable::aiPremiumTimes`/`CLoginData::m_aiPremiumTimes`
  `int[8]` (login-oldali memcpy-val) -> int64 (mindket oldalon: GameServer
  network/login_data + Database/LoginData + ECS premium tarolo).
- **Battle pass lejarat**: `dwBattlePassEndTime` uint32 ->
  `time_t`/int64 (TPlayerTable, ECS `BattlePassTiming::deadline`,
  SessionSystem mentes, EntityFactory); a Database `FROM_UNIXTIME(%u)` /
  `UNIX_TIMESTAMP` konverziok `%lld`-re.
- **DB migracio**: idobelyeg-oszlopok INT -> BIGINT ahol unix-masodperc
  (pl. `marriage.time`, `horse_hp_droptime`, `upgrade_limit_time`), a
  `player.playtime`/premium oszlopok egyeztetese a C++ tipusokkal.
- **Quest flag idobelyegek**: `QuestSystem::SetFlag/GetFlag` int32 ->
  int64 (lua-hatarok atnezese).
- **Pulse**: `heart.pulse`/`thecore_pulse()` int64-re az ECS pulse-mezokkel
  egyutt (a Phase 2-ben dokumentalt tullfolyas miatt).
- **LoginServer** tovabbi `time_t` hasznalatok atnezese (login_limiter).


## Phase 3B statusz (KESZ resz, 2026-10-13)

- **Battle pass lejarat int64**: `TPlayerTable::dwBattlePassEndTime` uint32 ->
  `time_t`; az ECS `BattlePassTiming::deadline` es az
  `AffectSystem::Get/SetBattlePassDeadline` is `time_t`; a Database mentes
  `FROM_UNIXTIME(%u)` -> `%lld` (+ cast). Igy a `SetBattlePassDeadline(e,
  get_global_time() + X)` hivasoknal nincs tobb uint32 szukites.
- **Pulse int64**: `heart.pulse`, `thecore_pulse()` es a `heart_new`
  inicializalas (pulse=0) int64 - a ~2.7 eves signed overflow megszunt;
  a fogyasztok (`% passes_per_sec`, diff-ek) valtozatlanul mukodnek, a
  struktura-mezokbe valo castolas megmaradt ahol szukseges.
- Build: GameServer/Database/LoginServer Debug + Release zold.

### Megjegyzes a TPlayerTable wire-hez

A `TPlayerTable` a GameServer <-> Database kozott `sizeof(TPlayerTable)`
alapu memcpy-val megy (PLAYER_SAVE/PLAYER_LOAD); a meret valtozasa miatt
a ket binari **egyutt** telepítendo (a PLAYER_SAVE oldal meret-ellenorzest
is dob, ha elter). A BOOT protokoll `bPacketVersion` emelese javasolt a
kovetkezo deploy-nal.

### Hatralevo (Phase 3C)

- **Premium idok**: `iPremiumTimes[8]` `int` a LoginServer <-> Database
  packetben (`login_db_link.h`), a Database `LoginData`-ban es a
  `TPlayerTable::aiPremiumTimes`-ben -> int64 (LoginServer, Database es
  GameServer egyutt; a LoginServer wire miatt kulon protokoll-egyeztetes).
- **DB migracio**: unix-masodperc tipusu INT oszlopok -> BIGINT
  (`marriage.time`, `horse_hp_droptime`, `upgrade_limit_time` stb.), a
  `player.playtime` es premium oszlopok egyeztetese.
- **Quest flag idobelyegek**: `QuestSystem::SetFlag/GetFlag` int32 -> int64.
