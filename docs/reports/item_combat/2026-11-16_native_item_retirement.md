# Natív item runtime: a régi backend és állapottárolás kivezetése

2026-11-17-i folytatás: a droptáblák futó szerver melletti szerkesztése és újratöltése elkészült, valódi Unity-öléspróbával: [élő dropfrissítés](2026-11-17_live_drop_reload.md).

Az aktív szerver és Unity kliens kizárólag a típusos itemállapotot használja. A helyi csomag frissítve, újraindítva és Unityből ellenőrizve. A 637, merge-re váró forrástételhez nem nyúltunk. A teljes konvertált katalógus SHA256-ja változatlan: `9ab3dac7f81d2898ab89aeeca641bffc4f59b28ac5f7683d080282ea9913babd`.

Ez a checkpoint a futó backend, a hálózati belépési pontok és a régi állapottárolás kivezetését zárja le. **A teljes régi ECS/vnum/APPLY/Lua forrásfa fizikai eltávolítása még nincs lezárva.** A megmaradt segédek és eredeti questforrások jelenléte nem jelent működő második item backendet; külön forrástakarítás és questenkénti tartalmi ellenőrzés szükséges. A letiltott vnumos Lua-jutalom nem válik automatikusan natív jutalommá.

## Runtime és protokoll

- Kötelező `items.catalog=required` és `items.drops=typed`. Az `off` és a régi dropbackend érvénytelen konfiguráció. A világdropok a telepített natív katalógusból érkeznek.
- A karakterbetöltésben nincs Legacy állapot és nincs hiányzó natív tulajdonos esetén régi itembetöltés. Sikertelen natív betöltés megszakítja a kapcsolatot. Az új karakter kezdőfelszerelése natív példányokból készül.
- A Database régi karakter-item SELECT/save/load/destroy, CItemCache, safebox/Mall, árlista és ItemAwardManager útja eltávolítva. A régi item-persistence üzeneteket hitelesített GameServer peertől is elutasítja. A karaktercache csak a karakterrekordot menti; a natív itemfence megmarad.
- A boot nem tölti a régi item/shop/refine/APPLY protókat és a régi drop/cube adatokat. A közös boot ABI üres mezői megmaradnak. A régi item-ID tartomány SQL-kezelése megszűnt.
- A GameServer régi CreateItem belépési pontja nem allokál. Az ECS legacy itemfactory kódja csak a történeti helperteszt célpontjában fordul; a GameServer objektumfájljában nincs CreateItemEntity szimbólum. A régi Mission item-award és Database award-scan/delivery adapter nincs a két éles célpont forráslistájában.
- A mount-inventory bejelentkezési SQL-lekérdezés, visszatérő kezelő és régi SQL-író eltávolítva. A natív hátas és társ saját runtime-ja megmarad.
- Unityben nincs választható vagy szerializált régi backend. A régi itemküldők esemény, szekvenciaszám és hálózati írás előtt hibát adnak. A natív raktárak, kiegészítő leltár és hátasfelület natív kérést küldenek. A közös VID-alapú lootfelvétel továbbra is működik.

## Natív céhjutalom

A használt céhrang-quest régi `item_award` írását natív jutalomsor váltja fel. A kilenc meglévő jutalom explicit natív kulcsa a `config/typed_quests/guild_rank_manager.lua` fájlban szerepel; egyik sem a 637 halasztott tétel része. Az új csomagkészítő beépíti ezt a forrást, és a helyi szerveren új quest-snapshot lett kiválasztva.

A 042-es migráció `player.item_native_award` táblája a még át nem vett jutalmat tartja nyilván. Egy szerver által készített kérés ugyanannak a céhtagnak csak egyszer ír jutalmat. A korábbi szemantika megmarad: két céhtag ugyanazon a fiókon két jutalmat kap. A producer ellenőrzi a katalógust, a fejlesztési szintet és a natív grant szerződését.

A hitelesített Mall-megnyitás kézbesít. A jutalomsor megjelölése, a konkrét példányok és az idempotens nyugta egy SQL-tranzakcióban íródnak. Megtelt Mall esetén a jutalom a sorban marad; későbbi helyfelszabadítás után átvehető. Az újranyitás nem ismétli meg. A régi, aktív questek által nem hívott `pc.give_award` és socketes változatának SQL-kötése megszűnt.

A teljes céhranglista kiértékelése és a rangstatisztika nullázása továbbra sem egy közös tranzakció a jutalomsor írásával. A GM/server-timer teljes, több céhet érintő rangosztási folyamatát ebben a körben nem futtattuk végig; a valódi producer és a Mall fogyasztó ellenőrzése elkészült.

