# Mission Engine – natív mag, 2026-10-25

Az aznapi Node Library specifikációval bővített állapotot és a módosított
sorrendet a `mission_node_library_delta_2026-10-25.md` dokumentálja. Az új
motorhoz nincs Lua-extension; a meglévő Lua csak a még nem migrált régi
alrendszerek miatt marad. Az alábbi mag azóta `.aurmission` csomagolást,
típusregisztert és katalógusaktiválást is kapott.

## Elkészült fázis

A `codex/mission-engine` ágon elkészült az audit 3. fázisának önálló,
natív C++23 magja. Verziózott definíciókat validál, karakterenként futtatja
a küldetésfát, és indexelt eseményekből, tényekből, illetve határidőkből
számít állapotátmeneteket. Játékvilág, SQL, EnTT registry és Lua VM nélkül
tesztelhető.

Ez még nem a játékba bekötött teljes Mission Engine. Nincs új klienspacket,
adatbázis-migráció, tárgyjóváírás, NPC/dialógus adapter vagy Mission Studio.
A régi NPC-questmotor a tartalom igazolt átvezetéséig működik. A szezonális
Mission Pass külön szolgáltatás marad.

A korábbi munkák commitjai:

- `e9d1ca90`: audit és migrációs terv;
- `c6e80183`: a régi BattlePass szerver- és kliensfüggőségeinek eltávolítása;
- `9ab0b12c`: a nyugdíjazott launcher BattlePass-exportjainak eltávolítása;
- Unity Version Control changeset **30**, `/main/codex-mission-engine`:
  a régi BattlePass felületének, assetjeinek és protokolljának kivezetése.

## Fájlok és felelősségek

Minden natív típus az `auriga::mission` névtérben található.

| Fájl | Feladat |
|---|---|
| `SRC/Server/GameServer/mission/MissionDefinition.hpp/.cpp` | Modell, szigorú JSON-beolvasás, immutable definíciók, verziózott registry |
| `SRC/Server/GameServer/mission/MissionValidator.hpp/.cpp` | Teljes szerkezeti ellenőrzés, szülő-/gyermekkapcsolatok és függőségek fordítása |
| `SRC/Server/GameServer/mission/MissionConditionEvaluator.hpp/.cpp` | Mellékhatásmentes, korlátos feltételkiértékelés |
| `SRC/Server/GameServer/mission/MissionEventRouter.hpp/.cpp` | Aktív esemény-, tény- és határidő-feliratkozások |
| `SRC/Server/GameServer/mission/MissionRuntime.hpp/.cpp` | Karakterállapot, tranzakciós állapotátmenetek, deduplikáció és action barrier |
| `SRC/Server/GameServer/mission/MissionCheckpoint.cpp` | Típusos checkpoint és visszaállítási invariánsok |
| `SRC/Tools/MissionCheck.cpp` | Ugyanezt a parser/validator kódot használó parancssori ellenőrző |
| `config/mission_engine_core.example.json` | Kis demonstrációs definíció, hibás választás utáni recovery ággal |
| `tests/server/MissionCoreTests.cpp` | Natív funkcionális és generált állapotgép-regressziók |

Az új források a GameServer célban is lefordulnak. A meglévő gameplay
egyelőre nem hoz létre `MissionRuntime` példányt, és nem küld neki eseményt.

## Definíciós szerződés, schema 1

A JSON kötelező mezői: `schema: 1`, `mission`, pozitív `version`, `root`,
`nodes`. Opcionális: `repeatable`, `prerequisite`, `predicates`, `choices`.
A mezőnevek és a `kind`/`op` értékek kisbetűsek; a teljes működő példa a
fenti configfájlban található. Az ismeretlen, rossz helyen megadott vagy
duplikált JSON-mező hiba, nem figyelmen kívül hagyott beállítás.

Az azonosítók legfeljebb 96 bájtos ASCII kulcsok; az elfogadott karakterek:
`A-Z a-z 0-9 _ . : - /`. A példában szereplő `demo.*` kulcsok tervezési
jelölések, nincsenek valódi NPC-, mob-, tárgy- vagy térképazonosítóhoz kötve.
Megjelenített szöveg és lokalizáció még nem része ennek a sémának.

