# ADR IC-049 — Időzített és kapcsolható tárgyhatások

2026-11-03, C21. Az IC-015/IC-017 tiszta tervezői az IC-047 közös,
tartós online órájára épülnek. A szerver és a külön típusos Unity-felület
együtt változik. A normál legacy leltár átállítása a felhasználó kérésére vár.

## Viselkedés

Az időzített ital elfogyasztott darabjához egy teljes bónuszcsomag tartozik.
Azonos csoport esetén a katalógus szerinti Reject vagy Replace szabály
érvényesül; a csere az egész korábbi csomagot eltávolítja. Az online idő
csak játékban csökken, a falióra szerinti hatás UTC-határidővel offline is
lejár. Halálkor a csoport remove-on-death beállítása érvényesül.

A kapcsolható harmat önálló, nem halmozható példány, véges aktív idővel.
Bekapcsolás nem fogyaszt darabot. Azonos csoport és változat másik példánya
elutasított, eltérő változat egy tranzakcióban cseréli le az előzőt. A régi
pontos maradék megőrződik. A gyors kikapcsolás előtt az eltelt ezredmásodpercek
levonódnak; kikapcsolva és offline nem fogy az idő. Kifogyáskor a példány
tombstone lesz. Aktív tárgy nem mozgatható, adható át, dobható el, adható el
vagy fogyasztható el más művelettel; ezt a közös commit-validátor is védi.

A szerver ellenőrzi a szintet, kasztot, nemet, harci/PvP/helyszabályokat,
cooldownt és a házassági/csoportvezetői követelményt. Követelmény elvesztése
elszámolja az addigi aktív időt és kikapcsol. A kikapcsolás a közös
tárgyműveleti munkamenet szabályain belül tiltott/dead állapotból is elérhető.

## Mentés és tényleges bónuszok

Az időzített hatások az owner recovery-sidecarjába kerülnek: azonosító,
ItemKey, online maradék vagy falióra-határidő. Az RCS2 kódolás csak akkor
használatos, ha vannak időzített hatások; a régi üres/RCS1 adatok és hash-ek
változatlanok. A futásidejű korlát 32 időzített csoport és 16 aktív harmat.

A `006_item_effects.sql` két nullable oszlopot ad a példánytáblához:
`toggle_remaining_ms`, `toggle_active`. Régi példányok hash-e nem változik;
a harmat digestje a TGL1 kiterjesztésben az új állapotot is fedi. Teljes
before-state CAS, owner-fence és receipt védi a forrásfogyasztást, cserét,
időt, közös cooldownt és az aktivitást. Betöltéskor a hibás állapot,
duplikált aktív csoport és sérült hash elutasított.

A monoton recovery-óra minden recovery-commit határán elszámolja a már
aktív harmatokat is. Egyéb tárgyműveletek nem nullázzák az órát. Logoutkor
a töredékidő mentődik, a következő same-core login megvárja a lezárást.

A hatáscsomagok a hiteles ownerállapotból, felszereléssel együtt épülnek a
valós statokba és harci bónuszokba. Nem ismétlődő affect-jóváírások. Lejárat,
csere, törlés és kikapcsolás eltávolítja őket, a maximumok normál clampjával.
A statprojekció a még folyamatban levő heartbeat előtt is figyelembe veszi
a lejáratot; az alapadat csak sikeres mentés után változik.

## Protokoll és külön Unity-felület

CG243: 26 byte + sequence, külön munkamenet a GC/CG242 visszatöltésektől.
Előnézet és egyszer használható nonce-os megerősítés, aktív lista, bezárás,
generációval védett egyedi/összes időzített törlés. GC243 v1 a forrásprofilt,
teljes bónuszcsomagokat, maradékokat és GC227 metadatát küldi. A kliens
szigorú kodekje kezeli a TCP-töredezést és elutasítja a hiányos csomagot.

GC225 v3 csak harmatok jelenlétében, 8-as flaggel ad maradék:u64 és
aktív:u8 blokkot. A v1/v2 továbbra is olvasható. GC227 v2 új Effects
actionbitje 1024. A kliens az aktív harmaton csak a kezelést engedi.

A külön típusos leltárban: **tárgy → Hatás → Használat áttekintése → Hatás
megerősítése**. A felső **Hatások** gomb az aktív listát nyitja. Egyedi és
összes időzített hatás törléséhez külön megerősítés kell; a harmatok külön
kezelhetők. Darabfogyást, statot és időt a kliens nem ír helyben.

Ellenőrzések: [C21 jelentés](../reports/item_combat/2026-11-03_item_effects_runtime.md).
Teljes player build, FreeBSD, többcore-os handoff, terhelés és crash-injection
külön kiadási kapu marad; az IC-047/048 HP/SP-mentési crash-rése változatlan.
