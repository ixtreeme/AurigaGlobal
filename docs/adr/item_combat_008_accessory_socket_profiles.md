# ADR IC-008 — Ékszerfoglalat: kapacitás, feltöltés és élettartam

Történeti v4 hash-lelet. A foglalatszerződés megmaradt; az aktuális v5
katalógus további recovery profiljai: [IC-009](item_combat_009_recovery_consumables.md).

2026-10-27. Állapot: **izolált domain- és SQL-részmegvalósítás**, nem runtime
átállítás. Az [IC-007](item_combat_007_accessory_material_catalog.md) egyszerű
ékszereit opcionális, típusos foglalatprofillal bővíti. A normál fejlesztési
szint továbbra is a tárgypéldány külön `refinement_level` állapota.

## Forrásból igazolt különbségek

| Forrás és szimbólum | Megfigyelés | Új szerződés |
|---|---|---|
| `ItemSystem_LegacyBridge.cpp:494`, `UseItemEx`, `USE_ADD_ACCESSORY_SOCKET` | A fájl bekapcsolja az `ENABLE_ADDSTONE_FAILURE` flaget; a nyitás 50%-os dobást használ és sikertelenségkor is fogyaszt. A beltet ebben az ágban tiltja. | Explicit kapacitás, nyitási esély és próbálkozásonkénti anyagköltség; belt nincs automatikusan támogatottnak jelölve. |
| `UseItemEx`, `USE_PUT_INTO_ACCESSORY_SOCKET` | A kompatibilis rendes feltöltés közvetlenül emeli az aktív fokozatot. A `constants.cpp` 70-es putPct tömbje nem bizonyít 70%-os dobást ezen az útvonalon. | A referenciafeltöltés 10000 bp. A profil tud eltérő esélyt, de ez nem jóváhagyott balance-módosítás. |
| `common/service.h:85`, `CanPutInto2`, `SetItemAccessorySocketGrade` | Az `ENABLE_INFINITE_RAFINES` engedélyezett; a tartós feltöltés több helyen értéktömbre, vnumra és 86410-es időjelölőre támaszkodik. | Külön kompatibilitási lista és explicit `permanent` állapot; nincs időszentinél. |
| `ItemSystem.cpp::StartAccessorySocketExpireEvent/StopAccessorySocketExpireEvent/AccessorySocketDegrade` | Kapacitás, aktív fokozat és idő külön adatok; visszaminősítéskor az alacsonyabb fokozat ideje indul. | Külön kapacitás, aktív fokozat és maradék idő; a lejárt fokozat nem törli a foglalatot. |
| `InventorySystem.cpp:2371,2491` | Levételkor az ékszertimer megáll, felvételkor elindul. | Viselési idő; táskában nem fogy. Az owner thread köteles a tényleges viselési szakaszt megadni. |
| `InventorySystem.cpp::ModifyPoints:3385` | A proto apply kapja a százalék/minimum növelést; a normál affixek és extra-proto bónuszok külön ágban vannak. Az APPLY_SKILL külön bitcsomagolt eset. | Később explicit bónuszforrás-szabály kell; nem minden affix szorzandó automatikusan. |

A célzott audit nem teljes feature-mátrix vagy minden régi adatállapot
veszteségmentességének igazolása. A forráshasheket a jelentés evidence fájlja,
a határokat a referencia-CSV rögzíti. A teljes régi import továbbra is külön kapu.

## Profil és publikálási ellenőrzés

Új profil: `AccessorySocketProfile`, legfeljebb 255 foglalatfokozattal,
azonosítóval/kóddal, nyitási és feltöltési anyagegységgel. A maximum nem a
normál refinement plafonja. A teljes, egyedi 0..max fokozatsor kötelező.

Fokozatonként explicit egész milliszekundumos idő, bp növekmény és külön
raw/bp/integer minimum szerepel, valamint a következő nyitás/feltöltés esélye.
A 0. fokozat minden idő- és bónuszértéke nulla, de a következő művelet esélye
lehet nem nulla. A terminális sor mindkét esélye nulla. Idő: 1..INT64_MAX a
pozitív fokozatokon; esély: 0..10000; növekmény és bp/integer minimum: 0..100000;
raw minimum: 0..10^15. A növekmények és minimumok monoton nem csökkenhetnek.

Három ItemKey-lista van: nyitóanyag, időkorlátos feltöltőanyag, tartós
feltöltőanyag. Az első kettő kötelező, a harmadik opcionális. Mindegyik
legfeljebb 256, egyedi, rendezett Material kulcsot tartalmaz. A két feltöltési
lista nem fedheti egymást. A catalog builder létező material child sort követel;
feltöltéshez Jewel kind kell. Nincs vnumcsalád-képlet vagy runtime feloldótábla.

`ItemAccessoryRow.socketProfile` opcionális FK; a null nem foglalatos ékszert
jelent. A megadott 0/ismeretlen profil, hibás fokozat, hiányzó anyag vagy nem
Jewel feltöltőanyag blokkolja a teljes snapshot publikálását. A validált profil
saját másolatot birtokol, a katalógus immutable marad.