| Node `kind` | Működés |
|---|---|
| `objective` | `event`, `subject`, `goal`, opcionális `distinct`; csak aktív állapotban gyűjt |
| `condition` | Vár a feltétel teljesülésére; tényváltozás vagy időhatár ébreszti |
| `branch` | Belépéskor az első igaz `when` ágat választja; kötelező az explicit `fallback`; a választott ág rögzül |
| `sequence` | A `children` sorrendjében halad; a következő cél új eseményt igényel |
| `parallel` | `all`, `any`, `race` vagy `first_terminal`; a race első sikere nyer, csak az összes ág hibája jelent hibát; az explicit first_terminal timeout-versenyhez használható |
| `timer` | Pozitív `duration` ezredmásodperc után sikeres; timeout-versenyhez `parallel/race` alá helyezhető |
| `reward` | Szimbolikus `reward` actiont kér; a megbízható nyugtáig aktív marad |
| `sub_mission` | Ebben a sémában pontosan egy saját, inline gyermekrészfát fog össze |
| `success`, `failure` | Explicit sikeres vagy sikertelen levél |
| `recovery` | Két gyermek: elsődleges út és az első hibájánál megnyíló helyreállító út |

A `completion_flags` csak a csomópont sikeres teljesülésekor ír történeti
tényeket. A flag tartósnak szánt, monoton tény; nincs flagtörlő API. Az
egyszer már teljesült feltételcsomópontot a későbbi tényváltozás nem nyitja vissza.
Különböző ágak összefutását a közös szülőjük utáni sequence-elemmel kell
leírni, nem ugyanazon állapotot több szülő alá hivatkozva.

Feltételoperátorok: `always`, `all_of`, `any_of`, `at_least_n`, `has_flag`,
`not_flag`, `choice_is`, `time_window`, `subgraph`, `not`, `xor`, `exactly_n`.
A NOT egy, a XOR pontosan két operandusú; az EXACTLY_N nulla találatot is
megkövetelhet. A `subgraph` itt névvel
hivatkozott tiszta predikátum, nem külső workflow. A `time_window` intervalluma
`[begins_at, ends_at)`. A `sequence`/`parallel` saját állapotot vezető node,
nem állapot nélküli logikai operátor.

Az `at_least_n` elutasítja az azonos operandusokat és a közös flag-/choice-
függőségből származó többszörös beszámítást. Ez konzervatív szerkezeti
függetlenségvizsgálat: a tényleges evidence-források jelentését a későbbi
tartalomkötésnek is ellenőriznie kell.

A validator ellenőrzi a típusokat, tartományokat, összes hivatkozást, a nem
használt predikátumokat, a maximális mélységet és az összes ágat. Schema 1-ben
minden vezérlési ciklus és megosztott gyermekállapot tiltott. Guardolt loop,
tetszőleges goto és külön definícióra hivatkozó SubMission későbbi kiterjesztés.
A szerkezeti validálás nem bizonyítja a teljes kampány végigjátszhatóságát.

## Futási API és eseményszemantika

A registry mélyen másolt, `shared_ptr<const CompiledDefinition>` példányokat
tárol. `Publish` nem cserélheti le ugyanazt az ID/verzió párt. `Get` pontos
verziót, `Latest` az aktív katalógusban kijelölt verziót adja. Önálló
tesztpublikálásnál ez a legnagyobb verzió; csomagaktiválás/rollback után
szándékosan lehet korábbi is. A registry olvasása/publikálása
zárolt; a runtime maga egyetlen authoritative tulajdonosszálhoz kötött.

| API | Szerződés |
|---|---|
| `Start(mission, version, instanceId, now)` | Ellenőrzi az előfeltételt; `version == 0` csak itt jelent latestet; egy missionből egyszerre egy aktív példány |
| `Dispatch(event, now)` | Kizárólag hiteles szerveroldali eseményből halad; ellenőrzi a karaktert, sessiont, időt és eseményazonosságot |
| `AdvanceTime(now)` / `NextDeadline()` | A scheduler a következő indexelt határidőre ébreszthet; nem kell minden frame-ben minden küldetést végigjárni |
| `SetFlag(flag, now)` | Csak szerveroldali, monoton történeti tényt ír |
| `PendingActions()` / `Acknowledge(key, now)` | Függő actionök és pontos, karakter-/verzió-/példány-/node-/reward-azonos nyugtájuk |
| `Abandon(instanceId, now)` | Megszakítja az aktív részfát és leiratkoztatja eseményeit, időzítőit, actionjeit |
| `Inspect(instanceId)` | Későbbi copy-on-write módosítások mellett is megtartható immutable állapotnézet |
| `Save()` / `Restore(checkpoint, now)` | Teljes típusos memória-checkpoint; a restore üres runtime-ot és minden pontos definícióverziót igényel |

