# ADR IC-056 — Szabadvízi halászat és szerveres minijáték

2026-11-04, C25c-2. Előzmény: [IC-055](item_combat_055_resource_gathering_runtime.md).

Elkészült a vízpartról, NPC nélkül indítható halászat, a meglévő halas UI-val,
szerver által ellenőrzött találatokkal. A három találat után a C25c-1 közös
tranzakciója adja a típusos fogást és a gyakorlópontot. A C25a halfeldolgozás
és sütés az így fogott halra is működik. A legacy inventory átállítása vár.

## Dobás és tartalom

A `typed_gathering.json` v2 a meglévő `sites` sorokat megtartja. A `race: 0`
szabadvízi szabályt jelent, csak horgászbothoz. A map és az első/második
botkészlet választja a táblát, a `range` a dobás távolsága (helyben 400).
Az idő legalább 5000 és legfeljebb 120000 ms; a helyi próba 15000 ms.
A v1 továbbra is csak valódi világentitásos helyeket enged. A v2 is képes
kezelni a korábbi helyeket, változatlan v3 csomaggal.

Az előnézet a szerveren ismert karakterirányból számolja a dobást. Az indulási
cella járható szárazföld, a végpont tényleges `ATTR_WATER`. A dobás vonalát
50 egységenként ellenőrzi: nem mehet át szárazföldi akadályon. A víz lehet
járhatatlan, ez természetes vízfelületnél nem zárja ki a halászatot.
A hely, map, teljes botpéldány, owner-session, login token és katalógus
rögzített. Elmozdulás, kijelentkezés és a korábbi admission tiltásai érvényesek.

Az eszközablak **Szabadvízi halászat** gombja és a fő kliens `FISHING`
művelete ezt az utat nyitja. NPC-cél nem szükséges. Bot nélkül a kliens
felszerelési útmutatást ad; csali és víz felé fordulás nélkül a szerver nem
enged indítást. A külön **Gyűjtés indítása** kezdi el a minijátékot.

## Közös halmozgás és input

A hal a korábbi fish-box tizenkét waypointján, 180 pixel/másodperccel halad.
A számítás egész számos, tizedpixelben történik. A szerver nonce-ból kapott
seed a hat kezdőútvonal közül választ. A natív és Unity-számítást 96 közös
mozgásmintás golden és az egész útvonalat vizsgáló tesztek egyeztetik.
A célterület a korábbi 74..175 × 50..141 pixeles téglalap.

A kliens kattintást, sorszámot és a megjelenített mintavételi időt küldi.
Nem jelentheti ki, hogy sikeres találatot ért el. A szerver a közös pályán
ellenőrzi a hal helyét. A sorszám következő értéke kötelező, legfeljebb 1024;
replay/átugrás nem ad találatot. A minta legfeljebb 250 ms-mal térhet el a
szerver monotonic idejétől, és két elfogadható próbálkozás között legalább
1000 ms szerveridő kell. Az érvényes inputok sorszáma az elutasított túl gyors
vagy rossz időpontú kérésnél is halad; ezek nem adnak találatot.

A halra kattintás csak célterületen találat; a vízre vagy a célon kívüli halra
kattintás hiba. A hiba 1000 ms-ot vesz el az időkeretből. A három szerveresen
ellenőrzött találat engedi az eredménydobást. A szerveres határidő lejárta
találatok nélkül is befejez, de sikertelenül: nincs loot vagy gyakorlás-RNG,
nincs jutalom/pont, a csali elfogy. Befejezés előtti kézi megszakítás vagy
elmozdulás megőrzi a csalit. Kliensoldali befejezés- vagy pontjóváíró RPC nincs.

A sikeres minijáték után a szabály szerinti fogási esély érvényes, tehát a
három találat önmagában nem írhatja felül a fogási profilt. A jutalom, a pont
és a csali továbbra is egy receiptes SQL-művelet. A receipt a seedet, input-
sorszámot, találat/hiba darabszámot és befejezési időt is rögzíti.
A régi animációs GC-fishing csomagok indulást/leállítást mutatnak; siker/kudarc
csak igazolt commit után jelenik meg. Item-vnum nem kerül az új jutalomútba.

## Protokoll és kliens

CG254 a meglévő recovery szerkezetben, új actionök:
14 víz-preview, 15 indítás, 16 kattintás, 17 megszakítás, 18 állapotlekérés.
A preview nonce=0; a további kérések a saját bot ID-ját és szerveres nonce-ot
igénylik. A `target` csak inputnál használatos: felső u32 a mintavételi ms,
alsó 31 bit a sorszám, bit31 jelzi a vízre kattintást. Máskor nulla.

GC254 v4 a v3 adatain a chance és a név közé további 24 bájtot illeszt:
seed:u32, elapsed:u32, need:u8, hits:u8, misses:u8, inputSequence:u32,
inputResult:u8, castX:i32, castY:i32. Target=0. A v1/v2/v3 utak megmaradtak.
Négy packet-golden és a mozgásgolden a szerződés része.

A meglévő `Metin2FishingNewPanel` külön typed módban a közös mozgást használja,
és a szerver találatszámát mutatja. A kattintás nem növeli helyben a számlálót.
Egy input megvárja a visszaigazolást; a régi visszaigazolás nem oldja fel az
új input várakozását. A régi legacy panelmód változatlanul megmaradt.
A lejárt kérés üres Stale válasza nem írja felül egy másik aktív cast UI-ját.

Ellenőrzések és helyi tesztadat:
[C25c-2 jelentés](../reports/item_combat/2026-11-04_free_water_fishing.md).
