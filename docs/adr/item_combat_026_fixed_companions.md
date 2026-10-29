# IC-026 — Fix pet- és mounthívó tárgyak

2026-10-29. Az IC-012 F2 kategórialefedésének része; nem játékbeli aktiválás.

## Hatókör és forrás

A 15-ös kategória fix követő peteket és követőként is megjelenő, lovagolható
mountokat fogad. Fejlődő pet, tojás, etetés, petképesség, szállítódoboz,
feltámasztó/újrasorsoló tárgy és pet/mount skin nem publikálható ilyen tárgyként.
Ezek megmaradó, külön átviendő működések; egyik sem DragonSoul.

A leltár a rögzített, változatlan 2026-10-28-as mentésen és a jelenlegi
`PetSystem`, `New_PetSystem`, `MountSystem`, `MountInventory`, `InventorySystem`,
`ItemSystem` és `questlua_pet` forráson alapul. A minták sor- és fájlhash-eit a
`2026-10-29_companions.audit.json` tartalmazza. A fájlokból nem történt futó
játékosállapot vagy személyes név publikálása.

A régi petquest 62 meglévő prototípust hivatkozik; kettő ezek közül skin típusú.
A quest formája és a visszaidéző által olvasott `value1` 14 prototípusnál eltér.
Ez ellenőrizendő forrásellentmondás, nem automatikus konverziós döntés.
242 costume-mount, 60 további questben hivatkozott pet, 32 skin, 13 további
pet típusú tárgy és 6 unique-ride sor szerepel a leltárban. Minden sor
`converted=false`; az árfekvés, policy, asset, óra, bónusz és hivatkozás teljes
megfeleltetése még szükséges. A 302 fix jelöltre 56 item- és 5 mounttárhely-sor
hivatkozik a mentésben; ezek állapota és tulajdona változatlan.

## Katalógus

`CompanionForm` rögzített azonosító, tartalomkód, explicit lény-sablonazonosító
és validált modellhivatkozás. Ez nem item-vnum feloldó: a hívótárgy közvetlen
típusos formát hivatkozik. A formák append-only tartalmak; a szerkesztő natív
tranzakciós kapuja megtiltja egy korábban kiadott forma módosítását/törlését.
Új megjelenéshez új forma készül. A formaválasztó a meglévő modellt is megmutatja.

Az `ItemCompanionRow` a tárgy ItemKey-ét, pet/mount típusát, követőformáját,
mountnál kötelező lovaglási formáját, időszabályát és dungeon-bónuszkorlátozását
tárolja. Pethez lovaglási forma, bármihez nulla/ismeretlen forma vagy szabad
általános értéktömb tiltott. A példány egy darabos; a megjelenést a formák adják.
Petnél a felszereléshez kötött binding tiltott, mountnál engedélyezett.

A bónuszok a meglévő typed BonusRegistry/profilok. Petnél a tényleges követés,
mountnál a lovaglás teszi aktívvá őket; a dungeon flag ezt tovább korlátozza.
Nem alkalmazzuk ugyanazt a bónuszt követőn és lovason egyszerre. A régi mount
forrás külön +50 mozgást és más viselési/tárhely-hozzájárulást is használhat;
ezek kiadása explicit, ellenőrzött tartalomleképezést igényel, nem vélelmezett
egyezést az új modellel.

A `Companion` refinement profil bónusz-alapú, konfigurálható +0..+max, legfeljebb
+255 szinttel. Nincs nyers fegyver/páncélstat és nincs külön +9 ág. A katalógus
és használati terv +30 szinttel is tesztelt. A generikus kovácsrecept egyelőre
tiltott: a későbbi writernek a teljes companion-példányállapotot kell hordoznia.
Ez a lépés nem állítja, hogy a petek fejlődési/refinement művelete már elkészült.

## Állapot, idő és tervek

Külön típusos állapot a `Dormant/Following/Riding` fázis, az aktuális forma,
a visszaidézési szándék és az élettartam. A fázist és aktuális formát a hiteles
élő ECS-kapcsolat adja; SQL-ből betöltött visszaidézési szándék nem bizonyít élő
szereplőt. Tartós ECS-handle, socketbe csomagolás és item-ID-aritmetika nincs.