A runtime időegysége **authoritative UTC ezredmásodperc**. A `now` nem
léphet visszafelé; az esemény `occurredAt` ideje nem lehet negatív vagy
jövőbeli. A timer összeadás túlcsordulása ellenőrzött. Azonos időpontban a
lejáró határidők megelőzik az új bemenetet. Hibás művelet a saját kezdeti
határidő-feldolgozását is visszagörgeti; a scheduler külön is tovább viheti az időt.
Késői ébresztéskor a már aktív időzítők lejárati sorrendje megmarad, azonos
határidőnél a gráf gyermeksorrendje dönt. A restore a checkpoint óta átlépett
határokat ugyanezzel a szabállyal dolgozza fel. A feltételek a tényleges
ébredési időt vizsgálják: egy közben teljesen elmúlt időablakot nem kap meg
utólag a játékos. Az újonnan megnyíló timer a tényleges ébredéstől indul.

A `GameplayEvent` karakter-/session-tulajdonost, `{stream, sequence}` ID-t,
időt, eseménytípust, szimbolikus célt, mennyiséget és opcionális választást
tartalmaz. A stream/sequence értékeket a későbbi szerveradapter állítja elő;
kliens nem küldhet kész progress- vagy jutalomeseményt. Támogatott események:
mobölés, tárgygyűjtés, craft, NPC-interakció, felfedezés, dialógusválasztás,
bossölés, puzzle és escort. A timer natív határidő, nincs kliens által küldhető
`TimerExpired` esemény.

Egy dispatch az indulásakor illeszkedő aktív célok listáját használja; ugyanaz
az esemény nem teljesítheti egy sequence következő, csak közben megnyíló
célját. Külön aktív párhuzamos célok ugyanazt az eseményt felhasználhatják.
Belépés előtti esemény nem számít. A számláló telítődik a célnál; a `distinct`
különböző célkulcsokat számol, nem példányszámot vagy mob-VID-ket.

Streamenként 256 sorszámos csúszó dedupablak fogadja a még nem látott,
ablakon belüli késői eseményeket. Duplikált és túl régi esemény nem léptet
állapotot. A szabályos, de épp nem illeszkedő esemény is bekerül az ablakba;
később nem játszható újra új célra. A streamtörténet nem ürül csendben.

A választási cél csak deklarált opciót és egyszeres mennyiséget fogad el.
Az egyszer elkötelezett choice tény változtathatatlan. Ugyanazon tartós
choice-kulcs opcióhalmaza másik definícióban/verzióban sem változhat;
ehhez új kulcs vagy későbbi explicit migráció szükséges.

## Atomi állapot és a következő tartóssági fázis határa

Egy művelet csak az érintett példányok változó node-állapotát másolja;
a definíció közös marad. Work-/kapacitás-/időhiba esetén visszaállnak a
számlálók, flag-ek, choice-ok, dedupablakok, revision, idő és indexek.
Visszagörgetett vagy ugyanabban a műveletben megszakított action nem kerül
ki az eredménybe. Az explicit gyermeksorrend a lapos node-deklaráció
sorrendjétől függetlenül meghatározza az egyidejű race-ek eredményét.

A `reward` jelenleg **kérés és nyugtázási akadály**, nem végrehajtott jutalom.
A core nem ad tárgyat, nem fogyaszt inventoryt és nem tesz SQL commitot.
Az action kulcsa reconnect után azonos; a hibás, idegen vagy megszakított
kérés nyugtája elutasított, az ismételt nyugta duplikált. A `Save` eredménye
nem lemezre írt mentés. A visszaállított dedupvédelem csak akkor tartós, ha
a következő fázis valóban tranzakciósan elmenti a checkpointot.

Következő szükséges munkafázis:

1. Additív MariaDB-séma és prepared statement alapú progress store,
   karakterenkénti revision/CAS és authoritative tulajdonlási szabály.
2. Ugyanabba a tartós tranzakcióba kötött action/outbox és jutalomledger;
   inventory-fogyasztás és jóváírás idempotens, újrapróbálható kezelése.
   A végrehajtás előtt a még érvényes actiont és revisiont is ellenőrizni kell.
