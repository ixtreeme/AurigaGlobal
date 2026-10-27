# ADR IC-010 — DragonSoul kivezetése a szerverből

Dátum: 2026-10-27. Döntés: a felhasználó a teljes szerveroldali DragonSoul
rendszer eltávolítását kérte az itemmodell további fejlesztése előtt. A korábbi
itemkatalógus-munka rögzítve: `b0e87969`.

## Hatókör

A DragonSoul nem kap új kategóriás modellt, migrációt vagy runtime-integrációt.
Ez felülírja a korábbi auditok és az eredeti specifikáció DS-re vonatkozó
portolási feladatait. Az eredeti specifikáció történeti forráspéldánya változatlan.
A launcher és a Python kliens kimarad ebből a munkából. Helyi forrásmódosítás
és teszt történik; telepítés és távoli adatbázis-módosítás nem része a döntésnek.

A külön `ITEM_SOUL` / `ENABLE_SOUL_SYSTEM`, a kosztümrúnák
(`ENABLE_RUNE_SYSTEM`), a vállkendő, a rúnadungeon és a Meley dungeon megmaradnak.
A korábbi `ENABLE_DS_RUNE` valójában a rúnadungeon bossfázisait, regenerációját
és saját sebzési bónuszát vezérelte; a DragonSoul manager nem használta.
Ez a funkció `ENABLE_RUNE_DUNGEON` néven önállóan megmarad, az eredeti
race/apply/point számkódokkal és változatlan harci szabályokkal.

## Eltávolított működés

- DS manager és táblabetöltő; ECS készlet, deck-állapot és rendszer.
- Kvalifikáció, aktiválás, setbónusz, finomítás, kinyerés, töltés és enchant.
- DS quest API, GM/játékos parancsok, hálózati kérések és válaszstruktúrák.
- Login/logout, attribútum, készlet, trade, shop, drop/pickup, stat és mentési
  kapcsolatok; a régi DS-specifikus pozitív tesztek.

A vállkendő enchantja korábban tévesen DS kapcsoló alatt állt. Most a
`ENABLE_STOLE_COSTUME` kapcsolóhoz tartozik.

## Tartós kódok és régi adatok

A többi funkció SQL- és protokollazonosítói változatlanok. A 29/30/31 itemtípus,
a 27/28/42 use-altípus, az 5/6/7 material-altípus és a 6-os inventory-window
fenntartott, tiltott hely. A korábbi DS wear pozíciókat sem készletművelet,
sem player-cache snapshot nem fogadhatja el. A DS hálózati kérésekhez nincs
packet-regisztráció vagy végrehajtó ág; más headerek számai nem tolódnak el.

A `common/retired_content.h` egy közös elutasítási szabály: a Database boot
szűri a protókat és újraépíti a pointerindexet; a GameServer protóbetöltése,
factoryja és közvetlen protófrissítése sem aktiválhatja őket. Az 540–542 és
563–566 affect nem tölthető be, nem csatolható és nem adható hozzá. A Mission
erőforrásképének régi 4-es window értéke is tiltott.

A régi TXT tokenek közül néhány kizárólag a bemeneti elutasításhoz ismert a
`ProtoReader.cpp`-ben. Ezzel a régi sor kiszűrhető, nem áll le miatta a többi
prototípus beolvasása. E tokenekhez nincs működő DS prototípus vagy viselkedés,
és az új `ItemDomain` katalógus nem használja ezt a beolvasót.

A friss `db/schema.sql` a korábbi enumpozíciót `RETIRED_WINDOW_6` néven tartja
fenn. Meglévő adatbázisban nem fut automatikus ALTER vagy DELETE; az ott
maradó DS sorokat a szerver nem tölti vissza. Visszaállítható mentés utáni
fizikai adattakarítás külön művelet lehet.

## Következmény az új itemmodellre

A draft kategóriaregiszter 14-es `Soul` helye nem számozható át vagy
hasznosítható újra. Jelenleg nincs implementált Soul kategóriakatalógus; a
későbbi tervezés kizárólag a megmaradó `ITEM_SOUL` szemantikáját vizsgálhatja.
DS grade/stage/strength, aktív deck és DS refine-adat nem importálható bele.
Az offline shop és a régi BattlePass kivezetése továbbra is érvényes.

## Ellenőrzés

Az aktuális futtatások és korlátok a
`docs/audits/dragonsoul_retirement_2026-10-27.md` jelentésbe kerülnek.
A `RetiredContentTests` a fenntartott kódokat és minden 16 bites pozíciót
ellenőriz; a meglévő natív item-, affect-, készlet-, mentési és Mission-tesztek
a tényleges belépési pontokon is vizsgálják az elutasítást.
