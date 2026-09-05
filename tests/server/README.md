# Item attribute regression tests

The `ItemAttributeTests` target compiles the production `ItemAttributeSystem.cpp`
with an EnTT registry and headless persistence, network, and RNG doubles. Items
in the fixtures have no `CItem`/`LegacyItemPtr`, and no database is started.

From the repository root:

```powershell
cmake --build build --config Release --target GameServer ItemAttributeTests
ctest --test-dir build -C Release -R '^item_attributes$' --output-on-failure
```

For an existing `build-asan` configured with `ENABLE_ASAN=ON`:

```powershell
cmake --build build-asan --config RelWithDebInfo --target ItemAttributeTests
ctest --test-dir build-asan -C RelWithDebInfo -R '^item_attributes$' --output-on-failure
```

Checks cover invalid/stale entities, invalid attribute sets, empty and
zero-weight tables, weighted probability boundaries (including partially
assigned level distributions), locked normal bonuses, rare-slot gaps,
atomic failure on insufficient candidates, addon removal, duplicate bonuses,
costume values, and force-attribute bounds/client updates.

The doubles verify save/update calls, not the DB packet encoding or a live
client session. Test in-game inventory, relog, bonus changers, rune items and
mount attributes separately before deploying.

Follow-up work: switchbot currently consumes the price before requesting a
reroll, and costume reset still clears and generates in separate calls. The
tests do not cover those caller-level transactions or the legacy `ChangeKKAK`
special-case attribute path.
