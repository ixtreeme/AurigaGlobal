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

Windowsos kiadáshoz a `MissionCompiler` CMake-cél és a mellette található OpenSSL DLL szükséges. A `build_studio.py --compiler <MissionCompiler.exe> --catalog <katalógus-olvasat.json> --output <kiadási-mappa>` a PyInstaller segítségével önállóan indítható alkalmazást készít. A választólistákhoz az aktív katalógus típusait és a szerverkezelő magyar névjegyzékét csomagolja.

## Kipróbált játékbeli folyamatok

2026. november 24-én két elkülönített helyi GameServer- és adatbázispéldányon, Unity 6000.3.27f1 klienssel futott az ellenőrzés.

- A Mission Studióban összeállított „Beszélgetés a kereskedővel” próbaküldetést a kliens a missziónaplóból indította. A kereskedőre kattintás teljesítette. Újrabelépéskor ugyanaz a küldetéspéldány teljesítettként jelent meg.
- A teleportgyűrű próbáján a régi gyűrűszkript a kiszolgáló betöltési listájából és a próbaszerver adatkönyvtárából is kikerült. A kilenc vadászterület menüje megnyílt és visszavonható volt. A három város közül a fővárost kiválasztva a karakter a célkoordinátára érkezett. A térképváltás után a gyűrű példányazonosítója és darabszáma megegyezett az induláskor rögzített értékkel.
- A Windows-kiadás elindult, a névkereső a `fulbevalo` szövegre több fülbevalót adott vissza. A szerkesztővel elmentett típusos tárgygyűjtési cél, XP és Yang visszaolvasható volt. A csomagolt C++ fordító a forrásból betölthető csomagot készített. Az XP és Yang jóváírása ebben a szerkesztőpróbában nem szerepelt.
