# Mission Engine – tartós checkpoint és csomagarchívum

**Dátum:** 2026-10-25. A 4/B réteg kódcommitja `b45ed489`, SQL-olvasási méretvédelme `9d33889f`.

**Frissítés, 4/C (`5a8b13b5`):** az outbox és a szimbolikus jutalomjogosultság-ledger azóta
elkészült. A jelen kódhoz a második, `mission_engine_actions.sql` migráció is
kötelező: sémaverzió 3, kilenc tábla, nyolc FK (a 4/D–3 award-mappinggel). Az alábbi 4/B eredmények a korábbi
alapot dokumentálják; az új réteget és határait a
[4/C leírás](mission_actions_and_entitlements_2026-10-25.md) részletezi.

## Elkészült réteg

A Mission Engine saját, additív MariaDB-sémát, verziózott checkpoint codecet,
változatlan csomagarchívumot és prepared tranzakciós mentési terveket kapott.
A tulajdonosváltás, a revízióellenőrzés és a bizonytalan commit egyeztetésének
adatbázis-primitívjei is elkészültek.

Ez önálló persistence réteg. A GameServer bejelentkezési/kijelentkezési és
core-váltási életciklusa még nem hívja; az új Mission Engine továbbra sincs
játékosokra bekapcsolva. A 4/B után elkészült outbox/entitlement réteghez képest
a tényleges inventory/XP/pénz kézbesítés továbbra is külön munkarész.

## 1. Additív séma

A `db/migrations/2026-10-25_mission_engine_persistence.sql` fájlt a kiválasztott,
konfigurált player adatbázison kell majd futtatni. Nincs benne `USE`, `DROP`,
régi táblák módosítása vagy játékosadat-törlés. A jelen fejlesztés csak külön
tesztadatbázison alkalmazta. A telepítéshez később külön rollout és mentés kell;
a MariaDB DDL nem válik az alkalmazás prepared tranzakciójának részévé.

| Tábla | Tartalom |
|---|---|
| `mission_engine_schema` | A 4/B alap verziója 1; a jelenlegi 4/C kód 2-t követel |
| `mission_engine_packages` | Pontos `.aurmission` bájtok, package ID/verzió és digest |
| `mission_engine_definitions` | Mission ID/verzió → gráflenyomat és eredeti csomag |
| `mission_engine_characters` | Tulajdonosi epoch/token, tárolási revízió, runtime-revízió, checkpoint |
| `mission_engine_checkpoint_refs` | A karakter checkpointjának pontos definíció-/tartalomfüggőségei |
| `mission_engine_operations` | Claim/save/release műveletek tartós, tartalomhoz kötött nyugtája |

Mind a hat tábla InnoDB. A bináris azonosítók és digestek bináris mezők;
szövegkolláció nem olvaszthat össze eltérő missionkulcsokat. Négy idegenkulcs
védi az archívum és a checkpoint-hivatkozások kapcsolatát. Nincs automatikus
kaszkádos törlés. A séma újrafuttatása nem nullázza a magasabb verziójelölőt.

A 4/B műveletek a sémaverziót, hat InnoDB-táblát és négy idegenkulcsot
ellenőriztek; a jelenlegi 4/C kód már nyolc táblát és hét FK-t követel.
Mindkettő ellenőrzi a bekapcsolt constraint-ellenőrzéseket. Hibás
előfeltételnél nem írnak progress-t. Ez nem helyettesíti a telepítési
sémaellenőrzést vagy a megfelelő DB-jogosultságokat.

A karakterazonosító létezését/jogosultságát a majdani hiteles gameplay adapter
ellenőrzi; a séma nem tesz idegenkulcsot a régi player/account táblákra.
A `mission_engine_operations` nyugtái mentési műveletekhez tartoznak,
nem bizonyítják tárgyak vagy egyéb jutalmak kézbesítését.

## 2. Checkpoint formátum

Az új `MissionCheckpointCodec` formátuma nem C++ struktúrák memóriaképe.
Little-endian, explicit státuszkódokkal és legfeljebb 16 MiB mérettel.

| Fejlécmező | Érték |
|---|---|
| Magic | `AURCHKP\0` |
| Formátum | 1 |
| Fejléc | 64 bájt |
| Hossz | A payload pontos bájthossza |
| Digest | SHA-256 a teljes fájlról, a digest mező nullázásával |
| Fenntartott mezők | Kötelezően nulla |

