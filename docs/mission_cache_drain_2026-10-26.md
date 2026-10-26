# Mission Engine 4/D–4e – natív Database player/item cache-drain

Dátum: 2026-10-26. Ág: `codex/mission-engine`. Csak helyi fejlesztés és teszt.

## Elkészült Database-adapter

Az új `CClientManager::BeginPlayerCacheDrain` és `PollPlayerCacheDrain` a 4/D–4d
natív betöltési lease-ét használja. Az aktuális logon, autentikált peer, pontos
account/karakter/descriptor és az összes kötelező betöltési rész szükséges.
Legacy fallbackból vagy még függő betöltésből nem készíthető ilyen nyugta.

A művelethez friss, nem nulla challenge kell. Egy login egy előkészítési műveletet
birtokolhat; ugyanaz a challenge idempotens, másik nem cserélheti le a folyamatban
lévő vagy lezárt műveletet. Új belépés új lease-t kap. A nyilvántartás legfeljebb
256 karaktert tart, és a lejárt sessionök bejegyzéseit a Database főciklusa törli.
Másik session/challenge lekérdezése nem törli az aktuális bejegyzést.

A view a tényleges player-cache-t és a teljes item-cache map karakterhez tartozó
sorait gyűjti össze, legfeljebb 1024 tárgyig. Nem csak a legacy owner-set indexre
hagyatkozik. Nincs cache-eviction vagy új snapshot kitalálása: a GameServernek a
végleges player-pillanatképet előbb a Database-be kell juttatnia. Ha ilyen cache
még nincs, az előkészítés elutasított. A sorok azonosítóit és ownerét ellenőrizzük;
hiányzó, duplikált, idegen vagy túlméretes view nem indít flush-t.

Az adapter a meglévő `CPlayerTableCache::Flush` / `CItemCache::Flush` mentőkódot
hívja. Csak a dirty sorok küldenek SQL-t, a cache és az item-tombstone megmarad.
Az itemablak-nevek korábbi leképezése változatlanul a cache SQL-kódjához került,
így a teszt is a termelési leképezést és DML-építőt futtatja.

Ezután a valódi `CDBManager::DrainPlayerWrites` mindkét player-workerének
owned FIFO-ticketje készül el. A baseline kizárólag a két worker különböző,
hibamentes induló origója lehet. Egy későbbi megfigyelési ponttal nem lehet régi
SQL-hibát lenullázni. A receipt a sessiont, challenge-et és mindkét lane tényleges
sequence/hibanyomát hordozza; nem küld SQL-szöveget vagy jelszót.

## Cache-generáció és érvénytelenítés

A közös cache saját monoton példányazonosítót és módosításgenerációt kapott.
A `Put` és az item `Delete` módosítja a generációt; olvasás vagy puszta flush
nem változtatja meg. Azonos végső adattal végződő A → B → A módosítás és az
azonos adatokkal újragyártott cache is különböző. A cache nem másolható, a
64 bites számláló kimerülése nem eredményezhet azonosító-újrahasználatot.

A drain csak saját értékeket, cache-stampokat és szálbiztos worker-ticketeket
őriz. Borrowed cache-pointer vagy ECS/descriptor-pointer nem él túl egy
főszálas `Begin`/`Poll` hívást. Pollkor az aktuális view újraellenőrzött.
Dirtyvá válás, újabb Put/Delete, tulajdonos-/tagság-/példánycsere blokkolja a
nyugtát. Kijelentkezés, peer-vesztés vagy azonos PID/handle melletti újrabelépés
után a régi eredmény `Stale` lesz. Egy blokkolt nyugta nem válik ismét sikeressé.

| Állapot | Jelentés |
|---|---|
| Pending | Legalább az egyik tényleges SQL-ticket még függő |
| Flushed | A rögzített cache-mentések mindkét FIFO-határig SQL-hiba nélkül lefutottak, és a view változatlan |
| Blocked | Cache-változás, SQL-bizonytalanság/hiba, hibás vagy megszakadt lane |
| Stale | A natív login/session már nem aktuális |

