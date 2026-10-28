# IC-019 — Offline adatátvétel és fejlesztési receptek

2026-10-28. Az IC-006 és IC-012 folytatása. Az aktuális kérés a teljes régi
tárgytartalom konverzióját, majd a helyi adatbázis és a játék átállítását kéri.
A távoli gép nem része a műveletnek.

## Megvalósított határ

A `capture_legacy_items.py` a kiválasztott csomag saját MariaDB-folyamatát,
portját és datadirját ellenőrzi. A játékszervernek állnia kell; az indítást a
supervisor lock zárja ki. Globális olvasási zár alatt teljes logikai mentés
készül az account/common/log/player/mission/protos sémákról. A konverziós
táblák külön, cellavesztés nélküli HEX szöveg-/bináris exportot kapnak.
A nyers mentés megőrzi a kivezetett rendszerek történeti adatait is; ezekhez
nem készül új runtime-modell vagy migrációs integráció.

A `verify_item_checkpoint.py` csak **új, saját** MariaDB-sandboxot fogad.
A mentésből minden tábla darabszámát, a konverziós táblákat pedig oszlopról
oszlopra visszaellenőrzi. A `legacy_item_migration.py` nem tud SQL-hez
csatlakozni vagy táblát módosítani.

## A családtérkép még tervezet

A meglévő katalóguskulcsok és a kiadott számlálók rezerváltak. A jelölt
családokat explicit refine él, típus, név, folytonos szint és invariáns
policy alapján keressük; nincs vnum/10 vagy vnum%10. Az egyesülő lánc,
önhivatkozás, kör, hiányzó cél, eltérő policy, hiányzó +0 és 255 fölötti
szint látható diagnosztika. Az eltérő családba vezető fejlesztés átalakítás.

Azonos név önmagában nem családazonosság. A jelölt térkép **nem** elfogadott
tartalomkiadás: minden család `reviewed=false`, a csomag `installable=false`.
Az összes régi sor és példány tételes hashhez kötött, így a meg nem oldott
mező vagy túlterhelt socket sem tűnik el „alapérték” címén.

A player.item mellett az account_mount_inventory és guild_renewal_storage
is példánytároló. Utóbbiak a mappingben külön forrástérrel és ütközésmentes
cél-InstanceId-val szerepelnek. A példányok nem olvadnak össze. A bolti
termék és az öt lehetséges cserealapanyag külön hivatkozás. A guild adó
item0..item4 mezői darabszámok; az object_proto.vnum épületazonosító — ezek
nem kezelhetők automatikusan tárgyazonosítóként.

A példány tulajdonosa típusos: játékos, account vagy céh. Az owner-azonosságokat
a konzisztens snapshotból vagy annak ellenőrzött visszaállításából olvassuk,
a kiegészítést a checkpoint hashéhez kötjük. Azonos sorszámú account és játékos
nem azonos tulajdonos. A hiányzó owner karanténjelzés, nem törlés vagy új owner
kiosztásának engedélye. Owner-bizonyíték nélkül a terv unchecked marad.

## Natív fejlesztési szerződés, v12

A `protos.refinement_recipe` egy konkrét ItemKey/szintről másik ItemKey/szintre
vezető művelet. A `refinement_ingredient` pontos ItemKey/szint és mennyiség.
A sikeresély bp, a pénzköltség 0..INT64_MAX. Sikertelen kimenetel explicit:
megtartás, megsemmisítés vagy alacsonyabb forrásszint. Ugyanazon kulcson csak
a következő szint engedett; másik kulcs explicit tartalmi átalakítás lehet.
A builder ellenőrzi a létezést, retirementet, szintlefedést, nem halmozható
forrás/célt, egyedi alapanyagokat és a 32 soros limitet.

A receptek és alapanyagok az immutable katalógus és a kanonikus hash részei.
SQL-betöltésük és a natív admin-tool tranzakciós mentése elkészült. A séma
friss, izolált példányhoz készülő draft; **nem** az aktív v7 adatbázisra
ráfuttatható, adatmegőrző updater.

A `PrepareRefinement` csak birtokolt változtatási tervet készít. Nincs RNG,
SQL, ECS-írás, pénzlevonás vagy kliens-visszaigazolás. A szerver rögzített
dobását kapja: 0/100% esélynél kötelező 0, egyébként 1..10000. A terv
katalógusrevízióhoz/hashhez, operation ID-hoz, tulajdonoshoz, forráspéldányhoz,
pénztárcaverzióhoz és minden fogyasztott halom verziójához/darabszámához kötött.
A forráspéldány nem fogyasztható el saját alapanyagaként. A kiválasztás stabil
InstanceId-sorrendű; nincs bemeneti sorrendtől függő levonás.

**A tartós writer még F3-feladat.** A receipt, wallet, forrás és összes
alapanyag egy tranzakcióban, session/owner fence és expected revision mellett
commitolandó. CommitUnknown esetén a tartós operation ID alapján kell egyeztetni.
Csak ezután módosítható az ECS, a stat és a kliens. A tiszta terv nem bizonyít
tartós idempotenciát, versenyhelyzet-mentes játékbeli fejlesztést vagy újrabelépést.

## Offline receptek

A konverter a normál kovács receptjeinek pénzét, esélyét és alapanyagait
közvetlenül új kulcsokra/szintekre vezeti át. A normál kovács régi failure
szabálya `DoRefine` szerint megsemmisítés. A forrásrecept-azonosító és régi
tárgyszám csak a külön auditkapcsolatban marad; a natív receptformában nincs.

Bot/csákány, soul, jelmez-kombinálás és mount külön viselkedés. Ezeket, a
tekercsváltozatokat és a céhes díj/esélyszabályokat nem helyettesíti egy
generikus ingyenes vagy 100%-os recept. Hiányzó recept/alapanyag explicit
blokkolt tervezet. A natív katalógusvalidálás a teljes tartalom fordításának
kötelező kapuja; az offline recipecsomag önmagában nem telepíthető.

## Meglévő új katalógus megőrzése

Az `upgrade_exported_catalog.py` az ellenőrzött v7 exportot formálja át a
jelenlegi natív sémára. Tartalommező elvesztése, szemantikai registry-átírás
vagy új kötelező mező explicit migráció nélkül hiba. A bónuszregiszter
bővülhet; régi definíció és engedélyezett forrás nem értelmezhető át.

A `stage_exported_catalog.py` ezt új sandboxba importálja, majd a játék
natív SQL-katalógusbetöltőjével újraolvassa és hash/számláló szerint ellenőrzi.
Nincs létező SQL-végpontot elfogadó opciója. A valódi v7 csomagadatok csak a
teljes F2/F3/F4/F5/F6 kiadási kapuk után cserélhetők; erre a felhasználói
kérés megadta a helyi hatókört, új jóváhagyást nem kérünk.
