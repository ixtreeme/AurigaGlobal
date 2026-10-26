# Mission Engine 4/D–4d – natív sessionazonosítás a két szerverfolyamat között

Dátum: 2026-10-26. Ág: `codex/mission-engine`. Csak helyi fejlesztés és teszt.

## Elkészült rész

A GameServer és a Database karakterbetöltési útvonala opcionális, verziózott
sessioncsatornát használ. Ez a tényleges descriptor-, account-, peer- és SQL-callback
életciklushoz kapcsolódik. Nem külön MissionServer és nem Unity-protokoll.

A kapcsolat, a konkrét betöltési kérés és a Database által kiadott session külön
128 bites, kriptográfiai véletlen tokent kap. A Database monoton 64 bites
generációt is rendel a betöltéshez. Az azonosítás fiókhoz, karakterhez, a natív
peerhez és descriptorhoz kötött; pusztán egy PID, handle vagy SQL-head megfigyelése
nem ad jogosultságot Mission-owner igénylésére.

A `ClientHandleInfo` saját értékként hordozza a lease-t. A főszál minden betöltési
SQL-válasznál újra megkeresi az aktuális fiókot és ellenőrzi a peer/logon/slot/lease
egyezését. Kijelentkezés, auth-adatcsere vagy peer-vesztés érvényteleníti a régi
sessiont; a későn visszaérő player/item/quest/affect/skill-color válasz eldobódik.
A peer elvesztése után a betöltés owned kontextusa is felszabadul.

A GameServer descriptor saját kérés-/sessionállapotot tart. A player-válasz után
a teljes EnTT-entitásazonosító, tehát annak generációja is rögzül. További részek
csak ugyanahhoz a belépéshez és ugyanahhoz az élő entitáshoz alkalmazhatók.
Duplikált rész nem alkalmazódik még egyszer. A zárás, karakterválasztásra váltás
és Database-kapcsolat bontása visszavonja ezt az állapotot.

## Belső protokoll és kompatibilitás

| Irány | Header | Feladat |
|---|---|---|
| GD | `PLAYER_SESSION_HELLO` (146) | Sessioncsatorna egyeztetése egy friss kapcsolat-tokenhez |
| DG | `PLAYER_SESSION_HELLO` (217) | Azonos token és támogatott verzió visszaigazolása |
| GD | `PLAYER_LOAD_SESSION` (147) | Fiók/karakter/slot és egyedi kérés-token |
| DG | `PLAYER_SESSION_REPLY` (218) | Sessionhöz kötött, típusos betöltési válasz |

A fejléc explicit little-endian, verziózott `AMS1` keret. A fenntartott bitek
nullák; ismeretlen verzió/típus, hibás hossz vagy 3 MiB-nál nagyobb body elutasított.
A beágyazott legacy player/item/quest/affect payload továbbra is a meglévő,
azonos szerver-ABI-t igénylő struktúra. A codec a sorok számát és a PID/owner mezőket
is összeveti a sessionnel. Más belső packet vagy újabb session-envelope nem ágyazható be.

Az egyeztetés csak a meglévő, kulccsal hitelesített natív peer-kapcsolaton kapcsol be.
Kulcs nélküli vagy régebbi Database esetén nincs session-ACK, a GameServer a korábbi
betöltési protokollt használja. A régebbi GameServer nem kezdeményez egyeztetést,
ezért az új Database a korábbi válaszformátumot küldi neki. A Mission-ownerhez ez a
legacy fallback nem tekinthető sessionhitelesítésnek; az új gameplay kapuja zárt.
Verziózott kérés mellett egy régi, azonosítatlan betöltési válasz nem kerülhet alkalmazásra.

## Kapcsolódó valódi betöltési hibák

- A Database a cache/SQL elérése és a logout-lista módosítása előtt ellenőrzi az
  aktuális logont, az autentikált peert, a pontos fiókot és a kijelölt négy karakterhely egyikét.
