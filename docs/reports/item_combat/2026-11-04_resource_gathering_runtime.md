# C25c-1 — Gyűjtőhelyes fogás/gyűjtés és szerzett gyakorlat

2026-11-04. Szerződés: [IC-055](../../adr/item_combat_055_resource_gathering_runtime.md).

## Elkészült és kipróbálható

Kijelölt, valódi világentitáshoz kötött, szerver által időzített halászat,
bányászat, favágás és növénygyűjtés. Két explicit botkészlet, súlyozott
ItemKey-jutalmak, eszközerő és csali alapján számolt esély, szerzett
gyakorlópont. Jutalom, gyakorlat és csalifelhasználás egy SQL-tranzakcióban
mentődik. A Unity inventory/eszközablakból preview, start és cancel elérhető.

Ez a C25c első, játszható gyűjtőhelyes menete. A legacy szabadvízi halászati
minijáték, annak visszajelzései és interakciói még C25c-2. A korábbi vnumos
fogási táblák teljes konverziója és a végleges gyűjtőhelyek/assetek továbbra
is külön tartalmi munka. A normál legacy leltár migrációja a felhasználó
kérésére vár.

## Tényleges ellenőrzések

| Ellenőrzés | Eredmény |
|---|---|
| Release GameServer / Database / LoginServer, segédek és végső GameServer | sikeres |
| Item-CTest az új resource csoporttal | 25/25 csoport |
| ResourceGatheringTests | 27 célzott ellenőrzés |
| ItemPersistenceSqlTests saját MariaDB-ben | 1875 ellenőrzés |
| PointCalculationTests | 7398 ellenőrzés |
| GmCommandTests | 31564 ellenőrzés |
| Unity 6000.3.27f1 scriptfordítás | sikeres |
| Új golden/kodek/TCP split/dispatcher/UI validátor | 11831 ellenőrzés |
| Korábbi C4–C25b Unity-validátorok | mind sikeres |
| Valódi Unity → TLS → GameServer → SQL/UI | 103 ellenőrzés |
| Ellenőrzött forrássnapshot | 5540 fájl, 1748 byteazonos C# |
| Korábbi helyi tárgyak | 139 teljes állapot, revízió és hash pontosan egyezik |
| Learning, karakterfejlődés és hely | változatlan; teszt HP/SP visszaállítva |
| Telepített binárisok, manager, NPC- és szabályfájl | hash-ellenőrzött |
| Érintett Python-szintaxis, git diff check | sikeres |

A natív tesztek a pontos siker- és súlyhatárokat, a sikertelen gyakorlásdobást,
a determinisztikus esetek RNG-mentességét, a pontkorlátokat, a teljes jutalom
halmokra osztását, az összes szükséges ID és hely RNG előtti ellenőrzését,
a befejezési kaput és a hibás RNG elutasítását fedik le. A receipt a dobások
boundját és értékét is megőrzi.

Az SQL-próba valódi közös jutalom/pont/csali commitot hajtott végre. Hibás
bot-before-state visszagörgette a jutalmat is. A receipt replay nem adott
második jutalmat vagy pontot; a csali elfogyott. A teljes felszerelt állapot
és az új jutalom új owner-sessionben is pontosan visszatöltődött.

Az élő próba a tényleges inventory- és eszközablakgombokkal indított. Csali
nélküli bot nem halászott. A kézi cancel és az elmozdulás nem adott jutalmat,
nem vont le csalit és nem adott pontot. A start és a korai poll csak Running
állapotot eredményezett; a szerver órájának lejárta adta a mentett jutalmat.
Az ismételt start az eredményt küldte új jutalom nélkül.

Az első botkészlet két sikerrel összesen 14 halat adott, 3-as maxStackkel.
A két befejezett fogás két valódi gyakorlópontot adott. Ezzel ugyanazt a botot
levéve, a közeli valódi horgásznál +1 szintre lehetett fejleszteni, 0 pontra
visszaállva. A második botkészlet saját táblájából 2 másik halat adott; az első
készlet mennyisége változatlan maradt.

