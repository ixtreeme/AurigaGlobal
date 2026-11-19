# A szerverkezelő áttekinthetőségének javítása

Az új Qt6 alkalmazás telepítési helye:
`D:/AurigaGlobal/Server2-Local/manager/AurigaServerManager.exe`.
Indítás a meglévő `D:/AurigaGlobal/Server2-Local/Szerverkezelo.cmd` fájllal.

## Elkészült

- Csoportosított navigáció: Szerver, Játéktartalom, Üzemeltetés, Kapcsolatok.
  A tíz oldal belső azonosítója megmaradt, így a korábbi átirányítások és
  az összeomlási jelzés tovább működnek.
- Ékezet nélkül is használható oldalkereső, Ctrl+K és Enter gyorsbillentyűkkel.
- Egységes oldalcímek és az oldal feladatát leíró magyarázatok.
- Gyors elérés a tárgyakhoz, küldetésekhez és biztonsági mentésekhez.
- Nem mentett dokumentumok jelzése, közvetlen visszaugrással a szerkesztőre.
- Gyakori, ténylegesen elérhető játékadatfájlok megnyitása a fájlválasztóból.
- Szöveges és súlyossági naplószűrő, találatszámmal; célgépváltáskor a
  korábbi gép naplótartalma a szűrők módosításával sem hozható vissza.
- Átrendeződő szervervezérlés kisebb ablakban; ellenőrzött 1024×720 méret.
  A folyamattáblában legalább négy adatsornak van hely.
- Az elrendezéstől független teljes fejlécszöveg és akadálymentes név.
- A csomagolás felismeri a Qt DLL-ek mindkét elhelyezését, és eltávolítja
  az inkompatibilis, a Windows ICU-t elfedő függőséget az új csomagból.

A tárgykatalógus meglévő szerkesztője megmaradt. A korábban tervezett teljes
Excel-szerű tárgytáblázat külön fejlesztés; ez a munkafolyamat az alkalmazás
egészének navigációját és a napi használathoz szükséges közös felületet javítja.

## Ellenőrzések

Az önálló Qt tesztmodulok külön folyamatban futottak, hogy a meglévő tesztek
globális QApplication- és QSettings-állapota ne befolyásolja egymást.

| Terület | Sikeres | Kihagyott |
| --- | ---: | ---: |
| Új navigáció, keresés, piszkozatok, naplók | 6 | 0 |
| Áttekintés és szervervezérlés | 8 | 0 |
| Valódi natív ellenőrzős konfiguráció- és fájlszerkesztés, alapkezelő | 46 | 0 |
| Forráskódszerkesztő, valódi Lua- és questszintaxis-ellenőrzés | 20 | 0 |
| Biztonsági mentések | 15 | 0 |
| Összeomlások | 14 | 0 |
| Windows-csomagolás | 7 | 0 |
| Távoli csomagok | 14 | 0 |
| Jelszókezelés | 11 | 6 |
| Távoli munkafolyamatok | 51 | 2 |
| SSH-profilok | 15 | 1 |
| Helyi/távoli célok elkülönítése és váltása | 23 | 0 |
| **Összesen** | **230** | **9** |

A kihagyások közül kettő teljes opcionális tesztosztályhoz tartozik;
ezeket az unittest a futtatott tesztek számába nem számolja bele.

A kilenc kihagyás: opcionális natív jelszavas SSH-próba, négy külön
kapcsolóval engedélyezhető jelszavas GUI-próba, a fagyasztott askpass
hitelesítőadat-próbája, külön CMake/Ninja buildpróba, jogosultságot igénylő
symlinkpróba és az opcionális SSH GUI tesztosztály. Valódi FreeBSD-kiszolgálóhoz
nem kapcsolódtunk; a célgépváltási próbák privát helyi RPC-csatornát használnak.

Az összeomlási teszt dump-időbélyegét determinisztikussá tettük: Windows alatt
az ugyanabban az órajelben írt fájl ideje korábbi lehetett a rögzített indítási
időnél. A szerver összeomláskezelési kódja nem változott.

A végleges fagyasztott EXE, valamint mindkét natív tárgysegédprogram
indulását ellenőriztük. A telepített EXE-vel az Áttekintés, Beállítások és
Szervernaplók képernyőképe is sikeresen elkészült. A fordítás a projekt
quest API-indexét is ellenőrizte. A módosított fájlokra a `git diff --check`
sikeres. Unity- vagy játékmenet-változtatás nem történt.

## Helyi telepítés

Az előző teljes manager mappa és a korábbi csomagjegyzék megmaradt itt:
`D:/AurigaGlobal/Server2-Local/run/manager-updates/clarity-20261116T170038-6c347782`.
Az ugyanott található `receipt.json` rögzíti a telepített EXE SHA-256 értékét,
a 306 kezelt alkalmazásfájlt, a védett szerverfájlok változatlanságát és
a futó folyamatokat. A régi mappa teljes cseréje megakadályozza, hogy az
eltérő korábbi Qt-elrendezésből elavult DLL maradjon az új alkalmazás mellett.

Csak a `manager/**` és `server_manager.md` csomagjegyzék-bejegyzések változtak.
A szerverkonfiguráció, a vezérlőszkriptek és a három szerver-EXE ellenőrzőösszege
azonos maradt. Az adatbázis és a játékfolyamatok leállítása nem volt szükséges.
Mind az öt szolgáltatás végig futott: database, login, ch1-core1, ch1-core2,
ch99-core99.

A telepített natív CatalogEditorTool saját helyi adatbázisból mind a 6935
tárgydefiníciót sikeresen beolvasta. A katalógus hash értéke változatlan:
`741577036e2516600031bd492d5b3f22d3f14b0561321586dfd431fa5a945c85`.

Naplók és képek a munkahely `build/manager-clarity-*` fájljaiban találhatók.
