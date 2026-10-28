# Képességkönyvek — elkészült F2 fejlesztési szelet

2026-10-28. Ág: `codex/mission-engine`. Kiinduló HEAD:
`45e76b230f83a9adc2a244881f488786679ef7e7`. A már módosított munkafában
folytattam a munkát; a korábbi szeleteket megőriztem. Nem történt commit,
helyi telepítés, meglévő adatbázis-frissítés vagy távoli művelet.

## Elkészült

- A 9-es **Képességkönyv** kategória betölthető és szerkeszthető.
  Egy prototípushoz több képességszint-szabály tartozik; nincs szintenként
  ismételt tárgyproto vagy régi vnum/socket/value adapter.
- Három típusos művelet: **tanulás**, **felejtés pontvisszaadással**,
  **lóképességpont szerzése**. A felejtés pontosan egy szintet vesz vissza,
  egy megadott pontkészletbe ad vissza pontot, és törli az olvasási előrehaladást.
- Fix cél vagy a tartós példányhoz kötött képesség engedélylistából. A ló
  pontkészlete külön cél, nem ál-képességazonosító. A nyelvkönyv saját
  birodalomra vonatkozó kizárása is explicit mező.
- Szintenként megadható következő szint, karakterminimum, szükséges sikeres
  olvasás, normál/erősített esély, XP-költség és olvasási idő. Például a
  10→20 ugrás adatszabály, nem a skill függvény rejtett mellékhatása.
- Előbb teljes admission, majd saját szabályadatokat birtokló, nem
  megváltoztatható `PreparedSkillBookUse`, utána szerveres döntésekkel
  feloldott eredményterv. A terv az ItemKey/profil/katalógus-revíziót is őrzi.
  A tiszta modul nem hív RNG-t, órát, SQL-t, questet, skill callbacket vagy registryt.
- Az elutasított használat nem ad fogyasztási tervet. Elfogadott, sikertelen
  próbálkozás is a profil szerinti könyv/XP költséget fogyasztja. Az olvasási
  számláló csak sikerre nő; a küszöbnél egyszer emel és nulláz. 0% kudarc,
  100% siker, egyik sem régi speciális algoritmusra utaló sentinel.
- A várakozáselhagyó hatás csak tényleges cooldownnál fogy. Az olvasási idő
  a képességhez vagy a lópontkészlethez tartozik, másik könyvvel nem kerülhető
  meg. A felejtést nem blokkolja a tanulási cooldown. Lópontnál a 20 szabad
  és a két alternatív kiosztás 60 pontos határa külön ellenőrzött.
- V11 katalógus: típusos `skill_book_profile`, `skill_book_target`,
  `skill_book_step`, `item_skill_book`. Snapshot, strict dekódolás,
  költségkeretek, normalizálás és kanonikus hash frissítve. 42 fizikai tábla,
  ebből 41 része a snapshotnak; az azonosító-kiosztó külön van.
- A meglévő prepared SQL motor menti atomikusan a teljes könyvet.
  A megosztott profil szerkesztése és a könyv másolása külön profilt készít;
  más könyvek szabályai megmaradnak. Tárolási hibánál a profilok és az
  azonosítókiosztás visszagördülése külön tesztelt.

## Grafikus szerkesztő

**Új tárgy → Képességkönyv**. Két belső lap: **Cél és használat**,
**Szintenkénti szabályok**. A célok táblázatban, a szintlépések listából
kiválasztott űrlapon szerkeszthetők; a számok nem széles SQL-sorban jelennek meg.

Nyolc szabályminta: mester, vezetés, kombó, nyelv, új support, anti-passzív,
felejtés és lópont. Ezek nem jóváhagyott balance vagy valódi adatimport.
A kézműves-, bányász- és idézésképességek a fix cél/szintszabály modellben
adhatók meg a ténylegesen auditált tartalmi adataikkal.

A művelet oda-vissza váltása megőrzi a még nem mentett beállításokat.
A kifejezett mintabetöltés lecseréli a könyv szabályait. Hibás mezőnél a
szintlista nem dobja el az értéket. Új szint csak engedélyezett fokozatba
kerülhet; új cél nem kap már használt azonosítót. Fix célhoz csak egy cél
adható. Nem alkalmazható cél/pont/birodalom mezők inaktívak. Kikapcsolt
olvasási bónusznál a mentés minden szinten a normál esélyt használja; nem kell
kézzel végigszerkeszteni az összes sort.

Az űrlapot 1400×950 és 1100×760 méretben rendereltem. Mindkét kisebb
képet vizuálisan ellenőriztem: olvasható mezők, görgethető részletek.
Képek a `build/skill-book-editor-rules[-small].png` és
`build/skill-book-editor-levels[-small].png` fájlokban. Ezek kizárólag
memóriában készített példatárgyak, nem a meglévő felhasználói adatbázis képei.

