# Fix petek és hátasok — helyi fejlesztési eredmény

2026-10-29, `codex/mission-engine`. Kezdő HEAD:
`90caf3a2720947b05c78771182df47ec154493cb`.

## Elkészült a forrásban

- A 15-ös kategória típusos fix pet- és mounthívó modellje, explicit követő-
  és lovaglási formával. Nincs item-vnum feloldó, általános értéktömb vagy
  socketbe csomagolt új példányállapot.
- Immutable formaregiszter: egy kiadott forma nem írható át vagy törölhető;
  más megjelenés új azonosítót kap. A hívótárgy egyetlen prototípus marad.
- Korlátlan, létrehozástól számított, első használattól számított és tényleges
  aktív időt fogyasztó élettartam. Az újraidézés és formacsere nem tölti újra
  a keretet. Lejáráskor a terv a megjelenés és a hívótárgy eltávolítását kéri.
- Mellékhatásmentes idézés/elküldés/felülés/leszállás/karbantartás tervek,
  tulajdon-, állapot-, policy-, térkép-, csatorna- és időellenőrzésekkel.
  A recall szándék és az élő szereplő ténye külön adat.
- Alap- és szintbónuszok a meglévő bonus registryből; petnél követés,
  mountnál lovaglás aktiválja őket. Külön állítható a csak dungeonban érvényes
  bónusz. Ugyanaz a mountbónusz nem adódik hozzá követés és lovaglás címén kétszer.
- Egy prototípushoz konfigurálható +0..+max, maximum 255 szintű, bónuszalapú
  refinement profil. A katalógus és a használati terv +30-cal is tesztelt.
  **A generikus kovácsrecept még tiltott companionhoz:** a példányállapotot
  megőrző tartós writer külön feladat; a játékbeli fejlesztés még nincs bekötve.
- SQL v19, `auriga.item_catalog.v19`, kanonikus tagek 42 és 43. A teljes
  000..025 migrációs lánc 61 katalógustáblát kezel; az új táblák a
  `protos.companion_form` és `protos.item_companion`.
- A tárgyszerkesztőben pet/mount, időszabály, időtartam, bónuszérvényesség,
  követőforma és lovaglási forma szerkeszthető. Másolás megőrzi a formát;
  hibás mentés a formákat, profilokat és kiosztási számlálókat is visszagörgeti.

## Ellenőrzések

| Ellenőrzés | Eredmény |
|---|---:|
| Új natív companion teszt, normál és ASAN | 76 és 76 ellenőrzés |
| Teljes normál CTest, a raid/Mission regressziókkal együtt | 128/128 |
| Célzott AddressSanitizer domain/dekóder/hash/konverter kör | 14/14 |
| Qt + modell + saját MariaDB mentés normál helperrel | 61/61 |
| Ugyanez AddressSanitizer helperrel | 61/61 |
| Friss SQL-séma belső ellenőrzései a végső CTestben | 35/35 |
| Végső panel/modell/SQL-próba normál és ASAN helperrel | 3/3 és 3/3 |

A tesztek a többek között +30-as maximumot, óra-visszaállítást, pontos lejárást,
időkeret-megőrzést, élet/térkép miatti takarítást, fel-/leszállást, formacserét,
inaktív űrlapmezőket, másolást és részírási hiba teljes rollbackjét ellenőrzik.
A pet- és mountpanel képi ellenőrzése is megtörtént.

Független Python encoderrel és mindkét natív binárissal ellenőrzött, kitöltött
companion-fixture SHA-256:
`630b99480d6ffcc7600fece3c01dcca932a69a40849dfdeb62bfc404ad89079d`.

A meglévő felhasználói katalógus külön MariaDB-sandboxban v7→v19 átemelve,
az eredeti kulcsok/cellák/számlálók megőrzésével visszaolvasva:
`00dc1711bfc1124a186df92db3992e795964ac620c3d7fe765a6939b61d79350`.
Ez egy meglévő felhasználói fegyver megőrzési próbája, nem a régi teljes
item_proto vagy a pet/mount állomány kész konverziója. A sandboxok leálltak.

Az aktív helyi `protos` 33 táblájának metaadata és minden cellája egyezik a
mentéssel: továbbra is v7, egy felhasználói tárgy. A telepített app, a játék,
a Unity és a távoli gép nem módosult.

## Leltár és nyitott megfeleltetések

A változatlan 2026-10-28-as forrásmentésben 242 costume-mount, 60 további
questben hivatkozott fix pet, 32 pet/mount skin, 13 további pet típusú tárgy
és 6 unique-ride szerepel. A quest 62 létező prototípust hivatkozik, de kettő
skin; ezeket nem számoltuk fix petnek. A 302 fix jelöltre 56 item- és 5
mounttárhely-sor hivatkozik; egyik sor állapotát vagy tulajdonát sem módosítottuk.

**14 prototípusnál eltér a quest formája és az automatikus visszaidézés által
olvasott proto `value1`.** Ezekhez ellenőrzött offline döntés kell. A régi
mount külön +50 mozgását és esetleges külön viselési/tárhelybónuszait is
explicit kell megfeleltetni; automatikus összeadás vagy elhagyás nem készült.
A forrásban lévő hétnapos alapértelmezést az új modell nem találja ki:
időtartamot a tartalomban kell megadni.

Minden leltározott forrássor `converted=false`. A policy-, ár-, asset-, óra-,
bónusz-, tartalomhivatkozás- és példányállapot-konverzió még nem teljes.
A fejlődő pet, tojás, etetés, képesség, szállítás, feltámasztás, újrasorsolás,
skin, unique ride és mountgyűjtemény saját működés marad; nem alakultak át
fix hívótárggyá. A kivezetett DragonSoul nem került vissza.

## Következő munka

**Ez az F2 kategórialefedés következő lezárt része; a teljes itemrendszer
játékbekötése még nincs kész.** Következő a fejlődő petek és kapcsolódó
tárgyaik átvilágítása és típusos lefedése. Maradtak további speciális,
accessory/ring/belt/unique, stole/rune és resource-feldolgozási hiányok is.

F3..F7-ben jön a tartós példány- és gazdasági writer, a GameServer/Database
összes fogyasztója, az új Unity-protokoll, a teljes ellenőrzött offline
konverzió és a helyi játékpróba. Az owner-commit, spawnhiba, CommitUnknown,
invariáns és inkompatibilis óraváltás kötelezettségeit az ADR rögzíti.

- Szerződés: [IC-026](../../adr/item_combat_026_fixed_companions.md).
- Forrásleltár: [audit JSON](2026-10-29_companions.audit.json).
- Forráshash-ek és tesztbizonyítékok: [evidence JSON](2026-10-29_companions.evidence.json).