A payload a karakterazonosítót, runtime-revíziót, authoritative UTC-időt,
a pontos definícióhivatkozásokat, flag-eket, választásokat, dedupablakokat és
minden instance/node állapotát tárolja. A node-állapot része a számláló,
különböző célok halmaza, kiválasztott ág, időzítők, gyermekállapot-számlálók
és a nyugtázott reward állapota is. Session és élő entitypointer nincs benne.

Az azonosítók, mapek és halmazok sorrendje kanonikus; duplikált vagy rossz
sorrendű bejegyzés hiba. Nincs tömörítés, script, szabad bytecode vagy
korlátlan allokáció. Érvényes, azonos állapot azonos bájtokat ad.
A digest integritásellenőrzés, nem hitelesítés vagy digitális aláírás.

Három külön művelet van:

1. `EncodeCheckpoint`: tisztán ellenőrzi a runtime-invariánsokat és a pontos
   gráftartalmat, majd elkészíti a bájtokat és a definícióhivatkozásokat.
2. `ScanCheckpoint`: ellenőrzi a teljes kódolást és hash-t; kizárólag a
   metaadatokat és az archívumból szükséges definíciólistát adja vissza.
   Ez önmagában nem engedélyez futtatható progress-t.
3. `DecodeCheckpoint`: az összes pontos definíció és lenyomat egyezése,
   valamint a teljes runtime-invariánsok ellenőrzése után ad checkpointot.
   Nem változtat registryt és nem lépteti az időt.

A referencia a mission ID/verzió mellett a kanonikus, típusos gráfbájtok
lenyomatát is tartalmazza. Ugyanaz az ID/verzió más jutalommal vagy más
gráftartalommal ezért nem fogadható el helyettesítésként.

## 3. Csomagarchívum és régi verziók

Az `ArchivePackage` előbb a valódi csomagloaderrel validál. Egy tranzakcióban
eltárolja az eredeti fájlt és az immutable gráfazonosságokat. Az ismételt,
azonos archiválás elfogadott. Ugyanaz a package ID/verzió vagy mission
ID/verzió eltérő tartalommal az egész munkát visszagörgeti.

Ugyanaz a változatlan gráf újabb csomagban is megjelenhet. Az első archív
csomagra mutató hivatkozását megőrizzük; közben az új csomagot is eltároljuk.
A csomagbájtok kiolvasáskor újból teljes loaderellenőrzést kapnak. Hiányzó
verzió, sérült csomag vagy eltérő metaadat esetén nincs latest fallback.

A `DefinitionRegistry::Retain` atomikusan importálhat több régi verziót,
akár ugyanannak a missionnek több kiadását is. Nem változtatja meg az aktív
katalógust vagy a `Latest()` eredményét. Részleges import hibánál nincs.

Az aktív katalógus kiválasztása továbbra is a csomagkezelő feladata. Ez a
réteg nem hoz létre automatikus, adatbázisból élesítő publish-folyamatot.
Archívum-/nyugta-/instance-GC és tartalmi állapotmigráció még nincs.

## 4. Tulajdonlás és kétféle revízió

Az `Owner` karakterazonosítót, növekvő 64 bites epochot és 128 bites, nem
nulla tokent tartalmaz. A `NewToken()` OpenSSL-alapú véletlen tokent készít.

- `Claim`: az elvárt epochhoz képest explicit tulajdonosváltás, epoch + 1.
- `Save`: csak a pontos epoch/token és tárolási revízió mellett írhat.
- `Release`: visszavonja a tokent és szintén növeli az epochot.

Egy korábbi owner késői írása a váltás után elutasított. A token későbbi
újrafelhasználása sem támasztja fel a régi epoch jogosultságát.

A claim **nem automatikus lease**: nincs időalapú lejárat vagy automatikus
core-failover. Az explicit átadást egy megbízható koordinátornak kell majd
engedélyeznie. Az epoch kiolvasása önmagában nem ilyen engedély.

A `runtime_revision` a motor állapotátmeneteit jelöli. A `storage_revision`
minden elfogadott checkpoint-mentéskor külön eggyel nő. Egy mentésben lehet
több memóriabeli esemény eredménye. Már létező checkpointnál a runtime-revízió
nem maradhat azonos és nem csökkenhet; az idő sem mehet vissza.
Változatlan, már elmentett állapothoz nem kell új save műveletet küldeni.

A tulajdonosnak claim után a tokennel szűrt snapshotot kell betöltenie;
korábban cache-elt állapotból nem indulhat el. Release előtt a végső mentés
sikerét vagy egyeztetett eredményét meg kell várni. Ezt az életciklus-vezérlőt
még nem integráltuk a bejelentkezésbe, logoutba és core-váltásba.