3. Valódi MariaDB integrációs tesztek: két core versenye, commit előtti/utáni
   crash, elveszett válasz, tele inventory, reconnect és ismételt kézbesítés.
4. Csak ezután gameplay/ECS adapterek, natív Action/Condition/Event service-ek és az
   első NPC-quest vertikális szelet. A teljes 80-as kampány és a régi questek
   migrációja külön tartalom- és elérhetőségi ellenőrzést igényel.

## Kezdeti korlátok

| Erőforrás | Korlát |
|---|---:|
| JSON méret / definíció | 256 KiB |
| Node / vezérlési és kifejtett predikátummélység | 512 / 32 |
| Feltételcsomópont / definíció | 4096 |
| Choice-kulcs / definíció; opció / choice | 64; 2–32 |
| Megőrzött / aktív instance karakterenként | 256 / 32 |
| Flag / choice-tény karakterenként | külön-külön 4096 |
| Stream / dedupablak | 32 / 256 sorszám |
| Számláló célértéke | 1 000 000 000 |
| Distinct cél / objective; tárolt distinct elem / karakter | 2048; 8192 |
| Állapotművelet munkakerete | legfeljebb 16 384 lépés, konstruktorban csökkenthető |
| Registryben tárolt definícióverziók | 4096 |
| Függőségi fordítási munkakeret / definíció | 65 536 |
| Katalógus / megtartott registry függőségi elemei | 262 144 / 1 048 576 |

Ezek biztonsági határok, nem mért játékosszám- vagy memóriaígéretek. Nagy,
érvényes gráf művelete túllépheti a runtime munkakeretét; ekkor atomi
elutasítás történik. Előzményarchiválás, adatbázisos ritkítás, teljes
kampányszolvabilitás és üzemi terhelésmérés még nincs ebben a fázisban.

## Ellenőrzés

A Windows x64 / MSVC RelWithDebInfo ellenőrzések sikeresek:

- teljes `GameServer`, valamint `MissionCoreTests` és `MissionCheck` fordítás;
- **8723 natív ellenőrzés**, köztük 64 rögzített seedből generált vegyes
  vezérlési fa, minden lépés után pontos checkpoint-visszaállítással;
- a fixture két bináris választásának mind a négy kombinációja recoveryvel
  teljesíthető; ez nem az összes jövőbeli kampányág bizonyítása;
- verziópinning, változtathatatlan progress-nézet, duplikált/késői esemény,
  reconnect, régi session elutasítása, ténykapuk, offline időablak, időzítők,
  race/cancel, action receipt, túlcsordulás és atomi work-limit rollback;
- közös flagre ébredő race, eltérő/azonos határidők késői feldolgozása,
  offline prioritás, elmulasztott időablak és inkonzisztens gate-checkpoint;
- 400 párhuzamos cél aktiválása és teljesítése 2048 lépéses műveleti kerettel;
- **22/22 CTest**: az új core/példa és a korábbi quest, Lua, SQL, player,
  dungeon, combat, item, shop, lifecycle, config/csomagkezelő és szezonális MP
  célzott regressziói;
- külön AddressSanitizer build és **2/2 CTest** a core-ra/példára,
  memóriakezelési hiba nélkül.

Naplók: `build/mission-core-integration-build.log`,
`build/mission-core-checks.log`, `build/mission-core-regression-tests.log`,
`build/mission-core-asan-build.log`, `build/mission-core-asan-tests.log`.
A buildnaplók helyi, Git által figyelmen kívül hagyott ellenőrzési nyomok.
Ebben a fázisban nem történt FreeBSD build vagy játékbeli end-to-end próba.

Futtatás a repository gyökeréből:

```powershell
cmake -S . -B build
cmake --build build --config RelWithDebInfo --target MissionCoreTests MissionCheck GameServer --parallel 4
ctest --test-dir build -C RelWithDebInfo --output-on-failure -R '^(mission_core|mission_definition_example)$'
& build/SRC/Server/GameServer/RelWithDebInfo/MissionCheck.exe config/mission_engine_core.example.json
```

FreeBSD/Linux esetén az executable a választott buildrendszer megfelelő
kimenetében található, `.exe` és többkonfigurációs alkönyvtár nélkül.
A CLI kódja ugyanazt a parser/validator párost fordítja, mint a core;
szerkezeti siker esetén 0, hibás definícióra 1, hibás használatra 2 a kilépési kód.
