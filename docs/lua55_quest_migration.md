# Lua 5.5 és közvetlen questbetöltés

## Működés

A GameServer az ellenőrzött Lua 5.5.1 forrást használja, a vendorizált
`SRC/3rd/LUA55` könyvtárból. A forrás eredete, SHA256 összege és a hivatalos
hibajavítások az `UPSTREAM.md` fájlban találhatók. A régi VM kizárólag az
opcionális történeti QC összehasonlító eszközhöz tartozik. Ez alapból nem épül;
összehasonlításhoz `-DAURIGA_BUILD_LEGACY_QC=ON` kapcsolóval engedélyezhető.

Induláskor a szerver a beállított questmappa `quest_list` fájlját olvassa.
Minden aktív sor egy relatív forrásútvonal. A `quest … state … when … begin`
formátum továbbra is használható: saját feldolgozó bontja eseményekre, majd
a Lua 5.5 ellenőrzi a törzseket és a feltételeket. Ezekből memóriabeli
eseménytábla készül. Nincs külön fordítás és nincs generált `object` mappa.
A `questnpc.json`, `questcategory.json`, `questlib.lua`, `locale.lua` és a
`libraries` könyvtár továbbra is szükséges.

A questnevek, a `start = 0` állapot és a többi állapot eredeti, előjeles CRC32
azonosítója megmarad. A játékosok mentett questflagjeit nem kell átírni.
Az ismételten megnyitott állapotok eseményei összeadódnak, ahogy a QC-ben.
A régi `!=`, `!` és blokknyitó `begin` jeleket a feldolgozó alakítja át;
a karakterláncokat és megjegyzéseket megőrzi. A forrásfájl bájtjait nem írja át.

Az azonos questnevet deklaráló két külön forrás indulási hibát okoz.
Ez korábban csendes felülírás volt; migrációkor külön jelentés készül róla.
A listán kívüli régi objectek többé nem aktiválnak elfelejtett eseményeket.

## Kompatibilitás és futtatás

A szükséges `table.getn`, `table.foreach`, `table.foreachi`, `math.mod`,
`string.gfind`, `unpack` és `loadstring` segédek átmenetileg elérhetők.
A migráló a régi `select(...)` segéd automatikus `arg` tábláját explicit
`{...}` táblára alakítja a másolt `questlib.lua` fájlban.
A `select`, `wait`, `input`, `confirm`, `select_item` párbeszédek a Lua 5.5
coroutine API-ján folytatódnak. A questflag egészértékek 64 bitesek.
A futtatás szöveges Lua-kódot fogad; a korábbi verzió bytecode-ja nem tölthető be.

Az `io`, `debug`, `package`, `os.execute`, `os.exit`, `os.remove`, `os.rename`,
`os.tmpname`, `os.getenv` nincs megnyitva. Az `os.date` és `os.time` megmarad.
Az ismert könyvtárbetöltésekhez szükséges `dofile` továbbra is elérhető;
a questforrások adminisztrátori, megbízható kódnak számítanak.
Ez nem általános, nem megbízható kód futtatására szolgáló sandbox.
Egy esemény vagy feltétel futtatására és minden párbeszéd-folytatásra
ötmillió Lua-utasításos keret jut; túllépése hibával megszakítja az adott kódot.
Ez a hosszú C++ műveletek idejét és a VM memóriahasználatát nem korlátozza.

A régi `questlib.lua` külső mysql programot indító `execute_query` segédje
nem használható. A jelenlegi 70 aktív forrás nem hívja. Új kódban a szerver
regisztrált adatbázis API-ját kell használni.

## Questek frissítése játék közben

1. A szerverkezelőben szerkeszd és mentsd a questforrást vagy Lua-könyvtárat.
   Éles szervernél előbb töltsd fel a fájlokat a profil célgépére.
2. Helyben az Áttekintés oldalon válaszd a **Questek frissítése** gombot.
   SSH-nál az Kapcsolatok és eszközök **Távoli questek → Frissítés játék közben** műveletét.
   A profil Távoli szervermappa mezője az új csomagra mutasson.