Csak a `Found` státuszú, ellenőrzött ownerhez tartozó üres snapshot jelent
új állapotot. A `Missing` lehet elavult owner következménye, az `Invalid`
sérült adatot jelent; egyikből sem szabad csendben új karakterprogress-t indítani.

## 5. Mentés és bizonytalan commit

Egy save tranzakció tartalmazza:

1. A séma-előfeltételeket.
2. Az owner- és storage-revízióhoz kötött checkpoint UPDATE-et.
3. A checkpoint pontos definícióhivatkozásainak cseréjét, idegenkulcs-védelemmel.
4. A műveletazonosítóhoz és kéréslenyomathoz kötött tartós nyugtát.

Hiányzó archív definíció, párhuzamos írás, prepare hiba vagy nyugtaütközés
esetén ezek együtt gördülnek vissza. Egy már használt művelettoken eltérő
tartalommal nem írhatja át a korábbi mentést.

Nem visszaigazolt írás után a `Resolve` ugyanazon karakter sorát zároló
olvasással megvárja az eredeti írót. A nyugtát szintén `FOR UPDATE` olvassa,
így REPEATABLE READ alatt sem régi snapshotból dönt a hiányáról.
Egy egyszerű, nem zároló SELECT ehhez nem elegendő.

A `DecodeResolution` eredménye: `Committed`, `Absent`, `TokenConflict`,
`SqlError` vagy `Invalid`. Hálózati/SQL-hiba nem minősül hiányzó műveletnek.
A receipt történeti tényt igazol; nem bizonyítja, hogy az akkori owner
jelenleg is érvényes. Használat előtt a sessiont és a tulajdonost is ellenőrizni kell.

A `Confirmed()` csak az adott SQL-munka visszaigazolt commitját jelenti.
Az aszinkron callbacket a megfelelő függő kéréshez kell párosítani, majd a
karaktert/sessiont újra megkeresni. SQL-worker nem érhet közvetlenül az ECS-hez.

