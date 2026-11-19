# Csatornák és core-ok kezelése

## Használat

**Áttekintés → Szervervezérlés → CH-k és core-ok…**, vagy
**Beállítások → CH-k és core-ok…**.

A tervezőben új CH és core hozható létre, teljes CH vagy kijelölt core
törölhető, és térképek oszthatók át ugyanazon CH core-jai között. A normál
csatornák 1–255 tartományban használhatók, a CH99 külön közös csatorna.
A létrehozás egyedi konfigurációs portokat és core-azonosítókat oszt.
Az indításkor a felügyelő a tényleges portelérhetőséget is ellenőrzi.

Új CH másik CH térképfelosztásából is indulhat; a térkép nélküli core-ok
kimaradnak. A mintából nem másoljuk át a korábbi SQL-/TLS-/játékbeállításokat:
az új core az aktuális közös alapértékeket örökli. Saját konfigurációjában
az identitás, portpár, maplista és futási mappa szerepel. Egy core legfeljebb
64 térképet kap, összhangban a natív routing kapacitásával.

A tervezőben a **Módosítások átvétele**, majd a Beállítások oldalon
az **Ellenőrzés és mentés** véglegesíti a változásokat. A meglévő piszkozatok,
natív ellenőrzés, fájlváltozás-ellenőrzés, biztonsági másolat és atomikus
konfigurációcsere megmaradnak. Futó játék mellett mentés tiltott. Kizárólag
`game_servers`-módosításnál a helyi MariaDB tovább futhat; a közös konfiguráció
és SQL-beállítások változtatásához továbbra is le kell állítani.

A definíció törlése nem töröl karaktereket, közös játékadatot, naplót vagy
mentést. Az utolsó normál CH utolsó core-ja nem törölhető. CH1 nélkül a
parancssori alapértelmezett indítás a megmaradt első normál CH-t használja.
Sok csatornánál az Áttekintés választógombjai több sorba rendeződnek.

## Közös adatok és kliens

Bináris, konfiguráció, SQL-/TLS-adat, térkép és quest nem kerül külön másolatban
a core mappájába. Az előkészítés csak a game core-oknak másolja a ténylegesen
használt BANIP/CMD/CRC/VERSION alapfájlokat. Céhjel-adatot csak bekapcsolt
céhjel-kiszolgálásnál készít elő. A korábbi saját futási fájlok megmaradnak.

A **Unity CH-lista exportálása…** kizárólag a mentett publikus végpontokat
exportálja. Ha a tervezett CH-végpontok eltérnek, előbb menteni kell a
konfigurációt. Unityben a **Metin2 → Import server channels…** menü importálja
a listát. A host, TLS-pin, kliensverzió és typed-item beállítás nem változik;
érvénytelen importnál a korábbi profil teljes egészében megmarad.
A CH99 nem jelenik meg választható normál CH-ként.

## Ellenőrzések

- 14 új Python/Qt teszt: létrehozás, üres core-ok kihagyása, öröklés,
  térképátadás, ütközések, törlésvédelem, kompakt felülírások, export,
  piszkozatmegőrzés, 16 CH megjelenítése 1024×720 ablakban, minimális
  runtime-fájlok és valódi natív konfigurációmentés/létrehozás/törlés.
- 46 meglévő GUI/backend teszt, valódi natív konfigurációellenőrzéssel.
- 23 helyi/távoli cél- és RPC-teszt; SSH-t helyi privát csatorna helyettesítette.
- 8 Áttekintés- és 6 navigációs/naplóteszt.
- 37 csomagkezelő/felügyelő teszt; a natív checkerhez kötött eset külön,
  a megfelelő friss ServerPackageCheck binárissal is lefutott.
- Unity **6000.3.27f1**, az aktív `AurigaUnity-20261103` munkapéldány
  forrásaiból külön ellenőrző projektben fordítva. A CH7/CH8 import,
  a kliens valódi csatornaválasztó-parancsai, a hibás/duplikált lista atomikus
  elutasítása, a megszűnt port helyettesítése és a TLS megtartása sikeres:
  `SERVER_CHANNEL_TOPOLOGY_VALIDATION_OK`, Editor exit 0.
- Az új fagyasztott EXE és a telepített változat tervezőablaka is sikeresen
  elindult, képernyőkép készült. Mindkét natív tárgysegédprogram indulása és
  a módosított fájlok `git diff --check` ellenőrzése sikeres.

Valódi FreeBSD-gép és új CH-ra belépő játékos tesztje nem történt.
A szerver konfigurációját a próba létrehozásával/törlésével külön ideiglenes
csomagon teszteltük. Az élő helyi CH-kiosztást és a Unity helyi kapcsolatprofilját
nem írtuk át tetszőlegesen választott új csatornákra.

## Helyi telepítés

A frissített alkalmazás:
`D:/AurigaGlobal/Server2-Local/manager/AurigaServerManager.exe`.
A meglévő `Szerverkezelo.cmd` indítja. A frissített `manage.py` is a helyi
csomagba került; az előző felügyelőszkript, a teljes manager mappa és a
csomagjegyzék biztonsági másolata:
`D:/AurigaGlobal/Server2-Local/run/manager-updates/channels-20261116T172821-c7084df4`.
Az ugyanitt lévő `receipt.json` rögzíti az ellenőrzőösszegeket és a folyamatokat.

A server.json és a három szerverbináris ellenőrzőösszege változatlan. A
database, login, ch1-core1, ch1-core2 és ch99-core99 szolgáltatás futott a
telepítés előtt és után. Adatbázist, játékfolyamatot vagy felhasználói adatot
nem állítottunk le és nem töröltünk.

Naplók: a munkapéldány `build/manager-channel-*` fájljai. A képernyőkép:
`build/manager-channel-installed.png`. A Unity-ellenőrzés külön forrásmásolata:
`build/unity-channel-topology-snapshot`; az ellenőrzés nem a felhasználó
megnyitott Unity Editorát vezérelte.