3. A napló minden core előkészítését és aktiválását jelzi. A CH99 is részt vesz.
   A siker végén `QUEST_RELOAD_OK` jelenik meg.

Parancssorból ugyanaz: `python3 -B manage.py reload-quests`;
állapotlekérdezés: `python3 -B manage.py quest-status`.
Közvetlen SSH és ProxyJump egyaránt használható; nincs új hálózati port.
A kezelőt ugyanazzal az operációs rendszerbeli felhasználóval futtasd,
amelyik a csomag felügyelőjét és core-jait futtatja.

A mentés és az aktiválás külön művelet. Futás közben a verziózott Lua- és
questforrások, a `quest_list`, `questnpc.json`, `questcategory.json` menthetők.
A `server.json`, a globális játékadatok, a `settings.lua` és a
`restart_city_pos.lua` módosításához újraindítás kell. A frissítést minden
core-nak támogatnia kell; a korábbi binárisokkal az app ezt nem engedi.
Ha ezeket a közös Lua-beállításokat is megváltoztattad, állítsd le a szervert,
mentsd a fájlokat, majd válaszd a **Questek előkészítése induláshoz** gombot.
Távolról: **Távoli questek → Előkészítés leállított szervernél**.
Parancssorból: `python3 -B manage.py apply-quests`. Ez ellenőrzött új
indulási csomagot jelöl ki a megőrzött questazonosítókkal, majd indítható a szerver.
E lépés nélkül az újraindítás a korábban elfogadott csomagot tartja meg.

Az aktiválás új Lua-környezetet készít, ellenőrzi a teljes forráscsomagot,
és csak az összes futó core jóváhagyása után teszi közzé a döntést.
Előkészítés alatt a játékot módosító natív API-hívások tiltottak.
Az induláskori globális regisztrációk hívásait és paramétereit összehasonlítjuk
a futó változattal. Ha például egy könyvtár új teleport- vagy arénaadatot
regisztrálna, a frissítés újraindítást kér, és nem hagy részben módosult C++ adatokat.
Szintaktikai vagy inicializálási hiba esetén minden core a régi verzión marad.
A fájlokról külön, SHA256-tal ellenőrzött másolat készül, így a core-ok
azonos bájtokat kapnak. Az új események a következő biztonságos
eseményhatártól használják az új környezetet; a váltás nem szakít félbe futó Lua-kódot.

A már megnyitott párbeszéd a régi környezetben folytatódik, a régi segédekkel
és a régi `dofile`/`loadfile` tartalommal. Az egyszeri időzítők szintén a régi
kezelőt tartják meg. Az ismétlődő időzítők a következő ticknél az új verzióra
váltanak, ha az azonos nevű timerkezelő továbbra is szerepel a forrásban;
egyébként a régi verzióban maradnak törlésükig. Az ütemezésük nem indul újra,
és nem kapnak második példányt. A játékosflag-ek megmaradnak.
A befejezett párbeszédek és törölt/lejárt időzítők után a régi VM felszabadul.
Legfeljebb 16 egyidejű generáció maradhat meg; e korlátnál a következő
frissítés elutasítást kap, amíg a régi párbeszédek/időzítők be nem fejeződnek.

Meglévő quest vagy állapot törlése, átnevezése és állapotazonosító-változás
futás közben elutasított. Új quest és új állapot hozzáadható. A questazonosítók
nem követik a lista átrendezését; a közzétett azonosítótérkép újraindításkor
is megmarad. Szerkezeti adatátalakításhoz külön migráció és karbantartás kell.

Az elfogadott döntés a csomag `run/quest-reloads/current.json` fájlja,
a pillanatképek a tranzakciónkénti almappákban találhatók. Ezeket őrizd meg:
a következő indulás a legutóbb jóváhagyott csomagot tölti be, az esetleges
félkész szerkesztések helyett. A már közzétett döntést nem vonjuk vissza
automatikusan. Ha utána megszakad az SSH vagy lejár az időkorlát,
`QUEST_COMMIT_PENDING` marad; ugyanazzal a gombbal/paranccsal folytasd
a visszaigazolások begyűjtését. Ez a már futó játékhatásokat nem ismétli meg.
A `/reload q` a felügyelt csomagban erre a közös műveletre irányít;
külön indított, egycore-os szervernél önálló újratöltést végez.

