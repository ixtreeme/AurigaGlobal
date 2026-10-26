# Mission Engine 4/D–4f – folyamatközi cache-drain és a későbbi mentések felismerése

Dátum: 2026-10-26. Ág: `codex/mission-engine`. Kizárólag helyi fejlesztés és teszt.

## Elkészült kapcsolat

A 4/D–4e natív Database cache-drain API-ja elérhető a GameServer belső kapcsolatán.
A GameServer `CLIENT_DESC::RequestPlayerCacheDrain` hívása és a Database
`HandlePlayerCacheRequest` kezelője ugyanazt a teljes natív session-kulcsot és
challenge-et használja. A peer és a descriptor továbbra is a hiteles kapcsolat
adata; a bejövő kérés ezeket nem választhatja át másik fiókra/core-ra.

| Irány | Header | Payload |
|---|---|---|
| GD | `PLAYER_CACHE_HELLO` (149) | 24 bájtos képességegyeztetés a meglévő link-tokenhez |
| DG | `PLAYER_CACHE_HELLO` (220) | 24 bájtos visszaigazolás |
| GD | `PLAYER_CACHE_REQUEST` (148) | 96 bájtos Begin vagy Poll, session-kulcs és challenge |
| DG | `PLAYER_CACHE_RECEIPT` (219) | 176 bájtos státusz, hibakategória és két SQL-lane eredménye |

A `Core/player_cache_protocol` formátuma explicit little-endian, `ACD1` magic,
1-es verzió, külön üzenettípus és nulla reserved mezők. A teljes sessionben a
link-, kérés- és session-token, Database-generáció, account és PID is benne van.
A nyugta mindkét lane azonosítóját, sorszámát, hibaszámlálóját, utolsó hibás
sorszámát, SQL-hibakódját és completion állapotát viszi át. SQL-szöveg, jelszó,
folyamaton belüli pointer vagy worker-ticket nem megy át a kapcsolaton.

Sikeres `Flushed` nyugtát csak két különböző, teljesült, nem nulla és nem kimerült
FIFO-határ, nulla hibanyom és megfelelő session/challenge mellett fogadunk el.
Pending/cancelled lane, hibás verzió, státusz, típus, méret vagy reserved bájt
nem tüntethető fel sikeres cache-flushként. Az első Pending két lane-azonosítója
és sorszáma rögzül; ugyanazon challenge későbbi pozitív válasza sem cserélheti őket. A meglévő 9 bájtos GD/DG külső
framing és a karakteradatok korábbi ABI-ja nem változott.

## Képességegyeztetés és életciklus

Az új képesség a 4/D–4d session-ACK után, külön Hello/ACK-kel egyeztetett.
Csak a meglévő HMAC-hitelesített, ugyanahhoz a linkhez tartozó kapcsolaton indul.
Régi Database esetén nincs cache-ACK és nincs cache-kérés. Régi GameServer
nem kezdeményezi az új protokollt. Kulcs nélküli vagy legacy betöltési fallback
nem válhat Mission-owner vagy hiteles cache-drain forrásává.

A descriptor saját értékeket birtokló `player_cache_wire::Client` állapotot kap.
A natív API a teljes betöltést, élő karaktert, account/PID-t és teljes EnTT-
entitásgenerációt ellenőrzi. A Database az aktuális, teljesen betöltött
lease-t és a valódi peer/descriptor egyezését is ellenőrzi.

Az első Begin újraküldhető, amíg nem jön ACK; az első Pending után Poll követi.
A lekérdezés legfeljebb 200 ms-onként történik, legfeljebb 256 figyelt művelettel.
A 10 másodperces határ vagy kapcsolatszakadás érvényteleníti a helyi bizonyítékot;
nem ad kész nyugtát, writer-lease-t vagy automatikus source-feloldást.
A Database azonos login/challenge ismétlésére nem flushol újra.

Lejárt session, másik challenge, újrabelépés, descriptor-/EnTT-generációcsere,
késői vagy ismételt terminális válasz nem alkalmazódhat az új karakterre.
Zárás és karakterválasztás törli az állapotot; új Database-link új képesség-
egyeztetést igényel. A nyugta csak az aktuális natív scope ellenőrzése után
olvasható vissza a `PlayerCacheReceipt` API-n.

## Később induló mentések

Az audit megmutatta, hogy több tényleges mentés `DBPacketHeader` + `Packet`
párral készül: item save/destroy/flush, quest-save, skill-color és cache-flush.
Ezért a megfigyelés a közös `CLIENT_DESC::DBPacketHeader` ponton történik.
A hagyományos `DBPacket` is ezen halad át.

Az ismert, tisztán heartbeat/control üzenetek megengedettek. Új mentés, vegyes
célú vagy ismeretlen GD-kérés a figyelt nyugtákat érvényteleníti. A besorolás
konzervatív és core-szintű: másik karakter ilyen kérése is elveheti a nyugtát.
A mentés továbbmegy, nincs csendben eldobott vagy újrapróbált gazdasági művelet.
Az érvénytelenített nyugta késői Flushed válaszból sem éledhet újra.

