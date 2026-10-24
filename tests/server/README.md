# Server ECS regression tests

## Server audit fixes (2026-10-18)

CTest is enabled at the project root. With Python available, 53 server tests
are registered. The `ServerTests` target builds all native server test
executables, including the ones excluded from the default build;
each registered test carries the `server` label and a 60-second timeout.

```powershell
cmake --build build --config RelWithDebInfo --target GameServer LoginServer Database ServerTests --parallel 2 -- /p:CL_MPCount=2
ctest --test-dir build -C RelWithDebInfo -L server --output-on-failure --parallel 4
cmake --build build-asan --config RelWithDebInfo --target GameServer LoginServer Database ServerTests --parallel 2 -- /p:CL_MPCount=2
ctest --test-dir build-asan -C RelWithDebInfo -L server --output-on-failure --parallel 4
```

## Shared server configuration (2026-10-19)

Database, LoginServer and named GameServer instances load one `server.json`.
The example, migration and startup commands are documented in
[server_configuration.md](../../docs/server_configuration.md).

`ServerConfigTests` executes the production resolver and all three typed
readers. It covers shared settings, recursive overrides, replaced arrays,
absolute paths, isolated working directories, atomic failure/reload,
duplicate keys, port collisions, DB-link/key consistency and numeric bounds.
Error assertions verify that supplied passwords stay out of diagnostics.

`server_config_tools` checks the Python converter and launcher. To include
actual executable preflight from an unrelated working directory, run:

```powershell
python -B tests/server/ServerConfigToolsTests.py --bin-dir build -v
python -B tests/server/ServerConfigToolsTests.py --bin-dir build-asan -v
```

This also checks visible launcher success/failure output and that preflight
creates no runtime files. `CheckLoginStartup.ps1` now supplies the central
configuration format; refused and silent SQL still abort startup before TLS
or a listener can start. These checks do not replace a full game boot with
the downloaded data, certificates and restored database.

Verified on Windows 2026-10-19: all three normal/ASAN server binaries built;
CTest passed 51/51 in each build. ServerConfigTests passed 60 checks, and
the explicit Python run including native binaries passed 9/9 in each build.
Database's ASAN post-build now copies the same runtime DLL as the other two
servers, allowing preflight without a developer-toolchain PATH.

## Server2 package and lifecycle (2026-10-20)

[server2_package.md](../../docs/server2_package.md) describes the generated
localhost distribution and its limits. `ServerPackageCheck` uses the production
JSON reader, quest metadata reader and linked Lua parser from a foreign cwd.
`server_package_tools` runs 18 converter/ownership/native/lifecycle cases,
including real subprocess startup, ordered token-based graceful shutdown,
startup rollback and exclusive supervisor locking. Normal and ASAN CTest
passed 53/53; the expanded Python cases passed 18/18 with both checkers.
The file-only `prepare` path creates runtime defaults without SQL or service
startup and preserves existing runtime content. Archive cases check ZIP CRCs
and SHA256, exclude mutable runtime files, and reject changed assets or an
existing output archive before writing it.

`MotionTimingTests` uses the real motion getters with millisecond DB payloads:
normal/skill animation durations are returned in seconds, movement speeds
retain px/s, and invalid durations become unknown. ServerConfigTests also
rejects mismatched shutdown PID/token pairs and consumes only the owned request.

## Qt6 server manager (2026-10-20)

`ServerBackupTests.py --backup <adapted folder> --mariadb-bin <bin>` passes 8
tests including actual MariaDB import, all-table count/checksum verification,
nonempty-database refusal and automatic rollback after injected partial failure.
The source schema additionally passes 365 SQL prepare/execution checks and 175
invariants on MariaDB 11.4.13. Package tools now have 21 tests, including headerless
Dragon Soul row preservation and bundled-runtime/existing-data guards. Windows
venv lifecycle fixtures use the base interpreter so child PIDs match native processes.

`ServerManagerSshTests.py --gui` covers the remote-connection page:
real OpenSSH configuration parsing for separate ProxyJump identities, strict
host trust, shared SSH/SFTP routing, loopback-only forwards, profile locking,
and Qt child-process failure, timeout and cleanup. All network fixtures use
loopback; no production host is contacted. `server_manager_ssh` runs the
Qt-independent portion under CTest. FreeBSD 15.1 execution, production service
management and deployment remain separate integration work.

`ServerManagerPasswordTests.py` covers migration of old key-only profiles,
memory-only credentials, authenticated askpass requests, cleanup and rejection
of missing passwords or unrelated prompts. Its `--gui` mode checks the masked
password field and profile save. `--native --gui` additionally uses a disposable
Paramiko loopback server with the actual OpenSSH and SFTP clients: different
jump/target passwords, UTF-8 passwords, untouched RPC stdin, batch upload,
local forwarding, Qt probe, wrong password and strict host trust. First-contact
coverage includes exact SHA-256 approval and persistence, rejection before
authentication, changed-key refusal, both ProxyJump hops, SFTP without a prior
probe, a real CMake/Ninja worker build over SSH and the Qt confirmation with
paused connection timeout. Closing a pending approval must not leave a late
dialog or accept the host. Paramiko is a
test-only dependency; the installed GUI continues to use Windows OpenSSH.
PAM tests advertise only keyboard-interactive authentication, exercise both
FreeBSD password prompts, SSH RPC reconnection, Qt probe, first-contact SFTP,
separate ProxyJump credentials and the real build worker. OTP/password-change
prompts must receive no stored password; repeated challenges or fallback
between password/PAM cannot resubmit the secret within one SSH process.
`--askpass-exe <AurigaServerManager.exe>` tests the frozen helper too.
`server_manager_password` runs the dependency-free checks under CTest.
Remember-password coverage includes an actual Windows Credential Manager
write/read/delete with a unique temporary credential, restoration in a fresh
interpreter and the frozen worker, checkbox save/clear, profile deletion,
endpoint changes, absent plaintext profile secrets and rollback on profile-write
or vault failure. Existing key-only and session-only profiles default to no
persistent credential storage.
Password-mode UI checks also require inactive private-key/identity fields to
be disabled and hidden; invalid hidden drafts cannot block profile saves.
Saving an active profile with a changed route leaves it disconnected until
explicit connection, without SSH or local key-file checks during the save.

Remote build preflight tests cover the FreeBSD non-interactive PATH, missing
Ninja stopping before CMake with an installation hint, recovery from a cached
NOTFOUND build tool, explicit tool overrides and generator conflicts without
cache deletion. The Qt status must display the concrete worker error.
`SystemDependencyTests.py` configures the production system-dependency module
with isolated SDK fixtures: classic MySQL headers, recovery from an empty old
cache, explicit include overrides and MariaDB forwarding headers. It requires
CMake and Ninja, without a C/C++ compiler or a database process.
It also checks that an absent nlohmann JSON SDK fails configuration with the
FreeBSD package command, before any C++ compilation begins.

[server_manager.md](../../docs/server_manager.md) covers the packaged PySide6
application. `server_manager_tools` exercises file ownership, optimistic edit
conflicts, exact backups, failed-validation rollback, native configuration/Lua
checks, encoding preservation and the guarded GUI installer. The explicit run
with `--package D:/AurigaGlobal/Server2-Local` passed 18/18 on disposable fixtures.
The normal CTest invocation omits that optional native integration case.

With the Qt environment, `ServerManagerGuiTests.py --package <release>` passed
11/11: masked forms, real keyboard editing and asynchronous save, rejected port
collision, typed tables, JSON view/row operations and Latin1/CRLF Lua editing.
They also exercise sparse inherited overrides/reset, invalid draft retention,
save-while-typing and toolbar visibility at 1100 px with long file names. These tests
copy the release first and never initialize MariaDB. Frozen executable workers
also pass all 15 native configuration checks, data parsing and TLS checks.
GUI updates preserve operator edits to config/data and their original manifest
hashes, back up replaced GUI files, and reject externally changed GUI binaries.

## Server audit coverage

SpatialLifecycleTests now executes the production `CInputMain::Move` with a real
descriptor object over controlled transport/services, then the real movement
tick. Destination publication belongs to the movement command. Active timing
retains its start, duration and origin until arrival; interpolation uses elapsed
milliseconds, including counter rollover. Tests cover identical timestamps,
fine/coarse update cadence, lagged arrival, point/mount speed budgets and the
existing spatial/reentrancy contracts. Older spatial fixtures now supply an
explicit time interval instead of relying on a fixed distance per update.

LoginAuthTests covers one attempt per session through both the account lookup
and the Database acknowledgement. SQL failure, registration refusal, offline
link and disconnect release the pending account and session reference; late
results cannot authenticate a closed connection. LoginDbLinkTests exercises
real loopback framing with unique echoed request handles, out-of-order results,
unknown/duplicate results, malformed ACKs, disconnect and a ten-second
registration deadline. No new wire fields are introduced: the existing GD/DG
handle identifies the request.

LoginLimiterTests checks that an existing session never exempts new connections
from the burst block, and that the per-address cap of 128 simultaneous login
sessions is released on close. Existing sessions are retained. The pre-existing
30 connections per ten-second window and credential locks remain in effect.

CharacterSelectionTests covers all 256 byte-sized character indices, empty and
non-renamable slots, authentication, unterminated names and the name policy over
the production outgoing rename-request builder. PlayerLoadTests executes the
same secondary-skill normalization the Database uses: old and new skill costs
are added, caps and low levels are retained, and repeated loads do not refund
spent points. These tests use controlled services; live SQL, client rendering
and production deployment still require integration verification.

## Source-derived MariaDB schema (2026-10-18)

The fresh-install schema and deployment instructions are in
[db/README.md](../../db/README.md). It contains 102 tables across four databases.
The disposable Windows runner needs MariaDB binaries, Python 3.9+ and a matching
Connector/C library:

```powershell
& ./db/tools/TestSchema.ps1 `
  -MariaDBBin 'C:/Program Files/MariaDB 13.0/bin' `
  -ClientLibrary 'C:/vcpkg/installed/x64-windows/bin/libmariadb.dll' `
  -Python 'C:/path/to/python.exe'
