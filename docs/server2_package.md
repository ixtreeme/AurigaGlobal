# Server2 átalakított localhost csomag – 2026-10-20

Forrásmentés: `D:/AurigaGlobal/Server2/srv1`.
Elkészült csomag: `D:/AurigaGlobal/Server2-Local`.
A letöltött szerver FreeBSD-binárisokat tartalmaz; az új csomag Windowsos
localhost futtatáshoz tartalmazza a módosított Database/LoginServer/GameServer programokat.

## Ismételhető előállítás

```powershell
cmake --build build --config RelWithDebInfo --target Database LoginServer GameServer ServerPackageCheck --parallel 4
python -B SRC/Tools/prepare_server.py --source D:/AurigaGlobal/Server2 --output D:/AurigaGlobal/Server2-Local --build-dir build --archive D:/AurigaGlobal/Server2-Local.zip
```

A kimeneti mappa nem létezhet, és nem lehet a forrás könyvtára, szülője vagy
alkönyvtára. Az újrageneráláshoz új kimeneti nevet kell megadni. A forrás
egyetlen fájlja sem íródik felül; az előállítás az összes felhasznált eredeti
fájl SHA256 értékét a feldolgozás előtt és után ellenőrzi.

A ZIP elkészítése előtt az előállító a csomag összes állandó fájlját összeveti
a SHA256-manifesttel, utána a ZIP CRC-it is ellenőrzi. A ZIP mellé külön
`.zip.sha256` ellenőrzőfájlt ír. Létező archívumot vagy ellenőrzőfájlt nem ír felül.
A változó `run/` állományok és a helyi `server-files-ready.json` nem részei az
archívumnak vagy a manifestnek; kicsomagolás után a `prepare` készíti elő a futási mappákat.

## Elkészült automatizálás

- `db/conf.txt` és 13 core `CONFIG` beolvasása az eredeti SQL-mezők eltérő
  sorrendjének kezelésével; saját auth CONFIG helyett új LoginServer-szekció.
  Közös `server.json`, az eredeti portok, csatornák és MAP_ALLOW listák megtartásával.
- A localhost profil címeket `127.0.0.1`-re állít, új helyi SQL-jelszót és
  peer-auth kulcsot generál. Az élő SQL-/adminjelszavakat nem másolja át.
- Drop-/group-/blend-/cube-/fishing-/gaya-/guild-/map-/regen-/questmeta-adatok
  JSON-konverziója a meglévő konverterekkel. Az élő locale guild táblája az
  elsődleges; a forrás 30-as guildszintkorlátja megmarad.
- Közvetlenül betöltött, szerkeszthető Quest források megőrzése; ismert
  erőforrásliterálok átvezetése JSON-ra, a nem UTF-8 Lua-bájtok megőrzésével.
  Az objecteket kihagyja; nincs QC-futtatás. Az azonos nevű questek közül a régi fordítás sorrendje szerinti utolsó marad aktív, jelentéssel. Lásd: `lua55_quest_migration.md`.
- Mobmozgások és skill splash rekordok sütése az eredeti MSA/motlist adatokból
  a `db/motion.sql` fájlba. A futtatott szerver a DB-t használja, nyers kliensmodelleket nem igényel.
- Saját localhost TLS-cert/kulcs generálása és párjuk ellenőrzése. Nyilvános
  Unity-végpontok és tanúsítvány-pin exportja `client-connection.json` néven.
- A Gaya játékosállapot és a guild mark/symbol állományok megőrzése; az indító
  csak hiányzó futási állományokat inicializál, meglévő állapotot nem cserél le.
- Adatbázis nélküli `prepare`: mind a 15 példány futási mappájának és
  alapállományainak előkészítése, natív konfiguráció-/adat-/TLS-ellenőrzéssel.
  Nem nyit SQL-kapcsolatot és nem indít szolgáltatásokat.
- Régi binárisok, core dumpok, logok, PID-ek és kihasználatlan locale/map
  mentések kihagyása. Az eredetiben továbbra is megtalálhatók.
- Új, kizárólag a csomaghoz tartozó helyi MariaDB automatikus inicializálása,
  séma/seed/live skillgörbe/motion import, SQL-jogosultságok létrehozása;
  létező adatkönyvtár újrainicializálásának tiltása.
- Függőségi indítás, portellenőrzés, folyamatfelügyelet és sorrendben végzett
  szabályos leállítás. A CH99 közös core a kijelölt normál csatorna mellett elindul.
  A felügyelő kizárólag a saját folyamatoknak küld PID/token-egyező leállítási kérést;
  PID-újrafelhasználást és párhuzamos indítást is ellenőriz.

## Használat

```powershell
cd D:/AurigaGlobal/Server2-Local
.\server.ps1 prepare
.\server.ps1 check
.\server.ps1 status
```

A fájlcsomag állapota `SERVER_FILES_READY`. A `prepare` ismételhető, a meglévő
futási állományokat megtartja. A szerverfájlok összeállítása az adatbázistól
függetlenül ellenőrizhető. A jelenlegi `Server2-Local` mappában az illesztett
adatbázis már be van töltve; új ZIP-példányban a kezelő **Adatbázis előkészítése**
gombja vagy a `setup-db` parancs végzi az első betöltést.

Új példány előkészítése és közös futtatási próba:

