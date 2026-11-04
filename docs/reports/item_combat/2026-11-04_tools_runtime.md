# C25b — Gyűjtőeszközök, fejlesztés és csalizás

2026-11-04. Szerződés: [IC-054](../../adr/item_combat_054_tools_runtime.md).

## Elkészült

A csákány, horgászbot, fejsze és metszőolló tartós példányállapota, kézhelyre
felszerelése, adatablaka és NPC-nél fejlesztése; csali alkalmazása a felszerelt
botra. A szerver, SQL-mentés, protokoll, Unity-kodek, hálózati kezelés és az
inventoryból elérhető megerősítés együtt készült és helyben ellenőrzött.

A fogás/bányászat/gyűjtés és az ezekből szerzett gyakorlat még C25c. Az élő
fejlesztési próba adminisztratívan adott, ellenőrzött kezdőpontokat használt.
A védett practice API csak domainben és SQL-ben futott, nem valódi resource
eseményből. A normál legacy leltár bekötése a felhasználó kérésére még vár.

## Ténylegesen lefutott ellenőrzések

| Ellenőrzés | Eredmény |
|---|---|
| Release GameServer / Database / LoginServer és natív segédek | sikeres |
| Item-CTest a gathering és tool runtime csoportokkal | 24/24 csoport |
| ToolRuntimeTests | 32 célzott ellenőrzés |
| ItemPersistenceSqlTests külön saját MariaDB-ben | 1804 ellenőrzés |
| ItemBenchToolTests külön saját MariaDB-ben | sikeres |
| PointCalculationTests | 7398 ellenőrzés |
| GmCommandTests | 31564 ellenőrzés |
| Unity 6000.3.27f1 scriptfordítás | sikeres |
| Új golden/kodek/csonkítás/TCP split/dispatcher/UI validátor | 11800 ellenőrzés |
| Korábbi C4–C25a Unity-validátorok | mind sikeres |
| Valódi Unity → TLS → GameServer → SQL/UI | 54 ellenőrzés |
| Forrássnapshot | 5524 fájl, 1744 byteazonos C# |
| Korábbi helyi tárgyak | mind a 129 teljes állapot, revízió és hash egyezik |
| Learning, karakterfejlődés és hely | változatlan; teszt HP/SP visszaállítva |
| Manager build, migráció és telepített binárisok | hash-ellenőrzött |

A domainpróba a teljes példánymegőrzést, fegyvercserét, a gyűjtési erő harctól
függetlenségét, gyengébb csali felülírását, az aktív gyűjtés tiltásait,
fejlesztési esélyhatárokat, pontvesztést, szint-visszaállítást, maximumot és
a védett gyakorlást ellenőrzi. SQL-ben a bot hibás before-state-je a csali
levonását is visszagörgeti; a receipt csak egyszer engedi a műveletet.
A pont és a csali hash-hamisítása betöltési hibát ad, az új owner-session
a teljes felszerelt állapotot megőrzi.

Az élő Unity-próba tényleges inventory- és megerősítő gombokat használt.
Új fegyver növelte a támadást; a nagy gyűjtési erejű botra cserélve az érték
pontosan visszaállt a pusztakezes értékre. Mind a négy eszköz kézre került.
A magas szintkövetelményű próbaeszköz felszerelése elutasítást kapott.

Az erős csali egy darabot fogyasztott, a gyengébb utolsó darab pontosan
felülírta; a bot gyakorlata megmaradt. Felszerelt bot nélkül nem fogyott csali.
A horgász előnézetet adott, de eltávolodás után a megerősítés és az új
előnézet is elutasítást kapott. Visszatérés után a bot ugyanazzal az ID-val
+1 szintet kapott, pontja nullázódott, csalija megmaradt. A következő szintet
a hiányzó gyakorlat blokkolta. A 0%-os fejlesztés a 3 pontból 2-t hagyott,
a maximális szint elutasítást kapott.

Az utolsó erős csali elfogyása után a mentett eredmény olvasható maradt.
Újrabelépés visszaadta a felszerelt bot pontos ID-ját, szintjét, pontját és
csaliját; a régi login megerősítése nem ismételte a műveletet. Levétel után
a kézhely és a statok is pontosan visszaálltak. A fejlesztés és csalizás
ismételt megerősítése sem növelte újra a szintet, sem nem fogyasztott ismét.

## Helyi csomag és verziókezelés

Csak az igazolt helyi próbakarakter kapott új C25b tárgyakat. A player/protos
SQL, korábbi binárisok, manifest, karakter, learning, tárgyhash-ek és NPC-fájl
mentése a csomag `run/deployments/tools-*` könyvtárában van. A teljes korábbi
manager külön mentésben maradt. A katalógus v26; az új példánymigráció 009.

A próba után 139 élő itemállapot van. A korábbi 129 minden mezője/hash-e
megmaradt. A bot megőrizte a +1 szintet, 0 pontot és 50-es csalit, a sikertelen
eszköz 2 pontot; az új csalik újra rendelkezésre állnak (1 gyenge, 2 erős).
A horgász és kovács külön helyi NPC-fixture a teszt kiindulási helyén;
az összes korábbi NPC-sor, köztük a C25a tűz megmaradt. Öt saját szerverfolyamat
újra fut, az ellenőrzött binárisokkal és manifesttel.

Unity: **cs65** a `/main/codex-mission-engine` ágon, kizárólag a 29 saját fájllal.
A jelenet-, világasset-, kapcsolati asset-, Packages- és ProjectSettings-
módosítások kimaradtak. A ProjectSettings.asset ellenőrzött hash-e változatlan.
Az eredeti Editor nyitva maradt, a próba külön forrássnapshotban futott.

## Korlátok

Tényleges halászati/bányászati esemény, jutalom és abból származó gyakorlás
még nem készült; ezek C25c. Az eszközmodell/mozgás bekötés fordult, a teljes
világjelenet és minden végleges eszközasset vizuális ellenőrzése nem futott.
A snapshotban hiányoznak a SpeedTree/VFX/Crest világassetek, playerbuild nem
készült. Arena/PvP tiltások fordultak, külön élő arena/PvP-próba nem futott.
A kovács út fordult, a fejlesztés élő NPC-próbája a horgászt használta.
Külön C25b Qt manager UI-próba, FreeBSD-build, több core közötti handoff,
teljes hibainjektálás/stressz és éles migráció nem futott. A teljes adatkonverzió,
legacy leltárátállás, C19 HP/SP crash-atomikusság és az F7 kapuk nyitottak.
