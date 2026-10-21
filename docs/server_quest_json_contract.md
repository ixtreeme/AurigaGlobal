# R8: quest metadata JSON

GameServer loads `questnpc.json` and `questcategory.json` from the configured
`quest.dir` (`g_stQuestDir`). Both files are required, including for empty lists.
There is no TXT fallback. Quest source, compiled object directories and Lua
programs retain their existing format and lookup paths.

```json
{"npcs": [{"vnum": 20016, "name": "blacksmith"}]}
```

Each NPC row associates a vnum with its script directory, exactly as the old
TXT alias did. Vnums are integers in 1..4294967295; names are ASCII identifiers
matching `[A-Za-z_][A-Za-z0-9_]*`. Vnums and names must each be unique. The runtime
always loads its reserved `0/notarget` entry; it must not appear in the file.
Numeric object directories still use the existing dynamic NPC registration.

```json
{"categories": [{"quest": "guild_war_join", "category": 3}]}
```

Quest names use the same identifier grammar and must be unique. Categories are
integers in 0..65535, matching the existing client field. The renewal dictionary
is resolved against compiled quest names after Lua initialization. Uncompiled
names are logged and skipped as before; a compiled quest without a category
still gets 0. The retained `ReadQuestCategoryFile` compatibility method reads
the validated snapshot and retains its historical missing-category result 99.
Lookups no longer open a data file on every request.

The JSON Schemas are [questnpc.schema.json](schemas/questnpc.schema.json) and
[questcategory.schema.json](schemas/questcategory.schema.json). The typed C++
reader additionally checks record uniqueness and duplicate JSON object keys.
Strings/floats/booleans cannot stand in for integer fields. Unknown/missing
fields, unsafe directory names and out-of-range values are rejected with a
file name and JSON row/field path. Duplicate keys and syntax failures identify
the offending key or syntax position. Empty arrays are valid.

Both files are validated before Lua/script initialization. Missing or invalid
JSON makes initial startup fail. Reload validates a new snapshot before closing
the old Lua state; invalid metadata preserves the running state. A successful
reload also clears old quest index/name and category associations. This guard
covers metadata validation; an unrelated Lua/script initialization failure is
still a reload error and is not rolled back.

## Conversion and rollout

`SRC/Tools/txt2json/txt2json_quest.py` supports Python 2.7 and Python 3:

```text
python txt2json_quest.py npc questnpc.txt questnpc.json
python txt2json_quest.py category questcategory.txt questcategory.json
python txt2json_quest.py npc questnpc.txt questnpc.json --check
python txt2json_quest.py category questcategory.txt questcategory.json --check
```

The converter accepts UTF-8 (optional BOM), blank lines, full-line `#`/`//`
comments, whitespace separators and quoted category names. It validates every
record before writing. Malformed or duplicate rows report their source line;
they cannot silently overwrite an earlier mapping. Historical malformed or
numeric alias rows that the TXT readers skipped must be corrected explicitly.
`--check` compares every record and field without writing and also reports a
record count and numeric sum.

Generate and verify both JSON files in a staging quest directory, then deploy
them together with the new GameServer. The files must be present before startup
or quest reload. Original TXT files may be kept as migration backups; the server
does not read them. No Database/LoginServer wire or client packet changes are
introduced by R8. Live Unity verification remains part of R12 after connection
migration.

## Verification

GameServer and ServerTests build successfully in normal and ASAN RelWithDebInfo
configurations; all 49 CTest cases pass in each. The reader suite passes 77
checks, the extended quest runtime 629 checks, and all 7 Python converter tests
pass with each C++ reader.

`QuestDataTests` runs the production reader over valid, boundary and malformed
data, duplicate records/keys, missing files and a second-file failure without a
partial snapshot update. Its `--dump <directory>` mode prints the typed records
for converter/consumer parity checks. `QuestRuntimeTests` exercises the real
manager startup/reload and lookup methods over controlled Lua/NPC services,
including `notarget`, defaults, stale mapping removal and rejected reloads.

```powershell
python tests/server/QuestDataConversionTests.py --reader build/SRC/Server/GameServer/RelWithDebInfo/QuestDataTests.exe -v
```

The Python suite also drives the actual converter CLI, verifies BOM/CRLF input,
non-destructive error/check modes and exact parity with the production C++
reader. Repeat with the ASAN reader after building `build-asan`.

The local `D:/AurigaGlobal/Auriga-Server/srv1/share/data/quest` metadata was
converted and checked: 0 NPC aliases and 1 category (`guild_war_join`, 3). Both
normal and ASAN C++ readers accepted it and returned identical records. Original
TXT hashes are unchanged; those files remain migration backups. No server
restart or live client verification was performed.