- Egy fióknak vagy karakternek nem indulhat párhuzamos második betöltés.
- A duplikált SQL-callback a hiba-, logon- és cache-kezelés előtt kiesik;
  egy ismételt player-válasz nem zárja le a már elfogadott betöltést.
- A cold player SELECT a PID mellett az `account_id` egyezését is megköveteli.
- A cache-ből és SQL-ből jövő player/item/quest/affect/skill-color válasz ugyanahhoz
  a lease-hez kapcsolódik. A régi quest-cache ág hiányos PID/account kontextusa is javul.
- A már cache-ben lévő skill-color sem előzheti meg az elfogadott player-részt;
  cold player esetén a sorba állított lekérdezés biztosítja a sorrendet.
- Másik peer lejárt logoutja a logout/cache-ürítési listát sem módosíthatja.
- Hibás méretű player-load/logout kérésből nem készül struktúra-cast.

## Tesztek és fennmaradó munka

A `PlayerSessionTests` a tényleges codecet, authorityt, descriptorállapotot és
Database-engedélyezési predikátumot futtatja. A próbák hibás slotot/fiókot/peert,
párhuzamos betöltést, csonkolt/hibás keretet, duplikációt, session- és
entitásgeneráció-cserét, reconnectet és 256 egymást követő sessioncserét fednek le.
A külön izolált MariaDB-próba tényleges async SQL-választ késleltet egy kijelentkezés
és azonos account/PID/descriptor melletti újrabelépésen át. A régi válasz nem
kapja meg a csere-session jogosultságát. Ez nem Unityből végzett teljes belépési próba.
A végső helyi ellenőrzések:

- `PlayerSessionTests`: **810** natív ellenőrzés; izolált async MariaDB-vel **818**,
  normál és ASAN alatt egyaránt.
- Célzott normál CTest-regresszió: **27/27** sikeres, a valódi Mission- és
  item-award MariaDB crash/restart próbákkal együtt.
- Külön natív ASAN CTest-regresszió: **16/16** sikeres.
- GameServer, Database és LoginServer: Windows x64 RelWithDebInfo build normál
  és ASAN konfigurációban is sikeres. Az utolsó Database-callback sorrendjavítás
  után mindkét Database ismét lefordítva.
- Hash-ellenőrzés: **34** védett szezonális Mission Pass-fájl és **8** korábban
  módosított Unity-fájl változatlan.

Helyi naplók: `build/mission-session-native-build.log`,
`build/mission-session-native-asan-build.log`, `build/mission-session-final-build.log`,
`build/mission-session-final-asan-build.log`, `build/mission-session-regression.log`,
`build/mission-session-asan-regression.log`, `build/mission-session-asan-mariadb.log`.
Ezek fejlesztői buildtermékek, nem verziózott telepítési fájlok.

Ez a live session-koordinátor azonosítási és betöltési része. A következő feladat
a hiteles source-stop, dirty player/item cache-flush és mindkét SQL-lane nyugtájának
folyamatközi összekötése; utána a tartós Mission/resource writer-lease átadása,
a guarded legacy DML és az ECS/cache-rebase/publikálás. A kapcsolatszakadás után
már bent lévő karakterek Mission-újracsatolása külön helyreállítási út lesz.
A safebox/offlineshop és más, betöltésen kívüli legacy mentések nem váltak ettől
automatikusan Mission-fenced műveletté. A legacy logout még a meglévő peer/account
szerződést használja; a teljes writer-átadási protokollnak saját generációs zárás kell.

A korábbi egyszeri ASAN hatásnyugta-visszaolvasási észlelés oka változatlanul nyitott.
A jelen rész nem bizonyítja annak javítását, és nem aktivál Mission-jutalmazást.
Nem történt távoli kapcsolat, feltöltés, telepítés vagy valódi játékosadat-módosítás.
