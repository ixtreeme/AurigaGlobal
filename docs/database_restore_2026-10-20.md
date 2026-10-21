# Adatbázis-illesztés és localhost próba — 2026-10-20

Az eredeti `D:/AurigaGlobal/Database` négy SQL-fájlja változatlan. A Navicat
mentés MariaDB 11.4.13-ról készült; a vizsgálat és az import ugyanilyen verziójú,
elkülönített Windows-példányon futott, kizárólag loopback kapcsolattal.

## Elkészült állományok

- `D:/AurigaGlobal/Database-Compatible`: új séma, négy adatdump, AUTO_INCREMENT
  helyreállítás, JSON-jelentés és SHA256-manifest. Az `archive-original` az
  eredeti négy fájl bájtpontos másolata; a régi többlettáblák/oszlopok, triggerek
  és eljárások ebben teljes egészében megmaradnak.
- `D:/AurigaGlobal/Server2-Local`: az átalakított adatok már betöltve a saját
  `run/mariadb/data` adatkönyvtárába. A négy adatbázis 102 táblás, az SQL-port
  `127.0.0.1:3307`. Az import után minden tábla sorainak száma és tartalmi
  ellenőrzőösszege egyezett; a csomag 1101 generált motion/splash sora megmaradt.
- A csomag `db/backup` mappája az ellenőrzött, importálható SQL-adatokat tartalmazza.
  A `tools/mariadb` az eredeti hivatalos, SHA256-tal ellenőrzött MariaDB 11.4.13
  Windows-disztribúció; így a localhost futás nem függ a fejlesztői build cache-től.

Az átalakított aktív adatok fő számai:

| Adat | Sor |
|---|---:|
| Fiók | 3501 |
| Karakter | 209 |
| Karakterindex | 204 |
| Tárgy | 3078 |
| Tárgyprototípus | 10714 |
| Szörnyprototípus | 2044 |
| Skillprototípus | 132 |
| Bolt / bolttárgy | 85 / 922 |
| Összes aktív importált sor | 585443 |

Az összes régi tábla sorainak száma és a nem használt oszlopok/táblák listája a
`Database-Compatible/report.json` fájlban található. Az aktív forrás által nem
használt webes és történeti adatok az eredeti archívumban vannak; nem
tekintjük őket az új szerver aktív tábláinak.

## Adatmegőrzés és szükséges átalakítások

- A jelszóhashek, a 2550 bájtos skill- és a 108 bájtos gyorsgombadatok
  bájtra azonosak az eredetivel. Minden karakter és tárgy megmaradt.
- A `player_index` kizárólag `pid1`–`pid4` oszlopokat tartalmaz. Egy fiók
  korábban mind az öt helyet használta: az ötödik hivatkozás csak az eredeti
  archívumban maradt meg, kiválasztható ötödik hely és új megőrző fiók nincs.
  A karakter rekordja és hozzá tartozó adatok nem lettek törölve.
- Az ENUM/SET adatokhoz szöveg szerinti, illetve numerikus bitjelző szerinti
  átvitel tartozik. Az eltérő bónuszazonosítókhoz explicit megfeleltetés készült.
- Három régi bónuszdefinícióhoz nincs aktív implementáció: két `STATUS_INC`
  és egy wolfmanhoz tartozó `ATTBONUS_IRR_ARTIGLIO` sor csak az archívumban
  marad. A normál / ritka / nyolcadik bónusztáblák aktív sorszáma 66 / 15 / 7.
- Az érvénytelen, nulla hónapot tartalmazó öt lejáratidő-oszlop 1156-1156
  értéke `NULL` lett. Az eredeti értékek az archívumban megvannak.
- A binárisan tárolt fordítások UTF-8-ra alakultak. Érvényes UTF-8 esetén
  az eredeti szöveg marad; a régi magyar/cseh/lengyel/román mezőknél Windows-1250,
  töröknél Windows-1254, a többi nyugati nyelvnél Windows-1252 a visszaesési
  kódolás. A naplók régi szövegeinél Windows-1250 a helyi alap. Ez kódolási
  feltételezés, a neveket a Unity próbában is ellenőrizni kell. A szervercsomag
  SQL-kapcsolatai `utf8mb4` kódolást használnak.
- A 45 számos skillgörbékből a forrás által használt 0–40. szint változatlanul
  maradt meg; az aktív beállítás 41 számot tartalmaz.
- A régi AUTO_INCREMENT következő azonosítók megmaradtak. Ez üres tábláknál
  is fontos: például a guild következő azonosítója 120, nem 1.
- A régi két névellenőrző trigger és a teszteljárás nem futott le az import
  során. Az új szerver saját validációja működik; a régi objektumok az archívumban
  felülvizsgálhatók.

## A séma pontosításai

A tényleges mentés alapján a sémából kikerült néhány túl szigorú, korábban
következtetett egyediség. A szintlépési napló több sort enged ugyanahhoz a
karakternévhez; a bónuszdefiníciók és a régi birodalomváltási rekordok nem
vesznek el ismétlődő kulcs miatt. Az affect elsődleges kulcsa a mentéshez
igazodó `dwPID,bType,bApplyOn,lApplyValue` négyes.

