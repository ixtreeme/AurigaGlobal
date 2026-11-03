# C22 — Képességkönyvek

2026-11-03. Szerződés: [IC-050](../../adr/item_combat_050_skill_books_runtime.md).

## Eredmény

Tanuló-, mester-, felejtő- és lópontkönyv használható a futó szerveren és a
külön típusos Unity-leltárból. Fix és példányhoz kötött skillcél, sikeres
olvasásszámláló, garantált/sikertelen eredmény, XP-költség, skillhez kötött
várakozás és pontvisszatérítés mentődik a könyvfogyással együtt. A normál
legacy leltár átállítása a felhasználó kérésére továbbra is vár.

A tényleges képességszint a szerver skillkomponensében és a kliens szokásos
GC76 csomagjában is változik. Az új ablak nem helyi szintemelést szimulál.
A régi késői teljes karaktermentés nem írhatja vissza a könyv előtti állapotot.

## Lefutott ellenőrzések

| Ellenőrzés | Eredmény |
|---|---|
| Release GameServer, Database, LoginServer | sikeres |
| Célzott item-CTest | **18/18 csoport** |
| SkillBookTests | **1100 ellenőrzés** |
| BookRuntimeTests | **2649 ellenőrzés** |
| Külön saját MariaDB, ItemPersistenceSqlTests | **1569 ellenőrzés** |
| Külön saját MariaDB, ItemBenchToolTests | sikeres |
| PlayerLoadTests | **15 ellenőrzés** |
| PointCalculationTests | **6650 ellenőrzés** |
| GmCommandTests | **31564 ellenőrzés** |
| PlayerCreateTests | **167 ellenőrzés** |
| PlayerCacheDrainTests unit / külön MariaDB | **38 / 455 ellenőrzés** |
| Érintett Python fájlok szintaxisa, git diff check | sikeres |
| Unity 6000.3.27f1 scriptfordítás | sikeres |
| GC244 golden, csonkítás, TCP split, UI, dispatcher | **2929 ellenőrzés** |
| Korábbi C4–C21 Unity-validátorok | mind sikeres |
| Valódi Unity → TLS → GameServer → SQL és UI | **37 ellenőrzés, sikeres** |
| Canonical source snapshot | **5466 fájl, 1730 byteazonos C# fájl** |
| Korábbi helyi tárgyállapotok | **mind a 69 változatlan** |

A natív tesztek lefedik az admissiont, fejlődési számlálót, egyetlen
szintemelést, pontos ezredmásodperces cooldown-határt, felejtés pontját,
sikertelen próbát, kötött cél engedélylistáját, lópontot, mastery-t,
töltetfogyasztást és BOK1 roundtripet/csonkítást. A különböző skillre kötött
grantok nem olvadnak össze. GC225 v4 megőrzi a kötött célt.

SQL: item-CAS és karakter-revíziós CAS hibáján teljes rollback; sikeres
forrás/XP/progress/skill/deadline commit és receipt replay; eltérő rögzített
eredmény elutasítása; új login és sérült sidecar-hash. A régi revíziójú
teljes karakter-UPDATE megőrzi mind az öt védett könyvmezőt, miközben a más
karaktermezőket elmenti. A cache-tesztek a tényleges save-query generátort
és a friss kanonikus sémát is ellenőrizték.

Az élő próba valódi inventorygombbal és megerősítéssel indult. Első siker
egy olvasást és 100 XP-költséget, második siker egyetlen tényleges szintet
adott; a számláló nullázódott. Másik könyv ugyanazt a várakozást kapta.
Szinttiltás és saját birodalom nyelvének tiltása nem fogyasztott könyvet.
Ugyanaz a megerősítés nem olvasott újra. Felejtés a várakozás alatt is
működött és pontosan egy másodlagos pontot adott vissza. A garantált kudarc
elvitte az egy könyvet és beállította a várakozást. Kötött könyv a példány
121-es célját tanította; lópont és 20→21 mesterkönyv külön is lefutott.
Újrabelépés után a részleges olvasás, cooldown, XP, visszakapott pont,
lópont és tényleges mesteri szint megmaradt; régi login-control elutasított.
Az elfogyó halom utolsó egységének eltűnését is kezelte a felület.

Az élő ellenőrzéshez az 5-ös dedikált tesztkarakter 50-es szintet kapott a
valódi lópontkövetelmény miatt. Közben javítottuk a harci skill maximumának
feloldását: a protó bMaxLevel mezője ezeknél skálázási paraméter is lehet,
a tanulási korlátot a tényleges 40-es mastery-létra adja. A hideg betöltés
régi másodlagospont-rekonstrukciója book-revíziós karakteren már nem írja
felül a mentett visszatérítést. A könyvablak saját nevű ECS-sessiont kapott,
amelyet más ablak heartbeatje nem kezelhet saját sessionként.

## Helyi állapot és újrafuttatás

Aktív Unity: `D:/Auriga-Unity6/SRC/AurigaUnity`,
`/main/codex-mission-engine`, **cs59**, pinned Editor `6000.3.27f1`.
A checkin csak a tizenkilenc C22-fájlt tartalmazza. A külön batch
snapshot a teljes aktuális C# készletet fordította. A felhasználó jelenet-,
kapcsolat-, Packages- és ProjectSettings-módosításai megmaradtak; az Editorát
nem zártuk be. A snapshot hiányzó világ/VFX/Ocean assetjeinek ismert jelzései
nem akadályozták az itemutakat. Ez nem teljes player-build vagy jelenetpróba.

A saját `D:/AurigaGlobal/Server2-Local` öt szerverfolyamata és MariaDB-je fut.
PID 1610 dedikált codexitem karakter: **94 tárgyállapot**, a 69 korábbi
változatlan; tíz könyvtípusból összesen öt-öt darab, pótló halmokban is.
Learning-revízió 8, XP 700, fő/másodlagos/ló pont 5/42/1, karakterszint 50.
A 121-es skill 1-es szintű, egy sikeres részleges olvasással; a mesteri skill
21-es szintű. HP/SP induló érték 1000/20. A Game, Database, Login, helper és
007-es migráció hash-e egyezik a manifesttel és az elkészült fájlokkal.

Újrafuttatás: saját stack stop, leállás megvárása;
`SeedBooksUnityLive.py` a korábbi effects/books privát fixture-rel, **külön
--game, --database és --login binárissal**; stack start;
`PrepareUnityValidationSnapshot.py` és `BooksUnityLiveSmoke.py`.
Bridge módok: `books-offline`, `books-live`. A seed kizárólag az ellenőrzött
codexitem PC könyv-fixture-jét állítja alaphelyzetbe, korábbi tárgyait megőrzi.
Privát fixture/logok: nem követett `build`. Teljes karakter- és binárismentés:
`Server2-Local/run/deployments/books-*`. A telepítő a tényleges korábbi
binárisokat és a manifest szerinti hash-t is rögzíti a mentésben.

## Nyitott kapuk

Teljes Unity player build, FreeBSD, többcore-os handoff, terhelés és
teljes gameplay crash-injection nem futott. Normál progression közbeni
hálózati/folyamatleállítások teljes hibainjektálása külön kiadási kapu.
A typed könyvbónusz/várakozáselhagyó töltet fogyasztását natív teszt
ellenőrizte; a töltetet adó új segédtárgy és a legacy affectek konverziója
még hátravan. Az élő készlet normál, töltet nélküli könyveket használ.
A normál leltárátállás és teljes adatkonverzió külön munka.
