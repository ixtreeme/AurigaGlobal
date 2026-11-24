# A Lua-küldetések átvezetése a Mission Engine-be

A cél, hogy a GameServer C++-ban futtassa az összes küldetést. A Mission Studio a küldetés nevét, szövegét, céljait, sorrendjét és jutalmát verziózott csomagforrásba menti. A `MissionCompiler` ebből készíti el a szerver által betöltött `.aurmission` fájlt. Új játékmechanikához C++ esemény vagy művelet kell. A már bekötött műveletekből a tartalomkészítő szerverfordítás nélkül állíthat össze küldetést.

## Kiinduló állapot

2026. november 24-én a helyi szerver aktív küldetésverzióján (`4cd41aa20d778e54883a8d57b08936e2`) a `ServerPackageCheck --check-quests` 76 forrást és 603 eseménykezelőt töltött be. Ebbe mindkét régi fájlkiterjesztés, a `.quest` és a `.lua` is beletartozik. A szerver által külön betöltött közös könyvtárak és fordítások ezen felül értendők.

| Forrásmappa | Betöltött forrás |
| --- | ---: |
| Rendszerek | 19 |
| Kazamaták | 15 |
| Események | 14 |
| Céh | 9 |
| Belépés és szintlépés | 6 |
| Tárgyhasználat | 5 |
| NPC | 4 |
| Képességek | 2 |
| Tökéletes lélekkő | 1 |
| Zodiákus | 1 |

A rendszerforrások között öt egysoros, üres tesztküldetés és egy próbakarihoz kötött Lua–Mission teszthíd szerepel. Ezek kivezetése önmagában nem pótol játékfunkciót. A helyi Mission-csomagban az ellenőrzéskor három külön próbamisszió volt.

## A költözés sorrendje

1. **Már működő C++ szolgáltatások.** A teleportgyűrű három városi és kilenc vadászterületi célját a natív tárgyhasználat kezeli. Külön ellenőrizendő minden ilyen régi szkript összes eseménye, a tartós flagjei, a belépési feltételei és a függő tervezete. Az aktív teleportgyűrű-forráshoz már létezik „Teleportgyűrű átvezetése” nevű tervezet. Az üres `start` állapot átmenetileg megőrzi a nevet a régi újratöltő számára.
2. **NPC-beszélgetések és küldetéslevelek.** A Zodiákus NPC (`20438`) a régi felületen kérdést, bezárási lehetőséget és öt lokalizált szövegrészt ad. Ehhez ismételhető, jutalom nélküli beszélgetés kell. A szerveróra levele az aktuális szerveridőt mutatja. Egyik viselkedést sem helyettesíti egy egyszer teljesíthető naplóbejegyzés.
3. **Automatikus küldetések.** Az `sd5` minden `20432`-es öléskor a karakter pillanatnyi szintje szerint fizet. A 137., 138. és 139. szinthez rendre 2, 3 és 4 millió XP tartozik. A hű átvezetéshez automatikus, megismételhető indítás és jóváírás kell. A kézi NPC-s felvétel és jutalomátvétel itt megváltoztatná a játékot.
4. **Állapotos szolgáltatások, céhek és világesemények.** Átvezetendő a karakter-, céh-, pálya- és csatornaszintű állapot, az időzítők, a GM-vezérlés, valamint a játékosonként már átvett jutalmak nyilvántartása. Az aktív lista és a jóváhagyott forrás az irányadó minden egyes költözésnél.
5. **Kazamaták és folyamatban lévő példányok.** A belépés, a csapatállapot, a pályafázisok, az időzítők, a visszalépés és a végjutalom együtt alkot egy átállítási egységet. A már natív kazamataszolgáltatásokat a misszióból lehessen hívni. Átváltás előtt dönteni kell a futó példányok kifutásáról vagy az állapotuk átadásáról.
6. **Közös Lua-függőségek.** A küldetéslista kiürülése után is ellenőrizendő a bootstrap, a lokalizáció, a közös könyvtárak, az időzített visszahívások és a Lua C API-t használó egyéb szervermodulok. Ezután vezethető ki a fordítási és a csomagolási függőség is.

Egy éles átállítási tervezet együtt mutassa a régi esemény kikapcsolását, az új megfelelőjét és a mentett állapot kezelését. Jóváhagyás előtt az új verzió külön szerverpéldányon próbálható ki. A játékos ugyanarra az eseményre egy jutalmat kaphat. Újrabelépés és megismételt kérés után is meg kell egyeznie a mentett haladásnak és a kiosztásnak.

## A Mission Studio első változata