## Adatbázis és helyi telepítés

A `db/retired_item_tables.json` 17 régi állapottáblát nevez meg: item, safebox, award, árlista, régi mount/pet, nyolc offlinebolt-tábla és három import-/tulajdonosarchívum. Ezek a saját `D:/AurigaGlobal/Server2-Local`, `127.0.0.1:3307` példányon már nem léteznek.

A kivezető csak leállított, ellenőrzött helyi csomagon dolgozik, nem üres táblánál az első DDL előtt megáll. Ismeretlen FK-függőséget elutasít, az ismert gyermektáblát a szülő előtt távolítja el. A natív állapotot tartalmi hash-sel ellenőrzi; a migráció csak a 041-es formatmarkert és az új, üres 042-es jutalomsort adhatja hozzá. A meglévő játékos-/natív tárgy-/pénztárcaadatok nem kerültek ürítésre. Új, dedikált tesztfiókok és egy natív céhjutalom készültek a játékpróbához.

A friss `init-db` a történeti `db/schema.sql` állapottábla-definícióit kiszűri. A teljes eredeti séma megmarad az explicit offline backup-visszaállítás eszközének. A régi item-prototípusok és konverziós források megmaradnak; a runtime már nem olvassa őket. A startup adatellenőrzése a natív formatmarkert és jutalomsort kéri a régi item/shop/APPLY adatok helyett. A működéshez a natív katalógus és a hozzá tartozó példánymigrációk továbbra is szükségesek.

A végleges GameServer, Database, segédeszközök, 041/042-es migrációk, supervisor-segédek és céhquest hash-ellenőrzötten telepítve. A helyi database, CH1 két core, CH99 és login szolgáltatás fut. Távoli telepítés, Git-commit, merge vagy Unity-checkin nem történt.

## Tényleges ellenőrzés

- MSVC Release GameServer, Database és érintett natív segédeszközök fordítása.
- 12 külön CTest sikeres: server_config, legacy_item_boundary, player_session, player_cache_snapshot, player_cache_drain, player_cache_protocol, player_cache_drain_mariadb, item_catalog_runtime_mariadb, item_persistence_completion, item_persistence_mariadb, legacy_item_retirement, item_runtime. A retirement teszt nyolc védelmi esetet tartalmaz. A történeti pickup-tesztek a visszautasítást és a változatlan itemállapotot ellenőrzik az eltávolított pozitív legacy út helyett.
- 106 teljes katalógus-konverziós teszt, 13 Mission/DB tooling teszt valódi elkülönített friss MariaDB-bootstrap próbával, 38 package-tool teszt: sikeres.
- Csomagellenőrzés: konfiguráció, 514 JSON, 14 Lua, 76 quest és TLS-kulcspár sikeres. Natív database-check sikeres.
- Unity 6000.3.27f1 fordítás, item-boundary-offline és item-integrációs validator sikeres.
- Új natív karakterekkel 29 ellenőrzés: kezdőfelszerelés, potion, mozgatás, fel-/levétel, földre dobás/felvétel, újrabelépés.
- A végleges telepítésen kétfiókos TLS-próba: natív login, mozgatás, helyi régi kérés és változatlan szekvencia, következő natív kérés, részleges dobás/felvétel, relog, közvetlenül TLS-re írt fix és változó hosszúságú régi mutáció szerveroldali elutasítása.
- Valódi natív céhjutalom-producerrel előkészített jutalom: Unity Mall-ablak, előnézet, megerősítés, kivétel, újranyitás és relog sikeres. SQL-utóellenőrzés: egy sor, egy claim, pontosan egy tárgy a karakteren; Yang változatlan. Teljes Mall, hibás claim és visszagörgetés külön izolált SQL-teszttel igazolva.
- Végső SQL-bizonyíték: mind a 17 régi tábla hiányzik, a kontrollált karakterek darabszámai és egyenlegei egyeznek, az összes többi natív tárgy/pénztárca változatlan, nincs földi escrow-maradvány. A katalógus hash-e és a 637-es halasztott halmaz változatlan.

Nem futott: FreeBSD build, standalone/Android/iOS build, teljes világ- és terhelési regresszió, minden eredeti Lua-quest végigjátszása. A további fizikai forrástakarításnak a megmaradt ECS itemkomponenseket, ItemSystem_LegacyBridge segédeket és vnum/APPLY/Lua hivatkozásokat kell lezárnia; ezeket nem jelöljük késznek pusztán a tiltott backend miatt.

Gépi összesítés: [bizonyíték](2026-11-16_native_item_retirement.evidence.json).
