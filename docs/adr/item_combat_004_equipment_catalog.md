# ADR IC-004 — Első típusos felszereléskatalógus és refinement

Az alábbi a weapon/armor első részszállításának történeti szerződése.
A bonus-reference elutasítást és hash v1-et azóta az
[IC-005 bónuszprofil- és hash v2 szerződés](item_combat_005_bonus_profiles.md) váltotta fel.

Dátum: 2026-10-27. Állapot: **izolált, tesztelt fejlesztési részszállítás;
nem végleges P1/P2 release és nem production balance**.

## Határ és adattulajdon

Az első implementáció két kategóriát fogad: weapon és armor. A teljes 18 elemű
kategóriaregiszter megmarad; a többi kategóriához tartozó base publikálását a
builder és a fejlesztési SQL-séma is tiltja, amíg a tényleges típusos szerződésük
nem készül el. A fegyver/páncél source típusok nem ismernek legacy proto/value
struktúrát, vnumot vagy régi evaluatorhívást.

`BuildItemCatalog` érték szerinti inputból saját, immutable, `shared_ptr<const>`
snapshotot állít elő. A snapshot birtokol minden stringet, sort és profilt;
nincs registry, SQL, Lua vagy kölcsönzött proto-pointer. A normalizálás és az
összes validálás a publikálás előtt történik. Más snapshotra váltás nem módosítja
a már megtartott példányt. Az owner-thread runtime csere mechanizmusa P4 feladat.

## Típusok és egységek

- A raw támadó-/védőértékek int64-ben, 1000 raw/pont skálán tárolódnak;
  a strukturális maximum 10^15. Az összegzés a határok ellenőrzése után történik,
  wrap és számtartományra telítés nélkül.
- A profil 0..max szintjeinek mindegyike kötelező; max<=255, nincs duplikált/lyukas
  sor. A 0. sor minden növekménye nulla. A tárolt növekmény a +0-hoz képesti
  kumulatív érték. A weapon profil csak támadást, az armor csak védelmet növel.
  A növekmények nemnegatívak és statonként monotonak. None profilnak csak 0. sora van.
- A builder a legmagasabb szinttel bizonyítja a base+gain capet; a monotonitás
  miatt ez minden köztes szintre elég. Nem jön létre új proto szintenként.
- A policy enumok új domainkódok, nem régi enumok castjai. A weapon subtype 1..7:
  sword/dagger/bow/two-handed/bell/fan/mount-spear. A régi arrow szemantikája külön
  ammo audit/implementáció nélkül nem importálható.
- Slotkódok: weapon=1, body=2, head=3, shield=4, feet=5. A régi wrist/neck/ear
  besorolás accessory irányú portja még hátravan. A weapon slotja csak 1 lehet.
- Use policy: a négy megmaradó osztály (a `length.h::EJobs` szerint warrior,
  assassin, sura, shaman) bitje 0..3; két nem bitje 0..1. A többi használati,
  birodalom-/alignment-/timer-specifikus korlátozás auditja és portja még nyitott.
  Az offline konverter ezeket nem dobhatja el a szűkebb profil kedvéért.
- Az attack profil idői tickben, hatótávja explicit milliméterben van. A jelenlegi
  map/mozgás koordinátáinak konverziója és range/LOS admission igazolása P3/P4
  feladat; a mostani kód nem hívja a régi float geometriát.
- A resistance mezők bp-ben, strukturálisan -10000..10000 között vannak.
  Ez nem jóváhagyott gameplay cap; a ruleset szűkebb aggregált capje külön készül.
- A pénzárak egész darabok, jelenleg csak a Gold domainkód fogadható el,
  legfeljebb INT64_MAX összeggel. A további pénznemek és wallet-integráció külön kapu.

## Szigorú hivatkozások

Minden base-nek pontosan a saját kategóriájához tartozó egy child kell. Hiányzó,
duplikált, más kategóriába írt vagy base nélküli child elutasítandó. A szükséges
use/trade/refinement, weapon attack/ammo és armor resistance profil nem hiányozhat.
Ha appearance hivatkozás szerepel, annak léteznie kell. Profilkódok és ID-k
táblánként egyediek, az eszköz- és lokalizációs kulcsok is validáltak.

A BonusRegistry még nincs portolva. A builder ezért **minden** nem-null base
vagy refinement bonus-profile referenciát elutasít, nem olvassa üres bónuszként.
Az SQL bonus_profile fejtábla csak FK-integritást készít elő; ez nem implementált
bonus loader vagy megőrzött általános bonus proto. Szintfüggő appearance override,
egyéb profilok és a teljes tartalomport még kiadási kapuk.

## Kanonikus hash v1

A SHA-256-ot a már használt OpenSSL Crypto adja, saját kriptográfiai implementáció
nélkül. A bemenet explicit little-endian bájtfolyam, `auriga.item_catalog.v1`
domainnel és uint32 sémaverzióval. Fix sorrendű, u8 tag + u32 darabszám után
következnek a táblák: use, trade, appearance, attack, ammo, resistance,
refinement, item_base, weapon, armor. A profilsorok ID, az itemsorok ItemKey,
a szintek level szerint rendezettek. String: u32 bájthossz + ASCII; opcionális
ID: u8 jelenlét + szükség esetén u32; bool: u8 0/1. A többi mező a deklarált
szélességében, név szerinti rögzített sorrendben van; nem structmemória.

A release revision **nem** része a tartalmi hashnek; azonos tartalom új revízióval
azonos digestet ad. Ez a szerver teljes, jelenleg támogatott source adatának
hashje, **nem** a későbbi publikus kliensprojekció hashje. Utóbbinak külön
projekció és manifest kell. Hashformátum-változás verzióváltást kíván.

Független Python-kódoló goldenje a rögzített fixture-re:
`146cc8a735536aa11d0f4335dbc39c804a45baab9e710f482ea096d987b41dd7`.

## Build

Az `AurigaItemCatalog` csak az `AurigaItemDomain` és OpenSSL Crypto célt használja.
Az olvasási threadteszt külön Threads célra linkel. MSBuild alatt a modul céljai
nem importálják egy szülő build `Directory.Build.props/targets` fájlját; ez
megakadályozza a nem deklarált statikus könyvtárak/CRT-beállítások átvételét.
A fő build CMake-ben deklarált ASAN/CRT/hardening beállításai megmaradnak.

Az önálló Windows buildhez a tényleges vcpkg toolchain adandó meg. A modul nincs
bekötve a GameServer/Database runtime-ba, és a csomag init-db útvonala nem futtatja
a fejlesztési sémát. Éles vagy meglévő játékadatbázison ebben a fázisban nincs írás.
