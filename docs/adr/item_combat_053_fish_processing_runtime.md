# ADR IC-053 — Halak feldolgozása a szerveren és Unityben

2026-11-04, C25a. Alapmodell: [IC-023](item_combat_023_gathering_tools_and_fishing.md).
Előzmény: [IC-052](item_combat_052_polymorph_runtime.md).

A C25 több külön játékfolyamatot érint. Ez a szelet az élő hal használatát és
a döglött hal tábortűznél sütését viszi végig. Az eszközpéldány, gyakorlás,
fejlesztés, csali és a fogás/gyűjtés bekötése a C25 további része marad.

## Tartalom és mentés

A 12-es kategória élő hala egy darabot fogyaszt, és a saját lootprofiljából
ad tárgyjutalmat. WeightedOne és korlátozott IndependentChance profil
támogatott, üres független eredménnyel is. Döglött halnál az explicit
`grillResult` határozza meg a sült tárgyat; a teljes kijelölt halom ugyanannyi
eredményt ad. A kisebb célhalom-korlát több grantot eredményez, veszteség nélkül.
Csali és sütési cél nélküli döglött hal nem kínál feldolgozási műveletet.

A natív `PlanFishProcess` tervre a `MakeFishProcessingCommit` épül.
Egy tranzakció menti a fogyasztást, minden grantot, az owner-fence-et és a
receiptet. A forrás teljes előállapota CAS-guard; új identitás csak a szerver
ID-foglalásából születik. Nincs részleges jutalom vagy kliensoldali levonás.
A szerver kizárólag a tartósan egyeztetett commit eredményét mutatja sikernek.
Ismételt megerősítés nem fogyaszt és nem ad jutalmat újra.

A helyigény és az ID-keret ellenőrzése minden lehetséges eredményre lefut,
a véletlen döntés előtt. Egy teli leltár nem szelektálhatja a kedvezőbb
sorsolást. A bizonytalan független bejegyzések száma legfeljebb nyolc;
a commit közös itemkorlátja változatlan. Nem támogatott jutalom vagy hiányos
példányadatot igénylő grant az egész műveletet elutasítja.

A halaknak és csaliknak nincs új, mutable példánypayloadjuk. A meglévő
perzisztencia, hash, áthelyezés és trade megtartja a teljes állapotot.
Típusos hal nem fejleszthető. Új SQL-migráció vagy katalógusverzió nem kell;
a katalógus v26 és az inventory formátuma változatlan.

## Szerveres feltételek

Saját, normál leltárban levő példány, szint, osztály, nem, harci use-policy és
aktuális szerveres admission szükséges. Halál, stun, observer, quest,
shop/trade/safebox/cube és a globális arena/PvP-tiltás nem kerülhető meg.
Az előnézet a teljes forráshoz, katalógushashhez és owner-sessionhöz kötött.
Elutasított előnézet megerősítése nem indít új műveletet.

Sütéshez a szerver a környező szektorokból keres valódi tábortüzet:
meglévő `fishing::CAMPFIRE_MOB`, azonos map, élő entitás, 500-nál kisebb
közelítő távolság. Az előnézet a kiválasztott, generációs entitáshandle-t
őrzi. Megerősítéskor ugyanennek a tábortűznek kell még megfelelőnek lennie.
A kliens nem küldhet tetszőleges campfire-azonosítót vagy koordinátát.

A típusos owner régi `UseFishEcs`/`GrillFishEcs` útja nem fogyaszthat legacy
halat az új tranzakció megkerülésével. A még át nem állított karakterek régi
halhasználata marad. A normál legacy leltár átállítása továbbra is vár.

## Protokoll és Unity

CG/GC254 v1 külön munkamenet. CG: action, session, source, nulla target,
majd a szokásos sequence byte; előnézet, megerősítés és lezárás.
GC: status/error, session, source/revízió/ItemKey/darabszám, művelet és halfajta,
fogyasztás, szerveres campfire-VID, lootmód, lehetséges és tényleges eredmények,
beágyazott GC227 metadata. A minimális külső keret 61 byte. GC227 v2 új
`Fish` actiont ad a feldolgozható 12-es kategóriának.

Unity: **külön típusos leltár → hal → Halfeldolgozás → Feldolgozás
áttekintése → Feldolgozás megerősítése**. Látható a fogyasztandó teljes
darabszám, a lehetséges jutalom és a mentett eredmény. A sütéshez az ablak
közeli tüzet kér. Forrásváltozáskor eltűnik a megerősítés; a szerver a
forrás teljes állapotát és a tűztől való távolságot külön is újraellenőrzi.
Az utolsó tárgy elfogyása után az eredmény olvasható marad.

Ellenőrzések és kiadási korlátok:
[C25a jelentés](../reports/item_combat/2026-11-04_fish_processing_runtime.md).