A zároló olvasás és tranzakcióhatár háttere:
[MariaDB FOR UPDATE](https://mariadb.com/docs/server/reference/sql-statements/data-manipulation/selecting-data/for-update),
[izolációs szintek](https://mariadb.com/docs/server/reference/sql-statements/administrative-sql-statements/set-commands/set-transaction).

## 6. Aszinkron használat

A `MissionPersistence` saját SQL-kapcsolatot nem nyit és nem végez blokkoló
adatbázishívást. Tulajdonolt `Plan` és `ReadCommand` értékeket készít.
Ezeket a meglévő CAsyncSQL workernek kell átadni:

```cpp
auto plan = persistence::Save(checkpoint, registry, owner, storageRevision, operationId);
// Validált plan és saját, élettartamában kezelt pending request szükséges.
sql.ReturnTransactionPrepared(plan.statements, pendingRequest);
// A válasz után: saját kérés/session ellenőrzése, majd szükség esetén Resolve.
```

A `ReadHead`, `ReadSnapshot` és `ReadDefinitionArchive` prepared read-parancsot,
a hozzájuk tartozó decode függvény típusos, ellenőrzött eredményt ad. A decode
egyszer fogyasztja a sorokat; hibás oszlopszámot, kurzort, NULL-t, számtartományt
vagy bináris hosszt elutasít. Az éles adapter ezt az API-t még nem hívja.

A checkpoint-/csomagmezők méretét már az SQL-projekció ellenőrzi: a
túlméretes mező helyett üres, hibás blobjelölést ad vissza, így az nem kerül
a klienskönyvtár nagy cellapufferébe. A valódi SQL NULL megmarad; a hibás
blobot a decoder nem tekinti új, üres progress-nek. A tesztben constraint-
ellenőrzés megkerülésével beírt, 16 MiB fölötti sorok is ezt az utat igazolják.

A generic prepared tranzakció SELECT-sorszám-előfeltételt is kapott:
`expectedResultRows`. Ez az `expectedAffectedRows` külön párja. A méretkeret
512 lépés és 32 MiB; így a legfeljebb 128 gráfos csomag archiválása és a
legfeljebb 256 definícióhivatkozásos checkpoint mentése is belefér a metadata
mellett. Az egyes csomag/checkpoint továbbra is legfeljebb 16 MiB.
A DB `max_allowed_packet` értékének a teljes statementet és paramétereit
is fogadnia kell; a maximális fájlméret feletti protokolltöbblettel is számolni kell.

## 7. Ellenőrzések

Új tesztek: `MissionCheckpointTests`, `MissionPersistenceTests`, valamint
külön új folyamatban futó cold-check. A valódi MariaDB-teszt ugyanazokat a
plan/codec/archívum API-kat és a checked-in migrációt használja.

- Determinisztikus checkpoint, összes rövidebb fájlhossz, hibás állapotok,
  több mint 100 KiB-os distinct checkpoint és 2500 rögzített seedű mutáció.
- Azonos ID/verzió eltérő gráftartalmának elutasítása, régi verziók atomi
  importja aktív új katalógus mellett.
- A migráció kétszeri alkalmazása és a régi quest/Mission Pass sentinelek megőrzése.
- Ismételt archiválás, csomag- és definícióverzió-ütközés, sérült csomag/checkpoint.
- Claim/save/release, token- és epochvédelem, párhuzamos save ugyanarra a revízióra.
- Hiányzó archívum miatti teljes rollback és egyeztetett újrapróbálás.
- Valódi aszinkron worker, `KILL CONNECTION` a checkpoint UPDATE után,
  valamint egy már commitolt save válaszának eldobása a tesztproxyval.
- Egyeztetés még futó írás közben: későbbi commit és későbbi rollback is.
- Az elkülönített MariaDB pontos saját folyamatának kényszerített leállítása,
  InnoDB crash recovery, majd új C++ folyamatban az eredeti progress, választások,
  dedupablak, időzítő és függő reward-kulcs visszatöltése.

A tesztlauncher kizárólag friss, privát datadirt és loopback-portot használ.
Nem állítja le a gép meglévő MariaDB-szolgáltatását. A protokollhiba-próba
plaintext-kivétele továbbra is csak a külön tesztszerverre és tesztgyermekre
érvényes; a normál szerverprogramok TLS-beállításai nem változnak.

Végső, sikeres Windows x64 / RelWithDebInfo eredmények:

| Ellenőrzés | Eredmény |
|---|---|
| GameServer, Database, LoginServer | Lefordult |
| Mission core / csomag | 8723 / 4842 ellenőrzés |
| Új checkpoint codec | 3210 ellenőrzés |
| Prepared SQL-egységteszt | 2232 ellenőrzés |
| Korábbi valódi SQL-infrastruktúra | 204 ellenőrzés |
| Új valódi Mission persistence | 270 ellenőrzés, majd 21 az új folyamatban |
| Célzott regressziós CTest | 27/27 sikeres |
| Külön ASAN CTest | 6/6 sikeres |
| Mindkét valódi MariaDB-suite ASAN alatt | Sikeres, a crash/restart és cold-check is |

A SQL-polling miatt a diagnosztikai ellenőrzések száma további futásokban
növekedhet; a táblázat a rögzített sikeres futást mutatja. A tesztszerver
MariaDB 11.4.13 volt. A naplók a helyi, ignorált buildmappában:
`mission-persistence-full-build.log`, `mission-persistence-final-tests.log`,
`mission-persistence-final-asan-tests.log`, `mission-persistence-asan-tests.log`,
`mission-persistence-base-sql-asan-tests.log`. A tesztpéldány server.log-ja
valódi InnoDB crash recoveryt is rögzített. FreeBSD és Unity end-to-end próba
ebben a részfázisban nem történt.

A későbbi, túlméretes soros bővítés után a GameServer és a persistence-teszt
újrafordult; a célzott integrációs kör normál és ASAN builddel is sikeres.
Naplók: `mission-persistence-bounded-tests.log` és
`mission-persistence-bounded-asan-tests.log`. A módosított olvasásokat lefedő
integrációs kör után a teljes 27-es kör is ismét sikeresen lefutott a végleges
változaton. A nagy tesztblobokhoz csak a privát tesztszerver kapott 64 MiB-os
`max_allowed_packet` beállítást.

## 8. Következő feladatok

1. Az outbox és a szimbolikus entitlement-ledger a 4/C-ben elkészült;
   az item/XP/pénz és fogyasztás pontos idempotens hatásszerződése még hátravan.
2. A tényleges kézbesítési réteg crash/retry/full-inventory próbái.
3. Authoritative session-/core-koordináció és a persistence callback-életciklus
   bekötése; ezek a jogosultsági rétegek még nem következnek automatikusan a DB-fence-ből.
4. Natív gameplay adapterek és az első NPC → célok → választás → jutalom szelet.
5. Unity presentation/dialógus, majd Mission Studio és tartalommigráció.

A tényleges kézbesítő és session-koordinátor hiányában a teljes Mission Engine
játékosokra való bekapcsolása még nem kész feladat. Az éles telepítés és a régi rendszerek
adatainak migrációja külön ellenőrzést és visszaállítási tervet igényel.