Ez a figyelő a cache-drain bizonyítékának érvényességét követi. Nincs automatikus
GameServer-kérés a futó gameplayben, nincs új Unity-packet vagy GUI-gomb.
A későbbi natív koordinátor a final player/item snapshot elküldése után hívja
az előkészítést; a jelen API nem állít elő és nem talál ki ilyen snapshotot.

## A tényleges source-stop fennmaradó határa

**A cache nyugtája és az új mentések felismerése még nem állítja meg az ECS
módosítását.** Nincs automatikus `ResourceFence::SourceStopped` visszaigazolás,
tartós writer-lease átadás, reward-dispatch vagy gameplay-aktiválás.

A kódban az alábbi termelőket kell együtt rendezni:

| Út | Konkrét belépési pont | Fennmaradó szerződés |
|---|---|---|
| Karakter mentése | `ecs/systems/SessionSystem.cpp`: SaveReal; `entity/char_manager.cpp`: FlushDelayedSave | Final snapshot és függő mentések, ugyanazon sessionben |
| Tárgy mentése/törlése | `item/item_manager.cpp`: SaveSingleItem, FlushDelayedSaveByOwner | Detached/ground/tombstone és tulajdonosváltás rendezése |
| Pénz/XP/progresszió | `ecs/systems/PointSystem.cpp`: Change; `PlayerRuntimeSystem.cpp`: SetGold/SetExp | A forrás megállítása a közvetlen ECS-módosítás előtt |
| Harc, quest, aktivitás | CombatSystem, quest callbackek, heartbeat/event_process | Nem veszhet el a várakozás alatt érkező esemény; safe pont vagy generációhoz kötött halasztás kell |
| Két karakter közötti művelet | Exchange, shop, pickup/ownership | Közös tranzakciós/scope szabály; féloldali fagyasztás nem elég |
| Raktár/offlineshop/pet/switchbot | Saját mentők és callbackek | Külső item/account források és folyamatban lévő műveletek rendezése |
| Külön SQL-kapcsolat | `network/db.cpp`: DBManager és AccountDB; legacy Lua SQL API | Nem a Database két player-lane-jén fut; külön hiteles drain/tiltás szükséges |

A mentésig még el nem jutott ECS-változást a GD-figyelő nem tudja visszavonni.
A GameServer saját DBManager/AccountDB és más core-ok/direkt SQL-írók sem
kerültek ettől automatikusan a két Database-lane bizonyítékába. Az események
csendes eldobása vagy csak a későbbi mentés letiltása nem megfelelő source-stop.

A következő rész egy safe ponthoz kötött, generációt őrző karakter-koordinátor,
a producer- és cross-character útvonalak átvezetésével. Utána kapcsolható hozzá
a most elkészült cache-drain csatorna, a tartós writer-handoff,
a guarded legacy DML és az ECS/cache-rebase. A korábbi egyszeri ASAN történeti
hatásnyugta-visszaolvasási észlelés külön nyitott vizsgálati tétel marad.

## Ellenőrzés

A codec-/állapotpróbák a tényleges Core-kódot használják: külön Hello- és
üzenettípusok, minden csonkolt keret, reserved/verzió-/státuszhibák, hibás SQL-
nyugták, két külön lane, session/challenge/entitáscsere, Begin retry, Poll,
terminális duplikáció, timeout/disconnect, írófejléc és 256 sessioncsere.

A két valódi SQL-workeres suite a termelési Cache/Save/DML kódját futtatja,
majd a nyugtát a tényleges ToWire → encode → decode → natív Client úton vezeti át.
A régi és új belépés közötti késleltetett SQL-válasz külön suite-ban ellenőrzött.
A GameServer/Database packetkezelő és poller bekötése teljes binárisbuilddel
ellenőrzött. Ez nem a két teljes szerverfolyamat közötti socketes/Unity elfogadás.

Végleges helyi eredmények:

- Codec és natív cache-client: **1506** ellenőrzés, normál/ASAN.
- Cache/DML → két worker → wire → natív Client: **404** ellenőrzés, normál/ASAN.
- Natív betöltés/session: **818**, izolált késleltetett SQL-lel **826**, normál/ASAN.
- Célzott normál CTest-kör: **30/30** sikeres.
- Natív ASAN CTest-kör: **18/18** sikeres, továbbá két külön ASAN MariaDB-suite.
- GameServer, Database és LoginServer: Windows x64 RelWithDebInfo build normál/ASAN.
- Az érintett DatabaseTimeTests mindkét konfigurációban újrafordítva és sikeres.

Helyi, nem verziózott fejlesztői naplók:

- `build/mission-cache-wire-native-build.log`, `build/mission-cache-wire-native-asan-build.log`.
- `build/mission-cache-wire-final-build.log`, `build/mission-cache-wire-final-asan-build.log`.
- `build/mission-cache-wire-regression.log`, `build/mission-cache-wire-asan-regression.log`.
- `build/mission-cache-wire-final-asan-mariadb.log`, `build/mission-cache-wire-final-asan-session-mariadb.log`.

A védett 34 szezonális Mission Pass-fájl és a nyolc korábbi Unity-fájl
hash-ellenőrzése változatlan eredményű. FreeBSD build és Unity gameplay-próba
ebben a fázisban nem történt.
SQL-séma, helyi játékadatbázis, Unity és távoli gép nem módosult.
