# Auriga Qt6 szerverkezelő

A kezelő a Windowsos helyi szervercsomagot és az SSH-n elérhető FreeBSD 15.1
szervercsomagot ugyanazokon az oldalakon kezeli, külön célgépként.
Qt6 / PySide6 Widgets felületet használ. A megnyitása nem indít sem szervert,
sem SQL-t; az adatbázismentés az alkalmazás elkészítéséhez nem szükséges.

## Indítás

A kész csomagban: `manager/AurigaServerManager.exe`, vagy a gyökérben
található `Szerverkezelo.cmd`. A teljes `manager` mappát együtt kell tartani,
az EXE mellett levő `_internal` tartalmazza a Qt6 és Python függőségeit.
Külön Qt-/Python-telepítés nem kell az alkalmazás futtatásához.

Induláskor a kezelő a mellette található szervercsomagot választja; másik
csomag a **Helyi szervermappa…** gombbal nyitható meg. A választást és az
ablakméretet a felhasználó Qt-beállításaiban tárolja; jelszavakat nem tesz oda.

## Helyi és távoli célgép

Az ablak tetején a **Kezelt szerver** választó tartalmazza a **Helyi szerver**
lehetőséget és a mentett SSH-profilokat. A nagy **HELYI / TÁVOLI** jelzés,
a célgép címe és a szervermappa minden oldalon látható. A választás az egész
alkalmazásra érvényes:

- **Áttekintés:** a kiválasztott gép folyamatai, csatornái és portjai;
  fájlellenőrzés, előkészítés, SQL-ellenőrzés, indítás, leállítás és questfrissítés.
- **Beállítások:** a kiválasztott gép `server.json` fájlja, valamennyi közös,
  core-, hálózati, SQL-, TLS- és játékszabály-mezővel.
- **Játékadatok, Questek és Lua:** a kiválasztott csomag fájljai,
  azonos űrlapos és forráskódos szerkesztővel.
- **Naplók:** a kiválasztott gép naplólistája és a naplók frissülő vége;
  a konfiguráció ismert jelszavai és vezérlőtokenjei kitakarva jelennek meg.

A helyi és távoli fájlok külön maradnak: a távoli dokumentumot memóriában
szerkesztjük, nem jön létre belőle helyi szervercsomag. Mentése a célgépen
futó natív ellenőrzés után, ottani biztonsági másolattal és atomikus cserével
történik. Ha a fájl közben változott, az app elutasítja a felülírását.
Nem mentett szerkesztésnél célgépváltás előtt rákérdez az elvetésre;
folyamatban lévő mentés vagy szerverművelet alatt a váltás tiltott.

Kapcsolati hiba esetén a távoli állapot **Nincs kapcsolat**, és a szerveroldalak
mentési és folyamatkezelési műveletei letiltódnak. Az app nem használ helyette helyi adatot vagy helyi folyamatot.
Az **Újracsatlakozás** gombbal ugyanahhoz a kiválasztott célhoz kapcsolódik.
Induláskor a profilok beolvasása önmagában nem létesít SSH-kapcsolatot.

### Távoli csomag előkészítése

1. A **Kapcsolatok és eszközök** oldalon ments egy közvetlen SSH- vagy
   ProxyJump-profilt. A **Távoli szervermappa** a futtatásra összeállított
   csomagra mutasson, például `/usr/home/auriga/server`.
2. A célgépen a módosított forráshoz tartozó csomag legyen: `.auriga-package.json`,
   `server.json`, friss `manage.py` (`PACKAGE_CONTROL_VERSION = 2`), `run_server.py`,
   a `data`, `db`, `tls` és a célgépre lefordított `bin` állományok.
   Régebbi vezérlőszkript mellett a távoli kapcsolat célzott frissítési hibát ad;
   nem állítja leállítottnak az ellenőrizhetetlen folyamatokat.
3. Az SSH-felhasználó legyen a csomag és a felügyelő tulajdonosa.
   Python 3.11+ és az SQL-ellenőrzéshez MariaDB parancssori kliens szükséges.
   Qt és külön telepített kezelőügynök nem kell a FreeBSD gépre.
4. A felső választóban válaszd ki a mentett profilt. A kapcsolat felépülése
   után a távoli Áttekintés és a többi oldal használható.

Az SSH-kérések az ellenőrzött kapcsolat szabványos bemenetén küldik át a
kezelőkódot, új figyelőport nélkül. Az indítás külön munkamenetben futó
felügyelőt indít, fájlba irányított naplóval: az SSH vagy az app bezárása
nem állítja le a szervert. A felügyelő naplója a távoli
`run/manager-jobs/remote-start-*.log` fájlban követhető.