A csákány, fejsze és olló a saját helyén 4 ércet, 3 fát és 2 növényt, valamint
egy-egy gyakorlópontot adott. A 0%-os csákánypróba receiptes kudarcot, nulla
jutalmat és nulla pontot eredményezett. Gyűjtés közbeni kilépés és újrabelépés
után a +1 bot, a pontok, a csali és minden megszerzett tárgy pontosan megmaradt;
a befejezetlen műveletből nem keletkezett jutalom. A régi login nonce-a Stale.

## Helyi készlet és használat

Az igazolt helyi próbakarakter hat új eszközt és külön csalit kapott.
Minden player/protos SQL, korábbi bináris, manifest, karakter/learning és
tárgyhash mentése a csomag `run/deployments/resource-*` könyvtárában van.
A teljes korábbi manager külön mentésben maradt. Új SQL-séma nem kellett;
a C25b 009-es példánybővítése és a v26 katalógus maradt.

A rendezett készlet 157 élő itemállapot. Mind a 139 korábbi tárgy mezői/hash-e
változatlanok. Az új első bot megőrizte a +1 szintet, 0 pontot és 50-es csalit;
a másik bot, a csákány, fejsze és olló egy-egy pontot. A kudarcpróba 0 pontos.
A jutalmak megmaradtak, az új csalikészlet ismét összesen 8 darab.
A kézhely üres; öt saját szerverfolyamat újra fut.

Külön helyi NPC-fixture négy új erőforrás-entitás. A régi NPC-lista minden
sora, a horgász, kovács és tűz megmaradt. A favágás/növénygyűjtés helyein a
helyi próba meglévő erőforrás-modelleket használ; végleges fa/növényassetet
nem igazol. A hat helyi szabály tesztbalansz, nem kiadási fogási tábla.

Manuális menet a külön typed inventoryban: felszerelés → **Eszköz** →
**Gyűjtés előnézete** → **Gyűjtés indítása**. Célzott megfelelő helyet használ,
cél nélkül a legközelebbit. A helyén állva megvárható, vagy megszakítható.
Botra előbb a külön új csalit kell alkalmazni. A normál régi leltár változatlan.

Unity: **cs66** a `/main/codex-mission-engine` ágon, kizárólag a 21 saját fájllal.
A felhasználó jelenet-, világasset-, kapcsolat-, Packages- és
ProjectSettings-módosításai kimaradtak. A ProjectSettings.asset hash-e változatlan.
Az eredeti Editorhoz nem kellett hozzányúlni; külön forrássnapshotban futott a próba.
A lezáráskor 16, nem érintett UI-atlasz/ikon `.meta` eltért a tesztelt snapshot
forráshashétől. Ezek megmaradtak és kimaradtak a commitból. A teljes 1748 C#
és mind a 21 saját Unity-fájl továbbra is egyezett a tesztelt forrással.

## Nyitott határok

C25c-2: szabadvízi/minijátékos halászati indítás és visszajelzések. A végleges
világhelyek, termelési táblák és vizuális/animációs munka még nincs konvertálva.
A resource események általános Mission/Mission Pass továbbítása külön bekötés;
vnumos itemeseményre nincs visszaalakítás.

A hiányzó SpeedTree/VFX/Crest assetek miatt teljes világjelenet-/playerbuild
nem készült. Külön élő arena/PvP, eltűnő cél és többjátékos terhelés nem futott.
FreeBSD, több core közötti handoff, crash/hibainjektálás, éles adatkonverzió
és külön Qt manager UI-próba nem futott. Friss csomag teljes újraépítése nem
futott, a telepített páros csomag hash-ellenőrzött. A JSON Schema külső
metaséma-validátora nem futott; a natív betöltő a hat élő szabályt ellenőrizte.
A legacy leltárátállás, C19 HP/SP crash-atomikusság és az F7 kapuk nyitottak.
