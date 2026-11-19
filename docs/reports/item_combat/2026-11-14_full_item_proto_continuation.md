# Teljes item proto konverzió – folytatás

A [legfrissebb funkcióbekötések és elszámolás](2026-11-14_full_item_proto_functions.md)
a 9 186 protós állapotot, a fejlesztett ládajutalmakat és a használati korlátozásokat tartalmazza.

2026-11-14. Az előző ellenőrzött állapothoz képest **149 további prototípus**
konvertálódik. A teljes átállás még nincs kész; a futó helyi szerver katalógusa
nem változott, a jelölt továbbra is `release_ready=false`.

| Eredmény | Előző állapot | Most |
|---|---:|---:|
| Natív definícióra fordított forrássor | 9,019 | 9,168 |
| Kivezetett / aktív buildben nem használható | 491 | 491 |
| Pontos akadállyal fennmaradó forrássor | 1,476 | 1,327 |
| Összes forrássor | 10 986 | 10 986 |

A jelölt **6,250 natív definíciót** tartalmaz. Az eredeti
1 574 definíció, a korábban kiadott katalógussorok és natív kulcsok megmaradtak.
Az összes forrássor pontosan egyszer szerepel a [részletes CSV-ben](2026-11-14_full_item_proto_continuation.csv).

## Új fordítások

- 11 fejlesztőtekercs és 3 lélekfejlesztő tekercs: a forrás sikertelenségi
  szabályaival, a feljegyzéstekercs pontos célszintjével és a lélektekercsek halmozásával.
- 7 rúna: a build tényleges bónusztáblájával, kezdeti töltésével és kapacitásával.
  A készletrúna a külön natív készletfeltételt használja.
- 20 bónuszszolgáltató és 6 bónuszeszköz: talizmán-, ötödikbónusz-, jelmez-
  és zárolási műveletek, rúnatöltő palackok, stólaforgatás és jelmezbónusz-eltávolítás.
- 10 másodlagos képességkönyv, forrás szerinti szinttartománnyal, sikereséllyel
  és várakozással; 5 példányon tárolt formát használó átváltoztató gömb.
- 31 sült hal hatásprofilja és 48 élő/döglött hal feldolgozási definíciója.
  Az élő hal feldolgozásának két eredménye pontosan 31% döglött hal / 69% halszálka;
  a sütési cél a haladat-tábla közvetlen hivatkozása. A régi fajtaindex ellenőrzött
  auditadatként marad meg, a natív működés közvetlen kulcsokat használ.
- 6 petellátmány: véges élettartam-növelés, végleges élettartam és 30%-os,
  +5-ös, 150/100/100 korlátú tulajdonságfejlesztés.
- 2 további láda jutalomfüggősége zárult.

A forrásban hibásan besorolt átváltozáskönyvek a név szerint meglévő tanulási
kezelő alapján javított fordítást kaptak. A döglött hal sütése a konfigurált
eredményt használja a régi eltolásos keresés helyett. Mindkét korrekció a
forrássor elszámolásában külön jelölést kap.

## Szerver és Unity

A talizmánok saját bónuszcél-bitet kaptak: a normál páncélok eszközei nem
módosíthatják őket. A forrás attribútumjogosultságának byte-konverziója és az
ötödik bónusz eltérő 30%/75%-os esélye megmarad. A zöld bónuszoló a tényleges
fejlesztési szint követelményét vizsgálja. Az SQL-korlát és a szerkesztő is követi
az új célbitet; a natív olvasó és Unity közös talizmán-mintacsomagot ellenőriz.

A viseletablak üreshely-ikontáblája 24 helyre bővült. A validátor által kimutatott
megnyitási kivétel megszűnt. Az egyedi tárgy a második egyedi felszerelési helyen
is kiválasztható a szerver szabályával összhangban.

## Ellenőrzések

- A GameServer, natív katalógus- és SQL-eszközök lefordultak.
- **23 célzott natív/SQL teszt sikeres**, köztük **27 konverziós teszteset**.
- A teljes 6 250 definíciós jelölt natív validációja és külön, erre létrehozott
  MariaDB-be írása/visszaolvasása sikeres, azonos katalógushash-sel.
- Unity **6000.3.27f1** újrafordítás és a `fish-offline`, `companion-offline`
  ellenőrzések sikeresek. Többek között a hal, könyv, attribútum, rúna/stóla,
  fejlesztés, felszerelés és leltár csomagjai, valamint a viseletablak megnyitása
  ellenőrzött. A tételes eredmények az [evidence fájlban](2026-11-14_full_item_proto_continuation.evidence.json) vannak.
- A helyi szerver 94 ellenőrzött játékosállapot-táblája továbbra is üres:
  account, karakter, régi tárgy és natív példány egyaránt 0. A futó katalógus
  változatlanul 1 574 definíció, séma 31.
- **A teljes konvertált katalógussal való játékfolyam nem futott le.** A jelölt
  nem került a futó játékba; a hiányos kiadást a telepítő továbbra is tiltja.

## Fennmaradó akadályok

| Csoport | Forrássor |
|---|---:|
| Viselkedés / külső kezelő vagy példányadat szükséges | 432 |
| Hiányzó ikon, modell vagy Unity hivatkozás | 467 |
| Hiányzó vagy még feloldhatatlan ládajutalom | 293 |
| Forrásadat vagy natív szabály eltérése | 135 |

Továbbra is szükséges az általános tárgyidőzítők és korlátozások teljes futásidejű
bekötése, a születési bónuszok/foglalatok/páros addonok létrehozása, a hiányzó
használati funkciók és tartalmi hivatkozások befejezése. A gömbökhöz a megbízható
példányforma is szükséges. A horgászat világbeli zsákmánytáblájának átállítása
külön marad a halak itt elkészült feldolgozási definícióitól.

Az általános bónuszforgatók páros addonja, a fejlesztett szintű ládajutalmak,
a lélekszintek eltérő megjelenése, a horgászbot/csákány fejlesztési gráfok,
a felszívó vállpántok, egyes skinek és külső küldetéskezelők még munkát igényelnek.
Hiányzó viselkedést vagy tartalmat a fordító nem helyettesít üres tárggyal.

Jelölt: `build/full-proto-continuation-final-v2.json`.
Natív hash: `04e8869ee78d440b2fce27d7715a690bbfd3d89130369355c29241ed078bca1b`.