## Tesztek

| Ellenőrzés | Eredmény | Bizonyíték |
|---|---:|---|
| Teljes ServerTests build és normál CTest | 106/106 sikeres | `build/skill-books-build-final.log`, `build/skill-books-final-ctest.log` |
| Könyv domain | 1100 ellenőrzés | `build/skill-books-domain-final.log` |
| Modell/Qt/natív SQL mentés | 38/38 sikeres | `build/skill-books-editor-final.log` |
| ASAN katalógus/domain célok | 17/17 sikeres | `build-asan/skill-books-final-tests.log` |
| Szerkesztő ASAN helperrel | 38/38 sikeres | `build-asan/skill-books-editor-final.log` |
| Végső cél/szint/bónusz UI finomítások utáni Qt regresszió | 9/9 normál + 9/9 ASAN | `build/skill-books-panel-regression.log`, `build-asan/skill-books-panel-regression.log` |
| Teljes szerverkezelő GUI regresszió | 14/14 sikeres | `build/skill-books-manager-gui-tests.log` |

A C++ teszt 1000 rögzített magú, független eredmény/számláló/költség
összehasonlítást is tartalmaz. A többi eset többek között az esélyhatárokat,
bónuszt, várakozást, tiltott/kötött célokat, polimorf állapotot, fokozatokat,
XP-t, kifogyott könyvet, túlcsordulást és lópontkorlátokat vizsgálja.
Prepared requestből a katalógus elengedése után is ugyanaz az eredmény adódik.

Az SQL-séma 27 tesztesetben futott újonnan indított, saját MariaDB 11.4.13
példányon. A mentési teszt 254 engedélyezett célt, 40 szintsort, 65535-ös
sikerszámot és INT64_MAX XP/idő értékeket is visszaolvas. Ez a tárolási
szerződés próbája, nem állítás arról, hogy mind a 254 skill létező tartalom.

A közös, változatlanul 17 tárgyas fixture független Python/C++ SHA-256 értéke:
`609c3bf0adc44ff0012cb6934bd6c0e0edf0be0ff4c5e4092256a2eb4aaf3005`.
A könyv tesztkatalógusa külön fixture-variáció, ezért a régi mintatárgyak
tartalma nem változott. A könyvmezők hash-hatása és a szintforrások
sorrendtől független normalizálása külön tesztelt.

## Audit és határok

A részletes szerződés: [IC-018](../../adr/item_combat_018_skill_books.md).
A forrás a 50902–50904 ágban kézművességet tanít; a korábbi lókönyv-címke
pontosítva. Az előre tiltott 55003–55005 nem lett visszakapcsolva.
A 50322-es átváltozási gyakorlókönyv külön mobforma/gyakorlás viselkedés,
nem erre a skillolvasási modellre lett leegyszerűsítve; a Polymorph port része.

A régi, nulla esélyparaméteres út negatív indexű logolása és a táblázathoz
képest eltérő, néhol +1-es olvasási számláló nem lett új motorba rejtve.
A jelen build az XP-költséget kikapcsoló flaget definiálja; a minták 0 XP-t
használnak. A valódi tartalom pontos megfeleltetése és karanténja F6.

Az izolált katalógus a skill ID 1..254 szerkezeti tartományát ellenőrzi.
A képesség tényleges létezése, maximuma, kaszt/csoport és előfeltételek
kötelező owner-döntések, alapértelmezetten tiltva. A külön skill-tartalom
keresztellenőrzése F4 kapu. A 40-es skillhatár nem új itemfejlesztési korlát:
a prototípusok egyszerisége és a külön refinement maximum változatlan.

A GameServer/Database/Unity még nem használja ezt a modult élő könyvekhez.
Az item/skill/számláló/pont/XP/cooldown/token/receipt közös ownership/revision
commit F3/F4; az új wire/példányhoz kötött könyvcél F5/F6. A tervet majd
közvetlenül kell alkalmazni, újabb rejtett SkillLevelUp-dobás nélkül.

Nem frissítettem a meglévő helyi katalógust vagy telepített kezelőt.
A friss draft migrációlánc nem adatmegőrző v7→v11 upgrader. Az upgrade és
visszaállítás próbája telepítés előtt külön szükséges. Távoli gépen nincs
módosítás, FreeBSD/Unity játékteszt nem történt.

## Következő pont

IC-014, 5. szelet: tárgyon végzett műveletek — bónuszváltoztatás,
foglalatműveletek, célpolitikák és fejlesztési kellékek. A kivezetett
DragonSoul/offline shop/régi BattlePass nem tér vissza. A megmaradó jelmez,
stóla, rune és ITEM_SOUL viselkedések külön, megfelelő audit alapján kerülnek át.
