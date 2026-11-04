# C25c-2 — Szabadvízi halászat, minijáték és teljes halas lánc

2026-11-04. Szerződés: [IC-056](../../adr/item_combat_056_free_water_fishing.md).

## Elkészült

Valódi víz felé dobás, közös determinisztikus halmozgás, szerveres inputidő- és
sorszámellenőrzés, három találat, hibabüntetés és szerveres lejárat. A fogás
típusos tárgyat ad; a sikeresen megszerzett gyakorlatból ugyanaz a bot
fejleszthető. A fogott hal használata és sütése is végigfutott az új útvonalon.
Szerver, Unity UI, protokoll, kodek és helyi csomag együtt frissült.

## Tényleges ellenőrzések

| Ellenőrzés | Eredmény |
|---|---|
| Release GameServer / Database / LoginServer és natív segédek | sikeres |
| Item-CTest az új minijátékkal | 26/26 csoport |
| FishingChallengeTests | 3625 ellenőrzés |
| Külön saját MariaDB ItemPersistenceSqlTests | 1884 ellenőrzés |
| PointCalculationTests / GmCommandTests | 7398 / 31564 |
| Unity 6000.3.27f1 scriptfordítás | sikeres |
| Új kodek, mozgás, TCP split, dispatcher és UI validátor | 12772 ellenőrzés |
| Korábbi C4–C25c-1 Unity-validátorok | mind sikeres |
| Valódi Unity → TLS → GameServer → SQL/UI | 75 ellenőrzés |
| Forrássnapshot | 5556 fájl, 1751 byteazonos C# |
| Korábbi helyi tárgyak | mind a 157 teljes állapot, revízió és hash egyezik |
| Karakterfejlődés és learning | változatlan; pozíció/HP/SP visszaállítva |
| Páros helyi binárisok, manager, szabály- és NPC-fájl | hash-ellenőrzött |

A natív próbák minden útvonalat végigjárnak, és igazolják, hogy időben három
találat elérhető. Ellenőrzik a pontos mozgást, cooldown-t, input replayt,
átugrott sorszámot, régi/jövőbeli mintát, hibabüntetést és a határidőt.
Az SQL-ben a sikertelen minijáték egyszer fogyaszt csalierőt, jutalom és
gyakorlás-RNG nélkül; receipt és teljes botállapot relog után is megmarad.

Az élő próbához a meglévő map41 attribútumfájljában természetes vízpartot
kerestem, a terep átírása nélkül. A teszt-PC helye a próba alatt
(948775,296425), a dobás 270° felé (948375,296425). Az ellenkező irányú száraz
dobás elutasított, csali nélkül sem indulhatott cast. A kézi cancel és a
mozgás nem adott jutalmat és nem fogyasztott csalit.

A tényleges eszközablak és minijátékpanel gombjai futottak. Az azonnali
inputspam nem fejezte be a játékot; a hamis jövőbeli minta és ismételt
sorszám nem adott extra találatot. A hibás nonce üres Stale választ kapott,
miközben az aktív UI megmaradt. A közös pályán megjelenített halra kattintva
a szerver három valódi találatot fogadott el.

Az első készlet két fogása 6 új élő halat és két valódi pontot adott.
Ebből ugyanaz a bot a valódi horgásznál +1 lett, 0 pontra visszaállva.
A második botkészlet egy másik élő halat adott, az első készlet változatlanul.
Egy fogott hal tényleges inventory-megerősítéssel 2 döglött hallá alakult,
majd a közeli valódi tűznél 2 sült hallá. Az indítás replaye nem adott
második jutalmat. Input nélküli lejárat elfogyasztotta a csalit, nulla halat
és pontot adva. Gyűjtés közbeni kilépés/relog megőrizte a teljes eredményt
és a még el nem használt csalit; a régi cast nem volt folytatható.

## Helyi készlet és verziókezelés

Csak az igazolt saját próbakarakter kapott új vízi eszközöket/csalit. A teljes
player/protos SQL, manifest, korábbi binárisok és manager, karakter/learning,
tárgyhash-ek, NPC- és korábbi szabályfájl mentése a `run/deployments/fishing-mini-*`
könyvtárakban maradt. Új példányséma nem kellett; katalógus v26, item_instance 009.

A rendezett készlet 165 élő itemállapot. A régi 157 minden mezője/hash-e
változatlan. Az új bot +1/0 pont/50 csali állapota, a második bot egy pontja,
5 első készletű hal, 1 második készletű hal és 2 sült hal megmaradt.
A vízi csalikészlet ismét 8 darab. A karakter visszakerült eredeti helyére,
HP/SP-je és fejlődése megegyezik a mentéssel. A kézhely üres.

A természetes vízpartnál külön horgász- és tűz-fixture maradt; minden régi
NPC-sor megmaradt. A hat C25c-1 helyi szabály változatlan, a két új vízi
szabály mellé került. A helyi stack öt saját folyamattal fut.

Unity: **cs67** a `/main/codex-mission-engine` ágon, 24 saját fájllal.
A saját fájlok és a tesztelt C# egyezése ellenőrzött, a más jellegű
felhasználói jelenet-/asset-/Packages-/ProjectSettings-módosítások kimaradnak.
Az eredeti Editor nyitva maradt; a tesztek elkülönített snapshotban futottak.

## Határok

A végleges fogási táblák, itemcsaládok és világhelyek konverziója nem készült
el ettől a runtime bekötéstől. A helyi szabályok tudatos tesztbalanszt használnak.
A minijáték időtűrése 250 ms; magas késleltetésű hálózati próbák még nem futottak.
Nem készült teljes világjelenet-/playerbuild a hiányzó SpeedTree/VFX/Crest assetek
miatt; a szabadvízi animációs csomagút fordult, a teljes világban minden vizuális
visszajelzés külön ellenőrzése még vár. Külön arena/PvP, többjátékos stressz,
FreeBSD, cross-core handoff és crash-injektálás nem futott.

Az általános typed Mission/Mission Pass események, további itemkategóriák,
legacy inventorymigráció, C19 HP/SP crash-atomikusság és F7 kapuk nyitottak.
