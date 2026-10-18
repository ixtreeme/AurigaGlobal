# R7: persisted time and internal packet widths

Event flag values use signed `int64_t` throughout the GameServer/Database API,
maps, SQL parameters, startup reload and setup broadcast. They share the quest
table's existing signed BIGINT column. GM input parses the full decimal integer
and rejects overflow/trailing garbage; flag names must fit the protocol field.
Settings consumed as bounded gameplay integers are clamped at that boundary.
Dungeon anti-spam deadlines keep the full event value.

The bundled Lua 5.0.3 uses double numbers. `game.set_event_flag(name, value)`
accepts integral numeric values in the exact range ±(2^53−1), or a decimal string
for the complete signed 64-bit range. Nonfinite/fractional/unsafe numeric values
and overflowing strings are rejected. Existing `game.get_event_flag` remains a
numeric API; use `game.get_event_flag_string` for an exact decimal result outside
the double's precision. Unix timestamps beyond 2038/2106 remain exact numerically.

Horse health-drop time is unsigned 64-bit in `THorseInfo`, `HorseRuntime`, the
player UPDATE and the row loader, matching `horse_hp_droptime BIGINT UNSIGNED`.
Loading/storing no longer truncates at 2106; generating/checking deadlines no
longer clamps the wall clock to UINT32_MAX. The existing exact-deadline behavior
is retained: the first health loss is after the deadline, not at equality.

Marriage creation time is explicit signed 64-bit in the internal packet, the
Database record and the GameServer couple. The Database INSERT binds the value;
setup/reload preserve it. Wedding start/end queues use unsigned 64-bit seconds.
The marriage bonus bounds elapsed days before narrowing/multiplication and
handles future dates. Wedding expiry copies the running entry before erasing
it, so engagement cleanup cannot read a freed map entry.

Guild/empire privilege durations, deadlines, APIs and queues use explicit signed
64-bit seconds. The GameServer uses a 64-bit clamp rather than the old int-only
MINMAX for timestamps; adding a nonnegative duration saturates at INT64_MAX.
The existing one-week GameServer duration policy remains. All remaining time_t
fields in common/tables.h, including the internal battle-pass player field,
have fixed 64-bit declarations.

The player UPDATE also maps the inactive battle-pass timestamp 0 to SQL NULL
before calling FROM_UNIXTIME. MariaDB's strict mode can reject FROM_UNIXTIME(0)
and otherwise fail the entire character save. Active battle-pass calendar
storage and its existing SQL conversion policy are otherwise unchanged.

## Protocol and deployment boundary

| Packed internal structure | Byte count |
|---|---|
| THorseInfo | 14; health-drop field begins at offset 6 |
| TPacketSetEventFlag | EVENT_FLAG_NAME_MAX_LEN + 1 + 8 |
| TPacketMarriageAdd | 16 + 2 × (CHARACTER_NAME_MAX_LEN + 1) |
| TPacketGiveGuildPriv / TPacketGiveEmpirePriv | 17 / 14 |
| TPacketDGChangeGuildPriv / TPacketDGChangeEmpirePriv | 18 / 15 |

These are existing packed, native-endian server packets. The horse widening
also changes TPlayerTable's internal layout. No client GC/CG packet is widened
by R7. The privilege/marriage/battle-pass widths stay the same on the current
64-bit Windows build but no longer depend on the platform's time_t declaration.
Supported server builds still need a 64-bit OS wall clock; this change does not
make every OS time API or unrelated legacy client timestamp 64-bit.

Rebuild and restart Database, GameServer and LoginServer together with identical
shared headers/features. Drain gameplay and flush old caches before stopping;
do not mix old/new binaries or reuse in-memory player snapshots across versions.
There is no rolling protocol negotiation. The Database's existing GD length
checks use current sizeof values; GameServer now checks the affected fixed DG
payload sizes before dispatch and closes a mismatched Database link.

The checked-in schema already has BIGINT quest values, unsigned BIGINT horse
deadlines and unsigned BIGINT marriage time. Verify the live schema and apply
the existing time migrations if needed as part of R12; no live schema or player
data was changed here.

## Regression coverage

GameServer, LoginServer, Database and all server tests build in normal and ASAN
RelWithDebInfo configurations. All 48 CTest cases pass in each configuration;
the isolated MariaDB 13.0.2 run passes 109 SQL/state checks with each build.

DatabaseTimeTests executes production event load/set/setup, the player UPDATE
builder, marriage hydration/INSERT/setup/wedding expiry, and privilege setup and
expiry over controlled SQL/transport/clock services. It pins packet widths and
rejects shorter/longer versions. Optional `--sql <port> db/schema.sql` runs the
same production writes on temporary tables extracted from the checked-in DDL in
the dedicated loopback `auriga_time64_test` database, reading back signed event
limits, horse deadlines through UINT64_MAX and a marriage time above 2106.

PrivilegeTimeTests executes the real GameServer request and apply paths, with
recorded packet bytes and controlled external services. QuestRuntimeTests uses
the real Lua binding and quest-manager API; GmCommandTests drives the real GM
parser. HorseRuntimeTests and MarriageLifecycleTests cover their native runtime
at 2038, 2106 and 2^40 seconds, plus future dates and signed/unsigned limits.

Live client verification remains dependent on the Unity connection migration.
