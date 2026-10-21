# Négy karakterhely fiókonként

A szerver `PLAYER_PER_ACCOUNT` értéke 4. A Unity kliens ugyanennyi karaktert
kezel a csomagolvasótól a Windows- és mobilválasztóig. Az ötödik helyre mutató
kiválasztás/létrehozás/törlés/átnevezés elutasított kérés.

A `TPacketGCLoginSuccess` headerje továbbra is 32, a mérete **337 bájt**:
`1 + 4 × (TSimplePlayer 65 + guild_id 4 + guild_name 13) + handle 4 + random_key 4`.
A belső `TAccountTable` mérete is megváltozik. A Database, LoginServer, GameServer
és Unity kliens összehangolt frissítése szükséges; régi ötkarakteres binárissal
nem keverhető. Az aktuális három szerverkomponenst újra kell fordítani és együtt
kell újraindítani.

A belépés, fiókcsatorna és birodalomváltás SQL-je csak `pid1`–`pid4` mezőket használ.
A kezdeti birodalomválasztás mind a négy karakter pozícióját frissíti; a korábbi
háromkarakteres ciklus és ötelemű birodalomváltó tömb megszűnt.

## Adatbázis

Új telepítésnél a `db/schema.sql` eleve négy oszlopot hoz létre. A már létező
adatbázishoz a [migráció](../db/migrations/2026-10-19_four_character_slots.sql) való.
Éles adatbázisra ezt a fejlesztés során nem futtattuk.

1. Mentsd az adatbázist, és állítsd le az összes Database/LoginServer/GameServer
   folyamatot. Író folyamat nem maradhat a migráció alatt.
2. A migráció `USE player` utasítását és a `CALL` táblanevét igazítsd a tényleges
   adatbázishoz és esetleges `TABLE_POSTFIX` utótaghoz.
3. Futtasd a migrációt MariaDB kliensből.
4. Az új szerverek és az új Unity kliens kerüljenek egyszerre használatba.

A migráció a korábbi ötödik karaktert az első üres helyre teszi az első négy közül.
Ha az ötödik mutató már az első négy valamelyikére mutat, a duplikált mutatót
megszünteti. Öt különböző, foglalt helynél a teljes migráció **adatmódosítás előtt
hibával leáll**: az ilyen karaktereket előbb másik fiókhoz kell rendezni egy külön
adatmigrációval. A script karaktert, tárgyat és questadatot nem töröl.

A rendezendő fiókok ellenőrzése a régi sémán:

```sql
SELECT id, pid1, pid2, pid3, pid4, pid5
FROM player.player_index
WHERE pid5 <> 0 AND pid1 <> 0 AND pid2 <> 0 AND pid3 <> 0 AND pid4 <> 0
  AND pid5 NOT IN (pid1, pid2, pid3, pid4);
```

Siker után a `pid5` oszlop megszűnik. A migráció ismételt futtatása a már négyhelyes
táblát változatlanul hagyja. A friss telepítési séma nem alakít át meglévő táblát:
a `CREATE TABLE IF NOT EXISTS` helyett erre a migráció szolgál.

## Ellenőrzés

- A szerver karakterválasztási regressziója minden 0–255 slotértéket ellenőriz.
- A kliens kiválasztás/létrehozás/törlés/átnevezés kérései szintén minden slotértékkel teszteltek.
- A valódi C++ headerből készített négykarakteres lista az éles Unity C# parserrel
  olvasható; a negyedik karakter és az eltolódott handle/random_key mezők is ellenőrzöttek.
- A TCP-szeletelés és a TLS-es login már a 337 bájtos listával fut.
- Az izolált MariaDB-próba ellenőrzi a friss sémát, mind a négy áthelyezési lehetőséget,
  a teljes fióknál való megállást, az adatmegőrzést és az ismételhetőséget.
- A Unity-prefabpróba mindkét platformon négy sort, helyes fizikai slotválasztást,
  tele fióknál letiltott létrehozást és üres hely újrafelhasználását ellenőrzi.

Ellenőrzött eredmény: Database/LoginServer/GameServer sikeresen újrafordítva;
49/49 szerverteszt normál és ASAN buildben; 23/23 kliens hálózati teszt;
365/365 SQL, 169 sémainvariáns és 6 migrációs ellenőrzés sikeres. A valódi C++
karakterlista 337 bájtos és a C# parserrel visszaolvasható.
A Unity runtime- és Editor-források az Editor saját C# fordítójával külön TEMP
kimenetre is sikeresen lefordultak. Mindkét prefab négy sort/portrét tartalmaz,
helyes belső hivatkozásokkal. Az új Unity futásidejű prefabpróba is sikeres:
`METIN2_FOUR_CHARACTER_SLOTS=SUCCESS slots=4 login=337 desktop=4 mobile=4 selection=physical create=free-slot`.
A teljes Unity/TLS-, mozgás-, skill/refine/shop-, csere- és végpontprofil-próba is
sikeres; a batch Editor és a loopback tesztszerver is 0-s kilépési kóddal állt le.