```

It creates a TEMP data directory and random loopback port, verifies ownership,
runs the validator and stops its own process, retaining logs and a JSON report.
The validator inventories active C++ SQL and reviewed dynamic assemblies with
source hashes; it is not a C++ compiler. MariaDB 13.0.2 passed 365 prepared and
executed SQL templates plus 168 data/layout invariants. Two SJIS runtime-locale
queries are explicitly excluded from the UTF8 profile.

For native SQL modes, first create auriga_r1_r4_test and auriga_time64_test on a
separate loopback test instance with an empty root password. Run
PlayerCreateTests and DatabaseTimeTests with --sql <port> db/schema.sql; both
use temporary tables. Their extractors support the schema's table options.
They passed 136 and 109 checks respectively, in normal and ASAN builds. The
character test intentionally invalidates a prepared statement to check recovery;
the resulting 1243 error log is expected. These modes are separate from CTest.

DDL validation does not supply game reference content or replace full-server,
restart/persistence or Unity client tests. db/check_ready.sql reports missing
reference data. Exact coverage limits are in the schema audit.

## Quest metadata JSON R8 (2026-10-18)

QuestDataTests executes the typed production reader, including missing fields,
integer bounds/types, directory identifiers, duplicate records/JSON keys and
failure of the second file without partial publication. QuestRuntimeTests also
drives real manager initialization and reload with controlled Lua/NPC services:
the reserved notarget entry, name resolution, category defaults and cached
lookups, successful removal of stale mappings and preservation of live state
when new metadata is invalid.

The converter suite checks actual CLI behavior and complete record parity with
the same C++ consumer. Run it after building the reader, with each configuration:

```powershell
python tests/server/QuestDataConversionTests.py --reader build/SRC/Server/GameServer/RelWithDebInfo/QuestDataTests.exe -v
python tests/server/QuestDataConversionTests.py --reader build-asan/SRC/Server/GameServer/RelWithDebInfo/QuestDataTests.exe -v
```

The schema, conversion/deployment instructions and unchanged Lua/client boundary
are documented in [server_quest_json_contract.md](../../docs/server_quest_json_contract.md).
The MSBuild `/p:CL_MPCount=2` above bounds compiler memory use on Windows; omit
the arguments after `--` for non-MSBuild generators.

## Persisted time and fixed packet widths R7 (2026-10-18)

Event flags use int64_t in the GameServer/Database API, maps and packets; horse
deadlines use uint64_t through runtime, DB snapshot, UPDATE and row load.
Marriage and privilege time fields have fixed widths. GameServer privilege
deadlines use a 64-bit clamp, replacing an int-only MINMAX that truncated them.
The wire sizes and coordinated deployment requirements are documented in
[server_time64_contract.md](../../docs/server_time64_contract.md).

DatabaseTimeTests runs the real Database event load/set/setup, player UPDATE
builder, marriage load/INSERT/setup and wedding queues, plus privilege setup and
expiry. Its SQL/transport/clock services are controlled in CTest. An optional
real SQL mode uses temporary tables from the checked-in schema in the dedicated
auriga_time64_test database:

```powershell
& build/SRC/Server/GameServer/RelWithDebInfo/DatabaseTimeTests.exe --sql <port> db/schema.sql
& build-asan/SRC/Server/GameServer/RelWithDebInfo/DatabaseTimeTests.exe --sql <port> db/schema.sql
```

It checks signed event limits, the production horse UPDATE through UINT64_MAX
and a marriage INSERT beyond 2106. PrivilegeTimeTests runs the actual GameServer
request/apply paths with controlled leaf services, checking wide timestamps,
week-long caps and overflow-safe addition. Both suites pin packet widths;
GameServer rejects affected fixed DG payloads with the wrong byte length before
casting them. Database already checks the corresponding fixed GD lengths.

QuestRuntimeTests exercises the real Lua event bindings and map, including
exact decimal strings for values outside the bundled Lua 5.0.3 double precision,
and rejects invalid or unsafe values. Numeric timestamps beyond 2038/2106 still
work; game.get_event_flag_string provides an exact full-width result.
GmCommandTests checks full-range decimal GM input and bounded rate settings.
HorseRuntimeTests and MarriageLifecycleTests cover 2038/2106/2^40-second dates,
deadline equality, future dates and long elapsed intervals. These are server and
protocol checks; live Unity client verification remains pending its connection
migration.

## Event-driven network publication R6 (2026-10-18)

The disabled `NetworkSyncSystem_Update` prototype is removed. State owners
explicitly publish committed changes; `DirtyTag` is a generic mutation marker,
not a network queue. The packet ownership, recipient and ordering contract is
documented in [server_network_publication.md](../../docs/server_network_publication.md).
Appearance UPDATE now uses the same spatial-revision-checked PacketView dispatch
as movement and point broadcasts.

EntityNetworkDispatchTests captures the actual packet builders and transport
selection. A moving player publishes exactly one appearance UPDATE to itself
and its nearby observer, with all compiled appearance parts, both affect words
and skill colors, without MOVE/ADD/full-points replay or timing changes. A
full-points refresh preserves all 255 signed 64-bit fields and reaches only the
owner. Missing sessions, retired sources and a spatial-revision change between
recipients are covered. PointCalculationTests additionally captures the real
PointSystem::Change HP/SP delta bytes, checks commit-before-send, 64-bit values,
private/explicit-broadcast selection and the amount-display policy.
GmCommandTests validates every skill level in the real owner's skill snapshot,
including the final slot, and checks unloaded/session/lifetime gates. Existing
affect, inventory and movement fixtures exercise their state-owner operations.

Packet transport and surrounding services are controlled in these fixtures.
Two-client rendering, equipment/affect changes while moving, skill colors,
HP/SP target/party UI and logout/relog persistence remain the R12 integration
checks. These live checks depend on completing the Unity client's connection
migration so it can connect to the current server. Until that prerequisite is
ready, continue the client-independent server/database automation and keep the
live client results pending.

## Follow-up fixes R1-R4 (2026-10-18)

LoginByKeyTests runs the same credential validator as Database's
QUERY_LOGIN_BY_KEY: normalized account names must match by content, both keys
must match, and every fixed string must terminate within its field. Invalid
credentials are rejected before the already-online lookup or account queries.

PlayerCreateTests checks the production INSERT builder's column/parameter
mapping, signedness, 64-bit HP/SP/gold, initial appearance, optional columns and
binary skills/quickslots. The Database now binds every inserted value through
the prepared protocol. An explicit SQL mode creates a temporary player_test
table using the player table definition in db/schema.sql and reads back every
inserted field through the production CAsyncSQL implementation. It then kills
only its own test connection and verifies a cached prepared query can recover:

```powershell
# Start an isolated local MariaDB, create the dedicated auriga_r1_r4_test DB;
# the test uses loopback, the supplied port, root and an empty test password.
& build/SRC/Server/GameServer/RelWithDebInfo/PlayerCreateTests.exe --sql <port> db/schema.sql
& build-asan/SRC/Server/GameServer/RelWithDebInfo/PlayerCreateTests.exe --sql <port> db/schema.sql
```

PacketStringTests checks the production fixed-string gate for LOGIN/LOGIN2/3,
create/delete/rename, both versions, whisper, quest, guild, hack, shop, pet and
admin whisper packets. Tests use exact heap allocations without any terminator,
last-byte terminators and every shorter packet length. The normal packet
processor runs this gate before dispatching client handlers; variable-payload
framing and semantics remain the handlers' responsibility. Bounded
trim_and_lower also handles unterminated/all-whitespace and overlapping input.
Offline-shop create/rename subpacket handlers validate their name arrays after
the existing payload-length check; filter names already use explicit lengths.

LoginAuthTests now covers ten-second account/registration deadlines, immediate
detach on close, a replacement attempt, and late callbacks after destruction.
Callbacks hold weak attempt tickets rather than retaining a raw session.
LoginSessionTests uses real TLS sockets to check silent application handshakes
and missing LOGIN3: twenty seconds from accept and thirty seconds from the
completed handshake, respectively. Failed/completed authentication also starts
a thirty-second session deadline. Partial packets and retries do not extend it.

AsyncSqlPreparedTests scripts delayed successful/failed initial SQL connections
and a lost connection followed by a successful queued query. Login SQL uses
five-second connector connect/read/write timeouts, waits up to ten seconds for
actual readiness, completes failed queries instead of retrying indefinitely,
and discards unstarted work on shutdown. SQL and HWID initialization failures
abort LoginServer before listening. The logger outlives SQL cleanup on all exits.
On Windows, `CheckLoginStartup.ps1 -Binary <LoginServer.exe>` additionally runs
the actual server with a refused SQL port and a silent local TCP endpoint. Both
must exit with code 1 within twelve seconds, before TLS/listener initialization;
this also detects a crash during SQL cleanup after logger shutdown. Run it for
the normal and ASAN executables. It keeps its isolated logs in the temp folder.
The tests do not establish full login/create/select/gameplay integration or
production schema compatibility; those remain in the release verification work.

## Login key and limiter hardening R5 (2026-10-18)

LoginAuth defaults to OpenSSL RAND_bytes, retaining the nonzero 31-bit key
range. Zero samples are rejected; retries are bounded. A provider error or
eight zero results completes the attempt with SHUTDOWN, without registering or
issuing a key. LoginAuthTests injects failed, partially written and zero-only
providers, verifies pending-state release and successful retries, and checks
the Database acknowledgement still precedes key delivery. Normal success cases
also execute the real OpenSSL provider.

LoginLimiter now uses steady_clock for every window, lock and retention time.
Unused host/account state expires after fifteen minutes. Collect runs from the
listener's regular game-loop collection and inspects at most 256 entries per
map per call; it retains live sessions and unexpired locks. Last-use lists
keep lookup/touch and cleanup constant-time on average without full map scans.
Each map holds at most 65,536 entries. At capacity, unknown entries are refused
until expired state is reclaimed; protected state is never evicted to admit
them. An account entry is reserved before SQL, so admitted failures remain
trackable even if concurrent requests fill the remaining capacity.

LoginLimiterTests uses deterministic monotonic timestamps, including epoch
zero and exact window/lock deadlines. It covers escalation, bounded incremental
cleanup, refreshed entries, live-session counts, both full maps and recovery.
LoginAuthTests separately saturates the account map and verifies an unknown
login is blocked before SQL while an already tracked account can continue.
Connection and credential thresholds remain unchanged. Rebuild LoginServer,
LoginAuthTests, LoginLimiterTests and LoginSessionTests in both configurations;
the existing full server CTest commands include all of these checks.

## Native buildings and offline-shop world objects

Buildings and offline-shop avatars no longer allocate CObject/ShopEntity or
inherit CEntity. Their state and relationships are components; land/manager and
city indices hold versioned entities. The old entity.h, LPENTITY/LPOBJECT aliases
and object-to-entity conversion are removed. Existing entity.cpp retains only
the native view-packet operation, so no parallel implementation is introduced.

BuildingLifecycleTests executes production building.cpp and its registry against
controlled spatial, NPC and guild services. It covers stored boot coordinates,
show/delete, DB-ID versus VID indices, collisions, invalid maps/prototypes,
construction interruption, replacements during removal, guild bonus cleanup,
NPC ownership and exceptions. It does not execute live terrain, guild storage,
quests or DB transport.

SpatialLifecycleTests links both production world registries and the real
sectree/visibility path. Shop creation commits membership before publication;
destruction retires identity before callbacks. Tests cover partial construction,
recycled handles, rename visibility, final-retirement respawn rejection, failed
removal retries and reentrant batch cleanup that retains unfinished ownership.
EntityNetworkDispatchTests checks actual building/shop INSERT/REMOVE bytes at
the descriptor boundary, including missing state/session and retired handles.

Run building_lifecycle, spatial_lifecycle and entity_network_dispatch in normal
and AddressSanitizer configurations, plus the full server regression suite after
removing the shared CEntity header. In-game checks still need guild-building
boot/construction/reconstruction/deletion, collision regions and NPCs, guild
member bonuses, and offline-shop appearance/rename/click/closure in each city.
No live server restart or binary deployment is part of these isolated tests.

## Mount visibility and packet-boundary coverage

SpatialLifecycleTests exercises factory-shaped players and followers through the
real MovementSystem::Show, native sector membership, Goto, cross-sector movement
ticks and ViewCleanup. Two successive follower generations must publish movement
and removal to both the owner and a nearby player. Gameplay leaves and packet
dispatch remain controlled seams in this fixture.

EntityNetworkDispatchTests separately links the real EntityNetworkDispatch and
NetworkSyncSystem packet builders, capturing bytes at DESC::Packet. It checks
ADD/AdditionalInfo/MOVE/WALK/DEL identities and lengths, ridden-player mount
updates, expired movement and missing sessions. Socket delivery, encryption and
the running client are outside these tests; passing them does not establish that
a reported live ghost-mount problem is fixed.

The packet fixture also exercises real UpdatePacket broadcasts to two clients.
An active follower must send UPDATE only, not standalone AdditionalInfo (the
client interprets that as completion of its cached ADD). After visibility removal,
updates must send nothing even while the VID and sector membership still exist,
as they do during DestroyCharacterStatePre -> ClearAffect. Before the fix this
case captured UPDATE plus AdditionalInfo after DEL, which can recreate a ghost
at the cached spawn position. Alignment changes also use UPDATE, never the
append-only AdditionalInfo packet. Live follow/remount and monster AI still need
in-game verification; these tests do not simulate the running renderer or AI.

Build SpatialLifecycleTests and EntityNetworkDispatchTests, then run ctest with
`-C RelWithDebInfo -R "spatial_lifecycle|entity_network_dispatch" --output-on-failure`
in the normal and AddressSanitizer build trees.

EntityNetworkDispatchTests also links entity.cpp's real PacketView. A MOVE
payload must reach owner and observer unchanged, exclude its source when asked,
skip missing sessions and stop an old recipient snapshot after a spatial revision
change. Detached/retired sources send nothing. This covers the actual routing
seam alongside SpatialLifecycleTests' production movement encoder; it does not
claim to exercise the client renderer or its command clock.

## Native ore and idle despawn timers

The existing char_manager.cpp now owns ore-expiry and idle-retirement scheduling
and callbacks. Their old bodies are removed from PlayerRuntimeSystem.cpp; the
remaining CHARACTER prototype initializer calls the entity-native ore entry
point. Timers retain versioned entities and check exact event-slot ownership.
They detach before calling the manager's native destruction path. Replaced,
cancelled, recycled or untyped owners are not retired by stale callbacks.
Idle actors still defer while targeting an opponent; ore expiry is ignored if
the prototype is no longer an ore vein. The 5-minute idle and 7-15-minute ore
delays are preserved. Construction observers and throwing target lookups cannot
strand an old slot or overwrite a newer event.

CharacterManagerTests links the real scheduler and manager with controlled
factory, target and ore-classification services. It checks normal expiry,
reentrancy, index cleanup, cancellation/replacement, recycled generations,
component removal, construction callbacks, combat deferral and race changes.
This does not migrate the remaining CHARACTER prototype initializer or shell
teardown. Verify live mining, ore respawn, combat/idle despawn and map cleanup
before deployment; no live server was restarted by this change.

## Native HP recovery timer and SP distribution

The existing MovementSystem.cpp recovery entry points no longer resolve a
CHARACTER. Each tick validates the versioned entity, required components and
ownership of the exact scheduled event. Outward gameplay/notification calls are
followed by revalidation; a cancelled/replaced timer cannot continue healing or
clear its replacement. Missing prototypes and stopped ticks release their slot.
An exception also releases only the failing timer's slot before propagating;
this is not a global server exception-recovery policy. Initial slot construction
is checked for observer-driven removal, retirement and nested timer creation.

SpatialLifecycleTests now links the real event scheduler and exercises NPC/player
healing, doors, poison cadences, Meley exclusions, both special dungeon floors,
full HP, missing components/prototypes, recycled entities, callback deletion,
replacement and exceptions. It uses native fixtures with controlled point,
combat, dungeon and prototype services; no CHARACTER object is allocated.

CombatSystem::DistributeSP also uses native entities and SkillSystem's group
accessor. CombatStateTests executes its production formulas for class/group,
attack/movement/idle state, full HP/SP, kill-level thresholds, fly sizes and
retirement during fly publication. The caster branch intentionally retains its
source-based maximum when source and recipient differ. HP/SP intermediate
arithmetic is widened and bonus results saturate to the int32_t payload range.

Shared event storage, dungeon objects and prototype pointers remain separate
systems; this is not complete CHARACTER removal. Before deployment, check live
HP/SP recovery, combat/movement transitions, poison, dungeon floor changes,
death/revival and logout/relog. Packet delivery, real dungeon state and combined
live gameplay services are outside these isolated regression fixtures.

## Removal of the obsolete vital mirror

VitalRegenSystem.cpp/.hpp and its main-loop call are removed. The loop did not
regenerate anything: it read Health/Mana/Stamina through PlayerRuntime/PointSystem
and assigned them back to the same components, gated by LegacyCharPtr. Recovery,
damage and stamina changes already update those components through PointSystem.
PointCalculationTests now explicitly checks immediate native vital reads/changes,
upper caps, poisoned old arrays, missing components and recycled handles for both
player and NPC fixtures. PlayerRuntime setters and external services are doubles;
this suite does not test the whole recovery timer. Its native migration is
covered separately by the recovery tests described above.

## Entity-native OX event cohorts

OXEvent.cpp now stores participants, attenders and eliminated players as
versioned character entities, without PID lookups or CHARACTER pointers.
Only live players on the OX map can enter, receive event rewards or be warped.
Reconnecting with the same PID does not inherit an old generation's attendance;
an explicit new entry is required. Counts prune departed/retired attenders.

Answer checks, winner logging and grants use snapshots, while elimination,
audience warps and event closure commit/detach their work before callbacks.
Nested answer/reward/close calls are guarded; reward audit data is copied before
item delivery can destroy the recipient. Repeated explicit GM grants remain
allowed: this is a reentry guard, not an exactly-once reward ledger.
The manager cancels its timer on initialize/destruction. Quiz stages belong to
individual events, the level upper boundary is clamped, and invalid/overflowing
localization IDs are rejected instead of throwing from stoi.

OXEventTests executes the production OX manager, event scheduler and buffers
with entity-only character fixtures and controlled game/network/DB doubles.
It covers stale/recycled players, map departure, audience vs attender entry,
rectangle edges, callback deletion/closure, nested grants/warps/answer checks,
exception recovery, multifarm eligibility, quiz-list mutation and timer
replacement/reinitialization. Live quest status propagation, OX notices/effects,
audience movement, inventory grants and DB audit delivery still need an in-game
smoke test; those external services are not exercised by the isolated suite.


## Native quest character context and click dispatch

The existing quest manager/PC and trigger implementation now receive native
character entities for clicks, dungeon attribute members and reward delivery.
The input click handler resolves the wire VID once; the quest click entry point
does not turn the causer into a PID and resolve it again. GetPC(entity) binds that
generation directly; GetPC(PID) remains for existing ID-based event sources.

QuestContext stores separate entity references for its NPC target and NPC lock
owner. It no longer overloads one integer as a target VID and an owner's PID.
The current party member and nested other-PC stack also retain generations.
Deleted/recycled entities cannot inherit a target, lock or nested return context.
The character-pointer getters, CHARACTER::OnClick, AssignTriggers and CTrigger
are removed; the existing trigger.cpp implements ClickTrigger components.
Native quest-context accessors live together in the existing QuestSystem.cpp.

Reward delivery claims a local batch before calling external services, validates
the recipient between rewards, and does not clear rewards queued by reentry.
The strict EXP overshoot cap is preserved without underflow at/above the next
level threshold. EndRunning revalidates the recipient/current PC after reward
delivery before saving or touching quest state again. This is not a general
lifetime rewrite of quest::PC, Lua coroutines or all quest event handlers.

QuestRuntimeTests links the production manager, PC, NPC Lua entry points,
QuestSystem.cpp and trigger.cpp. It checks recycled NPC/item/player handles,
failed context selection, nested other-PC restoration, Lua lock ownership,
native attribute members, target/chat deletion, reward reentry and new batches,
EXP boundaries, disconnect during reward completion, native click dispatch,
exchange blocking, invalid trigger types and destructive/throwing component
construction callbacks. No CHARACTER object is created. Character data access,
script execution, reward sinks, networking, party/shop and dungeon services are
controlled doubles; unexpected calls fail the fixture.

Build QuestRuntimeTests and run ctest -C RelWithDebInfo -R quest_runtime
--output-on-failure in the normal and AddressSanitizer build trees. Live checks
remain necessary for NPC conversations and unlock-on-close/logout, item/EXP
rewards, party dungeon entry/exit, private-shop opening and C++ dungeon NPCs.
The unrelated LPCHARACTER paths elsewhere are not claimed to be migrated.

The mob victim search in trigger.cpp also consumes native sectree entities,
without LPENTITY/CHARACTER conversion. QuestRuntimeTests exercises the real
search and FCollectEntity enumeration with controlled snapshot/membership and
character-data seams. It covers self exclusion, nearest/range boundaries,
invisibility, terror/immunity, empire and attack-mob flags, construction-site
priority, retired/recycled entities, source retirement during enumeration and
extreme coordinates. The existing distance weights and construction preference
are preserved; coordinate arithmetic and the half-HP comparison no longer
overflow. Live map membership and NPC pursuit still require server smoke tests.


## Entity-native duplicate-load retirement

`ItemSystem::DestroyLoadedDuplicateItem` now lives beside the existing manager
destruction path in `item_manager.cpp`; its old pointer-based body is deleted.
It accepts a complete item entity without a CItem/CHARACTER allocation and does
not resolve owners through a player ID. It delegates detachment, index cleanup
and optional legacy allocation release to the manager's guarded lifecycle.
There is no unguarded pre-detachment or retained item pointer across callbacks.

Only the stale runtime instance is retired; its DB row belongs to the incoming
load. Save suppression is scoped to this operation and restored on a surviving
entity after rejection or exceptions. The return value is true only when the
original versioned entity was retired. Recycled replacements and transferred
ownership survive; failed cleanup does not permanently disable item saving.
The existing input_db duplicate-selection policy is unchanged.

Direct and delayed manager saves now respect skip-save, the destruction guard
and DB availability. Single-item save reports whether the request was handled;
single/owner/tick/shutdown flushes retain suppressed or disconnected entries for
retry rather than discarding them. Destruction latches its initial no-delete
policy so a callback cannot turn a runtime-only retirement into a DB deletion.

ItemManagerLifecycleTests executes the real duplicate helper and manager
destruction together, covering entity-only and legacy-backed items, absent DB
connection, equipment, incomplete/stale handles, detach/factory rejection,
exceptions, nested cleanup, transferred/dead owners, foreign occupants and
recycled generations. Inventory detachment and the entity/allocation factory
remain controlled doubles. Constructed descriptor objects and packet sinks test
all five save paths, disconnected/suppressed retry, native save payloads and
nested flushes during duplicate retirement without using a live DB. Failed
retirement is reported, not automatically retried by this helper. Live login,
DB-row retention and reconnect need
in-game checks. This change does not remove the remaining CItem allocation
boundary from other item operations.

## Transactional rune bottle charging

The rune-charge command now only parses/range-checks slots and forwards native
entities to ItemSystem::ChargeRune in the existing ItemAttributeSystem.cpp.
Owner/wear anchors, inventory restrictions, lock/exchange state, positive bottle
charge and valid rune durations are checked before any debit. Integer percentage
steps are preserved; time addition uses wide arithmetic and is capped at the
configured maximum. Full runes and invalid inputs do not split bottles.

Three paths share one charge commit:

- A partially used single bottle keeps its inventory slot and remaining charge.
- A fully used bottle consumes one unit, with no output allocation or space
  requirement. Zero stacks use the existing deferred-consumption retirement queue.
- A partially used stacked bottle uses InventorySystem::SplitItemWithCommit.
  This extends the existing MoveItem split algorithm, not a second implementation.
  Allocation, placement validation and payload revalidation precede a component-only
  companion commit. Rune time, remaining-bottle charge, source count, output slot,
  grid and ownership are all committed before split logging/save/packet callbacks.
  The output preserves the source payload instead of acquiring a fresh full charge.

The companion hook cannot fall back to a move, merge or equipment operation.
Rejected preparation leaves the original bottle/rune unchanged; an aborted
unplaced clone is retired with skip-save enabled, without requiring persistent
deletion. The rune operation guard prevents nested charges/toggles on the owner.
Post-commit retirement or mutation is not reported as a failed payment and does
not restore snapshots over newer state. Point/attribute and set-bonus refresh use
the existing native rune runtime after charge commitment.

ItemAttributeTests exercises the production charge algorithm with controlled
inventory/factory/point/affect/persistence doubles: all three paths, full inventory,
creation/placement failure, payload preservation, invalid and stale state, wide
duration arithmetic, bonus-threshold crossing, nested callbacks, post-commit
retirement/mutation and deferred destruction retry. QuickslotTests separately
executes the real shared splitter and its companion hook, including observable
commit ordering, rejected hooks, allocation/payload/destination failures and
publication-time retirement. GameServer compilation checks their integration.

This is an in-memory transaction, not a new database transaction or a guarantee
of atomic persistence across process/power failure. Live client/DB testing remains
necessary, especially active-rune charging, reconnect and stacked partial bottles.

## Entity-native rune runtime

Rune activation/deactivation, set-bonus selection, both attribute updates and
wear-time processing now live beside rune initialization in the existing
ItemAttributeSystem.cpp. All CItem rune methods and the five legacy-boundary
adapters are deleted. Commands pass item entities directly and validate slot
ranges before narrowing. Rune messages use the native localized item-name lookup.
The generic wear-event callback only forwards to the rune system and retires
its own event handle on exhaustion; it does not own a parallel rune algorithm.

Runtime operations validate rune components and their owner/wear anchor. A
registry-scoped operation guard prevents nested rune changes on the same item
or owner. State changes precede publication; components are reacquired after
point, affect, save, packet and chat calls. Both attribute values are checked,
including a changed second value with an unchanged first. A malformed duration
cannot divide by zero in bonus checks. Deactivating the seventh rune no longer
removes its points twice. The existing missing-slot RUNE1 visual-affect rule,
50-percent set threshold and paused/bonus-rune charge behavior are preserved.

Activating a recharged rune restarts its wear timer; failed scheduling does not
activate its points. Wear scheduling uses the same callback-safe native timer
publication helper as other item timers. Stopping a wear timer detaches it before
publication, clamps remaining time to zero, and does not reuse an event-component
reference after a callback. A callback-installed replacement timer survives.

ItemAttributeTests runs the production rune algorithms without CItem/CHARACTER
fixtures. It covers transitions, the 50-percent boundary, invalid/missing/stale
state, both attributes, exhaustion, repeat calls, timer restart/failure and
callbacks retiring/changing items, owners or bonus prerequisites. Shared point
calculation, affects, persistence, networking, inventory lookup and scheduling
are controlled doubles; the full point-calculation implementation is not exercised
by these rune tests. ItemRuntimeTests separately runs the actual wear scheduling,
stopping and localized-name lookup, including allocation/publication failure,
component removal, restart, retirement and replacement-event callbacks.

GameServer compilation checks the command/event integration. Live client visuals,
DB persistence, charge-bottle splitting and reconnect/equipment integration still
need in-game checks; this migration does not claim the entire item system is ECS-only.
The former debit-before-create charge-command issue is addressed by the later
transactional rune bottle charging change described above.

## Entity-only item identity registry

ItemRegistry stores ID/VID-to-entity bindings and an entity-to-key pair for
generation-checked removal. The CItem pointer indexes, pointer lookup and unused
legacy-item invariant adapter are deleted. Registration validates ItemIdentity,
rejects live ID or VID collisions without replacing either item, and preallocates
new map nodes before committing both indexes. Lookups reject stale handles,
removed identities and keys that no longer match the entity's current identity.
Reindexing removes old aliases; cleanup of an older generation cannot remove
its successor. Zero-ID gold and ID-only items remain supported.

The allocation factory no longer finds/rebinds an existing item by a legacy
pointer or a duplicate database ID. Repeated binding of the same allocation
uses the existing entity state instead of rehydrating it from CItem. A new
allocation colliding with an existing item is rejected. The CItem allocation
boundary itself remains and is not claimed to be fully migrated.

ItemRuntimeTests now links the real ItemRegistry.cpp rather than lookup doubles.
It exercises ID/VID conflicts, identity mismatch, idempotency, reindexing, all
unregister forms, missing identities, recycled generations, zero sentinels and
the public ItemSystem lookup APIs. Factory integration is compiled with the
server, not executed by this headless target; allocator failure is not injected
into the registry maps. Live login/duplicate-load/persistence checks remain needed.

## Entity-native item load initialization

`CheckItemUseLevel` and `OnAfterCreatedItem` now execute in ItemSystem.cpp,
without a CItem lookup. Their CHARACTER-era item methods and bridge bodies are
deleted. The level check preserves the first matching LIMIT_LEVEL rule and
uses the full-width table value. Loaded first-use items resume only after use;
partly charged souls resume charging, while fully charged souls succeed without
a timer. Missing prototypes and stale/non-item handles are rejected.

Real-time and soul timer creation no longer retain an ItemEvents reference
through allocation callbacks. A removed component or retired generation rejects
and cancels the pending event; a nested timer start wins over an older pending
one. Event publication may destroy the entity, so load initialization rechecks
liveness before continuing and never reports that destroyed handle as ready.

`ItemRuntimeTests` compiles the existing ItemSystem.cpp. Entity-only fixtures
cover level boundaries above 255, missing/stale state, first-use and soul reload,
idempotent starts, allocation failure/retry, component removal, generation reuse,
nested creation and publication-time retirement. Scheduling, event callbacks,
network/affect and unrelated legacy entry points are controlled doubles; this
does not test the real event queue, DB hydration or client login. Item allocation
and other item operations still have CItem dependencies and need further migration.

## Single combat execution path

The prototype `CombatSystem_Update` and its private cooldown field are deleted,
including the call in `main.cpp`. It subtracted a fixed 1 HP in parallel with
real attacks, compared millisecond ticks with a pulse-count interval, and marked
victims dead without the normal Damage/Dead processing. Retiming it would still
leave a second source of damage. CharacterManager already drives the native AI
state machine; monsters dispatch `CombatSystem::Attack` there, and player melee
input dispatches the same attack entry point from `input_main.cpp`. Movement,
recovery, affects and dispatcher updates retain their existing order.

`AIStateTests` executes the actual AI scheduler and battle state with entity-only
fixtures. It checks one swing per scheduled pass, duplicate/early updates,
millisecond-to-pulse cooldown conversion, a minimum one-pulse retry, millisecond
clock wrap, and no automatic PC attacks just because a target is set. The Attack,
motion and stat services are controlled doubles: these tests verify dispatch
and timing, not real damage, death, loot, client packets or database persistence.
`CombatStateTests` retains target and full-width/wrapped attack-time checks;
the old assertion of the prototype's extra 1 HP damage is removed.

Before deployment, check player melee/ranged/skill attacks, mob attack cadence,
target loss, death rewards and logout during combat with a real client/test DB.
The shared Damage/Dead implementation still has legacy internals; removing the
parallel updater does not complete that migration.

## Native movement duration reads

MovementSystem::GetCurrentMoveDuration replaces the CHARACTER getter at all
three input/LostCastle callsites. It reads stored timing without resolving a
legacy character or creating components; packet and distance rules are unchanged.
SpatialLifecycleTests covers missing state, zero/full-width timing, destination-
independent reads, null/non-character sources and recycled entity generations.

## Native animation packets

MovementSystem::Motion builds and broadcasts HEADER_GC_MOTION from entity VIDs.
The two CHARACTER animation methods are deleted; LostCastle uses its existing
entity handles. SpatialLifecycleTests checks encoding, source inclusion, null or
stale/missing-VID victims, rejected sources and source retirement during transport.
Packet transport is a controlled test double; this is not a live animation test.

## Native movement commands and AI timing

Goto, Stop, motion-mode/speed selection and CalculateMoveDuration now live only
in the existing MovementSystem.cpp entity API. Their CHARACTER methods are
deleted; combat, skill, AI, mount, dungeon and session callers use this API.
The old Goto bridge prewrote the requested destination before calling CHARACTER,
so that call mistook a new target for a duplicate and skipped timing. Native
Goto commits destination, timing and the four-pulse AI duration together.
Repeat/current-position requests remain no-ops; descriptor attachment still
governs PC walking and NPC immobility/stop behavior, including TagPC clones.

MovementState.commandRevision guards nested commands, even with identical
parameters. It also participates in tick snapshots. Stop/sync-clear commit
timing before destination removal, preserving callback-owned timing writes.
Revision checks abort older commands after nested movement or spatial changes.
New components use single-entity insertion without a returned reference; a
construction callback can remove them without a post-callback EnTT lookup.
Retargeting writes destination fields directly, like timing (no on_update signal).
Callbacks must not replace a component while its own on_destroy signal is active;
such same-component replacement is not a supported movement command boundary.
Motion speed handles all weapon modes, polymorph, stamina walking, mounts and
horse fallback without resolving CHARACTER/CItem. Missing or non-finite/zero
motion data uses the existing 300-unit fallback; wide arithmetic and bounded
duration conversion avoid overflow on extreme targets. Normal float truncation,
movement-point limits and integer speed-factor semantics are retained.

The write-only m_posStart and the two legacy AI scheduling fields are deleted.
AIState owns stateDuration/nextStatePulse; all AI writers and the real character
state-machine pump consume those components. The state bodies and scheduler now
live in AISystem; CharacterManager calls the entity API directly. The pump keeps
the entity handle across the state callback instead of dereferencing a
potentially retired CHARACTER.

SpatialLifecycleTests exercises actual command, timing and selection logic on
entity-only fixtures, with controlled motion/equipment/point/descriptor services.
Cases include duplicate targets, retiming, stops, all weapon modes, mount/horse
fallback, PC versus descriptor-free walking, malformed motions, integer bounds,
immobile bosses, reentrant Goto/Stop, preparation-time recycling and AI scheduling
defaults/reentry. PointCalculationTests separately checks that an active speed
change calls the native timing API even without a legacy character, after point
commit, and that stationary/expired movement does not. The live animation-file
loader, AI state callbacks, network and live-client movement are not covered by
these isolated fixtures; verify them before deployment.

## Native movement tick and move packets

MovementSystem.cpp now interpolates placed character entities without a VID
lookup, LegacyCharPtr or CHARACTER call. The native sector index is committed
before PositionChangedEvent; old/new coordinates are captured before the write.
Arrival publishes once and the NPC combat/idle decision uses the entity target.
Dead, stunned and detached entities are not advanced; missing destination
sectors leave position and timing unchanged. Coordinate differences are widened
before subtraction; the existing step, rounding and timing rules are retained.

Each tick snapshots values, not component references. Versioned handles,
SpatialRevision, destination, timing and placement are rechecked after callbacks.
A retired/recycled entity, later retargeted snapshot entry or same-location
respawn cannot receive the old tick's follow-up movement/arrival operation.

SendMovePacket reads identity, destination/current position, timing and rotation
from ECS. CHARACTER::SendMovePacket is deleted and its callers use the entity
API. Motion victim encoding no longer converts an entity back to CHARACTER.
The existing MovementSystem file remains the only implementation; Show, Warp,
Move/Sync/OnMove and recovery/save callbacks still contain migration work.

SpatialLifecycleTests also compiles production MovementSystem.cpp and its move
packet encoder and SetPosition implementation. Pointer-free cases cover sector
crossing, single visibility publication, arrival/idle/fighting, walking preference,
diagonal/zero-speed interpolation, extreme destination subtraction, missing
sectors, dead/detached movers, event/packet deletion, new movement from a callback,
same-location respawn and packet field/fallback parity. Combat-target lookup,
AI scheduling, event cancellation and packet transport are controlled leaf doubles;
legacy-only link seams fail immediately if reached. These are not live-client
movement, motion-file or server-load tests. Test real player/NPC/mount movement
and peer visibility before deployment.

## Native ground placement, sectree membership and visibility

PlaceItemOnGround and RemoveFromGround live in InventorySystem.cpp and operate
on versioned entity handles. CItem::AddToGround and the ground legacy wrapper
are deleted. The original sectree files now keep one entity membership index;
native callbacks never resolve LPENTITY. Unmigrated character/building callbacks
still have a compatibility adapter at the iteration/entry boundary.

SpatialService commits membership before the caller arms expiry and publishes.
SpatialRevision distinguishes removal/reinsertion even at identical coordinates.
Preparation and publication revalidate handles, ownership and component state;
an older operation cannot save or overwrite a callback's new ground placement.
Claims and last-owner history survive initial placement. Native ground removal
cancels timers, clears the claim and commits detached state before remove packets.

VisibilitySystem is the sole directed graph implementation for characters,
items, buildings and shops. The parallel entity_view.cpp is deleted. Late viewers
discover existing ground items, departing viewers get correctly directed removes,
unchanged characters do not get duplicate inserts, and observer state is committed
before reconciliation. Shop reencode updates its viewers, while character
reencode preserves the self-only refresh policy. Sector insertion restores tags
after character cleanup. Native sector snapshots revalidate membership before
each callback; map enumeration also excludes removed and replacement entities.

SECTREE_MAP drains while lookup/neighbors are still alive and closes all sectors
before whole-map callbacks. Private-map destruction uses this same native drain
instead of its former LPITEM/LPCHARACTER cleanup pass. Direct SectorPlacement
destruction retires membership and the recorded PC contribution.

SpatialLifecycleTests compiles the actual sectree.cpp, SpatialService.cpp and
VisibilitySystem.cpp with no CItem/CHARACTER fixtures. Checks cover late viewers,
sector/private-map boundaries, PC-triggered NPC activation, registry destruction,
snapshot mutation/recycling, observer directions, shop reencode, native character
reinsertion, preparation mutation/deletion, network callback deletion/relocation/
same-location respawn, tag-destruction reentry and reentrant sector teardown.
Map loading/SECTREE_MAP methods, AI scheduling, retirement and packet transport
are controlled doubles; the real map-file loader and whole-map destructor are
not runtime-tested here. QuickslotTests executes actual ground placement/removal,
timers and claims against controlled spatial/network/DB services, including
inventory pickup, preclaims, reentrant respawn and publication deletion.

Run both suites plus the full server regression set in Release and ASAN.
Before deployment test login/warp, two moving players, observer transitions,
normal/quest/dice drops, late viewers, pickup/expiry, shop rename, NPC wake/sleep,
private dungeon teardown, disconnect/relog and persistence with the real client.
These isolated tests are not a live-server load test or proof that all engine
lifetimes are migrated.

## Native ground ownership and expiry

InventorySystem.cpp now owns SetGroundOwnership, IsOwnership, owner-PID refresh,
StartDestroyEvent and both ground timer callbacks. The old InventorySystem
SetOwnership entry point, ItemSystem wrappers and LegacyBridge callbacks were
removed; dungeon/login callers use the same entity API. No parallel production
file was introduced. Claim permission reads no longer create ItemEvents.

Inventory owner/ownerPID and the temporary reservation's ownershipPID are
independent. Refreshing the former no longer zeroes a ground reservation. The
native ground insertion preserves ItemOwner, including a quest's claim
set before insertion and last-owner history. Claims commit timer, PID and bounded
display name before publication. Clearing/expiry empties the display as well as
the PID; it never erases an actual inventory owner. A null owner releases the
claim; repeating the same claim succeeds without extending it, and a different
owner cannot replace it without release. The historical <=10-second request
default remains 30 seconds. Delays and absolute queue deadlines are checked for
int32 overflow. New claims reject stored/equipped items and zero-PID/non-PC owners.

Callbacks verify both the versioned entity and the exact active event lease.
Cancelled/replaced timers cannot clear a new claim or destroy a recycled entity.
Cancellation uses a local lease, not an address inside an item component. ECS
construction and scheduler callbacks are revalidated before committing. Ground
destruction refuses items that have moved to storage, even if pickup missed timer
cancellation. A refused retirement retries after one second only if still on the
ground with no newer timer. Publication may move/delete a committed item without
being followed by stale-state writes.

QuickslotTests executes these real implementations with entity-only fixtures.
Coverage includes public/claimed pickup permissions, pre-insertion quest claims,
PID refresh, duration/name bounds, duplicate/foreign claims, complete expiry and
clear, malformed ownership targets, expired/reused entity generations, ECS
construction deletion/mutation, scheduler failures/deletion, cancellation-time
replacement claims, packet-time reclaims/deletion, destroy-timer replacement,
stored-item survival and retirement retry/transfer/deletion. Its existing reward
fallback tests now execute the real ownership API too. The scheduler, retirement,
spatial insertion, view packets and database services are controlled doubles;
EventLifecycleTests separately exercises the real queue. Full engine callback
chains, spatial rendering, live persistence and real scheduler-to-item integration
are not covered by this fixture.

Before deployment verify normal/quest/dice drops, late-arriving viewers,
exclusive pickup then public pickup after expiry, pickup during expiration,
disconnect/relog, and ground cleanup on a test server.

## Native equipment operations

InventorySystem.cpp now owns EquipItemEcs, UnequipItemEcs, equipment policies and
GetWearItem as well as the native EquipTo slot commit. The previous CHARACTER
EquipItem/UnequipItem/CanEquipNow/CanUnequipNow/SwapItem implementations and the
CItem IsEquipable method were deleted. ItemSystem.cpp's equipment wrappers no
longer prewrite ownership/location/equipped state, resolve CHARACTER/CItem, or
restore and resynchronize legacy state after a failed call. No replacement file
or second equipment implementation was added.

Equipment slot/grid and item owner/location/equipped components are committed
together before publication. The higher-level path checks ownership, source
anchors, item locks, job/stat/sex restrictions, unique groups, riding, costume
dependencies and dragon-soul decks. Replacement validates the displaced item's
permissions again after callbacks. Full-width source cells and their inventory
window survive swaps, including extra-inventory and belt sources. Transfers use
pure insertion without acquisition-only attribute randomization or rune equip.

Public equipment actions have an owner-scoped recursion guard. Publication
stages recheck generation, owner and wear slot; recovery only touches the original
still-detached entities, never a recycled entity or another owner's item.
First-use expiry indices and counter overflow are checked. GetWearItem accepts
the dragon-soul deck range too: the previous ECS query returned null there even
though CHARACTER::GetWear supported it. The real query is now linked into the
headless test rather than replaced by a service double.

QuickslotTests exercises the production high-level equip/unequip/policy bodies,
GetWearItem and low-level slot operations with entity-only fixtures. Coverage
includes full-inventory swaps, failed displacement recovery, occupied footprints,
source windows/cells above 255, stale/foreign owners, observer/duel/lock/stat/sex/
marriage restrictions, recursive actions, callback destruction/recycling,
dragon-soul equip/unequip and first-use timers. The existing CMake test targets
also link the production EcsDiagnostics.cpp after its recent introduction.

Item metadata/FindEquipCell, point and mount effects, quest/timer services,
persistence and packet transport are controlled doubles. ModifyPoints is linked
but these fixtures do not exercise every apply/appearance branch. Effect-level
reentrancy and all weapon-costume/mount combinations still need broader coverage.
The swap is guarded sequential work, not an atomic database transaction: recovery
stops when the owner is invalidated and never overwrites callback-owned state.
If the old wear slot was taken, it tries carrying storage instead; if no recovery
slot remains for a live owner, it logs the unresolved detached item.
Live client equipment UI, relog, persistence and mount integration remain required
before deployment. Remaining legacy move/use callers have not been migrated by
this equipment change.

Verified on 2026-09-08 (Windows/MSVC): GameServer Release builds successfully;
all 13 headless suites pass in Release and AddressSanitizer RelWithDebInfo.

## Native inventory placement and detachment

The existing InventorySystem.cpp now owns ordinary placement/removal and the
main/extra/dragon-soul empty-slot queries. The previous ItemSystem.cpp bodies
were removed, not retained as a second implementation. PlaceItemEcs no longer
prewrites ownership, calls a legacy boundary, restores possibly destroyed
components, or resynchronizes from CItem. The extra-inventory entity query no
longer resolves either CHARACTER or CItem. Ordinary AddToCharacter uses the same
native insertion core; its acquisition-only rune/accessory rules remain separate
from pure transfer placement.

Placement validates entity generations, raw ownership, ground state, window,
footprint, page/unlock limits, DS boxes, extra categories and cross-window aliases.
Both item anchors and occupancy grids must be empty. Slot/grid and ownership/
location change together before saving or sending packets. Pending destroy-event
leases are moved out of components before cancellation and the destination is
rechecked afterward. Publication reads current slot state, so a nested move or
replacement is not overwritten by a rollback. Extra unlock multiplication uses
64-bit arithmetic before clamping.

RemoveItemEcs now enters the actual removal engine BEFORE clearing metadata.
Previously the owner was cleared first and the engine returned without removing
inventory references. Removal clears matching anchors/footprints together with
ownership; it rechecks location after component-construction callbacks. Unequip
no longer calls SetItemCell with the old owner after detaching (that setter also
reattached ownership). It commits the wear slot and detached metadata together,
preserves the EQUIPMENT/absolute-cell wire format, rejects recursive unequipping,
and stops after callback destruction or reownership.

QuickslotTests was extended in place: it links the real InventorySystem.cpp,
including placement, detachment, unequip and query bodies. Fixtures have only
entities/components, with no attached CHARACTER/CItem. Tests cover main, extra,
DS, belt and switchbot storage; invalid cells, grids and aliases; stale/same-PID
owners; timer cancellation, save/packet callbacks, nested relocation and entity
recycling; equipment detachment and acquisition-vs-transfer accessory behavior.
External item accessors, combat/quest/timer services, persistence, switchbot
registration and packet transport are controlled doubles. Container removal is
covered separately by the existing safebox/item-manager suites, not by a live
cross-subsystem server session.

The construction-destruction test initially exposed an ASAN heap-buffer-overflow
in the new helper's use of EnTT get_or_emplace: its signal mixin calls get after
on_construct. The helper now uses single-entity insert, which publishes the same
construction signal without returning a reference to a possibly deleted object.

Verified on 2026-09-06: GameServer Release and all 13 Release/ASAN tests pass.
Run the expanded inventory test through the existing QuickslotTests target:

```powershell
cmake --build build --config Release --target GameServer QuickslotTests
ctest --test-dir build -C Release --output-on-failure
cmake --build build-asan --config RelWithDebInfo --target QuickslotTests
ctest --test-dir build-asan -C RelWithDebInfo --output-on-failure
```

This is not the complete equipment/shop migration. The equipment follow-up above
replaces EquipTo's legacy writes and the high-level wrappers; other legacy callers
and broader effect-level callback tests remain. Actual client inventory/equipment UI, relog, mount
account storage, safebox transfers and DB persistence require integration tests
before deployment. Saves are not a durable multi-record transaction.

## Entity-owned exchange sessions

The existing exchange.cpp/exchange.h now contain the complete exchange state
and implementation. CExchange, its owner/company pointers, CHARACTER's exchange
pointer and duplicated exchange timestamp are removed. ExchangeRef stores a
generation-checked session entity; both offers belong to one ExchangeSession.
input_main decodes the request and passes native entities to ExchangeSystem.
Only START resolves a network VID. Social/window guards query live ECS state.

Completion validates item identities, original slots/owners, counts, attributes,
sockets, locks, duplicate inventory references, gold, player/window state and
available space. Main, extra and dragon-soul grids are planned as value snapshots.
Both outgoing offers are removed from those snapshots first, so full-inventory
swaps work. Live inventories, ownership/location, balances and old quickslot
bindings are updated before the first publication callback. Transfer does not
rerun the item-creation rune-autoequip or accessory-randomization rules.
Old CGrid heap allocations and sequential unchecked detach/add calls are gone.

Cancellation and participant/session destruction release matching reservations
and item flags. END publication retains both reservations until completion;
revision checks stop old publications after reentrant changes. Item-slot packets
read current state, and quickslot packets stop on revision changes.
VCard account trading was removed in upstream commit a9c100a6. Its obsolete
test calls were removed too; former vnums 90008/90009 now have regression checks
for ordinary item transfer without retirement or account-credit DB requests.

ExchangeTests links production exchange.cpp with entity-only inventory fixtures.
It covers lifecycle cancellation/destruction, stale handles, source/offer guards,
display bounds and overlap, changed item metadata, duplicate slot aliases,
full-inventory swaps, page and capacity boundaries, all six extra categories,
locked extra slots, dragon-soul boxes, invalid/insufficient/overflowing gold,
64-bit gold transfers, missing DB/quest/inventory state, cooldown changes,
callback disconnects/cancellation/slot replacement and ordinary retired-card items.
Inventory/owner mutations and packet construction are production code. Actor,
item-accessor, quest, persistence and packet-transport services
are test doubles; no CHARACTER or CItem is attached to a fixture.

The tests also exposed a pre-existing MSVC enum truncation: GOLD_MAX became
-1727379968 instead of 999000000000. The shared limit is now an explicit int64_t
constant, outside EMisc, with a compile-time width check.

Verified on 2026-09-06: GameServer and Database Release builds succeeded;
all 13 tests passed in Release and MSVC AddressSanitizer RelWithDebInfo.
ExchangeTests reported 11,984 checks in each configuration.

```powershell
cmake --build build --config Release --target GameServer ExchangeTests
ctest --test-dir build -C Release --output-on-failure
cmake --build build-asan --config RelWithDebInfo --target ExchangeTests
ctest --test-dir build-asan -C RelWithDebInfo --output-on-failure
```

Before deployment verify actual client exchange UI, all inventory types, quickslot
updates, disconnect/relog and item/gold persistence. The headless tests do
not execute input_main, the production item manager, real sockets or DB workers.
In-process all-or-nothing ownership changes are not a durable multi-record DB
transaction: process/DB failure between saves still requires persistence-layer
work. Existing gold-log numeric fields are also still 32-bit. Other subsystems
and services retain legacy internals; this is not the end of the ECS migration.

## Skill runtime, colors and input recipients

SkillDamageBonus::useInfo is now the only skill-use runtime store. The
CHARACTER map, CheckSkillHitCount and GetUsedSkillMasterType methods were
removed, along with the splash functor's retained TSkillUseInfo pointer.
Cast registration, hit consumption, per-target limits, main-target identity
and used mastery operate on generation-checked entities. Rejected cooldown
requests no longer clear target hit counts or change the accepted cast's mastery.
The shoot path resolves the target before setting it; a network VID is not an
EnTT entity and must not be converted by static_cast.

SkillColor is the sole color store: client changes, DB loading and buff-color
copying use the existing SkillSystem. CHARACTER's array/getter/setter and its
initialization were removed. Paid color changes check consumption success,
reject recursive purchases, reacquire state after item callbacks, and persist
the committed snapshot before publishing the character update. Zero-color
resets remain free. Payment and DB persistence are separate operations, not a
durable transaction; process/connection failure can still leave a partial save.

The previous zero-result native GetSkillPower stub now uses the real skill
power tables, language-ring and guild rules. Existing legacy callers delegate
to this same implementation; the migrated skill calculations and chat
recipients invoke it directly with their existing entities. Chat recipient/
party delivery and melee victim checks no longer resolve a character just to
recover its entity. The unreachable FYmirChatPacket implementation was removed
from input_main rather than kept alongside the active implementation.

This pass reduces input_main's textual LPCHARACTER count from 48 to 44,
LegacyCharOf from 32 to 31 and GetEntityHandle from 31 to 18. Across the changed
server files, GetEntityHandle occurrences decrease by 103. Counts include
comments/debug text and do not establish that the whole combat engine is native.

GmCommandTests additionally compiles the real skill_power.cpp. Entity-only
fixtures execute all byte-sized skill hit-limit branches, forbidden attack
skills, independent targets, cooldown rejection, shared splash hit budgets,
accepted-cast resets, stale/recycled owners/targets, mastery and power-table
selection/clamping. Color tests cover load/save, slot bounds, missing/failed
payment, free resets, all five buff mappings, self buffs, callbacks that
destroy/recycle entities or change colors, recursive purchases, and missing DB.
Inventory consumption, guild lookup, character/network publication and DB
transport are service doubles. CombatStateTests only adds a fail-fast link
double for the unexecuted shoot-to-skill call.

These headless tests do not execute input_main/input_db, live damage or splash
traversal, real item payment, sockets or DB persistence. GameServer compilation
checks those integrations. Before deployment exercise melee/multi-hit skills,
charge/ranged skills, GM cooldown mode, self/party buffs, paid coloring and free
reset with logout/relog, cross-empire chat/language rings and party delivery.
Legacy damage execution, sector traversal and SkillLevels array ownership still
need further migration; this is not a complete ECS conversion of combat.

## Main input: storage, messenger and inventory guards

The preceding input_main pass reduced LPCHARACTER occurrences from 68 to 48 and
LegacyCharOf from 42 to 32 (textual counts, including old comments/debug strings).
Main/dead dispatch uses DESC::GetEntity and verifies the current player/session
binding. Safebox/mall and account mount-inventory handlers, messenger requests,
quest confirmations, party removal, guild creation and chat helpers pass entities.
The admin-whisper Manager entry point also accepts an entity directly.
No parallel production input implementation was created.

CanHandleItems, inventory capacity/grid checks, belt occupancy and refine mode,
scroll cell and NPC identity live in the existing InventorySystem/components.
The obsolete CHARACTER refine fields and warp-event member were removed; old
callers delegate into the same state. ClearRefineMode deliberately retains the
scroll cell consumed by DoRefineWithScroll after closing the mode. NPC handles
are generation-checked, not re-resolved through a potentially reused VID.

Storage handlers retain safebox lifetime, revalidate ownership/location after
callbacks, restrict source windows and only clear quickslots after successful
check-in. Rollback never overwrites occupied inventory or claims a moved item.
Messenger/guild/admin-whisper text fields are bounded; quest confirmation no
longer writes into the input packet. The unsafe unbounded test-server chat log
was removed from dispatch.

QuickslotTests compiles the real InventorySystem.cpp and now also exercises
entity-only item-handling guards, refine state and stale NPCs, pending warp,
inventory-size clamping, the entire 16-bit main-slot range, page crossing,
extra-inventory locks/large indices, exception cells, Dragon Soul secondary
occupancy, belt contents, and invalid/stale owners. Quest flags, item validity,
event reference counting and unrelated engine services are test doubles.
The existing safebox and item-manager suites exercise their real production
container/lifecycle code. These tests do NOT execute input_main, the account
mount container, messenger/admin-whisper dispatch, real network or DB effects.
GameServer builds check those integration signatures, not runtime correctness.

Before deployment verify all three storage windows, failed/full-slot moves,
quickslots, mount bonuses and logout/relog persistence with the actual client/DB;
also check messenger, guild creation, quest confirmation and admin messages.
Transfers are not durable DB transactions. Item use/drop/move, combat/movement,
cube, battle pass and other remaining character-based services need more ECS
work; native packet boundaries do not imply a completely legacy-free engine.

## GM commands and skill state

The cmd_gm migration removes 28 of its 46 LegacyCharOf calls, plus pointer
target lookups and repeated GetEntityHandle conversions. Stat, skill, affect,
quest-flag, user-list, notice, item-purge and item/socket command paths pass
entities directly. Skill prerequisite checks and group-change packets now live
in the existing SkillSystem implementation; player saves read the ECS skill
group. SkillLevels still references the existing skill array: this is not a
complete conversion of skill storage ownership.

Inventory purge snapshots selected slots, checks item generation/ownership and
slot identity before deletion, and only clears quickslots after successful
deletion if the slot is still empty. Missing pet systems are accepted; summoned
growth pets block purge as before. Selectors now require exact documented names
or aliases instead of arbitrary matching prefixes. Item grant failures are not
logged as successful grants and surviving unowned items are cleaned up.

GmCommandTests compiles the complete production cmd_gm.cpp, SkillSystem.cpp and
skill_power.cpp, the real PID registry, argument parsing and temporary buffers.
Entity-only fixtures execute inventory purge, user listing, socket commands,
setskill and set_skill_group, notice/P2P serialization and native skill setters
and resets. Coverage includes all supported purge windows/aliases, duplicate
entries, invalid/stale/non-PC owners, active/missing pet systems, failed deletion,
callback-time owner/item destruction, recycled indices, ownership/slot changes,
new/replacement items, nested user commands, disconnected/new notice recipients,
map/empire/GM filtering, literal percent signs, 10,000-character notices, numeric
range/junk rejection, skill caps/master thresholds, helper prerequisites,
anti-skill level/book rules, zero resets, group reset and packet/point callbacks
destroying the owner.

Inventory storage/destruction, quickslot delivery, pet state, character lookup,
point calculation, descriptors/chat/network transport, skill-prototype lookup
and DB services are doubles. Other linked GM/legacy services fail fast if called.
These tests do not execute the real inventory allocator, quest flag-list
implementation, character save/relog, remote warp, command authorization/dispatch
or client UI. The main GameServer build checks those integration signatures.
Before deployment verify /set, /setskill, /setskillother, /set_skill_group and
skill-group persistence across relog; quest flag/state commands, item grants,
purge/quickslots and local/cross-core notices in-game.

18 LegacyCharOf boundaries remain in cmd_gm: remote PID warp, sector purge/
weaken, reset/save, safebox sizing, monster-control commands, observer/build,
horse commands, cannot-dead/all-skills horse state, and the legacy Gaya balance.
Sector traversal and some spawn paths also still use legacy entity pointers.
Movement, item and other services may retain legacy internals; no additional
wrapper was introduced to hide those boundaries or duplicate a production file.

```powershell
cmake --build build --config Release --target GameServer GmCommandTests
ctest --test-dir build -C Release -R gm_commands --output-on-failure
cmake --build build-asan --config RelWithDebInfo --target GmCommandTests
ctest --test-dir build-asan -C RelWithDebInfo -R gm_commands --output-on-failure
```

## General commands and entity-owned window state

The current cmd_general migration removes 21 of its 54 LegacyCharOf calls.
Stat commands, duel settings/target lookup, party leave, walking preference,
growth-pet commands, stone crafting, shop-window guards, safebox-open position,
costume visibility, event-manager checks and itemshop protection use entity APIs.
The timed logout countdown resolves a CHARACTER only at its logging/disconnect
boundary. Horse/riding, inventory sorting, fishing/restart, skill learning,
storage persistence, party requests, logging/block mode, observer cleanup,
legacy cube and disabled guild-renewal branches still need further migration.
No parallel production command implementation was added.

Cube NPC and dragon-soul refine opener state now store versioned entities;
every existing reader was updated. Safebox-open and preferred walk/run mode
are component-owned, with CHARACTER compatibility accessors delegating inward.
Quest current-PC/NPC entity getters no longer resolve CHARACTER pointers.
Duel option access goes directly through entity quest flags, preserving the
boolean GetDuel contract (including BetMoney).

RefineWindowTests compiles the complete production DragonSoulSystem.cpp. It
covers entity-only open/self-open/reopen/close, missing descriptors, null/stale
owners/openers, recycled indices, failed-open permissions, packet-time owner or
NPC destruction, nested close and component-pool growth. Packet/descriptor,
item and affect services are doubles; deck operations fail fast if invoked.
PointCalculationTests executes the real stamina point-change branch, verifying
that exhaustion preserves the player's preference and recovery restores it
instead of the current movement tick's isWalking flag. Walking packet delivery
is a double in that test.

These targets do not execute cmd_general dispatch, native session helpers,
quest context getters, duel persistence or live client/DB traffic. The complete
GameServer build checks their integration. Before deployment smoke-test both
duel participants, stat add/reset, walk/run after exhaustion, pet evolution,
stone crafting, competing windows, itemshop malformed/repeated requests and
disconnect/relog. Craft reward/payment ordering and multi-material pet costs
are not converted into an atomic inventory/DB transaction by this change.

```powershell
cmake --build build --config Release --target GameServer RefineWindowTests PointCalculationTests
ctest --test-dir build -C Release --output-on-failure
cmake --build build-asan --config RelWithDebInfo --target RefineWindowTests PointCalculationTests
ctest --test-dir build-asan -C RelWithDebInfo --output-on-failure
```

## Character manager indices, queues and event drops

`CharacterManagerTests` compiles the complete production `char_manager.cpp` and
the VID/PID registries. Name lookup, selection, PC/FSM snapshots and pending
destruction use versioned entities, including fixtures without a CHARACTER shell.
Checks cover case-insensitive names, stale/recycled handles, replacement index
entries, incomplete names, reservoir sampling and job-mask shift bounds, deferred
and recursive destruction, stale-index shutdown, removed snapshot members and
preservation of an outer pending-destruction scope.

Save tests attach inert CHARACTER shells to exercise the actual manager's
SaveReal boundary: callback requeue, destruction of another queued save, exactly
one final save and ECS state remaining alive until shell teardown. The shell
destructor and EntityFactory are doubles; this is not an end-to-end inventory,
dungeon, session or mount teardown test.

Drop tests force successful event rolls and vector growth, verifying unique
clone handles, counts, invalid/failed items, stacking, boss variants, mission
books, dungeon tickets and the soul-only filter. Item allocation is a double.
Disconnected itemshop entry points must return without contacting live services.
Actual SQL/packet exchange and the full legacy spawn/destructor paths are not
executed by this target. The itemshop coin query now reads the entity's session;
protection times are ECS-owned. Neither change replaces DB-side purchase
validation or makes the length-less itemshop DB packet parser bounds-checked.

`CombatStateTests` additionally exercises the production ECS chat/mount counters:
default reads, increments, independent mount reset, combined periodic reset,
byte rollover and null/stale/recycled owners.

```powershell
cmake --build build --config Release --target GameServer CharacterManagerTests CombatStateTests
ctest --test-dir build -C Release -R '^(character_manager|combat_state)$' --output-on-failure
cmake --build build-asan --config RelWithDebInfo --target CharacterManagerTests CombatStateTests
ctest --test-dir build-asan -C RelWithDebInfo -R '^(character_manager|combat_state)$' --output-on-failure
```

Remaining `LegacyCharOf` calls in `char_manager.cpp` are explicit boundaries for
Disconnect, CHARACTER/dungeon teardown, pointer-returning FindPC and SaveReal.
No spawn entry point returns a pointer any more; `for_each_pc` still has legacy
callers. This change does not claim the entire character manager or AISystem is
legacy-free.

## Affect ownership and point application

`AffectLifecycleTests` compiles the complete existing `AffectSystem.cpp` and
`affect.cpp` and `horsename_manager.cpp`, using entity-only fixtures with no
CHARACTER or CItem allocation.
`AffectList` owns the live records, saved skill buffs, flags and loaded state.
There is no CHARACTER list/flag mirror or periodic copying. Storage insertion
initializes the entire record before publication, and removal detaches it before
point callbacks. Short-lived shared leases keep removed records alive only while
an operation/snapshot still uses them; the registry owns normal live membership.

Addition/overwrite, point application/refresh, lookup, flags, individual/type
removal and good/bad affect removal are native entity operations. Checks cover foreign/repeated
removal, destruction and recycled entity handles, shared allocation lifetime,
both flag words and item IDs in the flag field, loaded-state transitions,
INT32_MIN bonus reversal, invalid apply values, the missing-guild gate,
deduplicated snapshots, nested refresh, removal/replacement/addition during a
refresh, replacement of the component on the same entity, exceptions, HP/SP
clamping, revive/mount exceptions, finite type-removal batches and 1,000 repeated
allocation/refresh/removal cycles. The obsolete global ECS expiry pass is a
no-op: the affect event drives the single native `ProcessAffect(entity)` pass
through its still-legacy outer recovery tick.

Addition also tests cube apply-slot matching, non-overwrite stacking, zero
duration, invalid apply rejection, scheduling failure before any grant, polymorph
conflicts, movement abort on stun, same-type reentrant changes during overwrite,
owner destruction at scheduler/point/update/DB boundaries, and packet ordering.
Real client/DB packet structures are captured by inert descriptor transport
doubles; tests check fields, player ID, no-save types, DB remove/add order,
independent wire keys and latest-value publication after nested replacements.
The (type, apply) wire key cannot represent multiple same-key records: runtime
stacking is retained, but only the newest matching record is published.

Expiry checks cover one-second duration/SP-cost application, ordinary expiry
without full point recomputation, insufficient SP, zero/negative and extreme
durations, soul item IDs in the SP-cost field, the missing-guild gate, HP/SP
clamping, duplicate leases, recursive ticks, exceptions, callback replacement
of current/future records, owner destruction/recycling, replacement of affect
storage, and reentrant client/DB publication. Records added/replaced during the
ordinary duration batch wait for the next pass; absolute-deadline preprocessing
still runs before that batch, preserving the existing order.

Battle-pass deadlines are ECS-owned with no CHARACTER mirror; account premium,
battle-pass and quest-driven hair/horse deadlines are covered at expiry and
integer boundaries. `PREMIUM_MAX_NUM` is treated as an array count, not an extra
premium slot. Horse-name validation uses versioned entities and owned affect
leases; tests cover expiry, recursive validation, callback replacements,
destruction during quest/summon/removal callbacks and the actual name DB packet.
The horse summon transport is a double: engine spawn/movement still has legacy
internals. The redundant entity -> CHARACTER -> entity summon wrapper is gone,
but this does not make the full horse actor lifecycle legacy-free.

`AffectTickState` owns the scheduled event. Its move-only component cancels the
timer on destruction; CHARACTER no longer has an affect-event member. Start/stop,
shutdown cancellation before event-queue teardown, pending/reentrant starts,
component relocation/replacement, stale callbacks and recycled owners are
checked with an in-memory scheduler double. The callback
validates both entity generation and current event identity before the legacy
outer recovery leaf; an entity without that leaf stops safely. Native expiry
can now run independently through `ProcessAffect(entity)`, but the scheduled
recovery/item/recall tick is not native yet. Muyeong/Gyeonggong are separate
skill timers and have not been migrated in this step.

```powershell
cmake --build build --config Release --target GameServer AffectLifecycleTests
ctest --test-dir build -C Release -R '^affect_lifecycle$' --output-on-failure
cmake --build build-asan --config RelWithDebInfo --target AffectLifecycleTests
ctest --test-dir build-asan -C RelWithDebInfo -R '^affect_lifecycle$' --output-on-failure
```

Point changes/recomputation, guild/premium/quest lookup, clock, movement/posture,
horse spawning, scheduler and packet transport are service doubles; unrelated
legacy services fail if called. This
does not execute the complete point/affect feedback cycle, live guild-war rules,
socket/SQL I/O, the real event queue, login hydration, clear-on-death policy,
outer recovery or skill timer callbacks. Load/clear/recovery internals still
include legacy CHARACTER work, although they now use the same owning ECS
storage; the Muyeong/Gyeonggong timers are `ecs::SkillTimers` in `SkillSystem`.
The disabled vote-for-bonus branch
is retained but not runtime-tested. The player factory seeds the deadline
component; legacy DB hydration/serialization call sites use the new deadline
API, but complete login/save is not executed by these headless tests. Raw CAffect*
lookup remains a borrowed compatibility API: do not retain it across callbacks,
and do not release it directly. A lease prevents deallocation, not logical
membership changes or arbitrary mutations of a retained record. Expiry guards
prevent duplicate work within a recursive pass; an exception does not roll back
already-applied point changes.

Refresh suppresses recursive refresh and skips removed/changed snapshot entries;
it does not transactionally roll back point changes or queue/rebuild all effects
after arbitrary reentrant mutations. Newly added records are not replayed in the
current batch. Before deployment verify login/logout and character selection,
death/revive saved buffs, poison/fire/bleeding expiry, recovery item locks, guild
war buffs, repeated equipment/point recomputation and both skill timers against
the real client and DB. This is a bounded ECS migration, not a claim that every
affect lifecycle path or the original core crashes have been resolved.

## Real event-queue lifecycle

`EventLifecycleTests` compiles the existing `event.cpp` and `event_queue.cpp`,
using the production allocator, scheduler and queue with only a fixed clock and
fatal-error/logging hooks. Checks cover self-cancellation inside a running
callback, ECS affect-timer destruction during a callback, EnTT component
relocation, cancellation before execution, repeat timing, reset with a cancelled
older queue entry, shutdown and externally retained event handles.

Deleting a queue entry now clears its event's backlink only when that entry is
still current. This prevents cancellation from dereferencing freed queue memory
without erasing a newer entry installed by a timer reset. These checks complement
the affect scheduler double above; they do not run full affect expiry or network
and database integration.
Before the fix, the self-cancellation case reproduced an AddressSanitizer
heap-use-after-free in `event_cancel`, with the entry freed by `CEventQueue::Delete`
inside `event_process` before the callback. This proves the queue defect, not the
cause of a particular historical live-server crash.

```powershell
cmake --build build --config Release --target EventLifecycleTests
ctest --test-dir build -C Release -R '^event_lifecycle$' --output-on-failure
cmake --build build-asan --config RelWithDebInfo --target EventLifecycleTests
ctest --test-dir build-asan -C RelWithDebInfo -R '^event_lifecycle$' --output-on-failure
```

## Combat state

`CombatStateTests` compiles the complete existing `CombatSystem.cpp` and
`battle.cpp`, including `ENABLE_ANTICHEAT` and `ENABLE_CHECK_BATTLE` in this test
target only. Alignment,
its tier calculation, killer/PK mode and attack/damage multipliers operate on ECS
components with no CHARACTER allocation or entity-to-pointer lookup. The legacy
alignment, timer and multiplier fields are removed; compatibility methods only
forward to the native services, and saving reads the component. Both point
recomputation and alignment updates use the same tier function.

Checks cover every tier boundary, signed reductions and saturation (including
INT64_MIN/MAX), unchanged values, sub-display changes, a newer nested alignment
update including ABA, destruction/recycled entity indices during compute/packet
callbacks, killer-mode expiry across signed/unsigned tick wrap, guild-mode
normalization, absent components, non-character/stale/null handles and finite,
zero, negative and non-finite multiplier inputs. The alignment revision suppresses
an obsolete outer publication; state commits before external callbacks. It is not
a transaction or rollback mechanism for point calculation or networking.

Battle checks exercise entity-only target ownership and clearing, attack/skill
state, melee and arrow calculations, weapon/refine/defense formulas, live mob
prototype and berserk data, stale items and recycled character handles. Monster
damage uses the legacy non-PC meaning rather than the narrower `TagNPC` meaning.
Normal-hit poison/stun callbacks destroy their victim to check that processing
does not continue on a dead generation. Anti-cheat checks cover riding, a zero
speed denominator, both attack logs, recycled target identities and clock wrap.
Attack timestamps remain milliseconds; AI scheduling uses AIState's pulse fields.

The two remaining `battle.cpp` character resolutions are explicit boundaries to
soul consumption and the complete legacy `Damage` pipeline, not converted
operations. Those pipelines, marriage services, networking, and affect execution
are not integration-tested by this headless target; the dependent services are
doubles and unexpected legacy character operations fail immediately.

```powershell
cmake --build build --config Release --target GameServer CombatStateTests
ctest --test-dir build -C Release -R '^combat_state$' --output-on-failure
cmake --build build-asan --config RelWithDebInfo --target CombatStateTests
ctest --test-dir build-asan -C RelWithDebInfo -R '^combat_state$' --output-on-failure
```

Point recomputation, time, guild lookup and packet publication are test doubles;
other combat/loot/DB/quest/legacy services fail if called. This does not exercise
damage resolution, actual network serialization, client rendering, the complete
point/affect reentrancy cycle or a live database. The GM target-alignment fix,
bounded Lua input conversion, factory hydration and persistence call sites are
compiled with GameServer, not executed by these headless tests. Before deployment
verify relogged alignment, GM/quest changes, rank bonuses, killer-mode expiry,
guild changes and dungeon/quest multipliers with the real client and DB. Only
non-finite and negative multipliers are rejected here; downstream damage overflow
for extremely large finite multipliers is outside this state migration.

## Point calculation

`PointCalculationTests` compiles the complete existing `PointSystem.cpp` and
`StatSystem.cpp` with entity-only fixtures, without creating a `CHARACTER` or
`CItem`. The recomputation pipeline, point arrays, flat/base/final HP and SP
separation, alignment bonuses and battle formulas run as production code.
The old CHARACTER entry points forward to ECS; the alignment cache and legacy
skill-damage map are removed. Login and both spawn paths initialize the full
archetype before prototype loading invokes point calculation, without a late
legacy-to-ECS point copy.

Checks cover repeated calculations and zero-delta updates, SP percentage caps,
current-HP/SP preservation, removed equipment and skill bonuses, immunity,
entity-only PC versus NPC formulas, inactive/duplicate/foreign rune equipment,
active and invalid Dragon Soul decks, belt whitelist/duplicate/malformed applies,
mount bonus input, support-skill bounds, alignment upgrades/downgrades, incomplete
archetypes, stale handles, recursive callbacks, exception/guard recovery and owner
destruction with entity-index reuse.

```powershell
cmake --build build --config Release --target GameServer PointCalculationTests
ctest --test-dir build -C Release -R '^point_calculation$' --output-on-failure
cmake --build build-asan --config RelWithDebInfo --target PointCalculationTests
ctest --test-dir build-asan -C RelWithDebInfo -R '^point_calculation$' --output-on-failure
```

Item point application, mount-inventory aggregation, skill/prototype lookup,
affects, pets, events and network services are doubles in this target. Balance
tables are test fixtures, not deployed data. The guard suppresses nested computes;
it does not provide transactional rollback or queue equipment changes made by a
callback. A retry rebuilds the point arrays, not arbitrary external side effects.
Legacy leaves still exist for moving-character motion duration, affects/pets and
some level-up, save, mount and Gaya operations. This is not a fully legacy-free
server. Factory/bootstrap changes are compiled, not executed by these headless
tests. Before deployment test login, every NPC/monster/stone/horse spawn type,
equipment swaps, buffs, mounting, alignment transitions, death and logout/relog
with the real client, prototypes and database.

## Quickslots

`QuickslotTests` compiles the complete production `ecs/systems/InventorySystem.cpp`
with entity-only fixtures. Quickslot implementation stays in that existing file;
`CHARACTER::m_quickslot`, its pointer-returning getter, add/delete/swap methods
and unused chain helper are removed. The remaining CHARACTER sync entry point
only forwards unmigrated callers; it stores no mirror. Its byte-sized arguments
are still a legacy boundary: the new native sync API accepts wider cells and
rejects values that cannot be represented instead of wrapping them.

The native setters validate before removing duplicates, commit the complete
state before packet publication and suppress an obsolete publication after a
nested modification or owner destruction. Relocation and database hydration
retain the last valid duplicate, matching the old ordered assignment behaviour.
Factory hydration filters malformed entries before publishing the component.
Login sends that completed state (including empty-slot clears); save reads ECS
values, and auto-give no longer sends a second add packet itself.

```powershell
cmake --build build --config Release --target GameServer QuickslotTests
ctest --test-dir build -C Release -R '^quickslots$' --output-on-failure
cmake -S . -B build-asan
cmake --build build-asan --config RelWithDebInfo --target QuickslotTests
ctest --test-dir build-asan -C RelWithDebInfo -R '^quickslots$' --output-on-failure
```

Checks enumerate all byte-sized type/position combinations and invalid target
indices; exercise add/delete/swap, duplicate removal, item relocation/removal,
wide-cell rejection, invalid/stale/recycled owners, nested publication, malformed
and duplicate persisted slots, full login publication and value-copy roundtrips.
Client-facing validation covers missing/foreign/stale items, allowed item types,
extra-inventory alias 12, and potion rejection for both alias and canonical
extra-inventory types without modifying the caller's request value.

Item lookup and network services are doubles. The target verifies requested
packet kind, recipient, ordering and values, not actual socket serialization or
client rendering. It executes the production client-validation service, not the
input dispatcher/framing code. The thin packet handlers, network packet builders,
factory and character-save call sites are compiled with GameServer; the complete
login/DB-save flow is not executed headlessly. Existing packet framing still owns
the minimum-length checks. Other inventory/equipment/spatial services fail if
the tests accidentally call them. Test shortcut drag/drop/delete/swap, potion
auto-assignment, item movement/consumption, empty-slot clearing on login and relog
persistence with a real client/DB before deployment. Bulk item-manager shutdown
still needs migration.

## Item-manager creation and destruction

`ItemManagerLifecycleTests` also exercises the production target-info drop
preview for stones and normal monsters. Configured `drop` entries and every
`kill` choice must be listed with their configured counts, without calling RNG.
Repeated queries, empty/null groups, invalid entities, player targets and zero
item/count entries are covered. This changes the preview only, not actual loot
rolls or probabilities; packet delivery and deployed drop data still require an
in-game server test after rebuilding/deploying GameServer.

`ItemManagerLifecycleTests` compiles the complete production `item_manager.cpp`,
`safebox.cpp` and `Base/grid.cpp`. Both high-level `RemoveItem` and low-level
`DestroyItem` read identity, location and ownership from ECS; an optional legacy
allocation is resolved only at the final release boundary.
The existing implementation is edited in place, not duplicated into another system.

`CreateItem` now operates on entity identity, quantity, sockets, attributes and
SIG group after its explicitly retained CItem allocation/bootstrap boundary.
It snapshots prototype rules before initialization callbacks, rejects live
duplicate entity IDs and VID exhaustion, and normalizes gold/non-stack/MAKECOUNT
quantities before allocation. Initialization suppresses delayed persistence;
successful completion queues one save without a legacy state resync. Timer
startup follows payload preparation; fully charged souls correctly skip growth
timers. Initial socket timestamps are saturated to the signed 32-bit storage
range rather than overflowing. This does not extend the wire/storage format.

Creation checks execute the real manager path against both entity-only and
legacy-backed factory doubles. They cover new/load/skip-index behavior, special
item payloads, magic/addon/blend dispatch, SIG selection, failed allocation,
count/rune/dragon-soul/timer initialization, callback deletion, recycled entity
generations, ownership transfer, recursive duplicate creation, exceptions and
rejected rollback. Cleanup preserves externally acquired ownership and failed
retirement indexes; it does not overwrite callback state after final publication.
Skill-book selection is shared with quest rewards and implemented once in this
manager file; exhaustive candidate/job/boundary tests exercise the actual bounded
selection code. Empty skill data now fails instead of spinning indefinitely.

The real rune setup/tier calculation is additionally executed by
`ItemAttributeTests`: all seven rune types, both attributes, tier boundaries,
integer rounding, malformed durations (including the old zero-divisor cases),
charge potions, missing components and recycled handles. Creation requires a
valid rune duration of at least 100; gameplay duration tables need checking.

Creation factory/bootstrap, timer, blend, magic/addon, dragon-soul, save and
packet services remain doubles in the manager target. These checks do not
exercise real EnTT bootstrap signals, legacy allocation internals, SQL or full
engine event callbacks. Rune calculations and attribute/DS operations have
separate real-code tests, not a complete combined creation integration test.
Before deployment test drop/reward creation, blend/rune/stole items, skill books,
new and fully charged souls, time-limited items, loading persisted sockets,
logout/relog and DB persistence. Bootstrap and bulk shutdown remain
separate migration work; this is not a fully pointer-free item factory.

```powershell
cmake --build build --config Release --target GameServer ItemManagerLifecycleTests
ctest --test-dir build -C Release -R '^item_manager_lifecycle$' --output-on-failure
cmake -S . -B build-asan
cmake --build build-asan --config RelWithDebInfo --target ItemManagerLifecycleTests
ctest --test-dir build-asan -C RelWithDebInfo -R '^item_manager_lifecycle$' --output-on-failure
```

Fixtures cover entity-only and legacy-backed items with entity-only owners,
including zero/mismatched last-owner PIDs, absolute equipment cells, missing or
foreign slots and invalid owners. They verify recursive manager removal during
ground/detach/factory callbacks, repeated destruction, recycled entity indices
and ID/VID reuse, ownership transfer during ground/detach callbacks (also to
stale owners), guard release after exceptions, rejected factory cleanup and
retry, mismatched legacy bindings, and persistent-item deferral without a DB
descriptor. Factory callbacks see unpublished indices; unsuccessful factory
cleanup restores still-live identities without overwriting newer mappings.
High-level tests cover normal/extra inventory, equipment, dragon-soul, switchbot
and mount windows, wide quickslot cells, logging/quickslot/mount callbacks,
reentrant removal, ownership transfer, recycled IDs and rejected detachment.
The real safebox/mall integration is exercised for removal, close during logging
or detachment, shared-lease lifetime, failed removal and transfer to another owner.

Item access, inventory, factory, logging and DB services are doubles. Legacy
CItem/CEntity construction and destruction are doubles too: the tests verify
allocation release order, not engine timers, component destruction callbacks,
spatial removal, packet delivery or durable persistence. Unrelated manager
drop dependencies fail immediately if called. Bulk shutdown `Destroy`
still needs migration. The ItemSystem dispatch now uses the same high-level
manager path for entity-only and legacy-backed items; this adapter is compiled
with GameServer, not executed by this target. Native mount lookup/packet code
and session adapters are also compile-checked only. Point computation still
uses the existing legacy-backed PointSystem service; its internals are not
migrated by this change. General client-facing inventory lookup is unchanged:
storage-window lookups are limited to the manager's internal removal path.

The missing-descriptor guard is not a reconnect queue or a DB acknowledgement.
It does not undo side effects already performed by a higher-level caller, nor
does factory failure roll back a previously buffered DB delete. Direct factory
callers and engine callbacks that independently delete legacy allocations are
outside this manager reentry guard. Before deployment test expiry, ground-item
removal, inventory/equipment/mount removal, last-unit consumption, disconnect,
shutdown, DB interruption/reconnect and relog with the actual client and DB.

## Safebox and mall lifecycle

`SafeboxLifecycleTests` compiles the existing production `safebox.cpp` and
`Base/grid.cpp`. The owner and every item are versioned EnTT entities with no
`CHARACTER`, `CItem`, or legacy-pointer component. The implementation remains
in `safebox.cpp/.h`; there is no parallel ECS copy of the storage system.

```powershell
cmake -S . -B build
cmake --build build --config Release --target GameServer SafeboxLifecycleTests
ctest --test-dir build -C Release -R '^safebox_lifecycle$' --output-on-failure
```

For ASan, configure `build-asan` with `ENABLE_ASAN=ON`, build the same test target
in `RelWithDebInfo`, and run the test with `ctest --test-dir build-asan -C
RelWithDebInfo -R '^safebox_lifecycle$' --output-on-failure`.

Coverage includes both storage windows, detached-item attachment, duplicate and
overlapping placements, multi-cell footprints, invalid heights/positions,
growth up to the fixed item-array limit, and preservation of occupied cells.
It also covers invalid/NPC/recycled owners, recycled items, items moved to
another owner/window/cell, failed publication/detachment, and teardown after
owner destruction. Reentrant flush/detach/destroy callbacks see an already
unpublished container and cannot add, remove, move, save or destroy it again.
Post-callback item identity/ownership is revalidated before further work.

Grid ownership is RAII; copy/move construction is disabled. SafeboxRef owns
both containers through shared leases; CHARACTER no longer has duplicate raw
owning pointers. SafeboxSystem lives in the existing safebox.cpp/.h. Close
unpublishes the container before callbacks, blocks reopening that window during
teardown, and retires contents immediately even if another caller holds a lease.
Component tests cover independent windows, stale owners, callback reentry,
lease release and post-close reopen. Session/character teardown adapters are
compiled by GameServer, not executed in the headless tests; legacy GetSafebox/
GetMall borrowed-pointer accessors remain for unmigrated callers.

Persistence, descriptor lookup, item detachment/destruction and stack-count
services are doubles. No socket, DB, CHARACTER, CItem, or item-manager instance
is created. Tests cover safe no-DB saving, not successful account-save packet
delivery or actual engine item cleanup. Stack tests cover locks/exchange flags,
caps, oversized counts, normal partial/full merges and rejected consumption.
The existing debit-then-credit merge is still sequential: a credit failure
after a successful debit is logged but cannot be rolled back reliably. No DB
transaction, durable recovery, or general atomic stack-transfer API is added.

Before deployment verify safebox/mall load, deposit/withdraw/merge, resize,
close/reopen, relog/disconnect and DB interruption on a test server. Confirm
that closing unloads runtime items without deleting their stored DB rows.

## Item attributes

The `ItemAttributeTests` target compiles the production `ItemAttributeSystem.cpp`
and the complete `new_switchbot.cpp` and `DragonSoul.cpp` with an EnTT registry and headless inventory,
payment, persistence, network, and RNG doubles. Items
in the fixtures have no `CItem`/`LegacyItemPtr`, and no database is started.
Switchbot implementation and declarations stay in `new_switchbot.cpp/.h`;
the tests compile that complete production translation unit. Timer/UI/manager
service doubles fail immediately if called, so these transaction tests cannot
silently exercise unimplemented server services.

From the repository root:

```powershell
cmake --build build --config Release --target GameServer ItemAttributeTests
ctest --test-dir build -C Release -R '^item_attributes$' --output-on-failure
```

For an existing `build-asan` configured with `ENABLE_ASAN=ON`:

```powershell
cmake -S . -B build-asan
cmake --build build-asan --config RelWithDebInfo --target ItemAttributeTests
ctest --test-dir build-asan -C RelWithDebInfo -R '^item_attributes$' --output-on-failure
```

Checks cover invalid/stale entities, invalid attribute sets, empty and
zero-weight tables, weighted probability boundaries (including partially
assigned level distributions), locked normal bonuses, rare-slot gaps,
atomic failure on insufficient candidates, addon removal, duplicate bonuses,
costume values, and force-attribute bounds/client updates.

Paid operations also cover failed preparation, rejected payment, insufficient
stacks/Yang, foreign or detached materials, locked/exchanging/equipped items,
last-unit material destruction, stale handles, wrong switchbot owners/slots,
limited and Zodiac changers, stack selection, and atomic costume reset. The
payment doubles assert that original attributes remain unpublished until debit.

The costume change/add/remove entry point runs with entity-only characters and
items. Checks include strict dialog selection parsing, invalidation of a prior
selection after malformed input, negative/overflowed indices, empty rare slots,
rare-slot compaction, duplicate bonuses, signed socket values and narrowing
bounds, invalid owners/materials, and exactly one publish after payment.
These tests call the production selection handler and operation, not the chat
packet dispatcher or UI. In-game, verify both remover choices, addition of the
sixth/seventh bonuses, and that rejecting an equipped costume leaves its buffs
unchanged. The legacy CHARACTER selection field/accessors have been removed;
the state now belongs to the character's CostumeAttributeSelection component.

Stole enchant tests cover all four variants at grades 1-4, the existing cap
for higher positive grades, rejected zero/negative grades, six bonuses in one
commit, and preservation of the seventh slot. Attribute-lock tests enumerate
every add/change candidate, skip the current/empty slots without an RNG retry
loop, reject changes with no alternative, and allow the remover to repair a
malformed stored lock. All four operations cover owner/material validation,
equipped/exchanging/locked items, rejected payment and last-unit consumption.
The payment doubles also assert that the lock index has not changed before
debit. The native lock getter/setter are compiled from ItemAttributeSystem.cpp;
only their network and persistence services are doubled.

In-game, check stole enchant at each grade and all three lock consumables,
including rejection on equipped items, lock preservation during subsequent
bonus rerolls, and persistence after relog. The item-use dispatcher and its
chat/buff side effects are not executed by these headless tests.

Dragon-soul checks cover entity-only initialization, bonus refresh and paid
enchant, missing/failed table loads, reload ownership, invalid counts/weights,
NaN/infinity/narrowing bounds, weighted sampling without replacement (including
zero weights and both RNG endpoints), stale handles and rejected payments.
The original bonuses remain unchanged until preparation and payment succeed;
one complete update clears obsolete additional slots. Active stones are
rejected, while inactive equipped stones retain the legacy allowance. Tests
cover the absolute main-inventory cell used for equipped dragon souls.

The DS table loader/accessors are doubles, not the real text parser or deployed
balance tables. Refinement/extraction/activation services fail immediately if
entered; the tests call the real attribute refresh, not the complete strength
refinement flow. Before deployment, verify the live DS table, new stone creation,
enchant in the DS bag and inactive equipment, active-deck rejection, strength
refinement, and relog. Failed new-stone initialization now returns no item after
cleanup; the actual item-manager creation/destruction path needs in-game testing.

The doubles verify save/update calls, not the DB packet encoding or a live
client session. Test in-game inventory, relog, bonus changers, rune items and
mount attributes separately before deploying.

The transactions are synchronous game-thread operations, not cross-service DB
transactions or recovery from process termination. An item payment uses one
validated stack (the current switchbot cost is one item). The tests do not run
the switchbot timer/UI, shop listings, rank/Battle Pass side effects, the actual
inventory destruction path, or the legacy `ChangeKKAK` special-case path.

## Dragon-soul extraction and pull-out

The per-stone ActivateDragonSoul/DeactivateDragonSoul operations now also use
only entity ownership/wear-slot checks and native logging. Their redundant Ecs
resync wrappers and the final CHARACTER-based DSManager state refresh were
removed. Active socket writes use existing components without emitting a
callback before the point operation. Repeated/reentrant toggles do not apply
points twice, and activation does not restart a timer after nested deactivation.
The tests cover deck/slot/owner validation, signed lifetime bounds, expired and
permanent stones, failure compensation, callback cancellation, owner/item
destruction, recycled generations and preservation of another active stone.

Point modification and wear timers are service doubles in these checks; the
internals of ModifyPoints and the real timer scheduler are not made atomic or
fully reentrancy-safe by this change. Live bonus arithmetic, expiry, relog and
deck switching still need integration testing. Deck orchestration is a separate
migration step from these individual item operations.

DragonSoulSystem deck orchestration now uses fresh component lookups after
callbacks, scoped owner-operation guards and component-destruction tracking.
Read-only deck queries no longer create runtime state. Constructor callbacks
may destroy an owner without leaving a dangling emplace return value. Nested
disable/logout requests cancel in-progress activation; explicit disable clears
selection/affects, while logout preserves them. Hydration resets all loaded
active flags before publication, and repeated hydration removes existing
runtime bonuses first. Set bonuses require the same complete, active, unexpired
set before and after each affect callback, not stale vnums from earlier slots.

The existing RefineWindowTests now also exercises both decks, same-deck
idempotence, switch/cleanup/disable, partial and expired sets, failed affects,
nested activation/cancellation, removed/replaced stones, destroyed/replaced
runtime components, constructor/dirty-tag destruction and repeated login
hydration. Its stone activation, affect, save and network services are doubles;
these tests complement, but do not combine, the real per-stone tests above.
LoadAffect calls Initialize with its existing entity; the CHARACTER initializer
shim was removed. Live login/logout, set-affect arithmetic and timers still
require a client/server integration test.

Deck verification on 2026-09-08: GameServer Release build and all 13/13 tests
passed in Release and ASAN RelWithDebInfo; RefineWindowTests ran 2,516 checks.

Verified on 2026-09-08 with Windows/MSVC x64: GameServer Release build, all
13/13 headless tests in Release and AddressSanitizer RelWithDebInfo, including
3,632 checks in ItemAttributeTests. No live server/client/DB test was performed.

`ExtractDragonHeartEcs` and `PullOutEcs` now contain the implementation in the
existing `DragonSoul.cpp`; the CHARACTER-taking methods and their legacy-state
resync wrappers were removed. The four still-legacy use/move callers hand off
the owner entity at that boundary. This does not migrate the main MoveItem or
UseItemEx dispatchers, the other DS refinement wrappers, or the whole DS system.

`ItemAttributeTests` executes both complete extraction algorithms with no
CHARACTER/CItem fixtures. Coverage includes ownership and slot anchors, stale
owners/materials, aliasing, equipment/lock/exchange restrictions, extractor
subtypes, blocked inventory handling, malformed probabilities/charging values,
failed output creation/socket initialization, callbacks invalidating inputs,
occupied/full destinations, rejected placement and equipment recovery, source
destruction during removal, recursive extraction, last-unit costs, deferred
destruction/retry, and post-commit owner destruction without an orphaned reward.

Both costs commit together through the existing shared item-cost/retirement
engine before its first save/packet/destruction callback. Dragon-soul inventory
consumption is explicit opt-in; ordinary attribute costs retain their previous
inventory policy. Equipped stones are moved to validated DS storage before
charging or retiring them. A failed placement/payment attempts to restore only
the original live item, never a replacement or an item transferred by a
callback. Occupied original wear slots are not overwritten; a detached stone
falls back to a free DS cell, with unresolved recovery logged.

The deployed extractor rules are preserved: pull-out uses the extractor's
chance instead of adding it to the base chance; a missing pull-out row allows
free guaranteed removal; ENABLE_DS_EDITS takes heart charge from the extractor.
Invalid nonfinite/out-of-range configuration is rejected. Byproducts are
prepared before destructive changes, so allocation failure leaves inputs intact.

Only the algorithms and batch commit/retirement engine are real in this target.
Table loading, RNG, inventory/equipment mutation, creation, destruction,
automatic reward delivery, logs, chat and persistence are controlled doubles.
Inventory/equipment engines have separate QuickslotTests coverage, but live
DS deactivation/bonus/timer callbacks and reward delivery are not integrated
here. A committed cost is not refunded after a later callback destroys its
owner; undeliverable detached outputs are logged and cleaned up. This is not a
durable DB transaction or recovery from a process crash.

Before deployment test equip/pull-out in both decks, all grades, full DS bags,
both extractor types, success/failure/no-table-row behaviour, heart charging,
byproducts, ground delivery with full ordinary inventory, disconnect and relog
against the actual DS table, client, timers and DB.

## Costume attribute transfer

The same `ItemAttributeTests` target also compiles the complete existing
`attr_transfer.cpp`. The window, NPC binding and all three selections are ECS
state; the command handler and operation run without `CHARACTER` or `CItem`.
Selections retain the original inventory cell as well as the versioned entity.
The tests cover strict command parsing (including the client's `del` alias),
negative/overflow/trailing-junk indices, invalid/stale owners and NPCs, distance
and map changes, conflicting windows, duplicate/replaced/moved/foreign items,
locked/exchanging/equipped items, invalid costume types and stacks, malformed
bonus types, rare-slot clearing, rejected batches, reentrant operations and
exactly one target-attribute publication after the complete batch commits.

The batch payment, retirement queue, attribute preparation/commit and full
transfer command/window implementation are real production code. Tests assert
that every count and the new attributes are already visible at the first
save/packet/destruction callback, without invoking sequential ConsumeItemEcs.
They cover 1-64 costs, rejected empty/oversized/duplicate/invalid batches,
last-unit retirement, failed cleanup and retry without a second debit,
recursive processing, queue growth from callbacks, stale/recycled handles,
post-commit target destruction and nested changes not overwritten by an older
publication snapshot. Pending zero stacks cannot be spent again.

Only synchronous, main-game-thread state changes are atomic. After that commit,
deletion failure means pending cleanup, not a failed payment or a refund. The
item-manager update/shutdown retries cleanup; failed entries remain retired and
are logged once. No DB transaction, journal or cross-process crash recovery is
implemented: separate save/delete messages can still persist partly if the
process or connection fails. This is not a durable all-or-nothing guarantee.

Position, quest lookup, chat, logging, inventory lookup, network/save and actual
item destruction are doubles. The native count getter, setters and signed-delta
operation now run as production code in ItemAttributeTests. Checks cover invalid
and stale handles, missing count components, configured stack caps, UINT32_MAX
inputs, independent INT_MAX gold caps, signed-delta overflow, rejected zero-count
removal, pending-retirement write guards, nested updates and deletion/component
removal during save or packet callbacks. A positive result means the count
committed, not that the item survived subsequent callbacks; zero writes report
verified removal instead of first publishing a zero-count stack.

ItemCount is the only count storage: CItem has no count member, and legacy
resynchronization cannot overwrite it. The remaining CItem GetCount/SetCount
methods are compatibility boundaries for unmigrated callers. The old SetCount
branch accessed the deleted CItem; it is removed, including its obsolete
CHARACTER refine-window-close shim. These boundaries and factory hydration are
compiled with GameServer, not executed by this headless count fixture. The real
item-manager tick/persistence integration, inventory split algorithm
and full gameplay callback graph are not covered here. Verify stack caps,
splitting/merging, zero and last-unit consumption, quickslots and DB
reconnect/logout on a test server.

Before deployment verify NPC interaction, each supported costume subtype,
selection/preview/clear/close, competing trade windows, disconnect/death,
successful consumption and relog persistence with the actual client and DB.

## Entity-native stack merges

`ItemAttributeTests` runs the real pairwise merge and automatic detached-reward
merge in the existing `ItemAttributeSystem.cpp`, together with count publication
and deferred retirement. Both counts commit before any save, packet or deletion
callback. Depleted sources cannot be reused even when deletion must be retried.
The result records the committed quantity, not whether callbacks kept either
entity alive. This is main-thread in-memory atomicity, not a durable DB transaction.

Checks include 1,890 source/count/cap/request combinations, zero-as-all and
oversized requests, partial and full merges, invalid/stale/foreign handles,
slot anchors, normal/extra inventory, equipment/ground/storage exclusions,
locks/exchange flags, sockets, attributes, prototype flags and identity metadata.
Different payloads are not silently discarded by a count-only merge. Blend rewards
keep their remainder when a destination reaches its cap. Automatic delivery uses
a bounded snapshot of anchored entities in slot order, without retaining registry
views or component references through callbacks. Tests cover recursive merges,
recursive reward delivery, failed retirement/retry, publication exceptions,
owner/source/destination deletion, generation reuse, moved/replaced/locked
candidates and stale slot aliases.

The old automatic and blend merge implementations were removed. Inventory drag
now calls this operation from the entity-native MoveItem transaction described
below, without an LPITEM destination or sequential consume/add. Its negative-count
check does not apply abs(int) to INT_MIN. AutoGive now dispatches this same
full-payload merge after item creation; the former VNUM-only credit path is
removed (delivery coverage is described below). Inventory lookup, extra-item category,
item validity/lock/owner access, save/network and actual deletion are doubles.
Verify client drag/drop, quickslots, blend rewards, full inventory, disconnect
during rewards and relogged persistence with the real client and test database.

## Entity-native item delivery

AutoGive's existing/new-item and dragon-soul delivery share one implementation
in the existing `InventorySystem.cpp`. The old implementation in `ItemSystem.cpp`
and the duplicate CHARACTER item-pointer/DS overloads and socket helper are
removed. The CHARACTER VNUM overload remains only as an entry adapter for
unmigrated callers; it rejects nonpositive signed counts and forwards rarity,
message and highlight settings. No new production file is introduced.

Creation precedes merging, so the actual sockets, attributes, identity metadata
and anti-stack rules reach the existing native transaction. A reward entity can
have only one active delivery; losing a fully merged receipt never recreates it.
Inventory insertion uses the acquisition path (including rune auto-equip and
accessory initialization), not the pure transfer path. Every later log, chat and
quickslot operation revalidates the owner and anchored receipt. Auto-assignment
of potion shortcuts is restricted to representable main-inventory cells.

One VNUM call produces one normalized item/stack: nonstackable counts become one,
stack counts/MAKECOUNT minima are capped at the configured stack limit and gold
at INT_MAX. Zero requests and invalid signed MAKECOUNT data are rejected. Unlike
the removed pre-credit loop, oversized input no longer yields an amount dependent
on free capacity in existing stacks. Callers needing more than one stack must
request several rewards explicitly. Messages and money logs use normalized
quantities. Check deployed reward tables against this contract before rollout.

Failed new-item placement retires only its still-detached temporary entity;
caller-supplied items remain the caller's responsibility on failure. Committed
placement is not rolled back after a publication callback, and AutoGiveDS retains
its committed-success result even if the receipt is deleted during publication.
The entity-returning API can return null after committed callbacks, so null is
not permission to replay a reward. Partial stack transfers are not a durable
all-or-nothing reward transaction. Failed retirement remains logged/recoverable.

`QuickslotTests` executes the real dispatcher, queries, acquisition/placement,
equipment and shortcut code with entity-only fixtures. It checks normalized
counts, rarity/highlight/message flags, full/partial/missing merge receipts,
same-item recursion, normal/extra/DS routing, wide extra slots, ground fallback
protection, rune/accessory acquisition, creation failures, exceptions, entity
recycling, owner destruction, reownership and failed cleanup. Creation, merge,
spatial membership, scheduling, persistence, packets and logs are controlled
doubles here; ground placement/claims/timer callbacks execute their real code.
Real creation and merge algorithms have separate tests, not a single integrated
live-server test. Actual CItem allocation remains a legacy boundary.
Verify quest/drop/refine rewards, full inventories, acquisition effects,
disconnect/relog and database persistence with the real client before deployment.

## Entity-native inventory drag and stack splitting

`InventorySystem::MoveItem` replaces the entire `CHARACTER::MoveItem` body and
declaration, in the existing InventorySystem.cpp. Input dispatch passes the owner
entity and copied packet positions/count directly; the two remaining item-use
call sites also enter this transaction. There is no new pointer conversion or
parallel implementation file. The old duplicate mount-bonus whitelist is reduced
to exactly equivalent ranges/singles, including its four excluded mount IDs.

Ordinary relocation commits source/destination anchors, footprints, location,
ownership and affected quickslots before external publication. Split preparation
creates a detached item before any source debit, retains sockets, attributes,
attribute lock, flags and identity metadata while keeping the new persistent
ID/VID, then commits both quantities and placement without callbacks in between.
Allocation, component-construction and timer callbacks force revalidation of
source identity, payload, quantity, ownership, restrictions and destination.
Failed preparation retires only the original still-detached temporary item;
committed operations never refund or replay old state over a callback's changes.
The split DB audit is retained through the native entity logging entry point.

The packet operation explicitly routes normal, extra, dragon-soul, switchbot and
equipment windows. A DS inventory cell cannot be mistaken for a numerically
overlapping belt/equipment cell. It rejects aliases, invalid footprints, negative
counts, inaccessible destinations, active switchbot entries, locked/exchanging
or pending-consumption items, and wrong DS/extra categories. Belt whitelist,
level/group uniqueness, removal affects, point recalculation and overhead updates
are preserved. Belt stack splitting is rejected to preserve one-per-type entries.
Safebox, mall and account-mount windows require their dedicated protocols.
Equipment drag dispatches the existing native equipment engine; explicit unequip
uses the requested destination when available (with the existing callback-safe
recovery), and DS equipment extraction goes through PullOutEcs. Dragging equipment
onto an occupied slot is rejected before unequipping a dependent costume.

The existing `QuickslotTests` target executes the real MoveItem implementation,
storage transactions, queries, quickslots and equipment engine with entity-only
fixtures. It covers multi-cell/overlapping moves, wide DS/extra cells, full moves
and splits, packet payloads, merge/extraction dispatch, equipment aliases, belt
restrictions and refreshes, switchbot registration, failed/clamped creation,
changed source payload/count/flags, stale/recycled entities, reentrant moves,
component destruction, timer cancellation, publication-time transfer/deletion,
and split/full-move reclassification during preparation.

Factory, metadata/accessor queries, split retirement, DB logging, merge and
DS-extraction dispatch, switchbot, points, mount/leaderboard and packet services
are controlled doubles here. Pairwise merges and extraction have separate real
implementation tests; this fixture does not exercise actual item allocation,
database durability, socket transport, leaderboard SQL or complete engine
callbacks. Atomicity here means synchronous main-thread in-memory state, not a
durable database transaction. Allocation and other systems still contain legacy
internals. Run client drag/split/equip, belt bonuses, switchbot transfers,
disconnect/relog and persistence checks on a test server before deployment.

## Mount and pet lifecycle regression tests

`MountLifecycleTests` compiles the complete, existing `MountSystem.cpp` and
`PetSystem.cpp`, plus `New_PetSystem.cpp`; there is no parallel production
implementation.
It exercises owner, follower and summon-item handles with entity-only fixtures,
without creating a `CHARACTER` or `CItem`. Checks cover summon/unsummon, repeated
destruction, stale owners/items/followers with recycled entity indices, ownership
changes, reused item VIDs, failed spawn/show, follow and map transitions, expired
items, malformed/missing prototypes, war restrictions, skin changes, mounting,
unmounting, cancelled timers and replacement subsystems/actors.

Pet checks additionally cover item locks/socket state, owner-death survival,
follower death, update cadence, walk/run thresholds, follow options, multiple
pets, complete UnsummonAll, reentrant ComputePoints -> RefreshBuff during actor
deletion, nested Destroy, and independent updates after another actor's AI fails.
They verify stable pet identity and dungeon-bonus restrictions under skins,
failed skin respawns, duplicate item binding rejection, stale-owner callbacks,
prototype-bonus removal after item destruction/dungeon exit, skill-bonus
removal encoding, malformed apply types and unnegatable bonus values.

```powershell
cmake --build build --config Release --target GameServer ItemAttributeTests MountLifecycleTests
ctest --test-dir build -C Release -R '^(item_attributes|mount_lifecycle)$' --output-on-failure
cmake -S . -B build-asan
cmake --build build-asan --config RelWithDebInfo --target ItemAttributeTests MountLifecycleTests
ctest --test-dir build-asan -C RelWithDebInfo -R '^(item_attributes|mount_lifecycle)$' --output-on-failure
```

Factory, spatial movement, horse, affect, timer, item-point and packet services
are doubles. The point-calculation double resets the fixture bonus total and
calls the real PetSystem::RefreshBuff, including during deletion; it is not
the complete CHARACTER::ComputePoints/ItemSystem::ModifyPoints implementation.
The tests do not execute the real `SpawnMobEntity` allocation, pending character
destruction, engine movement/network code, login/logout or persistence. They do
not execute the new native walking packet service or command/packet dispatch.

Before deployment verify NPC/monster/metin spawning (including event-spawned
metins), mount follow across sectors/maps, name/skin display, riding/unmounting,
item expiry and logout/relog, and pet summon/skin/bonus behaviour in-game.
`SpawnMobEntity` owns the shared spawn implementation, and no spawn entry point
returns a pointer any more. Allocation, movement and horse
services still contain legacy internals: this is not a fully legacy-free server.
Growth-pet checks use a fake SQL client and database (no connection or worker
threads), and exercise the actual SELECT parsing, save queries and actor code.
They cover missing/invalid rows and fields, null columns, range checks, stale
owner/item/follower generations, independent update/expiry timers, one-minute
expiry without unsigned underflow, no duration loss from summon/skin refresh,
initial creature-level publication, non-levelling DB hydration, evolution,
item-ID-based escaped renaming, rejected slot indices, selected-item replacement,
failed consumption, duplicate/foreign/locked skill books, multi-pet teardown
and retained callbacks against a replacement system.

The SQL and skill/EXP tables are fixtures, not the live schema, deployed balance
data, or actual MySQL escaping implementation. The active ENABLE_NEW_PET_EDITS
configuration is covered; alternative legacy skill rules are not runtime-tested.
The test packet/affect doubles do not simulate the complete client UI, nested
engine affect recalculations or real network insert/reencode serialization.
Before deployment, verify pet seal DB rows (level 1-120, evolution 0-3, valid
skill slots/levels, positive remaining duration), egg-to-first-summon, logout/relog,
age/expiry, evolution/EXP gates, skins, rename and feeding/skill windows in-game.
Runtime factories, movement, affect calculation and persistence still
have legacy internals; removing pointer round trips here is not a full engine
or DB-layer migration.

## Native item lifecycle and pickup (2026-09-12)

The runtime item pointer type, allocation class and pointer-binding component
have been removed. Item creation/retirement now initializes and retires EnTT
components directly in the existing ItemSystem implementation. `item.h` retains
only native event payloads; there is no parallel replacement item class.

`ItemRuntimeTests` executes the real native item factory, identity registry,
prototype refresh and pickup implementation. It checks masked/ranged identity,
ID-less gold, duplicate IDs/VIDs, initial component values, prototype removal,
extra-inventory classification, callback-time component removal, exceptions and
recycled generations. Retirement tests include recursive destruction, ownership
transfer, retry after an exception, and entity destruction during cancellation
of the first of eight timers.

Construction uses single-element insertion: EnTT's `emplace` would perform a
trailing component lookup after a construction callback had removed that component
or destroyed its entity. The sanitizer regression covers both cases. Prototype
snapshots also use the weapon magic-damage fields 1/2; the former initializer
incorrectly accessed field 6 of the six-element value array.

Pickup fixtures cover normal/extra/dragon-soul routing, stale or transferred
items, dead/observer characters, full inventories, cross-map and overflowing
coordinate differences, placement rollback, and reentrant/rejected gold
retirement. Storage insertion, sector lookup, party, quest, battle-pass,
network, persistence and stack-merge services are controlled seams here:
these tests do not simulate an entire connected game client.

`ItemAttributeTests` separately executes the real ground-to-inventory stack
transaction, including 255/256/700/1000 counts, complete payload comparison,
ground/ownership/lock guards, atomic two-stack publication and deferred
retirement without crediting the destination again on retry.

`ItemManagerLifecycleTests` retains creation/removal/persistence/reentry
coverage with native fixtures only. It additionally exercises entity-only
shutdown, index mutation during shutdown, rejected-detachment retry, native
prototype rebinding, removed ranges and empty reloads. Its low-level factory is
a double; actual factory behavior is covered by ItemRuntimeTests.

Run all configured tests in both normal and AddressSanitizer builds:

~~~powershell
ctest --test-dir build -C RelWithDebInfo --output-on-failure
ctest --test-dir build-asan -C RelWithDebInfo --output-on-failure
~~~

Before deployment, verify pickup (including party reservations and battle pass),
gold, inventory/equipment/DS/extra/mount storage, offline-shop loading, dungeon
purge, item expiry, logout/relog and persistence on a test server. The item
identity/lifecycle is native; character internals and database/prototype pointers
are not all eliminated by this migration.

## Native horse rider runtime (2026-09-13)

`CHorseRider` and its CHARACTER inheritance/wrappers are removed. The existing
horse_rider files now implement entity-based MountSystem operations. HorseRuntime
owns level, health, stamina, decay deadline and event handles; MountState remains
the sole riding flag. DB loading/storing uses THorseInfo snapshots, not a second
mutable horse object. Timer payloads hold versioned entities, never character
pointers. Component removal cancels both timers, and callbacks check event
identity before touching or rescheduling a rider. Publication does not retain
component references across callbacks.

HorseRuntimeTests links the real horse implementation and event queue. It checks
hydration, malformed DB ranges, offline recovery, level/stat limits, mounting
policies, consumption/regeneration, exhaustion, health decay, revival, shutdown,
component removal, entity recycling, stale/replaced timers and reentrant or
destructive packet/quest callbacks. No CHARACTER object is allocated. Player,
network, quest, skill, point and world-spawn services are doubles; this suite
does not validate live mount models, DB I/O or client rendering. The pre-existing
fixed horse grade of 2 is intentionally preserved.

The emotion command entry points also pass entities directly, including target
lookup and gender/riding checks. Their packet/UI behavior still needs in-game
verification. Costume mount actors, account mount inventory loading, descriptor
ownership, quest contexts and other CHARACTER internals are separate remaining
migration work; this is not complete LPCHARACTER removal.

Before deployment, test login while mounted, logout/relog, horse feeding and
revival, zero-stamina dismount, horse skills, GM horse level/stat changes,
costume mount switching and two-player emotions on a test server.

## Entity-owned quest runtime (2026-09-21)

The quest PC payload (flags, quest state map, running state and quest timers) is
no longer a PID-keyed manager map. It lives in ecs::QuestPCState on the
character entity; the component owns the PC through a unique pointer so the
running-state and timer addresses stay stable while the entity lives, and its
destruction cancels the PC timers. GetPC(pid)/GetPCForce(pid) resolve the
character through the existing PID registry; the pid-0 server timer keeps a
process-owned synthetic PC because it has no character.

DisconnectPC clears the manager's cached current-PC/party-member/other-block
pointers and removes the component. A process-wide on_destroy<QuestPCState>
hook clears those pointers when an entity is destroyed without an explicit
disconnect, so the manager never keeps a pointer into a freed PC. The PC class
internals and the quest Lua surface are unchanged; this phase moves ownership
and indexing only.

QuestRuntimeTests adds ownership checks: PC attachment and identity, one PC per
character, the stable server PC, offline pids returning none, disconnect
removal and entity-destruction cleanup of the cached manager state. It does not
cover live Lua script execution, DB flag persistence or login/logout with the
running server; those remain in-game checks.

## Validated mob prototypes and shared sector attributes (2026-09-21)

ecs::MobDataRef now stores the prototype vnum beside the CMob pointer, and
ecs::PlayerRuntime::GetProto only returns the pointer while CMobManager still
answers it for that vnum. Combat, rank/table and skill readers go through it, so
a reloaded or removed prototype table reads as no prototype instead of a stale
pointer. CMobManager::Initialize repoints every existing mob relation at the
freshly loaded table without rerunning spawn initialization; the previous rebind
loop only walked PCs and did nothing.

Private-map sector attributes are shared ownership now: SECTREE holds a
shared_ptr<CAttribute>, a clone copies it and either map can go first without
leaving the other on freed memory. Attribute writes still reach the origin
map's cells exactly as before; only the ownership changed.

CombatStateTests covers the manager-backed prototype read and stale-pointer
rejection; SpatialLifecycleTests covers clone sharing, origin-first destruction
and release with the last holder. Skill prototypes are not stored on entities
anywhere, so their reload path had no stored pointer to invalidate.

## Stable pet actor storage and the ASAN gate (2026-09-21)

ecs::PetRuntime::actors and ecs::NewPetRuntime::actors are std::deque
instead of std::vector. PetSystem::Summon/NewPetSystem::Summon return a
pointer into those containers; summoning another pet reallocated the vector and
invalidated every previously returned pointer, which AddressSanitizer caught as
a heap-use-after-free in MountLifecycleTests. A deque never moves its elements,
so the published pointer contract now holds. MountLifecycleTests dereferences
the first actor pointer after a second summon for both the regular and the
growth pet to keep the contract explicit.

The uild-asan configuration now builds GameServer, all 26 server test
targets and client TimerTests under MSVC AddressSanitizer and passes 27/27;
the Release configuration passes the same suite.

## Focused relation-regression fixtures (2026-09-21)

SocialRelationTests compiles the real SocialSystem.cpp and drives
GetGuild/GetWarMap with manager registry doubles. It stores a sentinel
guild/war-map pointer with no, stale or matching durable keys and asserts that
only the matching live relation is returned; the pre-fix code dereferenced the
sentinel to find its key, so the test is the regression guard for F1/F5.

OfflineShopSafeboxTests compiles the real OfflineShopSystem.cpp and covers
the safebox owner handover contract: a second safebox takes the back pointer
from the first, the clear path SessionSystem uses before the cache erase
reaches the live box, and null handles are rejected (F2 companion, F12).

Both targets run in the normal and AddressSanitizer configurations; the suite
is 29 tests in each. The remaining phase23 service fixes (F2 session ordering,
F3 pvp, F4/F8 guild manager, F6/F7 war-map events, F11 arena) still rely on
compilation and the existing suite; their focused fixtures would need the full
session, pvp, guild and arena service doubles.


## Lua 5.5 / direct quest sources (2026-10-21)

`LuaSourceTests` uses the actual Lua 5.5 VM and source parser. It covers
legacy operators, comments/strings, state IDs, repeated state blocks,
NPC/chat/target events, source-list errors, dialog yield/resume sequences,
64-bit integers, coroutine close, text-only loads and instruction budgets.
`QuestSourceMigrationTests.py --checker <ServerPackageCheck>` verifies isolated
migration, legacy state IDs, duplicate IDs and original source preservation.
Both are registered in CTest (`lua_sources`, `quest_source_migration`).
The manager's editor suite uses this same modern native parser.
See `docs/lua55_quest_migration.md` for deployment and gameplay checks.

`QuestRuntimeTests` also compiles the production coroutine execution,
generation switching, immutable snapshots, timer callbacks and reload control.
It checks an online player's suspended dialog retaining its old globals and
file contents while new events use the new VM, stable IDs/flags, loop timer
adoption versus one-shot retention, staging side-effect rejection, failed
syntax/state changes, and release of retired VMs.

`QuestHotreloadTests.py --core <QuestRuntimeTests> --checker <ServerPackageCheck>`
launches two native processes using that runtime and the production filesystem
publication protocol. It verifies shared activation, all-core abort, invalid
source rejection, postcommit timeout recovery, stale token rejection, state
removal rejection, ID stability and restart from a sealed committed generation
despite broken pending source edits. It is registered as CTest `quest_hotreload`.
These fixtures open no game/SQL ports and require no player credentials.
The manager backend and SSH GUI tests cover live saves under the publication
lock, restricted edit scope and the asynchronous remote reload command.

## Shared local/SSH targets (2026-10-21)

SQL-status coverage checks effective per-instance connection overrides, private
option-file cleanup, read-only queries, error classification, secret-free caching,
expiry/forced refresh/config changes, and copied Windows ownership on FreeBSD.
An external SQL failure must retain the package connection and its editable
settings. Qt coverage checks the SQL card, failing endpoint details, disabled
start and hidden local database controls. External SQL can remain running while
a stopped game server's configuration is edited.

Run `build/server-manager-env/Scripts/python.exe -B tests/server/ServerManagerTargetTests.py`
after building the native package validators. The suite sends the production
standard-library package RPC through local subprocess pipes instead of SSH.
It verifies remote inventory, encoding, server-side native validation, exact
backups/restoration, external edit conflicts, path boundaries, detached start
and stop, protocol version checks, and redacted log reads.

The Qt tests use two independent disposable packages. They check that the
overview, channels, configuration and documents follow the selected target;
remote saves never modify the local package; failed connections disable
operations; in-flight writes and unsaved edits prevent target switching; and
late or mismatched responses cannot replace another target's content.
No real SSH host, database or game process is contacted.

`ServerPackageToolsTests.py` also covers the FreeBSD/system-MariaDB branch:
per-role option files, password escaping/private cleanup, connection failure,
table-postfix rules, read-only readiness checks and absence of local SQL
bootstrap/start side effects. It remains part of CTest `server_package_tools`.

## Távoli buildtelepítés

A `ServerManagerRemoteWorkTests.py` a build és feltöltés mellett a négyprogramos
telepítést is ellenőrzi: valódi fájlcsere, ellenőrzőösszegek, mentés, zárolások,
futó folyamat/port elutasítása, konfiguráció/platform/kimenet ellenőrzése,
helyhiány, megszakítás és részleges csere utáni visszaállítás. Az ELF-futtatást
és a POSIX folyamatlistát a Windowsos tesztek helyettesítik; ezek külön FreeBSD
próbacsomagon tesztelendők. A Qt-tesztek ellenőrzik a profil célját, az új gombot,
a sikervisszajelzést és a futó/offline cél tiltását. A `ServerManagerTargetTests.py`
RPC-n is ellenőrzi, hogy befejezetlen telepítés után az indítás tiltott.

## Távoli FreeBSD-csomagkezelő

`build/server-manager-env/Scripts/python.exe -B tests/server/ServerManagerPackagesTests.py`
a Qt-környezetben futtatható. Ellenőrzi az argumentumok validálását,
a platform és a root jogosultság vizsgálatát, a shell nélküli parancsindítást,
a valódi alfolyamat-kimenetet és hibakódokat, valamint a `pkg install -n`
nem nulla, de sikeres terv-visszajelzésének szűk elfogadását. Az elavult tervet
a worker a telepítés előtt elutasítja; csak a sikeres telepítés és meglétellenőrzés
után küld sikervisszajelzést. A tesztek külön vizsgálják az átfedő tranzakciók
zárolását, a hibák naplózását és a megszakadt SSH-kliens melletti naplózást.

A Qt-próba valódi helyi alfolyamatokon vezeti végig a keresés → kijelölés → terv
→ telepítés folyamatot. Hiányzó, sérült vagy másik kéréshez tartozó válasz,
profilváltás és névváltoztatás nem engedhet korábbi tervvel telepíteni.
Kis ablakban a görgetés és a mindig elérhető telepítési gomb is ellenőrzött.
A teszt nem ér el valódi SSH-gépet, és nem telepít rendszerfüggőséget.
Valódi FreeBSD ellenőrzéskor a lekérdezést és a tervet írás nélkül futtasd;
a telepítési folyamatot külön tesztkörnyezetben vizsgáld.

## Crashfigyelő és core-elemző

`build/server-manager-env/Scripts/python.exe -B tests/server/ServerManagerCrashTests.py`
teszteli az esemény, az eredeti bináris és a friss core megőrzését, a korábbi
core elutasítását, a binárisváltozás és hibás architektúra kezelését, a debugger
indulási szkriptjeinek tiltását, a valódi alfolyamat-kimenetet és hibakódot,
az idő- és méretkorlátot, valamint a termelési SSH RPC-kódon át a jelszómaszkolást.
A Qt-rész ellenőrzi az aszinkron listát és elemzést, a célváltást és a
célgépenként megjegyzett nyugtázást. A `ServerPackageToolsTests.py` valódi
felügyelőpróbája a hibás indulás rögzítését és a szabályos leállítás csendjét is ellenőrzi.

Az automatizált Windows-próba szintetikus ELF-fejléceket és elkülönített helyi
debugger-alfolyamatot használ. FreeBSD-integrációhoz külön, hálózati portot nem
nyitó `-g -O0` C-próbaprogram SIGSEGV hibájából létrehozott valódi core szükséges;
a GDB és LLDB hibavermében a próba függvénynevét és forrássorát is ellenőrizd.
Ehhez az élő játékfolyamatot nem kell megszakítani.

## Mentési központ

`build/server-manager-env/Scripts/python.exe -B tests/server/ServerManagerBackupTests.py`
ellenőrzi a mentési archívumot, tartalomellenőrzést, importot, célútvonalakat,
symbolic-link elutasítást, felülírás előtti tervet, védőmentést, rollbacket és
megszakadt helyreállítás utáni indításvédelmet. A napi futás deduplikációját,
a megőrzési szabályt, a kézi mentések megmaradását és az idegen cron-sorok
megőrzését is méri. Qt mellett az aszinkron készítés, visszaállítás, egymásba
fűzött letöltési feladat és a termelési RPC-válasz is tesztelt; Qt nélkül
ezek kimaradnak. A Qt nélküli rész CTest: `server_manager_backups`.

A `ServerManagerRemoteWorkTests.py` azt is bizonyítja, hogy a build előtti mentés
hibája még a bináriscserék előtt megállítja a telepítést. A natív integrációhoz
friss, véletlen loopback porton futó saját MariaDB-példány kell: négy játékadatbázis,
MyISAM + InnoDB, BLOB/nullbájtok, view, tárolt rutin, utólag hozzáadott tábla,
majd visszaállítás és szándékos hiba utáni rollback. A rendszerséma érintetlensége
külön sentinel táblával ellenőrizhető. Élő adatbázison a destruktív próbát ne futtasd.

FreeBSD-integrációhoz külön ideiglenes csomagon próbáld az SSH-s készítést,
a valódi SFTP-letöltést/visszatöltést és a visszaállítást. Az ütemező saját
futtatója tesztelhető ideiglenes fake-crontab programmal, az igazi crontab módosítása nélkül.

## Naplózás a leállítás után

A `logging_shutdown` CTest (`LoggingShutdownTests`) a `logging::Shutdown()` utáni
`LOG_INFO`, `LOG_WARN`, `LOG_ERROR` és `SPDLOG_INFO` hívásokat ellenőrzi. Ezek
null logger helyett szinkron stderr-loggerbe írnak. A leállítás kiüríti a sorban
álló üzeneteket, és a naplózás újraindítható.

A 2026-10-22-i FreeBSD core-ban a GameServer managereinek `CAsyncSQL` destruktora
a naplózás leállítása után naplózott, így minden szabályos leállás SIGSEGV-vel
végződött. A GameServer azóta a Database és a LoginServer mintájára guarddal,
a managerek után állítja le a core-t. A régi getterekkel a teszt hozzáférési
hibával leáll.

## Felügyelő: készenlét a beállított bind címen

A `ServerPackageToolsTests.py` (CTest `server_package_tools`) két esete fedi,
hogy a felügyelő a szervereket a `server.json` szerinti címükön keresi:

- A `test_supervisor_reaches_cores_on_their_bind_address` egy `127.0.0.2`-re
  kötött maggal végigviszi a felügyelt indítást és leállítást. A végpontokat
  is ellenőrzi:
  - a `0.0.0.0` és az üres cím `127.0.0.1` lesz;
  - a LoginServer mindig `127.0.0.1`;
  - a magok a saját vagy a `game_defaults` címüket kapják.
- A `test_supervisor_names_a_foreign_bind_address` azt ellenőrzi, hogy a nem
  helyi cím hibája megnevezi a példányt, a címet és a portot.

A régi, csak `127.0.0.1`-et próbáló készenléttel az első eset elbukik.
Ha a `127.0.0.2` nem köthető, az eset kimarad.

## Core-ok indítása és leállítása egyenként

A `ServerPackageToolsTests.py` (CTest `server_package_tools`) valódi
felügyelőpróbái:

- **Utólagos indítás és egyenkénti leállítás:**
  - a futó stackhez utólag indítanak egy core-t, amely a LoginServer elé
    kerül;
  - az ismételt indítás „already running”;
  - csatorna szerint leállítanak egy másik core-t, és a stack összeomlási
    esemény nélkül fut tovább;
  - a Database egyenkénti leállítását és az ismeretlen csatornát a parancs
    elutasítja;
  - a teljes leállítás sorrendje: login → utólag indított core → database.
- **Elbukó utólagos indítás:** csak az a core esik ki, `starting` fázisú
  összeomlási eseménnyel, a stack fut tovább.
- **Futó stack nélkül** a vezérlés érthető hibával áll meg.

A `ServerManagerTargetTests.py` a távoli `start-cores`/`stop-cores`
továbbítását és a hibás kiválasztás elutasítását ellenőrzi, valamint
azt, hogy régebbi `manage.py` esetén a hibaüzenet a frissítést kéri.

A `ServerManagerGuiTests.py --package <release>` a gombokat ellenőrzi:
- a kijelölés szerinti engedélyezést;
- a megerősítést;
- hogy a leállítás csak a futó core-okat kapja meg;
- a csatornák utólagos indítását;
- a régi `manage.py` miatti tiltást.

Négy mutáció mindegyikét elkapja egy teszt:
- leállítás összeomlásként;
- utólagos core a LoginServer mögött;
- utólagos hiba miatti teljes leállás;
- leállított core leállítása.

A core-vezérlés hibakezelésének regressziós esetei valódi, ideiglenes
alfolyamatokkal ellenőrzik, hogy egy futási mappa ütközése vagy egy indítási
jogosultsághiba csak az új core kérését utasítja el. A lassú leállítás alatt
összeomló másik szerver esetén a leálló core az állapotfájlban marad, és a
Database csak a kilépése után áll le. Az utólagos indítás és leállítás korábbi
tesztjei továbbra is ellenőrzik a sikeres utat és az összeomlási naplózást.

A backend-, célgép- és GUI-tesztek a futó felügyelő `core_control_version`
jelzését vizsgálják: a friss fájl önmagában nem engedélyezi a külön vezérlést.
Régi vagy már nem futó felügyelőnél a gombok tiltottak, a távoli kérés érthető
hibával visszatér, és nem hagy megválaszolhatatlan kérést a vezérlési mappában.

A `ServerManagerCrashTests.py` Qt-próbái az eseménynapló nélkül meglévő core
fájl táblázatos megjelenését is ellenőrzik. Vegyes listán a rögzített eseményhez
tartozó core nem jelenik meg kétszer, a sor és a választó ugyanarra a fájlra
mutat, az archivált bináris kiválasztása megmarad. A lista frissítése megtartja
az aktuális debuggerjelentést és a kézzel megadott programot; megváltozott core
fájlnál a régi jelentést elveti, core nélküli eseménynél törli a korábbi célt.

A `ServerManagerTests.py` questlista-próbája a régi `data/quest/object` fa
elrejtését és a közvetlen szerkesztési kísérletek elutasítását is ellenőrzi;
a `quest_list` és az aktív források továbbra is megnyithatók.

A `ServerManagerEditorTests.py` a forrásból generált Lua-/quest-API-katalógus
frissességét és a Qt-s kiegészítést is ellenőrzi: `pc.` gépelése, prefix szerinti
szűkítés, Ctrl+Space, nyilak, Enter/Tab és egér, Esc, névtér és kulcsszó,
egylépéses visszavonás, azonosító közepén javítás UTF-16-os kurzorpozícióval,
meglévő argumentumok megtartása. Kommentek, hosszú sztringek, ismeretlen
névterek, JSON, kijelölés és fájlváltás esetén nem marad nyitott téves lista.

### Eltérésalapú távoli feltöltés

A `ServerManagerRemoteWorkTests.py` a tartalom szerinti összehasonlítást,
változatlan fájlok kihagyását, kijelölt mappák hatókörét és a kétoldali
kizárásokat is vizsgálja. Az új, módosított, üres mappás és csak távolin lévő
bejegyzések mellett azonos méret/dátum melletti eltérést is tesztel. Az
összehasonlítás nem hozhat létre célmappát vagy metaadatokat.

Külön eset a törlés engedélyezése, a távoli szerkesztés miatti ütközés,
feltöltés és törlés utáni rollback, megőrzött mentések, hibás válasz és
fájl/mappa ütközés. A Qt-próba a valódi workerrel, helyi alfolyamatokon át
futtatja az összehasonlítás → staging → SFTP-másolat → commit folyamatot;
változatlan vagy csak nem engedélyezett törléseket tartalmazó listán tiltott
a feltöltés, cél- vagy SSH-cím módosításakor elvész a korábbi terv.