## Pontos határ és folytatás

**A `Flushed` nem source-stop és nem írási jogosultság.** A GameServer gazdasági
termelőinek megállítása, a későbbi mentések tiltása/sorba állítása és a tartós
Mission/resource writer-lease átadása még nincs bekötve. Ezért a receipt nem
állítja át automatikusan a `ResourceFence` állapotát, és nem nyitja a Gameplay-kaput.

A bizonyíték a felsorolt cache-sorokra és a két ticket előtti queued SQL-re
vonatkozik. Nem két-lane-es atomi tranzakció, nem igazol SQL-rowcountot vagy
kanonikus resource-revisiont, és nem fedi le a később kiadott, direkt vagy másik
SQL-kapcsolatos írást. A nyers cache-pointeren végzett közvetlen mezőírás sem
válik ettől támogatott mutation API-vá. A következő integráció ezeket külön
rendezi; a safebox/offlineshop és a többi legacy író továbbra is átvezetendő.

Az új API csak natív Database-előkészítés. Nincs hozzá új GD/DG/Unity parancs,
a futó gameplay nem kezdeményezi automatikusan. A következő rész a GameServer
source-stop/final-snapshot protokoll és a folyamatközi nyugtázás, majd a tartós
writer-handoff, guarded DML és ECS/cache-rebase. A korábbi egyszeri ASAN
hatásnyugta-visszaolvasási észlelés oka külön, továbbra is nyitott.

## Helyi ellenőrzések

A `PlayerCacheDrainTests` a termelési cache-, player-save-, ablaknév- és drain-kódot
futtatja. A natív próbák lefedik az engedélyezési határt, hibás view-t, 1024/1025
soros korlátot, A → B → A változást, tombstone-t és unavailable workert.
A külön MariaDB-próba az eredeti `db/schema.sql` player/item DDL-jét veszi át
privát táblanevekre, és két tényleges FIFO-workerrel hajtja végre a mentéseket.
SQL advisory lock tartja vissza a workereket, tehát a pending-állítás nem
időzítésre épül. Külön esetben csak a második lane marad függő.

A 13 valódi SQL-eset között sikeres flush, korábbi async SQL-hiba, a tényleges
player-UPDATE-et hibára futtató trigger, késői item/player módosítás, Delete,
cache-eviction/újragyártás, logout, azonos PID/handle melletti újrabelépés és
kétszer ugyanaz a lane szerepel. A termelési CClientManager-bekötés teljes
Database-builddel ellenőrzött; ez nem két szerverfolyamatos vagy Unity-játékteszt.

Végleges helyi eredmények:

- Natív cache-drain suite: **38** ellenőrzés, normál és ASAN.
- Két tényleges workerrel, canonical DML-lel: **339** ellenőrzés, normál és ASAN.
- Célzott normál CTest-regresszió: **29/29** sikeres, a valódi Mission/item-award
  crash/restart próbákkal együtt.
- Natív ASAN CTest-regresszió: **17/17** sikeres. A cache-drain MariaDB-suite
  külön, saját ideiglenes példányon futott ASAN alatt.
- GameServer, Database, LoginServer: Windows x64 RelWithDebInfo build normál/ASAN.
- A CClientManager új tagjai miatt az érintett DatabaseTimeTests mindkét
  konfigurációban újrafordítva és sikeres.

Helyi fejlesztői naplók (nem verziózott buildtermékek):

- `build/mission-cache-drain-native-build.log`, `build/mission-cache-drain-native-asan-build.log`.
- `build/mission-cache-drain-final-build.log`, `build/mission-cache-drain-final-asan-build.log`.
- `build/mission-cache-drain-regression.log`, `build/mission-cache-drain-asan-regression.log`.
- `build/mission-cache-drain-asan-mariadb.log`.

A 34 védett szezonális Mission Pass-fájl és a nyolc korábbi Unity-fájl
hash-ellenőrzése változatlan eredményű.
Nem változott SQL-séma, helyi játékadatbázis, Unity vagy távoli gép.
