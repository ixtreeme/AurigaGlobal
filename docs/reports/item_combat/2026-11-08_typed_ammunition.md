# C31 — Típusos íj és lőszer

Szerver: `20261103`; Unity: `/main/unity63-integration-20261103`,
Editor `6000.3.27f1`, párosított Unity-changeset **cs:92**. Szerződés: [IC-066](../../adr/item_combat_066_typed_ammunition.md).

Elkészült az önálló lőszertípus, a 20. felszerelési hely, az íjjal
közös lőszerpolicy, a felszerelés/levétel és az elfogadott lövésenkénti
tartós fogyás. Az azonos tartalék automatikusan felszerelődik.
Aktív, jogosult lélek esetén a lőszer és a lélek ugyanabban az SQL-írásban
fogy. A Unity tényleges felszerelési gombja, harci lőszerkapuja és a
leltárban látható darabszám is be van kötve.

## Valódi helyi játékpróba

Két elkülönített TLS-karakter, tényleges Unity-leltár-/lélekablak-gombok,
valódi CG-fly-target/shoot és tanult íjászképesség: **274 ellenőrzés és
4 újrabelépés sikeres**. Kilenc elfogadott lövés pontosan kilenc lőszert
fogyasztott a három, egyenként háromdarabos saját csomagból.
A végső szervercsomag újraindítása után további **38 ellenőrzés és
1 újrabelépés sikeres**: az íj felszerelve, a lőszer elfogyva, a vörös
üres/inaktív, a kék aktív és egy elhasznált találattal tér vissza.
Összesen **312 élő ellenőrzés és 5 újrabelépés** sikeres.

| Eset | Igazolt eredmény |
|---|---|
| Felszerelés | Az íj lőszer nélkül is felvehető; hiányzó vagy más policyjú lőszerrel nincs lövés/fogyás |
| Érvénytelen kérés | Nem létező cél, önmagára célzás, tanulatlan képesség nem fogyaszt és nem sebez |
| Normál lövés | Egy lőszer és egy vörös lélektalálat fogy; a kék változatlan |
| Képességlövés | A szerver valódi 46-os íjászképessége egy lőszert és egy kék lélektalálatot fogyaszt; a vörös változatlan |
| Részleges csomag | Felszerelt íj, egy megmaradt lőszer és mindkét lélek elhasznált találata pontosan visszatér újrabelépéskor |
| Automatikus tartalék | Az utolsó darab és a következő azonos csomag felszerelése atomikus; az új csomag ugyanazzal az ID-val marad felszerelve újrabelépéskor |
| Levétel | A táskában tartott tartalék nem enged lövést; visszafelszereléskor nem változik a mennyiség |
| Kifogyás | Az íj megmarad, lőszer nélkül nincs további lövés; az idegen policyjú kétdarabos tartalék érintetlen |
| Elfogadott PvP | Egy lőszer fogy; a PvE-lélekállapot változatlan |

A kilenc tényleges damage-packet értéke: **1180, 11812, 1180, 946,
946, 708, 708, 472, 386**. A vörös hat normál találat után kiürül;
a hetedik normál találat bónusz nélkül sebez. A skill külön a kéket
fogyasztja. A legutolsó lövés a másik saját PvP-karaktert éri.

Az SQL-ben **9 egyedi lőszerevent**, összesen **9 levont darab** és
**2 automatikus tartalékváltás** van. A damage-packetek multihalmaza
megegyezik az SQL-kimenetekkel; **7 lélekevent** tartozik hozzájuk.
Mindhárom elfogyott forráspéldány a saját eredeti ID-jával,
nulla darabbal és felszabadított hellyel megmarad az auditban.
Offline várakozáskor az állapot változatlan.

A végső próba **994 másik typed példányt**, védett pénztárcát és tanulási
adatot változatlanul hagyott. Legacy tárgy nem került a tesztkarakterekre.
Az elkülönített előkészítő/részpróbák adatait nem állítottuk vissza.
A 65076-os ideiglenes szörnyet és a térkép regenjét visszaállítottuk.

## Natív, SQL, Unity és kezelő

- **46 kiválasztott tesztcsoport sikeres** a végső, érintett újrafuttatásokkal
  együtt. Összesítés: `c31-validation-evidence.json`; az utolsó natív lőszer-
  kiterjesztés **866 ellenőrzést** tartalmaz. A többdarabos policy és az
  int64 sebzés is ellenőrzött, nincs külön lőszeroldali sebzésplafon.
- A valódi MariaDB-writer próbája igazolja az együttes lőszer-/lélekírást,
  az operation replayt, azonos event új operation alatti elutasítását,
  teljes rollbacket, automatikus tartalékot és új sessionből visszatöltést.
- Unity-fordítás és **1340 új ellenőrzés sikeres**: natív golden,
  minden csonkolás, minden TCP-darabolás, tényleges felszerelési gomb,
  látható darabszám, kompatibilitási magyarázat és közös harci kapu.
  A korábbi companion/leltár/stat/jelmez/C29/lélek validátorok is lefutottak.
- A Qt-kezelő lőszerlapjának valódi betöltés/gyűjtés/natív validáció próbája
  sikeres. A policy a listából választható; a fegyveroldali profil másolása
  figyelembe veszi a rá hivatkozó lőszert is.
- A Windows GameServer és a két natív helper fordítása sikeres.
  A CG/GC-fejlécek ütközésellenőrzése sikeres; nincs új CG azonosító.

## Telepítés és ellenőrzési határ

A 035 katalógus- és 019 példánymigráció leállított saját stacken,
teljes player/protos mentéssel kétszer lefutott. **970 meglévő példány**
és a korábbi katalógussorok változatlanok; csak a szükséges kisebb
felszereléstárolók szélesedtek 20 helyre. Katalógus: **v29**.
A végleges natív binárisok, kezelő és migrációk a `Server2-Local`
csomagba kerültek mentéssel és manifesttel. Mindhárom natív
bináris és a **294 kezelőfájl** hash-e egyezik; **24 Unity forrás/meta**
egyezik az aktív workspace-szel. A korábbi **196 vizuális asset**
változatlan, az öt saját szerverfolyamat fut.

A próba az aktív Unity-forrásokkal egyező ellenőrzőmásolatban futott.
A normál legacy leltár és a teljes tartalomkonverzió külön feladat.
Nem futott teljes világjelenetes/animációs, production íjmodell-/VFX-,
standalone, FreeBSD-, terheléses vagy SQL-kiesés/crash-injection próba.
A korábbi legacy harci procrendszer és az ECS-világ HP-ja nem vált
tranzakciós harci outboxszá; nincs teljes világ-ACID állítás.

Bizonyítékok az ignorált `build` alatt: `c31-native-tests7.log`,
`c31-native-tests8.log`, `c31-native-tests9.log`, `c31-validation-evidence.json`,
`c31-manager-ui2.log`, `c31-unity-offline2.log`, `c31-ammo-live3.log`,
`c31-ammo-sql1.log`, `c31-ammo-sql2.log`, `c31-ammo-restart1.log`, `c31-migration-evidence.json`, `c31-game-build5.log`,
`c31-native-build9.log`, `c31-manager-build5.log` és `c31-deployment-evidence.json`.
A fixture hitelesítési adatai ignoráltak.