A FreeBSD folyamatellenőrzés procfs nélkül a rendszer
[ps(1)](https://man.freebsd.org/cgi/man.cgi?query=ps&sektion=1&manpath=FreeBSD+15.1-RELEASE)
PID-, indulásiidő- és programmezőire támaszkodik. A tényleges FreeBSD
fordítás és próba külön célgépes ellenőrzést igényel.

## Meglévő questmappa előkészítése

A következő eszköz új mappát hoz létre, az eredetit megőrzi. Nem másolja át
az `object`, `pre_qc`, QC/PQC futtatható állományokat. Ellenőrzi a korábbi
állapotazonosítókat, a forrásmásolatok ellenőrzőösszegét és a teljes új listát.

```powershell
python SRC/Tools/prepare_quests.py `
  --source D:/AurigaGlobal/Server2-Local/data/quest `
  --output D:/AurigaGlobal/quest-lua55-prepared `
  --checker build/SRC/Server/GameServer/RelWithDebInfo/ServerPackageCheck.exe `
  --duplicate-policy last
```

A `last` opció kifejezetten a régi QC sorrend szerinti felülírását követi.
Alapértelmezésben a névütközés hibának számít. A kimaradó bejegyzés megjegyzés
lesz az új `quest_list` fájlban; mindkét forrásfájl megmarad. A jelentés neve
`lua55-migration.json`. Hibás előkészítés után a részleges kimenet nem használható;
javítás után új, üres célútvonallal kell ismételni.

A jelenlegi adatokon: 70 forrás, 69 aktív quest, 533 eseménykezelő,
72 ellenőrzött és változatlan állapotazonosító, egy javított varargs segéd. A két `levelup` közül a
`Quest/login/levelup.lua` marad aktív, a `Quest/item/levelup.lua` listabejegyzése
inaktívvá válik, a korábbi fordítás sorrendjének megfelelően.

## Ellenőrzés és kiadás

```powershell
cmake --build build --config RelWithDebInfo --target GameServer ServerPackageCheck LuaSourceTests QuestRuntimeTests
ctest --test-dir build -C RelWithDebInfo -R "^(lua_sources|quest_runtime)$" --output-on-failure
build/SRC/Server/GameServer/RelWithDebInfo/ServerPackageCheck.exe --check-quests <questmappa>
build/SRC/Server/GameServer/RelWithDebInfo/ServerPackageCheck.exe --check-libraries <adatmappa>
build/SRC/Server/GameServer/RelWithDebInfo/GameServer.exe --check-quest-runtime <adatmappa>
```

A `--check-quest` és `--check-quests` csak feldolgoz és fordít, a questeket nem
hajtja végre. A `--check-libraries` ténylegesen betölti a megbízható közös
könyvtárakat, csak az induláskori regisztrációkhoz szükséges tesztpótlásokkal;
nem helyettesít játékbeli tesztet. Az egyfájlos ellenőrzést a szerverkezelő
fájlszerkesztője is ugyanazzal a ServerPackageCheck programmal végzi.
A GameServer `--check-quest-runtime` módja a tényleges játékbeli Lua-kötésekkel
ellenőrzi az indulást, előkészítési módban, hálózati és SQL-kapcsolat nélkül.

A `prepare_server.py` az új csomagban automatikusan előkészíti az aktív
questlistát és kihagyja az objecteket. A már futó csomagot nem módosítja.
Kiadás előtt külön localhost-próbában ellenőrizendő a belépés, questablak,
választás/bevitel/megerősítés, tárgyhasználat, NPC-beszélgetés, időzítők és dungeon.
Ezután szükséges a cél FreeBSD rendszeren végzett fordítás és próba.
A Windowsos tesztek nem igazolják önmagukban a teljes éles játékmenetet.
