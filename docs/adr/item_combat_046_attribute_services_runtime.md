# ADR IC-046 — Bónuszszolgáltatások szerveren és Unityben

2026-11-03, C18, F4/F5. Előzmény:
[IC-031 példánybónuszok](item_combat_031_instance_attributes.md),
[C17 időkezelés](item_combat_045_accessory_socket_runtime.md).

## Működés

Az IC-031 öt művelete saját, leltárban lévő, bónuszprofilos fegyverre,
páncélra és ékszerre elérhető: hozzáadás, csoport újrasorsolása, védelem,
védelem áthelyezése és feloldása. A normál/ritka/extra csoport külön
kapacitású (legfeljebb 5/2/1). A profil adja a célmaszkot, célszintkorlátot,
csoportot, sorigényt, hozzáadandó sorokat, esélyt, fogyasztást és szinteloszlást.
A forrás use policy-je és minimumszintje érvényesül; a cél viselhetősége
nem feltétel. Felszerelt célon nem működik.

Az ID, fejlesztési szint, kötés, kövek, ékszerállapot és a nem érintett
bónuszok megmaradnak. Részhalom azonos ID-n/helyen fogy, az utolsó darab
tombstone. Nincs új output, ezért teli leltárban is használható.

Hozzáadás nem ír felül régi sort. Újrasorsolás ugyanannyi sort ad, megtartja
a védett normál sor pontos értékét/helyét és a másik csoportokat. Az eredmény
a régivel azonos is lehet; ezt nem dobjuk újra. Normálon belül, valamint
ritka és extra között közösen tilos az ismétlés. A statikus base/refinement
kizárás a célprofil döntése.

Az első védelemhez tele normál csoport kell. A szerver egyenletesen választ
a foglalt helyekből; áthelyezéskor a már védett hely nem jelölt. Feloldáskor
csak a védelem szűnik meg. Mindhárom a profil eszközét fogyasztja; a
feloldásnak nincs véletlen döntése.

## Előnézet, sorsolás és mentés

A `PrepareAttributeRuntime` teljes, determinisztikus sikeres próbatervvel
ellenőrzi az IC-031 előfeltételeit. A választható típusokat, súlyokat,
értékeket, szintesélyeket, védhető indexeket és húzásszámot adja vissza.
Nem olvas gazdasági RNG-t és nem készít elrejtett új bónuszsorokat.

Megerősítéskor a teljes quote, forrás/cél és a tényleges storage írási terv
is újraellenőrzött RNG előtt. OpenSSL rejection sampling ad egyenletes,
inkluzív döntést. Szintdobás: 1..10000; típusdobás: az addigi kizárások után
megmaradt súlyösszeg. A választott típus kikerül a következő húzásból.
Nincs modulo bias, próbálgató újradobás vagy kliensoldali véletlen döntés.

Sikertelen hozzáadás a profil szerint fogyaszt vagy megtartja az eszközt;
a régi bónuszállapot megmarad. Ha a szinteloszlás összege nem 10000, a
fennmaradó tartomány **nem ad eredményt és nem fogyaszt** (IC-031). Nem
normalizálunk és nem dobunk újra. Ez is lezárja a nonce-ot. A Unity a teljes
siker esélyét az előzetes sikeresély és a lefedett szinttartomány
húzásonkénti valószínűségének szorzataként mutatja; a lépések külön is látszanak.

A meglévő `MakeAttributeCommit`/`RequestCommit` egyetlen tranzakcióban írja
a teljes bonus replacementet, pontos eszközdebitet és receiptet. Az `ATR1`
eredmény a success/lock/tier/type dobásokat és a teljes v1 AGAT állapotot
együtt tárolja. Azonos after state mellett eltérő dobás digestje is különböző.
A régi instance codec és DB-séma változatlan. CommitUnknown a közös locking
receipt reconciliation útvonalon rendeződik.

Egy nonce egyszer próbálható; ismétlése ugyanazt a státuszt/eredményt adja,
további dobás és fogyasztás nélkül. Más target, régi login vagy megváltozott
inventory/quote elutasított. A C17 kizárólag időmentést tartalmazó revisionjei
átvezethetők a friss fence-re, az eredeti source/target snapshot megtartásával.
Az időcheckpoint a Unity megerősítését sem zárja be; más változás igen.

## Protokoll és Unity

**I → bónuszeszköz → Bónuszok → céltárgy → Művelet áttekintése → megerősítés**.
Jelenlegi sorok, védett hely, pontos költség, teljes esély, szinteloszlás és
az első húzás jelöltjeinek súlyai láthatók. A UI jelzi a véletlen védelem-
választást, azonos eredmény lehetőségét, fogyasztó/nem fogyasztó kudarcot és
az eredménytelen szinttartományt. Inventoryt kizárólag a GC225 módosít.

- **CG236:** 26 byte + sequence: header/action:u8, nonce/source/target:u64.
  Action 1 preview, 2 confirm, 3 close; nincs kliensoldali dobás/bonus ID.
- **GC236 v1:** legalább 271 byte, legfeljebb 1 MiB. Nonce, két ID és revision;
  két ItemKey/szint; operation/pool/units/available/chance/failure-policy/
  capacity/draw-count; két 97 byte-os attribútumállapot; szintlista, súlyozott
  jelöltek és értékek, lock-indexek, targetlista és GC227. Eredmény előtt az
  after megegyezik a before-ral. A kliens a teljes átmenetet és védelmet,
  valamint a quote szerinti lehetséges eredményértékeket is ellenőrzi.
- **GC227 v2:** bónuszszolgáltató tárgyat tartalmazó metadata esetén csak az
  action mező bővül u8-ról u16-ra, Attributes bit 256. A többi metadata v1
  marad; a korábbi csomagok/goldenek változatlanok.

## Ellenőrzés és határok

Natív, külön SQL, korábbi Unity-regressziók és négy valódi TLS-fázis lefutott.
Részletek: [C18 tesztjelentés](../reports/item_combat/2026-11-03_attribute_services_runtime.md).
Kapcsolt/Gauss addon, jelmez/stóla/rúna/reset és a teljes APPLY-konverzió
külön munka marad. A lock az IC-031 szerinti véletlen választás, nem kézi
sorválasztó. A C17 időelszámolásának crash/handoff korlátja nem változott.

2026-11-07-i kiegészítés: a kapcsolt addon, jelmezreset/donor/törlés,
stóla és rúna közös szerver–Unity útját a
[C29 / IC-064](item_combat_064_regalia_special_bonuses.md) készíti el.
A C18 normál attribútumszolgáltatása és új protokollt használó C29 együtt
él; a C18 korábbi lezárásának fenti határa történeti. Teljes APPLY- és
tartalomkonverzió nem történt.