A mobnévmezők 49 karakteresek. A mob `st` és `size`, a bónuszok `talisman`
mezői és a régi előjeles pénznapló-azonosítók tárolása elég széles ahhoz, hogy
az eredeti értékeket ne kelljen levágni. A C++ futásidejű BYTE-mezők mérete
nem változott: a mentésben levő 300-as mobértékek és 3000-es talismanbeállítás
balanszfelülvizsgálatot igényelnek. Az SQL-tárolás megőrzi ezeket, nem talál
ki új játékértékeket.

## Használat és ismétlés

A jelenlegi gépen az adatbázis már előkészített. A `Szerverkezelo.cmd` után a
Qt6 appban a kívánt csatornák indíthatók; az adatbázist a kezelő indítja el.
A CH1 jelenleg két game core-t és a közös CH99-et jelenti.

A `Server2-Local-Database-Ready.zip` az importálható mentést, a kezelőt és a
MariaDB-motort tartalmazza; a `run` alatti élő adatkönyvtár és naplók nincsenek
benne. Ezért új helyre kicsomagolva egyszer szükséges az alábbi előkészítés.

Újonnan kicsomagolt, adatbázis nélküli példányban az **Adatbázis előkészítése**
gomb, vagy a `server.ps1 setup-db` parancs végzi az inicializálást, az ellenőrzött
mentés betöltését és az ellenőrzést. A már létező adatkönyvtár újrainicializálása
elutasított. Külső átalakított mentéshez `import-db --backup <mappa>` használható,
de csak üres, saját adatbázison. A korábbi állapotról privát SQL-mentés készül;
hiba esetén automatikus visszaállítás következik. Megszakadt import naplója
blokkolja a szerver indítását a helyreállításig.

A séma és az átalakítás újra előállítható:

```powershell
python -B db/tools/adapt_backup.py --source D:/AurigaGlobal/Database `
  --output D:/AurigaGlobal/Database-Compatible-New --work D:/AurigaGlobal/Backup-Work-New `
  --mariadb-bin D:/AurigaGlobal/Server2-Local/tools/mariadb/bin
```

Az output és a munkamappa új legyen; az eszköz a forrásfájlokat nem módosítja.
A kész csomag fiók- és játékosadatokat tartalmaz.

## Ellenőrzések és futásidejű megfigyelések

- MariaDB 11.4.13: 102 tábla, 365 prepare és végrehajtási próba,
  175 invariáns, 6 karakterhely-migrációs ellenőrzés, 0 feloldatlan SQL-hiba.
- 8 mentési/importteszt, köztük valódi betöltés és szándékosan megszakított
  részimport utáni automatikus visszaállítás; 21 csomag/felügyeleti teszt;
  18 szerkesztői háttérteszt és 11 grafikus teszt sikeres.
- Mind a tíz referenciaadat-ellenőrzés READY, mind a 15 natív konfiguráció
  ellenőrzése sikeres. A CH1 két core-ja, a CH99 és a LoginServer valódi,
  tanúsítvány-ellenőrzött TLS 1.3-kapcsolatot fogadott.
- A natív próba feltárt és javított egy korábbi TXT→JSON hibát: fejléc nélküli
  Dragon Soul sorok cellái elvesztek. Az öt GRADE_BRILLIANT sor 35 eredeti száma
  visszakerült; az ismételt CH99-indításnál az ehhez tartozó hibák megszűntek.

Nyitott motor- és adatfeladatok, amelyeket a séma önmagában nem old meg:

1. CH1 core2: nagy map/NPC-terhelés mellett az indulás több perc, és a
   CHARACTER_MANAGER felszabadítása túllépte a felügyelő 60 másodperces
   leállítási határidejét. A saját teszt-core SQL/item flush után fejeződött be
   kényszerített folyamatzárással; a Database és a MariaDB szabályosan állt le.
   A bizonyíték a csomag `run/integration/slow-shutdown.json` fájlja.
2. A felügyelő jelenleg a figyelőportokat tekinti indulási jelnek; a teljes
   mapbetöltés befejezése és a game TLS-kézfogás később következhet. A kész
   állapot jelzését valódi alkalmazáskészültséghez kell majd igazítani.
3. A guild power/handicap képlethez a jelenlegi forrás két számot fűz egymás
   mellé (`0 0`), amely érvénytelen kifejezés. Ez külön forrásjavítás.
4. A teleporter konfigurációja és néhány opcionális map regen/boss/dungeon
   fájl hiányzik. Az üres `priv_settings` tábla félrevezető hibabejegyzést okoz.
   A mentésben a guild tábla üres, miközben régi tag/grade rekordok vannak;
   hiányzó céhadatot nem találtunk ki.
5. Teljes Unity-belépés, karakterválasztás, mozgás, harc és játékosállapot
   újraindítás utáni ellenőrzése még szükséges. Éles vagy FreeBSD-migráció nem történt.

A portable szerver forrása és ellenőrzőösszege:
[MariaDB 11.4.13 Windows csomagok](https://archive.mariadb.org/mariadb-11.4.13/winx64-packages/).
A `CREATE IF NOT EXISTS` önmagában nem migrál meglévő oszlopokat:
[MariaDB CREATE TABLE](https://mariadb.com/docs/server/server-usage/tables/create-table).