```powershell
.\server.ps1 setup-db
.\server.ps1 database-check
.\server.ps1 start --channel 1
.\server.ps1 status
.\server.ps1 stop
.\server.ps1 stop-db
```

A `start` konzolos felügyelet; a `stop` másik konzolból is kérhető. SQL
referenciaadatok nélkül az indítás elutasított. A `start` alapból CH1-et és
a közös CH99-et választja; `--channel` többször megadható, vagy használható
a `--all-channels`. A `stop-db` előtt a szervereket le kell állítani.

A futó stackhez utólag is indítható core vagy csatorna, és a game core-ok
egyenként leállíthatók:

```powershell
.\server.ps1 start-cores --channel 2
.\server.ps1 start-cores --instance ch2-core2
.\server.ps1 stop-cores --instance ch1-core2
```

A felügyelő a kérést a `run/control` mappán át kapja. Az utólag induló core
ugyanazt a port- és készenléti ellenőrzést kapja, mint a teljes indításnál.
Ha nem indul el, csak az a core marad leállítva, és a stack fut tovább.
Ugyanígy: a kért leállítás nem számít összeomlásnak, a stack fut tovább.
A Database és a LoginServer csak a teljes stackkel együtt indul és áll le.
A core-onkénti vezérlés támogatását a futó felügyelő jelzi. Ha a `manage.py`
fájlt működés közben frissíted, a teljes szervert egyszer le kell állítani és
újraindítani; addig a külön core-vezérlés tiltott, a teljes leállítás használható.
Az SQL-kliens az adminjelszót privát futási fájlból kapja, nem parancssori argumentumból.

## Ismert adateltérések és hátralévő integráció

**Frissítés, 2026-10-20:** a négy SQL-mentés megérkezett, illesztése és a
Server2-Local saját MariaDB-példányába történő importja megtörtént.
Részletek: [adatbázis-helyreállítás és natív localhost próba](database_restore_2026-10-20.md).
Az eredeti összeállítás még SQL-dump nélkül készült. A mostani csomag a
`db/backup` alatt tartalmazza az ellenőrzött adatokat és a `tools/mariadb`
alatt a hordozható motort. A ZIP nem tartalmazza a `run` alatti adatkönyvtárat;
az első `setup-db` hozza létre, később az adatokat a csomagon belül őrzi meg.

A mentés MAP_ALLOW listái több, mapindexből hiányzó azonosítót tartalmaznak:
CH2/CH3/CH4 core1: 11; CH5/CH6 core1:
4,24,44,61,62,64,65,68,69,70,215,302,304,353,354; CH5/CH6 core2:
71,72,104,301,303. A konverter a listákat megőrzi és jelentést készít.
A CH1/CH99 engedélyezett mapjai mind szerepelnek az indexben.

A `PLAYER_DELETE_LEVEL_LIMIT` és `PLAYER_DELETE_LEVEL_LIMIT_LOWER` régi DB-
kulcsnak nincs megfelelője a jelenlegi típusos konfigurációban; a jelentés
retired listája rögzíti. A játékmenet a jelenlegi forrás törlési szabályát használja.
A régi adminpage-beállítások elhagyott protokollhoz tartoztak.

A fájlok és a Lua szintaxisa ellenőrzöttek, a Lua runtime API-hívások,
dungeonjáték, perzisztencia és Unity-belépés a visszaállított adatbázissal
végzendő integrációs próba részei. A nyilvános kliensprofil exportja nem írta át a Unity projektet.

## Ellenőrzött eredmény

- 13 game core + Database + LoginServer natív konfigurációs preflight: sikeres.
- 48 indexelt map; 510 JSON, 668 futási Lua/quest chunk natív ellenőrzése: sikeres.
- 926 mobcsoport, 216 csoportcsoport, feloldatlan csoport-vnum: 0.
- 39 questfájl erőforrásútvonala módosult, feloldatlan literal erőforrás: 0.
- 726 mobmotion + 375 skill splash rekord; a kiadott csomag 10004 bemeneti
  fájl SHA256 értékét ellenőrizte. A csomagfájlok manifestellenőrzése sikeres.
- Normál és ASAN build: 53/53 CTest. A csomagkészítő/felügyelő külön
  próbája 18/18 mindkét natív checkerrel, valódi alfolyamatos stop és hibás
  indulás utáni visszaállítás, indulás közbeni megszakítás és DB-leállítási sorrend tesztelésével.
  Az új esetek az SQL nélküli előkészítést, a meglévő futási fájlok megtartását,
  a ZIP tartalmát és sértetlenségét, illetve a megváltozott csomag és létező ZIP elutasítását fedik le.
- MariaDB 12.2 saját TEMP-példányon: init/start/stop, 1101 motion import,
  hiányzó 8 referenciaadat jelzése és az újrainicializálás tiltása sikeres.

Forrásjavítások: a `data/...` JSON-útvonalak a beállított adatgyökérre
oldódnak fel a core munkakönyvtárától függetlenül; a mobanimációk DB-ben tárolt
ezredmásodperceiből az AI másodpercet kap. A korábbi értelmezés ezerszeres
animációs várakozást okozhatott. A normál server loopok kezelik a felügyelő
hitelesített helyi leállítási kérését, hogy a meglévő flush/cleanup útvonalak fussanak.
