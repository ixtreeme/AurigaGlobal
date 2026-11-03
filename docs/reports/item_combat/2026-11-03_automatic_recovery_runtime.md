# C20 — Automatikus HP/SP-italok

2026-11-03. Szerződés: [IC-048](../../adr/item_combat_048_automatic_recovery_runtime.md).

## Eredmény

Véges és végtelen automatikus HP/SP-palack használható a futó szerveren és
Unity 6.3-ban. A töltet, az aktivitás és a következő adagig hátralévő idő
mentett példányállapot. Bekapcsoláskor nincs darabfogyás; a két pool külön
működik, az azonos poolú csere egy tranzakció. Aktív palack zárolt,
kifogyáskor a véges példány eltűnik. A maximum-bónusz a valós statot módosítja.

Az élő teszt közben láthatóvá vált, hogy a késve érkező legacy affect-betöltés
teljesen visszatöltötte a typed karakter HP/SP-jét. Typed owneren ez most
maximum-clampot végez, így a mentett állapotot és az aktív italokat megőrzi.

## Lefutott ellenőrzések

| Ellenőrzés | Eredmény |
|---|---|
| Release GameServer és érintett natív célok | sikeres |
| CTest célzott regresszió | **13/13 csoport** |
| AutomaticRecoveryTests | **1078 ellenőrzés** |
| AutomaticRuntimeTests | **20 ellenőrzés** |
| Külön saját MariaDB, ItemPersistenceSqlTests | **1417 ellenőrzés** |
| Külön saját MariaDB, ItemBenchToolTests | sikeres |
| Python telepítő/teszt/próbapad szintaxis | sikeres |
| Unity 6000.3.27f1 scriptfordítás | sikeres |
| Új GC242 v2 golden, framing, UI és dispatcher | **3161 ellenőrzés** |
| Korábbi C4–C19 Unity-validátorok | mind sikeres |
| Valódi Unity → TLS → GameServer → SQL és UI | **42 ellenőrzés, sikeres** |
| Canonical Unity source snapshot | **5446 fájl, 1722 byteazonos C# fájl** |
| Korábbi helyi próbakészlet megőrzése | **mind a 49 korábbi tárgyállapot változatlan** |

A natív regresszió: automata domain/runtime, tartalékos/folyamatos recovery,
consumable, persistent items, inventory wire, stat, trade, bolt, crafting,
kőbehelyezés, accessory és bónuszszolgáltatások. A C20 esetek külön ellenőrzik
a kezdeti teljes töltetet, a zárolást, a poolcserét, a teljes/nem használható
poolt, a tartalék és a tört kredit elsőbbségét, a kifogyást és a maximum-bónuszt.

Az SQL-próba töltet/aktivitás roundtripet, teljes rollbackot elavult töltet
esetén, receipt-replayt, relogot és sérült töltethash elutasítását is lefedett.
Az előző v1 inventory-golden változatlan maradt.

Az élő teszt a valódi inventorygombot és megerősítést használta. HP-palack
aktiválása után a 4%-os maximum-bónusz és a tényleges HP-jóváírással együtt
csökkenő töltet látszott. SP-palack mellette működött; apró HP-palackra
váltás megőrizte az SP-t és a korábbi HP-palack töltetét. Kifogyáskor csak
az érintett palack tűnt el. Végtelen ellátás, szint- és kontextustiltás,
három másodperces offline szakasz, új login-control és régi control
elutasítása is lefutott. Végül mindkét aktív palack külön kikapcsolható volt.

## Helyi állapot és újrafuttatás

Aktív Unity workspace: `D:/Auriga-Unity6/SRC/AurigaUnity`,
`/main/codex-mission-engine`, **cs57**. Csak a tizenkét C20-fájl került a
checkinbe; a felhasználó korábbi jelenet-, kapcsolat-, Packages- és
ProjectSettings-módosításai megmaradtak. A külön snapshot a teljes aktuális
C# forrást fordította; a hiányzó világ/VFX/Ocean assetek ismert jelzései nem
akadályozták az itemvalidációt. A felhasználó Editorát nem zártuk be.

A saját `D:/AurigaGlobal/Server2-Local` csomag öt szerverfolyamata és saját
MariaDB-je fut. A dedikált PID 1610 tesztkarakteren **55 tárgyállapot** van:
a korábbi 49 mellé hat automata próba került. Mind a hat kikapcsolt állapotú;
a kis kapacitású kifogyási próbát a teszt után újra megadtuk. HP/SP induló
állapot 1000/20, a korábbi 85%-os kőbehelyezési beállítás megmaradt.
A telepített GameServer, próbapad-helper és 005-ös migráció hash-e egyezik
a manifesttel és az elkészült fájlokkal.

Újrafuttatás: állítsd le és várd meg a saját stack leállását, majd
`SeedAutomaticUnityLive.py` a recovery-fixture-ből; indítsd újra a csomagot.
`PrepareUnityValidationSnapshot.py` után az `AutomaticUnityLiveSmoke.py`
kezeli a külön batch Editort és az `automatic-live` bridge-kérést. A fixture
privát és csak az azonosított codexitem tesztfiókot kezeli. A legacy
DatabaseServer HP/SP cache miatt közvetlen tesztadat-SQL után a saját stack
újraindítása szükséges. A seed megőrzi a többi tárgyat, és a hiányzó automata
próbákat adja meg. A logok és a privát fixture a nem követett `build` mappában,
a telepítési mentések a csomag `run/deployments/automatic-*` mappáiban vannak.

Unityben: **I → palack → Visszatöltés → Használat áttekintése → Állapotváltás
megerősítése**. Az aktív palackok a felső **Visszatöltések** gombból is kezelhetők.

## Nyitott kiadási kapuk

Teljes Unity player build, FreeBSD, többcore-os handoff, terhelés és
hibainjektálás nem futott. Az IC-047 szerinti item-debit és legacy HP/SP-mentés
közötti crash-atomikussági rés megmaradt: a mentett debit utáni, live
jóváírás előtti crash elveszett visszatöltést okozhat. Ez nem kész kiadási
kapu. Következő szelet: **C21 — időzített és kapcsolható tárgyhatások**.
