# Foglalatkezelés — tisztítás, kőkiemelés és ékszertöltés

2026-10-29, `codex/mission-engine`, kiinduló HEAD: `501ca1f8`.
[IC-030 szerződés](../../adr/item_combat_030_socket_services.md).

## Elkészült egy összefüggő egységben

- Törött foglalatok tisztítása, minden ép kő kiemelése, illetve az utolsó
  ép kő kiemelése. Külön névvel regisztrált műveletek, értéktömb nélkül.
- Tisztításkor nincs átrendezés; a foglalat pozíciója és minősége megmarad.
  A teljes kiemelés törött, az utolsó kő kiemelése üres helyet hagy.
- Minden kiemelt kő megtartja az ItemKey-t és a példányszintet. A +30-as
  tesztkő kiemelés–tisztítás–visszahelyezés után is ugyanazt a helyet,
  minőséget, típust és szintet kapja vissza. Nincs +9-es külön ág.
- Minden visszaadott kőhöz külön, előre foglalt inventory-hely és új
  példányazonosító kell. Hiányzó/dupla/idegen/fölösleges foglaláskor nincs
  részterv. Üres cél, kevés forrásdarab és hibás állapot sem fogyaszt.
- Közös, teljes foglalatvalidátor a behelyezés és a karbantartás számára:
  állapotforma, célkompatibilitás, minőség, kulcs, szint és kizárócsoport.
  Az ismert, retire-olt beépített kő visszanyerhető; nem törlődik csendben.
- Ékszerfoglalat nyitása és töltése a meglévő típusos profil köré épített
  teljes használati tervvel. Explicit esély/dobás, anyag és mennyiség,
  időzített vagy tartós töltés. Sikertelen kísérlet fogyaszthat anyagot,
  de nem növel állapotot; telt vagy tiltott cél elutasított.
- Owner/session/operation/ruleset/inventory- és itemrevision ellenőrzés,
  inventory-hely, zár, csere, felszerelés, stack, szint, kaszt/nem, harc és
  observer befogadási kapuk. A tervek teljes bemenetet és katalóguspint
  birtokolnak. Nem hívnak RNG-t, SQL-t, ECS-t vagy AutoGive-ot.
- V23 katalógus: egyetlen új, négymezős `socket_service_profile` tábla,
  és egy kizárólagos fogyasztható-hivatkozás. Friss lánc: 000..029,
  78 natív katalógustábla. Snapshot, hash, validátor, SQL-olvasó és
  atomi adminisztratív tartalommentés frissítve.
- A Qt fogyasztható-szerkesztő negyedik típusa a foglalatkarbantartás.
  Művelet és darabszám választható; másolat szerkesztése nem írja át az
  eredeti profilt, típusváltás kitakarítja a már nem használt hivatkozást.
  Inaktív ital-/várakozási-/foglalatmező hibás piszkozata nem kerül mentésbe.

## Ellenőrzés

| Ellenőrzés | Eredmény |
|---|---:|
| Natív foglalatszolgáltatások, normál / ASAN | 102 / 102 |
| Érintett itemmodulok ASAN CTest alatt | 29/29 |
| Teljes Qt/modell/izolált SQL-kör normál helperrel | 73/73 |
| Ugyanez ASAN helperrel | 73/73 |
| Friss séma belső ellenőrzései | 39/39 |
| Teljes szerver CTest első, egymásutáni köre | 131/132 |
| Javított SQL-loader teszt célzott ismétlése | 1/1 |

A teljes szerverkör egyetlen hibája a tesztindító régi, 000..028 láncra
rögzített ellenőrzése volt. A v23 lánc 000..029; ennek javítása után az
érintett SQL-betöltő teszt sikeres. A többi 131 cél az első körben átment,
így mind a 132 célhoz van sikeres, jelenlegi ellenőrzés. Az egész 132-es
kört nem ismételtük feleslegesen a tesztindító javítása miatt.

A tesztek az aliasolt vagy hiányzó foglalásokat, slot 0-t és zárt suffixet,
szint-/csoportütközést, típusváltást, másolási izolációt és két külön SQL-
írási hiba teljes rollbackjét is vizsgálják. A rollback az összes profilt,
prototípust és kategóriaszámlálót megőrzi. A Qt signal-kivételek hibának
számítanak. A formot képen is ellenőriztük.

Független Python encoderrel egyező natív szolgáltatás-fixture hash:
`0431adfc595888774ce0f005ca482759d6786782b3b225d8cf3e9dcdc756b051`.
A korábbi fixture-ök v23 hashét is az önálló Python mezőnkénti encoder
számolja, nem a natív eredmény másolása.

A meglévő, egytárgyas felhasználói v7 katalógus v23-as exportjelöltjét
külön MariaDB-sandboxban importáltuk és natívan visszaolvastuk, azonos
kulcsokkal, cellákkal és számlálókkal. Hash:
`d50fc6288bcc83bb24e164a4b279b8898f2c1c543cb6fa403a59fdc17b8126eb`.
Ez nem a teljes régi item_proto konverziója. Az aktív helyi adatbázis 33
táblája változatlan v7, egy felhasználói tárggyal. Telepített appot,
futó játékot, Unityt és távoli gépet nem módosítottunk.

A próba Windows/MSVC és MariaDB 11.4.13 alatt futott. FreeBSD- és Unity-
játékteszt nem történt. A korábbi párhuzamos Mission SQL-instabilitás
okát ez a csomag nem javítja; a jelenlegi egymásutáni Mission-teszt sikeres.

## Forrásleltár és további munka

A fagyasztott mentésben 49 kapcsolódó prototípus van: 44 ékszertöltő,
1 nyitó, 2 tisztító, 1 teljes és 1 utolsóköves kiemelő. A 72719-es külön
kiemelőnek nincs protója a mentésben. Érintett forráseszköz-példány nincs;
egy beépített kőre utaló régi példányadat van, amelynek pontos szemantikája
az F6 tételes foglalatkonverziójának része. A protólapok mind
`converted=false` állapotban maradtak. A típusfelismerés önmagában nem
bizonyítja az árak, flagek, bónuszok és példányadatok kész migrációját.

**Ez F2 tartalmi/domain csomag, nem a futó játék új gazdasági írója.**
F3/F4-ben a valódi helyfoglalás, a receipt, a forrásfogyasztás, a teljes
foglalatcsere és az összes visszaadott példány egy tranzakciója kell;
CommitUnknown esetén operation ID szerinti egyeztetés. A kiemelt kő új
inventory-példányt kap; az ItemKey és szint a megőrzött identitásadat,
nem egy korábbi önálló instance ID visszatalálása.

Következő nagy egység az instance-bónuszok, bónuszváltás/-hozzáadás és
zárolás típusos modellje, valamint a kapcsolódó tárgyműveletek. A teljes
F2 lezárása után jön F3 tartós példány/gazdasági commit, F4 fogyasztók,
F5 protokoll/Unity, F6 teljes offline konverzió és F7 helyi játékpróba.