A `SRC/Tools/MissionStudio/studio.pyw` önálló Windows-szerkesztő. A tervezeteket a felhasználó `%LOCALAPPDATA%/Auriga/MissionStudio/Tervezetek` mappájába menti. A **Forrás megnyitása** korábbi tervezetet és Mission JSON-forrást is beolvas. Egy megnyitott tervezet módosítása új példányba kerül.

Új küldetésnél válaszd ki a küldetésadót, állítsd be a szintet, majd add hozzá a sorrendben teljesítendő célokat. A névkereső ékezetek nélkül is működik. Az első verzió szörny- és bossölést, NPC-beszélgetést, tárgygyűjtést, tárgykészítést, valamint XP- és Yang-zárójutalmat tud szerkeszteni. Az összetettebb, importált gráfok megőrzik a további mezőiket. Az elágazások és a kazamaták vizuális szerkesztése további fejlesztés.

Az **Ellenőrzés és csomagolás** egy helyi mappába teszi a forrást, a lefordított `mission.aurmission` csomagot és a vele egyező `typed_mission_runtime.json` küldetésadó-szabályokat. A C++ fordító ellenőrzi a csomagot és a célokhoz tartozó játékbeli eseményeket. A közös szerverkezelő jóváhagyási és telepítési folyamatába a Mission-csomagokat még be kell kötni.

### Küldetésgráf

A bal oldali **Tartalomkönyvtár** mindhárom fülön használható. Az **NPC-k**, **Szörnyek**, **Tárgyak** és **Szövegek** között ékezet nélkül is kereshetsz. Az NPC a kezdésre húzva a küldetésadót állítja be, feladatra húzva a beszélgetési célpontot, párbeszédre vagy szövegdobozra húzva pedig a megjelenített beszélőt. Egy tárgy a vásznon gyűjtési feladatot hoz létre, jutalomra ejtve a kiválasztott szinttel és darabszámmal kerül a csomagba. Ugyanaz a tárgy újra ráhúzva összeadódik. A jutalom alsó tárgylistájából egy teljes tétel áthúzható másik jutalomba vagy a könyvtár alján a kivételi mezőre.

A szövegkönyvtárba saját szöveget menthetsz. Vászonra vagy vezetékre ejtve önálló, a játékos által nyugtázható szövegdoboz készül belőle. Meglévő párbeszédre ejtve annak szövegét írja át. A mintát a Studio másolja, így a már elhelyezett példányok külön szerkeszthetők. Egy válasz sora célcsomópontra húzva beköti az ágat, másik válaszsorra húzva a vezetékkel együtt rendezi át. A kampányban a szereplők és helyszínek küldetéskártyákhoz rendelhetők. A küldetésfából egy kártya másik felvonásra vagy a kampányvászonra is áthúzható.

Az **Eltávolítás** mezőbe jutalomtárgy, csomópont és vezeték húzható. Az Escape megszakítja a húzást. A szövegdobozhoz a szerver és a fordító egyválaszos párbeszédet támogató változata szükséges. A csomagolónak a `--actors` kapcsolóval adható át az NPC-ket és ellenfeleket elkülönítő, azonosító szerint kulcsolt JSON-névjegyzék. Minden rekord a `player.mob_proto` numerikus `type` értékét és a megjelenítendő `name` szöveget tartalmazza. A forrásból indított szerkesztő az `AURIGA_MISSION_TARGETS` környezeti változóval használhatja a kiadással azonos `mission_targets.json` állományt.

A Studio a **Küldetésgráf** fülön nyílik meg. A bal oldali fából küldetést választhatsz. A palettáról húzd a vászonra a párbeszédet, feladatot, feltételt, várakozást vagy jutalmat. Egy csomópont jobb oldali pontját húzd a következő lépés bal oldali pontjára. Kijelölés után a Delete billentyűvel bonthatsz egy vezetéket vagy törölhetsz egy lépést. Az üres vászon húzásával mozgathatod a nézetet, a görgővel nagyíthatsz. Az **Illesztés** megmutatja a teljes gráfot.

Dupla kattintással szerkesztheted a lépést. Egy párbeszédhez kettő–nyolc válasz és válaszonként egy útvonal tartozhat. A párbeszéd alsó táblázatában a válasz mellett látszik a következő lépés. A **Próbajáték** gombbal bejárhatod a választott utat. A feladat teljesítését és az idő múlását itt kézzel léptetheted. A jutalomcsomóponton XP és Yang szerkeszthető. A feltétel azt ellenőrzi, hogy egy adott feladat teljesült-e. Ez a tartós jelző egyszer teljesíthető küldetésekhez használható.