Az órák: korlátlan; létrehozástól UTC-határidő; első elfogadott idézéstől
UTC-határidő; tényleges aktív követés/lovaglás ideje. Új példányt kizárólag a
factory inicializálhat. Mentett példány nem kaphat így új időkeretet.
A valós idejű határidő egyszer indul. Az aktív keret csak ellenőrzött owner-
monotonidővel csökken, alvó/offline példány nem fogyasztja. A fogyasztás
felosztásfüggetlen, a lejárás ragadós: UTC-visszaállítás nem éleszti újra.
Overflow vagy szerkezetileg érvénytelen állapot elutasítás.

`PlanCompanion` birtokolt, mellékhatásmentes terv: idézés, elküldés, felülés,
leszállás, karbantartás. Kötelező a szerveradmission, tulajdon, példányazonosító,
revision, operation ID, ruleset, darabszám és létező szint. Idézés/felülés
ellenőrzi az osztályt, nemet, szintet, harci policyt, életet, megfigyelő/
átváltozási állapotot, térképengedélyt és a szabad saját csatornát. Az élet- és
térképengedélyek alapértéke tiltott. A terv nem hív SQL-t, RNG-t, ECS-t vagy Lua-t.

Felülés egy követőt tüntet el; leszállás egyet hoz vissza. Új publikált forma
lecserélheti a régi megjelenést az időkeret újraindítása nélkül. Halál vagy
térképtiltás megszünteti az élő megjelenést, de megőrzi a visszaidézési szándékot.
Kifejezett elküldés törli a szándékot. Kivezetés takarít, de nem törli a tárgyat.
Lejárás takarít, és külön `removeItem` kimenettel ugyanabban a commitban kéri
a hívótárgy eltávolítását is. A cleanup nem követeli meg a használati policy
aktuális teljesülését.

## F3/F4 owner-szerződés

Még nincs tartós companion-writer vagy élő GameServer-bekötés. A terv `after`
fázisa kívánt eredmény, nem egy már létrehozott actor bizonyítéka. Az owner:

1. Újraellenőrzi a sessiont, instance revisiont, szabály/katalóguspint,
   világ/actor-generációt, felszerelést és az egyetlen saját pet/mount csatornát.
2. Egy operation receiptben commitolja az élettartamot, bindingot, recallt,
   esetleges tárgylejárást és az érintett erőforrásokat. CommitUnknown után
   operation ID szerint egyeztet, nem küldi újra vakon az idézést.
3. Csak sikeres commitból állít elő megjelenést és stat/packet eseményt.
   A Following/Riding tényt csak sikeres actor/lovas létrehozás után publikálja.
   Spawnhiba esetén Dormant marad a live állapot, a recall-szándék és a már
   elindított valós határidő megmarad; nincs hamis actor vagy újratöltött keret.
4. Szétválasztja a változatlan formacsere és az inkompatibilis típus/óracsere
   esetét. Utóbbihoz kiadási kapu, régi pin vagy ellenőrzött offline állapot-
   migráció kell; mentett lifetime nem értelmezhető újra észrevétlenül.

A skin, mountgyűjtemény-bónusz és fejlődő pet későbbi saját fogyasztó. Ebben a
lépésben egyik sem olvad bele egy általános JSON-ba vagy kapcsolóhalmazba.

## SQL és szerkesztő

Nem kiadott v19, `auriga.item_catalog.v19`, kanonikus tagek 42..43;
`025_companion_items.sql`, friss lánc 000..025. Két új tábla: `companion_form`
és `item_companion`. A normál decoder/hash/editor/SQL tranzakciós út közös.
A korábbi sémák közvetlen runtime-betöltése tiltott.

A Qt-felület külön pet/mount, óra, időtartam, bónuszérvényesség és forma mezőket
ad. Inaktív lovaglási mező vagy korlátlan idő hibás piszkozata nem blokkolja a
mentést. A szintmaximum és a bónuszok a közös tárgyszerkesztőben vannak.
Másolás megtartja a formát; eltérő megjelenéshez új forma hozható létre.
Formahiba vagy tárgyírási hiba a profil-, forma- és kulcskiosztást is rollbackeli.

Az aktív helyi v7 adatbázis és a telepített app nem cserélődött. A meglévő
felhasználói katalógus csak saját v19 MariaDB-sandboxba került megőrzési próbára.