A fixture 3 fokozata, 24/12/6 órája és 10/20/40%-a forrásból követett
referenciaadat. Az 1000 raw minimum egy pont új egységét szemlélteti;
a bp/integer minimumok explicit új draft adatok. Ez nem minden régi APPLY
automatikusan elfogadott konverziója vagy kész balance-katalógus.

## Példányállapot és tiszta műveleti tervek

`AccessorySocketState`: kapacitás, aktív fokozat, maradék milliszekundum,
tartós jelző. `activeGrade <= unlockedCapacity <= maxGrade`. Üres állapotban
az idő és a tartós jelző nulla. Időkorlátos aktív állapotban a maradék idő
pozitív és nem nagyobb a saját fokozat teljes idejénél. Tartós aktív állapothoz
a profil tartós képessége kell; a maradék idő ilyenkor nulla.

- A nyitás csak a kapacitást emeli, az aktív fokozatot/időt nem.
- A sikeres feltöltés egy aktív fokozatot emel. Időkorlátos módban az új fokozat
  teljes ideje indul. Tartós módban az explicit jelző marad aktív.
- Aktív időkorlátos és tartós feltöltések nem keverhetők; a művelet hibával
  tér vissza, anyagfelhasználási terv nélkül.
- Sikertelen, engedélyezett próba változatlan állapotot és explicit
  próbálkozási költséget javasol. 0 bp esetén nincs művelet; 10000 bp mellett
  sikertelenként rögzített kimenetel érvénytelen.
- `AdvanceAccessorySocketTime` csak viseléskor fogyaszt; pontos határnál is
  visszaminősít, a maradék eltelt időt az alacsonyabb fokozatból is levonja.
  Legfeljebb 255 átmenet, kivonásos aritmetika, UINT64_MAX mellett is véges
  futás. Az idő feldarabolása nem változtatja meg az eredményt. Tartós
  feltöltés nem jár le. Normál időadatnak nincs különleges mágikus jelentése.

Ezek **tiszta tervek**, nem kész tárgytranzakciók. Nem generálnak véletlent,
nem írnak registryt/inventoryt/SQL-t és nem fogyasztanak valódi alapanyagot.
A későbbi íróhely feladata az owner/location/lock/revision ellenőrzés,
a viselési idő rendezése, a gazdasági kimenetel egyszeri rögzítése és az item,
anyag, pénz, revision és operation receipt atomi commitja. CommitUnknown után
receipt szerinti egyeztetés kell; egy új dobás nem megengedett újrapróbálás.

## SQL és kanonikus hash v4

A `009_accessory_socket_profiles.sql` három táblát ad:
`protos.accessory_socket_profile`, `accessory_socket_grade`,
`accessory_socket_material`, valamint nullable FK-t az item_accessory táblára.
Az operation_kind: 1=nyitás, 2=időkorlátos feltöltés, 3=tartós feltöltés.
A PK/FK/CHECK szabályok a közvetlen sorfeltételeket ellenőrzik; teljes
coverage, listaütközés, Jewel kind és monotonitás a builder feladata.
Ez kiadatlan **friss fejlesztési séma**, nem meglévő játékadatbázis frissítése.
A csomag init-db-ja nem tölti be, SQL→catalog loader még nincs.

Csak schemaVersion=4 fogadható el; v1/v2/v3 fallback nincs. Domain:
`auriga.item_catalog.v4`. A korábbi táblasorrend megmarad; a 13-as accessory
tag végén opcionális profilazonosító szerepel (u8 presence, majd u32 ID).
A 15-ös tag a foglalatprofilokat tartalmazza: identity, u8 max, két u16 egység,
u32 fokozatszám; fokozatonként u8/u64/u32/i64/i32/i32/u16/u16, majd a három
kompatibilitási lista u32 elemszámmal és hatbájtos ItemKey sorokkal.
Minden szám little-endian; profilok ID, fokozatok index, anyagok ItemKey szerint
rendezettek. Revision és konkrét itemállapot nem része a tartalomhashnek.

A hat prototípusos v4 fixture SHA-256-ja:
`42cbc346a51d732e04fd8b64bfaa2c5c2444f3309488ebf5e4fb9e17c49b78ca`.
Független Python encoder ellenőrzi a C++ kódolást.

## Fennmaradó integráció

Az ékszerbónuszok tényleges növelése még nem kész StatBuilder. Az új modellben
a base és az aktuális refinement profil azonos BonusId-jű részeit előbb össze
kell vonni, majd a foglalatnövelést egyszer alkalmazni; két külön minimum
dupla jóváírást okozna. Az előjel, cap és érintett bónuszok teljes leképezése,
a checked széles aritmetika továbbra is implementációs kapu.

Példány/ECS tulajdon, idő mentése, SQL loader, erőforrás-tranzakció, offline
szentinél-konverzió, Unity kijelzés és a többi kategória nincs ezzel késznek
jelölve. Ring/belt/unique/Soul/expiry külön viselkedés marad. A részletes
tesztlelet: [foglalatjelentés](../reports/item_combat/2026-10-27_accessory_socket_profiles.md).