A Studio helyi tervezetbe menti a vezetékeket és a pozíciókat. Az **Ellenőrzés és csomagolás** a bekötött ágakat a C++ missziómotor lépéseire fordítja. Az ágak közösen folytathatók. Nyitott kimenetnél vagy elérhetetlen lépésnél a csomagolás jelzi a hibát. A játékos a missziónaplóban olvashatja a kérdést és a válaszokat. Korábbi csomag megnyitásakor a Studio a hozzá tartozó gráfot is visszatölti. Külön szerkesztői gráf nélkül az egyszerű lépéssor szerkeszthető, az összetett natív szerkezet olvasási nézetben járható be.

### Kampánygráf

A **Kampánygráf** fülön egy kártya egy küldetést jelöl. A kártya testét húzva átrendezheted a felvonást. A jobb oldali csatlakozópontot egy másik kártyára húzva az első küldetés teljesítése a második felvételének előfeltétele lesz. Több bejövő nyílnál a **Mindegyik** vagy **Bármelyik** szabály alkalmazható. Egy kijelölt nyíl a **Kapcsolat bontása** gombbal távolítható el. A **Kapcsolatok…** ablakban két külön felvonás küldetései is összekapcsolhatók. A kártyára duplán kattintva a küldetés tényleges céljai és jutalma szerkeszthetők.

A felvételi feltételeket a Studio a C++ motor meglévő `has_flag`, `all_of` és `any_of` szabályaival írja a csomagba. Az importált küldetés korábbi felvételi feltétele és egyéb történeti jelzői megmaradnak. Új küldetésnél a Studio eleve felveszi a teljesítési jelzőt. Ha egy korábbi, ilyen jelző nélküli küldetéshez utólag kerül kimenő nyíl, a régebbi verzióban már teljesítő karakterekhez külön adatátvezetés szükséges.

Az **Elveszett korona** mintaterv a szerző által megosztott látványterv részleteit használja. Az első felvonás hét mintakártyát tartalmaz, a további négy felvonás szabadon kitölthető. A felvonások, szereplők, helyszínek, főküldetés/mellékküldetés/titkos besorolások, párbeszédjegyzetek és kártyapozíciók a szerkesztő tervezetébe kerülnek. Csomagoláskor a `kampanyterv.json` is elkészül a `forras.json` mellett, így a forrás megnyitásakor visszakapod az elrendezést. A mintakártyákhoz a szerző választ játékbeli küldetésadót és teljesítési célokat.

Windowsos kiadáshoz a `MissionCompiler` CMake-cél és a mellette található OpenSSL DLL szükséges. A `build_studio.py --compiler <MissionCompiler.exe> --catalog <katalógus-olvasat.json> --output <kiadási-mappa>` a PyInstaller segítségével önállóan indítható alkalmazást készít. A választólistákhoz az aktív katalógus típusait és a szerverkezelő magyar névjegyzékét csomagolja.

## Kipróbált játékbeli folyamatok

2026. november 24-én két elkülönített helyi GameServer- és adatbázispéldányon, Unity 6000.3.27f1 klienssel futott az ellenőrzés.

- A Mission Studióban összeállított „Beszélgetés a kereskedővel” próbaküldetést a kliens a missziónaplóból indította. A kereskedőre kattintás teljesítette. Újrabelépéskor ugyanaz a küldetéspéldány teljesítettként jelent meg.
- A teleportgyűrű próbáján a régi gyűrűszkript a kiszolgáló betöltési listájából és a próbaszerver adatkönyvtárából is kikerült. A kilenc vadászterület menüje megnyílt és visszavonható volt. A három város közül a fővárost kiválasztva a karakter a célkoordinátára érkezett. A térképváltás után a gyűrű példányazonosítója és darabszáma megegyezett az induláskor rögzített értékkel.
- A Windows-kiadás elindult, a névkereső a `fulbevalo` szövegre több fülbevalót adott vissza. A szerkesztővel elmentett típusos tárgygyűjtési cél, XP és Yang visszaolvasható volt. A csomagolt C++ fordító a forrásból betölthető csomagot készített. Az XP és Yang jóváírása ebben a szerkesztőpróbában nem szerepelt.
- A küldetésgráf külön helyi próbáján két küldetés ugyanazon párbeszéd két útvonalát fedte le. A Unity kliens megkapta a magyar választási szövegeket. Az A és B választás után a megfelelő NPC-feladat következett. Az egyik út 12 TP-t és 1000 Yangot, a másik 21 TP-t és 250 Yangot ajánlott. Mindkét jutalom átvétele sikerült. Újrabelépés után a szerver mindkét küldetést teljesítettként küldte vissza.