A FreeBSD gép rendszer-MariaDB-jét **SQL: külső szolgáltatás** jelzés mutatja.
A helyi Windowsos adatbázis-létrehozó és MariaDB-leállító gomb ilyenkor
nem jelenik meg. Az **SQL-kapcsolatok ellenőrzése** a `server.json` SQL-szerepeivel
csatlakozik, és csak olvasási előfeltétel-vizsgálatot végez; nem importál vagy
állít le rendszer-adatbázist. A jelszavakat védett, rövid életű opciófájlba írja,
a MariaDB [opciófájl-formátuma](https://mariadb.com/docs/server/server-management/install-and-upgrade-mariadb/configuring-mariadb/configuring-mariadb-with-option-files)
szerint; nem teszi őket a folyamat parancssorába.

A profilokat és alagutakat a **Kapcsolatok és eszközök** oldal kezeli.
A build és feltöltés ugyanazt a felső célgépválasztást követi; eltérő vagy
nem mentett kapcsolati útvonalnál az app előbb a megfelelő profil kiválasztását
és mentését kéri. A helyi szerver közben megmarad külön választható célként.

## Funkciók

- **Áttekintés:** Database, LoginServer és az összes game core állapota,
  csatornája, portja. Csatornaválasztás, előkészítés, fájlellenőrzés,
  függőségi indítás, szabályos leállítás és helyi MariaDB leállítása.
  Az **Adatbázis előkészítése** gomb új, üres csomagban a mellékelt
  `db/backup` mentést és a `tools/mariadb` futtatókörnyezetet használja.
  Ellenőrzi a teljes importot, és elutasítja meglévő adatok felülírását.
  Futó szervernél:
  - A **Kijelölt core-ok indítása** és a **Kijelölt core-ok leállítása**
    gomb a táblában kijelölt game core-okat külön indítja vagy állítja le.
    A leállítás megerősítést kér, mert a rajta lévő játékosok kilépnek.
  - A **Kijelölt csatornák indítása** a bejelölt csatornák core-jait
    indítja a futó szerverhez.
  - A Database és a LoginServer a teljes stackhez tartozik.
  - Ha a szervermappa `manage.py` fájlja régebbi, a gombok tiltottak, és
    a súgójuk a frissítését kéri.
    Ha a fájl már friss, de a felügyelő még a régi kóddal fut, a súgó egy
    teljes szerverleállítást és újraindítást kér. A futó felügyelő támogatása
    nélkül a kezelő nem küld külön core-vezérlési kérést.
- **Kapcsolatok és eszközök:** közvetlen SSH vagy ProxyJump, gépenként külön kulcsos vagy jelszavas belépés,
  kapcsolatellenőrzés, helyi porttovábbítás, távoli CMake-build és ellenőrzött
  fájl-/mappafeltöltés. A beállítások profilonként menthetők.
- **Beállítások:** szerverválasztó és külön kategóriák az általános,
  hálózati, SQL-, TLS- és játékszabály-beállításokhoz. A mezők űrlapon,
  kapcsolókkal és közvetlenül szerkeszthető értékekkel jelennek meg.
  A jelszavak és a szerverközi kulcs maszkoltak.
  Az öröklés a C++ szerver szabályát követi: közös, game-alapértékek,
  végül core-felülírások. Az örökölt értékek megjelennek, de csak tényleges
  szerkesztéskor kerülnek a core saját szekciójába. A visszaállító nyíl
  törli a saját felülírást. Ritkábban használt szerkezeti műveletekhez
  választható a **Haladó szerkezet** nézet.
- **Játékadatok:** a `data` JSON-állományai kereshető listában; az értékek
  táblázatos nézetben és JSON-szövegként szerkeszthetők. A fájlok mappák
  szerint csoportosulnak. Listasor másolható, hozzáadható, törölhető;
  a beágyazott csoportok dupla kattintással vagy a **Sor részletei** gombbal
  nyithatók meg. VNUM szerinti keresés az értékekre is működik.
- **Questek és Lua:** questforrások, Lua-könyvtárak és
  questmetadata. Az eredeti UTF-8/BOM/Latin1 kódolást és sortörést megőrzi.
  Az új szerver a questforrásokat közvetlenül tölti be; nem kell objecteket fordítani.
  A régi `data/quest/object` mappa nem jelenik meg a szerkesztőben, és tartalma
  közvetlen fájlmegnyitással sem módosítható. Sikeres migráció után a csomagból
  eltávolítható; a korábbi állapotazonosítók ellenőrzéséhez őrizd meg a mentését.
- **Naplók:** core-onkénti naplók és az ellenőrzések eredménye, frissülő,
  méretkorlátozott nézetben. Az ismert konfigurációs jelszavakat kitakarja.
- **Korábbi mentés:** ugyanahhoz a fájlhoz tartozó korábbi verzió betöltése
  a **Továbbiak** menüből; a visszaállítást a normál ellenőrzött Mentés végzi.

A mentés és a mentési állapot a szerkesztő rögzített felső sávjában látható.
A hosszú fájlnevek rövidítve jelennek meg, így nem tolják ki a mentés gombját
az ablakból. A műveleti napló alapból összecsukott, ellenőrzéskor és mentéskor
automatikusan kinyílik; kézzel a **Műveleti napló** gombbal nyitható/csukható.
Ctrl+S az éppen szerkesztett táblázatcellát is menti. Hibás számérték blokkolja
a mentést; az űrlap a javítandó bevitelt kategóriaváltáskor is megőrzi.

## Forráskódszerkesztő

A **Forráskód** nézetben JSON-, Lua- és quest-színezés, sorszámok,
aktuális sor- és zárójelpár-kiemelés, automatikus behúzás és kapcsolható
sortördelés segíti a szerkesztést. A kereső kis-/nagybetű és teljes szó
szerint is szűr, a találatokat kiemeli. A csere egyszerre egy találatra
vagy az összes találatra elvégezhető; az összes csere egyetlen lépésben
visszavonható. A keresés a megnyitott dokumentumban működik, egyszerű szövegre.

Lua- és quest-fájlban a `pc.`, `npc.`, `item.`, `d.` stb. után automatikus
függvénylista jelenik meg. További gépeléssel szűkíthető, a nyilakkal választhatsz,
Enterrel, Tabbal vagy kattintással beszúrhatod a kiválasztott nevet.
A **Kiegészítés** gomb vagy a **Ctrl+Space** kézzel is megnyitja a listát;
pont nélkül globális függvényeket, névtereket és kulcsszavakat ajánl fel.
Függvénynél zárójelpárt is beszúr, a kurzort középre helyezi; a már meglévő
nyitó zárójelet és argumentumokat megtartja. A beszúrás egy lépésben visszavonható.
Esc bezárja a listát. Kommentben, sztringben és JSON-ban nincs kiegészítés.

A beépített lista a szerver C++-os Lua-regisztrációiból és a használt Lua 5.5
könyvtárakból készül; helyi és távoli szerkesztésnél is, kapcsolat nélkül elérhető.
Az opcionális API-k is szerepelnek, tényleges elérhetőségük a célgép buildjétől
függ. A lista függvényneveket ad, az argumentumokat kézzel kell kitölteni;
nem helyettesíti a szintaxisellenőrzést. Tetszőleges saját Lua-könyvtárak
függvényeit jelenleg nem indexeli. API-változtatás után a fejlesztő futtassa a
`SRC/Tools/ServerManager/generate_quest_api.py` fájlt; a manager buildje ellenőrzi,
hogy a mellékelt katalógus friss-e.

| Billentyű | Művelet |
| --- | --- |
| Ctrl+F / Ctrl+H | Keresés / csere megnyitása |
| F3 / Shift+F3 | Következő / előző találat, körbefordulással |
| Ctrl+G | Ugrás sorszámra |
| Ctrl+Space | Lua-/quest-kiegészítések megnyitása |
| ↑ / ↓, Enter / Tab, Esc | A nyitott kiegészítési listában: választás, beszúrás, bezárás |
| Tab / Shift+Tab | Behúzás / behúzás visszavétele; kijelölt sorokon is |
| Ctrl+/ | Kijelölt Lua-/quest-sorok kommentelése / visszaállítása |
| Ctrl+Shift+F | Érvényes JSON formázása, a típusok megőrzésével |
| F7 | Az aktuális szöveg szintaxisának ellenőrzése |
| Ctrl+Z / Ctrl+Y | Visszavonás / ismétlés |

Gépelés után rövid késleltetéssel automatikus ellenőrzés indul. Az alsó
hibalistában a sorra kattintva a hibához ugorhatsz; a hibás sor száma és
szövege is jelölést kap. Egyes szerkezeti hibákhoz a parser nem ad pozíciót;
ezek szövegesen jelennek meg. 1 MiB felett az ellenőrzés kézzel, F7-tel indul.
A kódolási hibák, az ismétlődő JSON-kulcsok és a végtelenre túlcsorduló
számok szintén hibának számítanak. Hibás játékadat-JSON is megnyitható
javításra; mentéshez már érvényes tartalom szükséges.

A Lua- és questellenőrző a szerver Lua 5.5 környezetét használja.
A ServerPackageCheck `--check-lua` és `--check-quest` módja vizsgálja a fájlokat. A vizsgálat ideiglenes másolaton fut,
nem hajtja végre a kódot, nem készít aktív quest objecteket, nem indít szervert
és nem módosítja az eredeti fájlt. A legújabb szöveg ellenőrzési eredménye
jelenik meg; korábbi ellenőrzést új szerkesztés megszakít. Hiányzó ellenőrző,
indítási hiba vagy időtúllépés nem jelenik meg sikeres ellenőrzésként.
A feldolgozó a forrás sorszámával jelzi a szintaxishibát.

A szintaxisellenőrzés a helyes játékmenetet, a VNUM-ok létezését és a
futáskor elérhető függvényeket nem igazolja. Az aktív `quest_list` forrásainak
telepítése után biztonságos újratöltés vagy összehangolt szerverújraindítás szükséges. A központi konfiguráció
titkai az űrlapnézetben maszkolva maradnak; annak nyers forrásfüle rejtett.

Az indítást és leállítást a csomag meglévő `manage.py` felügyelője végzi.
A felügyelt szerver az ablak bezárása után is fut; a kezelő újra megnyitható
és a **Szerver leállítása** gombbal leállítható. A GUI nem öl folyamatot pusztán
PID alapján. A rövid ellenőrzési/mentési műveletek befejezését bezáráskor megvárja.

## Mentési ellenőrzések

A hotreloadot támogató szerver mellett a questforrások, a Lua-könyvtárak és
a questmetadata futás közben is menthetők. A mentés után az Áttekintés
**Questek frissítése** gombja aktiválja az új csomagot az összes futó core-on.
SSH-nál az Kapcsolatok és eszközök **Távoli questek → Frissítés játék közben** művelete ugyanezt végzi
a profil szervermappájában, közvetlen és ProxyJump kapcsolattal is.
A működés, a régi párbeszédek/időzítők kezelése és a félbeszakadt frissítés
folytatása a [Lua- és hotreload-leírásban](lua55_quest_migration.md) szerepel.

A többi játékadat, a `settings.lua` és `restart_city_pos.lua` mentéséhez leállítás
kell. A központi konfiguráció cseréjéhez a helyi MariaDB sem futhat.
Leállított szervernél a felügyelőzár, élő questmentésnél a közös questfrissítési
zár védi a fájlcserét. Korábbi szerverbinárisok mellett az élő mentés tiltott.
A leállított szerver **Questek előkészítése induláshoz** gombjával a közös
Lua-beállításokkal együtt kijelölhető a mentett csomag a következő induláshoz.

1. A szerkesztő megőrzi a megnyitáskori SHA256-ot; külső módosítást nem ír felül.
2. A JSON-szintaxis, az ismétlődő kulcsok és a nem véges számok ellenőrzöttek.
3. `server.json` esetén az összes példány natív `--check-config` ellenőrzése
   lefut, SQL-kapcsolat és szolgáltatásindítás nélkül. A futási mappáknak a csomag
   `run` mappájában kell maradniuk.
4. Játékadatoknál a natív JSON/Lua checker külön ideiglenes adatgyökéren
   ellenőrzi a jelölt fájlt. A két questmetadata a típusos questolvasón is átmegy.
   Ez JSON-/Lua-szintaxis-ellenőrzés; minden játékszabály és VNUM-kapcsolat
   helyességét nem helyettesíti.
5. Siker esetén az eredeti bájtok a `run/editor-backups/<id>/<fájl>` alá
   kerülnek, majd atomikus fájlcsere történik. Hibánál az eredeti megmarad.

A naplók a `run/manager-jobs`, a rövid életű szerkesztési munkafájlok a
`run/editor-pending` mappába kerülnek. A készítési manifest az eredeti kiadás
lenyomata marad; a szerkesztő nem hitelesíti újra automatikusan a megváltozott
csomagot. A runtime-fájlok nem részei a kiadási ZIP-nek.

## Adatbázis és további bővítés

A jelenlegi felület fájlszerkesztő és folyamatkezelő. Az SQL-ben tárolt
item/mob/skill/shop prototípusok és játékosadatok grafikus szerkesztése a
mentés illesztése utáni külön bővítés. A helyi Indítás gomb saját SQL-adatkönyvtár nélkül inaktív. FreeBSD-n
a konfigurált külső SQL-kapcsolatok ellenőrzése előzi meg az indítást.

A teljes játék, Unity-belépés és a valódi SQL-adatok együttműködése a későbbi
localhost integrációs próbával ellenőrizhető. A közvetlen questbetöltés és
a régi csomagok átalakítása a `lua55_quest_migration.md` leírását követi.

## Éles SSH-kapcsolat és alagutak

Két kapcsolódási mód választható:

- **SSH-ugrógépen keresztül (ProxyJump):** Windows gép → SSH-ugrógép → FreeBSD 15.1.
- **Közvetlen SSH (ugrógép nélkül):** Windows gép → a FreeBSD gép elérhető SSH-portja.
  Elegendő a célgép címe, portja, felhasználója és a választott belépés adatai; az ugrógép mezői
  eltűnnek, adatait nem használjuk. A porttovábbítás ebben a módban is opcionális.

Az alapértelmezett mód a ProxyJump.
A célgép közvetlen nyilvános SSH-elérése nem szükséges. Az ugrógépnek a
célgép SSH-portját el kell érnie, és engedélyeznie kell a TCP-továbbítást.
Az app az operációs rendszer `ssh` és fájlátvitelhez `sftp` kliensét
használja. Windows alatt elsőként a `System32/OpenSSH` telepítést keresi.

A **Kapcsolatok és eszközök** oldalon add meg a FreeBSD célgép belső címét, SSH-portját
és felhasználóját, majd az ugrógép elérhetőségét. Mindkét géphez külön
**Belépés módja** választható: **SSH-kulcs / ssh-agent** vagy **Jelszó**.
Kulcsos módban külön privátkulcsfájl választható. Üres kulcsmező esetén az OpenSSH alapértelmezett
kulcsai és az `ssh-agent` használhatók. Titkosított kulcsot előzetesen tölts
az agentbe; a háttérben futó kliens nem nyit jelszóbekérő ablakot.

Jelszavas tesztszervernél válaszd a **Jelszó** módot, írd be a **Jelszó**
mezőt, majd kattints a **Profil mentése** gombra. SSH-kulcsot ilyenkor nem kell
megadni. A privátkulcs mezője ilyenkor letiltott és rejtett, esetleges korábbi
piszkozata nem kerül bele a mentett profilba és nem blokkolhatja a mentést.
A kézi szerverazonosítási mezők szintén el vannak rejtve, ha mindegyik használt
gép jelszavas. Az app a korábban beállított szerverazonosítást használja;
ennek védelme jelszavas belépésnél is megmarad. A **Jelszó** mező a megadott
szerverfelhasználó belépési jelszava. Ugrógépnél a két belépés külön állítható, kulcsos és jelszavas mód
keverhető. Ezután a kapcsolatellenőrzés, az Áttekintés, a fájlszerkesztés,
a távoli build, az SFTP-feltöltés és a porttovábbítás ugyanezt használja.

A **Jelszó megjegyzése** jelölőnégyzet a célgéphez és az ugrógéphez külön
állítható. Bejelölés és **Profil mentése** után a Windows Hitelesítőadat-kezelője
tárolja a jelszót az aktuális Windows-felhasználóhoz és géphez kötve;
következő indításkor automatikusan visszatöltődik. A pipa kivétele és a profil
mentése törli a mentett jelszót; a profil törlése szintén eltávolítja a hozzá
tartozó két bejegyzést. Cím, port vagy felhasználó módosításakor a korábbi
végpont tárolt jelszava kikerül. Sikertelen profilmentéskor a jelszótár
korábbi állapotát visszaállítjuk.

Jelölés nélkül a jelszó az app futásának idejére, memóriában marad meg;
újraindítás után újra meg kell adni. A profilfájl, az SSH-konfiguráció, a parancssor és a
gyermekfolyamatok környezete nem tartalmazza a jelszót. Az OpenSSH saját
[SSH_ASKPASS](https://man.openbsd.org/ssh.1#SSH_ASKPASS) mechanizmusa kapja meg
egy rövid életű, tokennel védett helyi csatornán; a távoli parancs bemenete
és az SFTP batch-ellenőrzése megmarad. A **Jelszó** mód az SSH `password`
és a PAM/`keyboard-interactive` szokásos jelszóbekérését is támogatja.
Nem kell külön kapcsolót átállítani: ha a szerver csak az utóbbit kínálja,
ugyanazt a mentett jelszót használja. Ez a buildre, SFTP-re, alagútra és
ProxyJump esetén mindkét gépre érvényes. Az OpenSSH által a konfigurált
`felhasználó@gép` előtaggal ellátott `Password:` és a FreeBSD-féle
`Password for felhasználó@szervernév:` kérdést ismeri fel.
OTP-kódra, új jelszóra vagy más kérdésre nem küldi el a belépési jelszót;
többfaktoros belépés és jelszócsere itt nem támogatott.
Az eltárolt jelszót gépenként és SSH-kapcsolódásonként legfeljebb egyszer
adja át, akkor is, ha a két jelszavas módszer közötti váltás történik.
Egy következő fájlművelet új kapcsolódása ismét használhatja a jelszót.
A szerveren a kiválasztott felhasználó jelszavas/PAM belépésének
engedélyezettnek kell lennie. A kliens a root belépés szerveroldali tiltását
nem tudja feloldani.
A hostkulcs ellenőrzése jelszavas módban is kötelező.
A védett tárolás a Windows
[Credential Manager](https://learn.microsoft.com/en-us/windows/win32/api/wincred/nf-wincred-credwritew)
API-ját használja; külön jelszófájl és új függőség nem szükséges.

A szerver első elérésekor az app megjeleníti a címét, portját és a bemutatott
hostkulcs SHA-256 ujjlenyomatát. Ez a szerver azonosítója; jelszavas belépéshez
nem kell saját SSH-kulcsot készíteni. Ellenőrizd az ujjlenyomatot a szerver
konzolján vagy üzemeltetőjénél, majd kattints a **Szerver jóváhagyása** gombra.
A **Mégse** az alapértelmezett; elutasításkor nincs belépés vagy kulcsmentés.
A jóváhagyott kulcsot az OpenSSH a profil **known_hosts** fájljába menti;
ha a fájl hiányzik, az app létrehozza. Következő alkalommal ugyanazt a kulcsot
már nem kell jóváhagyni. Megváltozott ismert kulcsnál a kapcsolat továbbra is
leáll, az app nem ajánlja fel a korábbi azonosító felülírását.

Ez a kapcsolatellenőrzésnél, az Áttekintésnél, a buildnél, a feltöltésnél és
az alagútnál is működik. ProxyJump esetén külön ellenőrizhető az ugrógép és
a célgép. A jóváhagyásra két perc van; közben a művelet rövid kapcsolódási
időkorlátja szünetel. A profil mentése továbbra sem létesít kapcsolatot.
Meglévő, ellenőrzött **known_hosts** fájl is használható.
Az app az OpenSSH
[StrictHostKeyChecking ask](https://man.openbsd.org/ssh_config#StrictHostKeyChecking)
módját használja; nem fogad el automatikusan új kulcsot.

Forrásból futtatott Windowsos fejlesztői példánynál a többsoros kérdést
egy helyben fordított .NET Framework segéd továbbítja. A becsomagolt EXE
saját magát használja erre, így ehhez nincs külön .NET-fordítófüggősége.

Példa a felhasználói `.ssh/config` fájlhoz, kizárólag helykitöltő címekkel:

```sshconfig
Host auriga-jump
    HostName jump.example.invalid
    Port 22
    User tunnel_user
    IdentityFile ~/.ssh/jump_key
    IdentitiesOnly yes

Host auriga-live
    HostName 10.0.0.10
    Port 22
    User auriga
    IdentityFile ~/.ssh/live_key
    IdentitiesOnly yes
    ProxyJump auriga-jump
```

Az `ssh auriga-jump true`, majd `ssh auriga-live true` parancsokkal ellenőrizhető
a két belépés. Az appban ugyanezeket a tényleges címeket, portokat és kulcsokat
add meg. A háttérben saját, ideiglenes OpenSSH-konfiguráció készül, így a
felhasználói konfiguráció egyéb proxyi és továbbításai nem kerülnek bele.
A hostkulcs-ellenőrzés mindkét lépcsőn kötelező; hibánál nincs közvetlen
kapcsolódási kísérlet vagy automatikus hostkulcs-elfogadás.

A **Kapcsolat ellenőrzése** csak a rendszer nevét/verzióját, a bejelentkezett
felhasználót, a Python 3 elérhetőségét és az opcionálisan megadott szervermappa
létezését olvassa. A csomagjelölők megtalálása önmagában nem működési teszt.
Hiányzó könyvtárat nem hoz létre, nem telepít és nem indít szolgáltatást.

A **Porttovábbítások** fülön például `13307 → 127.0.0.1:3306` adható meg.
Ez a helyi gép `127.0.0.1:13307` címét a FreeBSD célgép saját
`127.0.0.1:3306` szolgáltatásához vezeti, ugyanazon ProxyJump-útvonalon.
A helyi figyelés mindig `127.0.0.1`; a távoli célhoz IPv6 is megadható.
Az indulási ellenőrzés a helyi figyelőportokat vizsgálja, a céloldali SQL/játék
szolgáltatás működését külön kell tesztelni. Az alagút az app bezárásakor
megszűnik; ez nem állítja le a távoli szerverfolyamatokat.

A profilok a felhasználó helyi konfigurációs könyvtárának
`Auriga/ServerManager/ssh-profiles.json` fájljában maradnak, a telepíthető
szervercsomagon és a `server.json` fájlon kívül. Privátkulcsot és jelszót az
app nem másol a profilba. A profilok beolvasása nem létesít kapcsolatot.
Az aktív célgép kapcsolati adatainak módosítása után újra kell kapcsolódni
a felső **Újracsatlakozás** gombbal. A profil mentése önmagában nem próbál
belépni, és nem ellenőrzi a helyi kulcsfájlok meglétét; nem mentett
szerverfájlnál előbb jóvá kell hagyni a célváltást.
Több megnyitott ablak ütköző mentését zárolás és tartalomellenőrzés akadályozza.

A közvetlen mód nyilvános SSH-porthoz és már meglévő alagút végpontjához is használható; utóbbinál az
opcionális **Hostkulcs neve** a valódi célgép ellenőrzött hostkulcsához
rendelheti például a `127.0.0.1:2222` végpontot. Ez külön, kifejezetten
kiválasztott profilbeállítás, nem automatikus kerülőút.

A távoli folyamatkezelés az **Áttekintés** oldalon használható, az előkészített
JSON-csomag saját felügyelőjével. FreeBSD-n a folyamat azonosításához a PID,
a kezdési idő és a futtatható állomány együtt szükséges; nem igényel procfs-t.
Azonos forrásból készült FreeBSD-binárisok és friss vezérlőszkriptek kellenek.
FreeBSD-n az SQL külső szolgáltatás: a kezelő a `server.json` tényleges SQL-kapcsolatait
ellenőrzi, a közös beállítások és a Database/LoginServer/game core felülírásainak
összefésülésével. A célgépen található `mariadb` vagy `mysql` kliens adatbázisonként
`SELECT 1` lekérdezést futtat. A kártya az elérhető, részben elérhető, hibás és
nem ellenőrizhető kapcsolatokat külön jelzi; a részletekben a cím, port és adatbázis
látható. A jelszó privát ideiglenes opciófájlba kerül, a naplóba és a cache-be nem.
A `run/sql-status.json` privát cache legfeljebb 30 másodperces; az **Állapot frissítése**
és az újracsatlakozás azonnal újra ellenőriz, konfigurációváltozásnál a cache érvénytelen.
A **SQL-kapcsolatok ellenőrzése** a teljes indulási referenciaadat-ellenőrzést végzi.
A külső SQL-t az app nem inicializálja, nem indítja és nem állítja le; a helyi
MariaDB-gombok távoli FreeBSD-n rejtettek. A Windows-csomagból átmásolt
`run/mariadb/owner.json` nem teszi helyivé a FreeBSD-s adatbázist.
Külső rc.d/screen/tmux indítást a kezelő nem vesz át automatikusan:
annak átállítása és a teljes kiadás élesítése külön telepítési feladat.
A fájlfeltöltés már listát készít, ideiglenes helyre tölt, ellenőriz és ment
(részletek alább). Ez nem teljes kiadások tranzakciós telepítője: a fájlokat
egyenként cseréli, szolgáltatást nem állít le és nem indít újra.
SQL-portot külön nyilvánosan megnyitni
ehhez nem szükséges. A Unity játékforgalmának elérése külön hálózati kérdés;
az adminisztrációs SSH-útvonal önmagában azt nem oldja meg.

Hivatalos leírás: [OpenSSH konfiguráció](https://man.openbsd.org/ssh_config),
különösen `ProxyJump`, `HostKeyAlias` és `ExitOnForwardFailure`.

## Távoli build

A **Kapcsolatok és eszközök → Távoli build** fül a kiválasztott SSH-profil célgépén
futtatja a konfigurálást és a fordítást. Közvetlen és ProxyJump módban ugyanazt
a hitelesítést használja. Futó helyi portalagút mellett is indítható.

1. Add meg a **távoli forrásmappát**, amelyben a projekt gyökér `CMakeLists.txt`
   fájlja található (például `/usr/home/auriga/source`). A forrás a Feltöltés
   fülön, a **Csak a mappa tartalmát** kapcsolóval is felmásolható.
2. Adj meg külön **buildmappát**, például `/usr/home/auriga/build-release`.
3. Válassz konfigurációt, `Ninja` vagy `Unix Makefiles` generátort és a géphez
   illő párhuzamos feladatszámot. Alapértelmezett célok:
   `GameServer Database LoginServer ServerPackageCheck`.
4. A **Csak konfigurálás**, illetve **Konfigurálás és build** gomb indítja a
   műveletet. Előtte a tényleges CMake-parancsok láthatók a fülön.

A további beállítások `-DNÉV=ÉRTÉK` formában adhatók meg, szóközös értékek
idézőjelezhetők. FreeBSD-n az alapérték `-DAURIGA_USE_VCPKG=OFF`.
A konfigurálás és fordítás nem futtat `git pull` műveletet és nem indítja újra
a szolgáltatásokat. A kész kimenet a megadott buildmappában marad; a telepítés
külön gombbal kérhető.

A célgépen előzetesen szükséges Python 3, CMake, a választott buildeszköz,
C++23 fordító és a projekt függőségei. A függőséglista a
`cmake/AurigaSystemDeps.cmake` fájl elején található; az app nem telepít
automatikusan rendszer-csomagokat és nem kér sudo-jogot.

A build előtt a worker ellenőrzi a CMake és a választott generátor
buildeszközének elérhetőségét. FreeBSD-n a folyamat PATH-ját kiegészíti a
`/usr/local/bin`, `/usr/bin`, `/bin` könyvtárakkal, az eredeti sorrendet megtartva.
Hiányzó Ninja esetén már a konfigurálás előtt jelzi a teendőt; rootként:

```sh
pkg install ninja
```

A [Ninja generátor](https://cmake.org/cmake/help/latest/generator/Ninja.html)
a külön telepített Ninja programot használja. A FreeBSD-csomagkezelésről
a [kézikönyv](https://docs.freebsd.org/en/books/handbook/ports/) ír részletesen.
A worker a megtalált program teljes útját átadja a CMake-nek, így egy korábbi
`CMAKE_MAKE_PROGRAM-NOTFOUND` bejegyzés után is újrapróbálható ugyanaz a build.
Kifejezetten megadott `-DCMAKE_MAKE_PROGRAM=...` esetén azt használja, hiányánál
nem választ helyette mást. Ha a buildmappában már másik generátor cache-e van,
új buildmappát kér; a régit nem törli. A fordító és a projekt könyvtári
függőségeinek tényleges ellenőrzését továbbra is a CMake végzi.
A worker konkrét hibaüzenete a napló fölött is megjelenik.

A kimenet folyamatosan látható, és helyi naplóba is kerül a felhasználói
alkalmazásadatok `remote-jobs` könyvtárában. A távoli build naplója és eredménye:
`<buildmappa>/.auriga-manager/jobs/<azonosító>/{build.log,result.json}`.
Sikertelen konfigurálás után a fordítás nem indul el. A sikerhez a távoli
worker sikerjelzése és nulla SSH-hibakód egyaránt szükséges.

A **Művelet megszakítása** külön SSH-kéréssel leállítást kér a futó buildtől;
a worker a hozzá tartozó build-folyamatcsoportot állítja le. Kapcsolatszakadás
után ellenőrizd a távoli eredményfájlt; a kapcsolat eltűnése önmagában nem
bizonyítja a build leállását. Az app saját futó művelete alatt a bezárás
blokkolt, így előbb megszakítás vagy befejezés szükséges.

### Build telepítése

1. Csatlakozz a felső szerverválasztóban a kívánt távoli profilhoz.
2. Fordítsd le mind a négy célt: `GameServer Database LoginServer ServerPackageCheck`.
3. Az **Áttekintés** oldalon állítsd le a játék- és adatbázis-szerverprogramokat.
   A MariaDB SQL-szolgáltatást ehhez nem kell leállítani.
4. A **Távoli build** fülön ellenőrizd a buildmappát, a konfigurációt és a
   **Telepítés helye** sort, majd kattints a **Build telepítése** gombra.
5. Siker után az **Áttekintés → Indítás** a profil szervermappájának `bin`
   könyvtárából indítja az új programokat.

A telepítő a CMake szokásos `SRC/Server/<komponens>/<program>` kimeneteit keresi
(több konfiguráció esetén a kiválasztott konfiguráció almappáját is kezeli).
A build- és a szervermappa különálló könyvtár legyen. Csak a célgép
architektúrájához illő 64 bites ELF-programokat fogad el; Windows `.exe` nem
telepíthető FreeBSD-re. Eltérő CMake buildkonfiguráció, hiányzó vagy többértelmű
kimenet esetén a célfájlokat nem módosítja.

A másolást a buildmappa, a szervermappa és a felügyelő zárolása védi. A worker
a felügyelő állapota mellett a ténylegesen futó programokat és a konfigurált
szerverportokat is ellenőrzi. FreeBSD-n ehhez a rendszer `procstat` programját
használja; a saját csomaghoz futtatási és fájlírási jogosultság szükséges.
Az előkészített programokon konfiguráció- és Lua-ellenőrzés fut, majd SHA-256
ellenőrzéssel és `0755` futtatási joggal kerülnek a `bin` mappába. Telepítés után
az ellenőrzések újra lefutnak. Konfiguráció, játéktartalom, TLS- és SQL-adat
nem része ennek a műveletnek.

A mentés és az eredmény helye:
`<szervermappa>/run/deployments/<azonosító>/{backup,result.json}`.
Kezelt hiba vagy megszakítás esetén visszaállítja a lecserélt fájlokat.
Az SSH-kapcsolat elvesztése után az eredményfájlt kell ellenőrizni;
`success` jelzi a befejezést. Áramkimaradás vagy kényszerített folyamatkilövés
esetén négy külön fájl cseréje nem atomi: `installing` vagy `rollback-failed`
állapotnál az app tiltja az újabb telepítést és az indítást. Ilyenkor előbb
a `backup` fájljaiból és a naplóban rögzített ellenőrzőösszegekből helyre kell
állítani a teljes készletet, majd rendezni az eredményfájlt. Kézi parancssoros
indítás előtt is ellenőrizd ezt az állapotot. Telepítés közben a megszakítás
gomb és az app bezárása tiltott; az indítás mindig külön művelet.

## Fájlok és mappák feltöltése

A **Kapcsolatok és eszközök → Feltöltés** fülön:

1. Add meg a távoli célmappát, majd válassz több fájlt vagy egy mappát.
2. Mappánál eldönthető, hogy maga a mappa vagy csak a tartalma kerüljön a célba.
   A kizárt nevek/minták `;` jellel választhatók el. Alapból kimarad például
   `.git`, `.plastic`, `build`, `Library`, `Temp`, `.venv` és `__pycache__`.
3. **Eltérések keresése:** a háttérben helyi pillanatkép készül, majd az app
   SSH-n összehasonlítja a távoli cél tartalmával. SHA-256 alapján csak az
   **Új**, **Módosított**, **Törölt / csak távolin**, illetve **Új mappa** sorok
   látszanak. A változatlan fájlokat nem listázza és nem tölti fel; önmagában
   az eltérő dátum sem jelent módosítást. Az összesítő jelzi a kihagyott fájlok
   számát. Az ellenőrzés a célon nem hoz létre fájlt vagy mappát.
   Az eredeti fájl későbbi szerkesztése nem változtatja meg a jóváhagyott bájtokat.
4. Meglévő fájlok cseréjéhez jelöld be a **Meglévő fájlok felülírása** kapcsolót,
   majd kattints a **Feltöltés indítása** gombra. Az összehasonlítás nem a Git
   állapotát, hanem a helyi és távoli fájlok tényleges tartalmát vizsgálja.
5. A **„Törölt / csak távolin” fájlok eltávolítása is** kapcsoló külön engedélyezi
   a listázott, csak a távoli célon lévő fájlok törlését. Alapból kikapcsolt.
   Törléskor a fájl a művelet saját távoli `backup` mappájába kerül.

A törlési lista kizárólag a kijelölt mappára vonatkozik: például `Server`
kiválasztásakor a cél `Server/` részfáját vizsgálja, testvérmappákat nem.
A **Csak a mappa tartalmát** módban a teljes célmappa a kijelölt hatókör.
Egyenként kiválasztott fájloknál a szomszédos fájlok nem törlési jelöltek.
A kizárások a helyi és távoli oldalon is érvényesek; a `.auriga-manager`
metaadatait mindenhol kihagyja. A csak távolin lévő fájl lehet szándékosan
ott tartott kiegészítés is: a kapcsolót csak a megjelenített lista áttekintése
után engedélyezd. Üres helyi mappa is összehasonlítható tartalom módban.

Fájl/mappa típusütközés **Ütközés** sorral jelenik meg, és blokkolja a műveletet.
Ha nincs eltérés, vagy csak nem engedélyezett törlés szerepel a listában,
a feltöltési gomb inaktív. A célmappa, forráskiválasztás, kizárások vagy
SSH-profil módosítása új összehasonlítást igényel.

Az app SFTP-vel egyedi `<cél>/.auriga-manager/uploads/<azonosító>/files`
területre tölt. Az összes fájl ellenőrzőösszegét és az ütközéseket a távoli
worker még az eredetik módosítása előtt ellenőrzi. Az összehasonlításkori célhoz
képest megváltozott, eltűnt vagy közben létrejött fájlnál új listát kér, az
aktuális változatot nem írja felül és nem törli. Felülírásnál a régi bájtok
és fájljogok ugyanott a `backup` mappába kerülnek; a `request.json` rendeli
egymáshoz az útvonalakat és a számozott mentéseket. Ezután fájlonként atomikus
csere történik. Kezelt telepítési hiba esetén a már elvégzett cseréket visszaállítja.

A kiválasztáson és a jóváhagyott törlési listán kívüli fájlokat megtartja.
A hiányzó üres mappákat is létrehozza, a kiürült távoli mappákat megtartja. Linkelt
célútvonalakat és kiválasztott symlink/junction forrásokat elutasít.
Meglévő fájl Unix-jogai megmaradnak; az új fájlok `0644` jogosultságot kapnak.
Új futtatható szkriptnél külön kell a megfelelő Unix-jogot beállítani.
A helyi pillanatkép átmenetileg a feltöltött fájlok összméretének megfelelő
szabad helyet igényel.

Fájlcsere közben a megszakítás és az app bezárása blokkolt. Gép- vagy
kapcsolathiba esetén ellenőrizd a távoli `result.json`-t és a mentést;
egy teljes mappafa cseréje nem egyetlen atomikus tranzakció. A mentési és
sikertelen staging mappák megmaradnak, az app nem törli őket automatikusan.
Konfiguráció, TLS-kulcs vagy futó szerver binárisának cseréjét csak tudatosan,
a szerver megfelelő karbantartási állapotában indítsd.

Parancshasználat: [SFTP](https://man.openbsd.org/sftp),
[CMake konfigurálás és build](https://cmake.org/cmake/help/latest/manual/cmake.1.html).

## Távoli FreeBSD-csomagok

A felső célgépválasztóban válaszd a távoli profilt, majd az **SSH-profilok és
távoli eszközök → Csomagkezelő** fület. Ugyanazt a jelszavas vagy kulcsos,
közvetlen vagy ProxyJump-kapcsolatot használja, mint a build és a feltöltés.
A szervercsomagnak nem kell futnia, a célgépen Python 3 és telepített `pkg`
szükséges. A keresés és a telepített lista nem módosít csomagokat.

1. Keress névrészletre (például `ninja`), vagy kérd le a **Telepített csomagok** listáját.
2. Jelöld ki a kívánt sorokat, majd **Kijelöltek hozzáadása**. Dupla kattintás is használható.
   A pontos neveket kézzel, szóközzel elválasztva is megadhatod.
3. **Telepítési terv**: a `pkg install -U -n` megmutatja a függőségeket, az esetleges
   frissítéseket, eltávolításokat és a letöltendő mennyiséget. Még nem telepít.
4. **Terv szerinti telepítés**: újraellenőrzi a tervet és a telepített csomagok
   állapotát, majd telepít. Változás esetén új tervet kér. Sikerhez a `pkg` nulla
   kilépési kódja és a kért csomagok meglétének ellenőrzése is szükséges.

A **Katalógus frissítése** csak a csomagjegyzéket frissíti (`pkg update`);
ezután új terv szükséges. Az app nem futtat teljes rendszerfrissítést.
A listák legfeljebb 500 sort mutatnak. Ha nincs találat, próbálj pontosabb
nevet vagy frissítsd a katalógust. A hiányzó `pkg` első telepítését az app
nem végzi el automatikusan; ehhez a szerveren `/usr/sbin/pkg bootstrap` kell.

A telepítés és katalógusfrissítés root SSH-felhasználót igényel. Ezek közben
a megszakítás és az app bezárása blokkolt. Kapcsolatszakadás után a megkezdett
csomagtranzakció tovább futhat: az eredményt a célgépen a
`/var/log/auriga-manager/packages/<feladatazonosító>.log` fájlban ellenőrizd,
majd kérd le ismét a telepített listát. Az app által indított párhuzamos
tranzakciókat a `/var/run/auriga-manager-pkg.lock` zár akadályozza meg;
beragadt zárat csak a hozzá tartozó folyamat és napló ellenőrzése után távolíts el.

Parancsok: [FreeBSD pkg install](https://man.freebsd.org/cgi/man.cgi?query=pkg-install&sektion=8),
[pkg rquery](https://man.freebsd.org/cgi/man.cgi?query=pkg-rquery&sektion=8).

## Összeomlásfigyelő és core-elemző

Az **Összeomlások** oldal a felső választóban kijelölt helyi vagy távoli
szerverhez tartozik. A friss `manage.py` felügyelője az indulás közben és
futás közben észlelt váratlan kilépést is rögzíti, az alkalmazás bezárása után
is. A kézi, szabályos leállítás nem crash. Az eseményben időpont, példánynév,
PID, kilépési kód/jel, indulási vagy futási állapot és binárisellenőrzőösszeg
szerepel. Új eseménynél az appban piros jelzés jelenik meg; a **Jelzés nyugtázása**
csak a jelzést rejti el, a mentést megtartja. Az ellenőrzés a meglévő
állapotlekéréshez kapcsolódik, távolról néhány másodperces késéssel jelezhet.

Mentési hely: `run/crashes/<esemény>/event.json`, az utolsó 160 kB-os
naplórészletek és az ugyanebben a futásban létrejött core fájlok. Az azonos
binárisok közös példánya a `run/crashes/binaries/<sha256>/` alá kerül;
Windowsnál az elérhető PDB-t is megőrzi. Az app ezeket nem törli automatikusan.
Az eseményrögzítés a megmaradt saját folyamatok szabályos leállítása előtt történik.
Váratlan, nulla kilépési kód is esemény, de ez önmagában nem bizonyít memóriakezelési hibát.

A listából válassz eseményt vagy core fájlt, majd **Core elemzése**.
Az eseménynapló nélküli, megtalált core fájlok is külön sorban jelennek meg
a táblázatban. Ezeknél az időpont a fájl módosításának ideje, a PID és a
kilépési jel ismeretlen. A sor kiválasztása kitölti az elemzés célfájlját.
Az archivált programot automatikusan felajánlja, a rögzített SHA-256 eltérését
elutasítja. Régebbi vagy kézzel megadott core mellett válaszd ki az ugyanabból
a buildből származó programot. Külső könyvtárban, például `/var/coredumps` alatt
levő fájlnál teljes célgépi útvonal írható be. Az automatikus keresés a csomag
gyökerére és `run` mappájára korlátozott, legfeljebb 500 core-t és 200 eseményt mutat.

FreeBSD/Linux ELF core esetén GDB-t, ennek hiányában LLDB-t használ. Az elemzés
a célgépen fut, nem tölti le a teljes memóriaképet Windowsra. A debugger
indulási és automatikusan betöltődő szkriptjei letiltottak, az app nem indítja
és nem folytatja a vizsgált programot. A jelentés az összes szál korlátozott
mélységű hibavermét és moduladatait mutatja; másolható, illetve helyi szövegfájlba
menthető. Az időkorlát 90 másodperc, a kimenet legfeljebb 512 kB.
Windows minidumphoz a célgépen elérhető CDB és megfelelő PDB szükséges.
Windows dumpok létrehozásához külön WER/ProcDump beállítás kell; ezt az app nem
módosítja automatikusan. Az ismert konfigurációs jelszavakat a jelentés kitakarja,
de a memória más érzékeny adatot is tartalmazhat: megosztás előtt ellenőrizd.

Fájlnevekhez és sorszámokhoz **Debug** vagy **RelWithDebInfo** build szükséges,
a binárisokat ne stripeld. A FreeBSD felügyelő csak saját, gyerekfolyamatokra
öröklődő core-méretkorlátját emeli a megengedett kemény korlátig; globális
`kern.corefile`/`kern.coredump` beállítást nem változtat. A tiltott dump, megtelt
lemez vagy máshová irányított core miatt egy eseményhez nem feltétlenül tartozik
core fájl. Az utóbbi kézzel is megnyitható. A felügyelő saját, erőszakos
megszüntetését nem tudja utólag saját kilépési kóddal rögzíteni.

A friss `manage.py`-nak a helyi és távoli szervercsomagban is szerepelnie kell.
A már futó régi felügyelő a következő szabályos szerverindítástól kapja meg a
figyelést; a kezelő ennek állapotát kiírja. Az új funkció telepítése nem indítja
újra a futó szervert.

Hivatkozások: [GDB automatikus szkriptbetöltés](https://sourceware.org/gdb/current/onlinedocs/gdb.html/Auto_002dloading.html),
[LLDB batch/core](https://lldb.llvm.org/man/lldb.html).

## Mentési központ

Az új történetalapú Mission Engine külön `mission` adatbázisa a Beállítások
oldalon **SQL · Küldetések** néven szerkeszthető. Az SQL-kapcsolatellenőrzés
és a mentési központ ezt az ötödik adatkört is figyelembe veszi. A player és
mission közös SQL-mentésben szerepel; a korábbi, csak négy adatbázisos SQL-
mentés részleges visszaállítása elutasításra kerül. Telepítés és részletek:
[külön Mission-adatbázis](mission_database_2026-10-25.md).

A **Mentések** oldal a felső választóban kijelölt helyi vagy távoli csomagot
kezeli. **Új mentés** előtt jelöld ki az összetevőket:

- **Konfiguráció:** `server.json`.
- **Játékadatok és questek:** `data`, `runtime_defaults`, valamint a konfigurált
  játékpéldányok `mark` tartalma és `guild_symbol.tga` fájlja.
- **TLS:** a csomag `tls` mappájának tanúsítványai és privát kulcsai.
- **Adatbázisok:** az effektív közös/core/login SQL-beállításokkal használt
  játékadatbázisok, általában account, common, player és log. SQL-rendszersémát,
  MariaDB-felhasználókat, grantokat és az SQL-szolgáltatás saját konfigurációját nem menti.

Az archívum nem tartalmazza a szerverprogramokat, teljes naplókat, core fájlokat
vagy az SQL adatkönyvtárát. A binárisok korábbi verzióját a buildtelepítés saját
mentése őrzi. A csomagon kívüli TLS-fájlokat sem másolja automatikusan.
A mentés a lemezen levő állapotról készül; előtte mentsd a szerkesztők módosításait.

Tárolás: `run/backups/<id>/backup.tar.gz`, mellette SHA-256 ellenőrzőösszegeket
tartalmazó leíró és kis méretű katalógusbejegyzés. Csak teljes, sikeresen
lezárt mentés kerül a listába. A **Letöltés / másolat mentése** a gépeden
ellenőrzött másolatot készít; távolról SFTP-t használ. A helyi célfájlt csak
sikeres méret- és hash-ellenőrzés után cseréli le. A **Mentési archívum betöltése**
egy korábban kimentett saját archívumot a kijelölt gép katalógusába tesz:
ez még nem állítja vissza a tartalmát. Csak megbízható, saját SQL-mentést tölts be.

SQL-mentéshez `mariadb` és `mariadb-dump` szükséges. A helyi saját SQL-példánynál
az ellenőrzött adatkönyvtár admin-hozzáférését használja. FreeBSD root SSH mellett
a helyi root socket-belépést előbb csak olvasó lekérdezéssel ellenőrzi, és csak
egyező SQL-portnál használja. Egyébként a konfigurált SQL-hitelesítést alkalmazza;
ennek a teljes logikai mentéshez szükséges jogosultságokkal kell rendelkeznie.
A jelszó privát, ideiglenes opciófájlban szerepel, nem parancssori argumentumban.

Az egy SQL-célgépen levő adatbázisokat közös globális olvasási zárral menti,
így a MyISAM és az InnoDB táblák együtt is következetesek maradnak. Eközben az
írások várakozhatnak. Több SQL-célgép külön pillanatkép, nem elosztott tranzakció.
A mentés futó játékszerver mellett is készülhet; a fájlváltozásokat és az élő
questszerkesztést zárolással, illetve tartalomellenőrzéssel védi.

Visszaállítás:

1. Állítsd le a játékszervert; az SQL-szolgáltatás maradjon elindítva.
2. Válassz mentést és visszaállítandó összetevőket, majd **Visszaállítási terv**.
3. A teljes archívum és minden tag ellenőrzése után megjelennek a fájlcserék,
   törlendő többletfájlok és a felülírandó SQL-célok.
4. Jelöld be a felülírást, majd **Terv szerinti visszaállítás**.

Alapból megőrzi a jelenlegi SQL-kapcsolatokat, így egy másik gépről származó
konfiguráció nem irányítja át a következő indítást a forrás SQL-szerverére.
Adatbázisneveket nem nevez át automatikusan. Az SQL-visszaállítás újralétrehozza
a kijelölt játéksémákat, ezért a mentés után létrehozott többlettáblák és rutinok
sem maradnak bennük. A fájloknál is a kiválasztott összetevők mentett tartalma áll helyre.

A változtatás előtt kötelező automatikus védőmentés készül. Hiba esetén ebből
visszaállítja az előző állapotot. A több adatbázis/fájl cseréje nem egyetlen
atomikus művelet: eredménye a `run/backup-restores/<id>/result.json` naplóba kerül.
Félbeszakadt vagy sikertelen rollback mellett a GUI és a friss `manage.py`
tiltja a szerverindítást. A Mentések oldalon egy ellenőrzött mentés sikeres
helyreállítása oldja fel ezt. Kapcsolatszakadáskor előbb csatlakozz újra,
ellenőrizd a naplót és a mentéslistát; a már megkezdett távoli művelet tovább futhat.

**Napi automatikus mentés:** állítsd be a célgép helyi idejét és a megtartandó
ütemezett mentések számát, majd mentsd a beállítást. A 0 minden mentést megtart.
A kézi, build előtti és visszaállítási védőmentéseket ez a megőrzés nem törli.
FreeBSD-n csak saját, csomagonként jelölt cron-sorát kezeli; a többi bejegyzést
megőrzi. Windowson saját Feladatütemező-feladatot hoz létre, amelyhez a felhasználónak
bejelentkezve kell maradnia. A GUI mindkét esetben bezárható. A napi futás aznap
már elkészült mentést nem duplázza meg. Az utolsó ütemezett siker/hiba az oldalon látszik.

**Automatikus mentés buildtelepítés előtt:** ugyanitt kapcsolható be. A távoli
telepítő a kiválasztott összetevőkről mentést készít a bináriscsere előtt;
ha a mentés hibázik, a telepítés nem írja felül a programokat. Ez kiegészíti
a telepítő meglévő binárismentését. Mindkét automatizmus alapból ki van kapcsolva.

A mentésekben fiókadatok, konfigurációs jelszavak és privát kulcsok is lehetnek;
a letöltött másolatot ennek megfelelően tárold. A központ nem titkosítja az archívumot.
Az aktív mentés/visszaállítás alatt az app célgépet nem vált és nem zárható be.
Az SFTP-archívumátvitel külön megszakítható; a részleges feltöltés még nem katalógusbejegyzés.

Részletek: [MariaDB dump és zárolások](https://mariadb.com/docs/server/clients-and-utilities/backup-restore-and-import-clients/mariadb-dump),
[Windows Feladatütemező](https://learn.microsoft.com/en-us/windows-server/administration/windows-commands/schtasks-create).

## Fejlesztői futtatás és fordítás

```powershell
python -m venv build/server-manager-env
.\build\server-manager-env\Scripts\python.exe -m pip install -r SRC/Tools/ServerManager/requirements.txt
.\build\server-manager-env\Scripts\python.exe -B SRC/Tools/ServerManager/app.py --package D:/AurigaGlobal/Server2-Local
cmake --build build --config RelWithDebInfo --target ServerPackageCheck
# A validators mappába az imént fordított ServerPackageCheck.exe kerüljön.
.\build\server-manager-env\Scripts\python.exe -B SRC/Tools/ServerManager/build_manager.py --output build/server-manager-dist --work build/server-manager-freeze --validators build/server-manager-validators
python -B SRC/Tools/ServerManager/install_manager.py --package D:/AurigaGlobal/Server2-Local --bundle build/server-manager-dist/AurigaServerManager
```

A build új kimeneti és munkamappát igényel. A frontend Qt6-ot használ.
A telepítő csak változatlan manifestű csomagot egészít ki, meglévő
kezelőt nem ír felül. A `prepare_server.py --manager-dir <bundle>` ugyanígy
beépíti a GUI-t egy új szervercsomagba a ZIP-készítés előtt.
A mentési/ellenőrzési backend önállóan, Qt nélkül tesztelhető:

```powershell
python -B tests/server/ServerManagerTests.py
python -B tests/server/ServerManagerTests.py --package D:/AurigaGlobal/Server2-Local
```

A második parancs ideiglenes másolaton valódi natív konfiguráció- és Lua-
ellenőrzést is végez; az élő csomagot nem szerkeszti. A grafikus tesztekhez
a Qt-környezet Pythonjával futtatandó a `ServerManagerGuiTests.py`.
Az új szerkesztő és a valódi Lua-/quest-parser tesztje:
`build/server-manager-env/Scripts/python.exe -B tests/server/ServerManagerEditorTests.py`.
Ehhez a ServerPackageCheck célt előbb fordítsd le.
Az SSH-konfiguráció és a hibakezelés távoli gép nélkül ellenőrizhető:

```powershell
python -B tests/server/ServerManagerSshTests.py
.\build\server-manager-env\Scripts\python.exe -B tests/server/ServerManagerSshTests.py --gui
.\build\server-manager-env\Scripts\python.exe -B tests/server/ServerManagerRemoteWorkTests.py
.\build\server-manager-env\Scripts\python.exe -B tests/server/ServerManagerPackagesTests.py
.\build\server-manager-env\Scripts\python.exe -B tests/server/ServerManagerCrashTests.py
.\build\server-manager-env\Scripts\python.exe -B tests/server/ServerManagerBackupTests.py
.\build\server-manager-env\Scripts\python.exe -B tests/server/ServerManagerTargetTests.py
.\build\server-manager-env\Scripts\python.exe -B tests/server/ServerManagerPasswordTests.py --gui
```

A teszt a helyi OpenSSH `-G` opciójával vizsgálja a két gép külön kulcsait és
portját. A Qt-tesztek helyi alfolyamatokon és loopback porton ellenőrzik a
megszakítást, időkorlátot, sikertelen indulást és a foglalt portot; éles géphez
nem kapcsolódnak. FreeBSD 15.1-en végzett valódi próbát ez nem helyettesít.
A távoli munkatesztek a valódi worker-kódot ideiglenes helyi célokon is
futtatják: ellenőrzőösszeg-hiba, ütközés, mentés, visszaállítás, megszakítás,
Qt-folyamatlánc és CMake/Ninja próbaprojekt. A teljes FreeBSD szerver fordítása
és az éles SSH/SFTP-kapcsolat külön integrációs próba.
A jelszavas kapcsolatok valódi OpenSSH/SFTP-próbáihoz a tesztkörnyezetben
telepített Paramiko és a `ServerManagerPasswordTests.py --native --gui` használható;
a Paramiko csak az ideiglenes, loopback SSH-tesztszervert biztosítja,
az alkalmazás nem igényli és nem csomagolja be. A
`--askpass-exe <kész AurigaServerManager.exe>` a becsomagolt jelszósegédet is
végigpróbálja: külön ugrógépes jelszó, UTF-8 jelszó, SFTP batch-feltöltés,
portalagút, Qt-kapcsolatellenőrzés, hibás jelszó, az első hostkulcs
jóváhagyása/elutasítása, a megváltozott kulcs elutasítása, valamint első
jóváhagyás után SSH-n át futtatott valódi CMake/Ninja-próbafordítás.
A PAM-próbák kizárólag `keyboard-interactive` hitelesítést kínáló
tesztszerveren futnak: a két FreeBSD-jelszókérés, a külön ugrógépes jelszó,
az SFTP és a Qt-felület mellett az OTP/jelszócsere elutasítását, az ismételt
jelszóbekérés védelmét és az egymás utáni RPC-k új kapcsolódását is mérik.
A megjegyzés tesztjei a valódi Windows jelszótárba csak egyedi tesztbejegyzést
írnak, új Python-/becsomagolt folyamatból olvassák vissza, majd eltávolítják.
A jelölés törlését, profiltörlést, végpontcserét és a sikertelen profilmentés
utáni visszaállítást is ellenőrzik.
A célgép-tesztek a valódi csomagkezelő RPC-t helyi csővezetéken futtatják:
helyi/távoli elkülönítés, natív fájlellenőrzés, távoli mentés és visszaállítás,
ütközés, kapcsolatvesztés, célváltás és profiltörlés alatti szerkesztésvédelem,
illetve az SSH-kérés befejezése után is futó felügyelő indítása szerepel bennük.

Már telepített kezelő frissítésekor előbb zárd be a régi ablakot, majd az
`install_manager.py` parancshoz add meg a `--update` kapcsolót. Ez csak a
manifest szerint saját GUI-fájlokat frissíti; a `server.json` és a játékadatok
felhasználói módosításai megmaradnak. A lecserélt GUI-fájlok a
`run/manager-updates` mappába kerülnek. A futó alkalmazást a frissítő nem állítja le.

## Tárgymásolás és mentésvédelem — 2026-10-28

A **Tárgyak → Másolat új tárgyként** gomb a kijelölt, mentett prototípust
új helyi azonosítóval és egyedi tartalomkóddal nyitja meg. A név-/leíráskulcs,
megjelenés, alapadatok, fejlesztési görbe és bónuszok átvehetők. A másolat
mentéséig az adatbázis nem változik; a forrástárgy profiljai módosításkor
védettek maradnak. A még nem mentett másolatot az Elvetés gombbal eldobhatod.

Ellenőrzés vagy mentés idejére a tárgyadatlap zárolódik, így a háttérművelet
visszaolvasása nem írhat felül közben begépelt értékeket. A teljes +0…+255
görbe egyedi szintbónuszokkal is menthető, egyetlen prototípussal. A natív
segédprogram csoportos prepared műveleteket használ egy tranzakción belül;
adatbázishibánál a profilok és az új tárgyazonosító léptetése együtt gördülnek
vissza. A mentés továbbra is csak a helyi, leállított játék saját `protos`
adatbázisát kezeli; a futó játék és Unity átállítása külön fejlesztési fázis.

Alapul szolgáló hivatalos Qt-dokumentáció:
[Qt for Python](https://doc.qt.io/qtforpython-6/),
[QProcess](https://doc.qt.io/qtforpython-6/PySide6/QtCore/QProcess.html).


### Időzített fogyasztható tárgyak — v8 fejlesztői katalógus

A **Fogyasztható** lapon a **Fogyasztható típusa** választóval válthatsz a
helyreállítás és az időzített stat-hatás között. Az utóbbinál megadható az
időtartam, a fogyó darabszám, a használati környezet, a PvP-szabály, a várakozási
idő és a teljes bónuszcsomag (például mozgási sebesség + erő + XP).
Az idő másodpercben, a százalék két tizedessel, az attribútum egész pontban
szerkeszthető; a mentett adatok továbbra is pontos egész számok.

A **hatáscsoport** dönti el, hogy az új használat elutasítást vagy teljes cserét
jelent, online vagy kijelentkezve is telő idővel, illetve halálkor megszűnéssel.
Meglévő csoport kiválasztásakor ezek közös, zárolt szabályok. Más szabályokhoz
válassz **Új hatáscsoport**-ot, és adj új kódot. Azonos kód eltérő szabályokkal
nem menthető. A bónuszokat a rendszer együtt kezeli; nincs csoporton belüli
részleges csere vagy időtartam-összeadás.

Egy megosztott hatás vagy bónusz szerkesztése másolatot készít, és nem írja át
más tárgy tartalmát. Gyógyításra váltáskor az árván maradó privát hatásprofil
és bónuszprofil törlődik, a kiadott hatáscsoport megmarad.

Ez a funkció **v8 fejlesztői katalógust** igényel. A friss draft migrációlánc
nem meglévő v7-adatbázis automatikus frissítése. Ebben a fejlesztési körben
csak külön tesztadatbázisokon ellenőriztük: a telepített helyi appot és a valódi
helyi katalógust nem cseréltük le. A játékbeli alkalmazás/időzítés az item
runtime későbbi integrációjához tartozik.


### Automata palack és arénakvóta — v9 fejlesztői katalógus

A **Fogyasztható → Gyógyítás / regeneráció → Hatás módja → Automata palack**
mezőivel kiválasztható HP vagy SP, véges/végtelen ellátás, induló kapacitás,
visszatöltési időköz, maximum-poolhoz viszonyított arány és aktív maximum-bónusz.
Az automata egyenként kezelhető: a halmozás 1-re áll, az aréna nem választható.
Végtelen ellátásnál a kapacitás 0 és nem szerkeszthető. A még nem mentett
véges kapacitást a választó oda-vissza kapcsolása megőrzi.

Az **Arénakvóta használatonként** a többi recovery típuson állítható,
ha az Aréna jelölve van. 0 kvótamentes, pozitív érték ennyi kvótát igényel
korlátozott mérkőzésen. Az aréna kikapcsolásakor a költség is 0 lesz.
A mérkőzés általános tiltása a kvótamentes italokra is érvényes.

A kapacitás közös prototípusadat, a megmaradt töltet a későbbi játékbeli
példányállapot része. Prototípus módosítása nem játékosfeltöltési művelet.
A mostani fejlesztés nem tölt be régi socketeket és nem ír át játékosadatot.
A v9 draft és az új felület külön tesztadatbázisokon ellenőrzött forrás;
a telepített app és meglévő v7/v8 katalógus frissítése külön, adatmegőrző lépés.


### Kapcsolható harmatok — v10 fejlesztői katalógus

A **Fogyasztható → Kapcsolható harmat** űrlapon az összes aktív idő,
a variáns, a házasság/csoportvezető feltétel, a használati környezet és a PvP
állítható. A halmozási korlát automatikusan 1. A bónuszlista a bekapcsolt
példányhoz tartozó hatásokat írja le, nem felszerelés nélküli alapbónusz.
A várakozási idő csak a bekapcsolásra vonatkozik; kikapcsolást nem blokkol.

Egy hatáscsoporton belül egy példány lehet aktív. Azonos variáns másik
példánya elutasított, eltérő variáns a korábbit lecseréli. Egymástól független
harmatokhoz külön csoport kell. Meglévő csoport halálszabálya zárolt; eltérő
szabályhoz válassz új csoportot. A kiadott csoport akkor is megmarad, ha az
utolsó rá hivatkozó tárgyat más hatásra állítod.

Az idő csak bekapcsolva és bejelentkezve fogy. A prototípus a kezdeti időt
határozza meg; a későbbi játékbeli példány tárolja a maradékot. Lejáratkor
levétel és példánytörlés javaslat keletkezik. Halálkor a csoportszabály szerint
kikapcsol, vagy aktív marad és tovább fogy az ideje.

A másolás és szerkesztés a megosztott bónusz/profil módosítása előtt külön
példányt készít a profilból; más tárgy szabályait nem írja át. Gyógyító,
időzített és kapcsolható típus között is lehet váltani. Az ellenőrző mindig
pontosan egy hatást fogad el. A meglévő tárgykulcs tartósan megmarad.

Ez jelenleg tesztelt forrás és izolált katalógusfunkció. A játékbeli alkalmazás
és a telepített app/adatbázis adatmegőrző frissítése további lépés; a meglévő
helyi katalógushoz a fejlesztés nem írt hozzá.


### Küldetéstárgyak — v13 fejlesztői katalógus

Az **Új tárgy → Küldetéstárgy** a 10-es kategóriában hoz létre prototípust.
A **Gyűjthető token** közvetlenül nem használható. Ebben a módban a
küldetéscsoport, használati környezet és megfigyelő beállításai inaktívak.
Az **Első megfelelő quest kezelése** és **Minden megfelelő quest kezelése**
mód használati szándékot ad a questkezelőnek. Üres csoportnál a tárgy saját
ItemKey kulcsa az útválasztás alapja; megadott csoportnál stabil tartalomkódot
kell használni, például `quest.horse_training`. Régi tárgyazonosító és Lua-fájl
elérési útja nem csoportkód.

A használat önmagában nem fogyaszt tárgyat és nem ad jutalmat. Ezek a quest
külön, szerver által ellenőrzött műveletei. A környezet, megfigyelőállapot,
karakterszint és harc közbeni tiltás a használat előtt ellenőrzött. A modell
itt a földre dobott tárgy megjelenése; nem teszi felszerelhetővé a tokent.

A kategória, SQL-betöltő, natív validátor és Qt-űrlap fejlesztői forrásban
elkészült, izolált adatbázison tesztelt. A telepített helyi app/katalógus még
v7; a játékbeli questbekötés és a teljes adatkonverzió további munka.
Részletek: [küldetéstárgy- és konverziós jelentés](reports/item_combat/2026-10-28_quest_items.md).

### Képességkönyvek — v11 fejlesztői katalógus

Az **Új tárgy → Képességkönyv** a 9-es kategóriában hoz létre egy prototípust.
A **Cél és használat** lapon tanulás, felejtés/pontvisszaadás vagy lóképességpont
választható. Fix célhoz egy képességsort adj meg; példányhoz kötött célzásnál
az engedélylistát szerkeszted, a konkrét kötést a későbbi játékbeli példány tárolja.
A képességazonosító 1..254 és nem tárgyazonosító. A szervernek ezen felül
ellenőriznie kell a képesség létezését, kaszt/csoport és előfeltétel szabályait.

Felejtésnél választható a visszaadott pont készlete. Nyelvkönyvnél a tiltott
saját birodalmat adhatod meg. Lópont esetén nincs célképesség, a célmezők
inaktívak. A próbálkozásonkénti darabszámot és a használható képességfokozatokat
is ez a lap tartalmazza.

A **Szintenkénti szabályok** lapon a bal oldali listából válaszd ki az aktuális
szintet. Jobbra a következő szint, karakterminimum, szükséges sikeres olvasások,
sikeresély, olvasási bónusszal erősített esély, XP-költség és olvasási idő
szerkeszthető. A szintlépés lehet például 10→20 is. A szükséges olvasásszám
elérésekor egyszer emel, majd nulláz; a sikertelen próba nem növeli a számlálót,
de az elfogadott próbálkozás elfogyasztja a könyvet. Az erősített esély nem
lehet kisebb a normálnál. 0% biztos kudarc, 100% biztos siker.

A 0/0 olvasási idő várakozásmentes; eltérő pozitív minimum/maximum közül a
szerver választ. Beállítható, hogy sikertelen olvasás után is kelljen-e várni.
A várakozás a képességhez tartozik, másik könyvpéldányra váltva is megmarad.
Felejtést nem blokkol a tanulási idő; felejtéskor a megkezdett olvasásszámláló
is megszűnik. A lópontkönyv legfeljebb 20 szabad pontig, illetve mindkét
alternatív kiosztás 60-as határáig használható.

Nyolc minta érhető el: mester, vezetés, kombó, nyelv, új support, anti-passzív,
felejtés, lópont. A **Szabályminta betöltése** kifejezetten lecseréli a könyv
céljait és szintszabályait. Ezek ellenőrizendő fejlesztői kiinduló adatok,
nem végleges balance vagy a régi adatbázis automatikus konverziója.
A műveletválasztó oda-vissza kapcsolása a még nem mentett szerkesztést megőrzi.
Érvénytelen szintmezőnél a lista nem dobja el a beírt értéket.

Másolás és közös profil szerkesztése nem változtat más könyveken. A 40-es
képességszint a jelenlegi skill domain határa; nem korlátozza a tárgyak
külön fejlesztési profilját. A könyv több szintlépése egyetlen prototípus része.
A futó GameServer/Database/Unity bekötése és a meglévő katalógus adatmegőrző
frissítése továbbra is későbbi lépés; ez a fejlesztés nem telepített átállás.
