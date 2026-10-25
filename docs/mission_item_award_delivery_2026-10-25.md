# Mission Engine 4/D–2 – az item_award tényleges tárgykézbesítése

Dátum: 2026-10-25. Szerverág: `codex/mission-engine`.

## 1. Elkészült rész és határa

Elkészült a közös, meglévő **item_award → item → SAFEBOX/MALL** kézbesítési
útvonal atomi javítása. A valódi tárgy létrehozása és az award átvételi
nyugtája egy tulajdonolt prepared tranzakcióban történik. Az új helper már
be van kötve a Database szolgáltatás raktárbetöltési útvonalába.

Ez az egyedi award kézbesítése. A Mission action ledger és az award létrehozása
még nincs összekötve. Nem készült el a teljes RewardBundle közös, atomi
végrehajtása, az XP, pénz, költségfogyasztás vagy az élő ECS-inventory
erőforrásprotokollja. A Gameplay binding kapuja továbbra is zárt.

Nem történt helyi futó szerver vagy távoli gép frissítése, tényleges
játékosadatbázis-módosítás, Unity-módosítás vagy push.
Korábbi, külön írásokkal működő verzióból származó árva tárgyak esetleges
adatjavítását ez a kód nem végzi el; meglévő adatoknál ezt külön kell vizsgálni.

## 2. A kódban talált hibák

Korábban a `RESULT_SAFEBOX_LOAD` külön `INSERT`-tel létrehozta a tárgyat,
majd az `ItemAwardManager::Taken` egy másik queue-munkában írta az átvételt.
A tárgy létrehozása utáni, de a taken UPDATE előtti leállás esetén az award
átvehető maradt, és újabb tárgy keletkezhetett.

További konkrét hiba: az INSERT kézzel megadta a `GainItemID()` eredményét,
az eredményt mégis a `uiInsertID` alapján vizsgálta és töltötte a csomagba.
A kanonikus `item.id` nem AUTO_INCREMENT, ezért a sikeres INSERT itt nulla
insert_id értéket ad. A teszt ezt az eredeti sémán külön igazolja. Az új
kód a lefoglalt tárgyazonosítót használja az SQL-ben, a nyugtában és a
GameServernek küldött tárgyban is.

A prototípus-/idő-/random-normalizálás korábban a közös award-cache adatait
módosította. Most helyi másolaton történik; az eredeti SQL-tartalom megmarad
az egyezés ellenőrzéséhez és a sikertelen kísérlet utáni újrapróbáláshoz.
A socketek előjeles tárgymezőbe kerülése bithelyesen történik; a `lockedattr`
SQL- és csomagértéke egyaránt -1.

## 3. Kézbesítési szerződés

Az `ItemAwardDelivery::{Deliver,Resolve,DecodeReceipt}` saját adatokat birtokló
SQL-terveket/eredményt ad. Nem módosít ECS-t, és nem foglal új tárgyazonosítót.
Az azonosítót a hiteles Database folyamat meglévő, saját item-ID-tartományából
kell egyszer lefoglalni. Nincs új MAX(id)+1 vagy automatikus számozás.

A trusted hívó feladata a bejelentkezett account/login összerendelése,
a prototípus ellenőrzése és normalizálása, valamint az, hogy a tároló
betöltés alatt zárva legyen. Az útvonal egyetlen hiteles Database
szolgáltatást feltételez. Nem általános, több írófolyamatra kiterjedő
inventory-lease és nem kliensről hívható API.

A lépések:

1. Az `item{postfix}`, `safebox{postfix}` és `item_award` InnoDB motorának,
   az engedélyezett FK/CHECK ellenőrzésnek és a három tábla triggermentességének
   ellenőrzése. Eltérésnél írás nélkül elutasítás történik.
2. A hiányzó alapértelmezett safebox-sor létrehozása, majd az account sorának
   zárolása. Az award-kiosztók azonos accountsor → award sorrendben zárolnak.
3. Az award eredeti login/vnum/count/mall/socket/attribute mezőinek aktuális,
   zároló összevetése. Átvétel csak `taken_time IS NULL AND item_id=0` esetén.
4. A tároló teljes elhelyezési snapshotjának összevetése: sorok száma,
   tárgyazonosító, pozíció és vnum. Többlet-, hiányzó vagy elmozdított tárgy
   elutasítja a régi snapshotra épített kiosztást.
5. A konkrét tárgy INSERT-je, majd az award taken_time/item_id UPDATE-je,
   végül közös COMMIT. Bármely korábbi hiba mindkét írást visszagörgeti.

A geometriai vizsgálat a meglévő ötoszlopos függőleges rácsot követi. Korlátos
a tároló, a prototípusmagasság, a pozíció, a snapshot és az SQL-paraméterszám.
Nem csak a tárgy kezdőcellája számít: a teljes magasság helyet foglal.
Telt raktárnál a helper nem ad SQL-írásokat; az award átvehető marad.

Ez a pillanatkép az adott zárt tárolóra vonatkozik. Nem teszi biztonságossá,
ha egy másik szolgáltatás protokollon kívül ugyanebbe a nyitott inventoryba ír,
vagy karbantartás közben átépítik a táblákat. A kanonikus, friss sémára épül;
régi MyISAM vagy egyedi triggerek automatikus migrációját nem végzi el.

## 4. Bizonytalan eredmény és raktárbetöltés

| Eredmény | Database viselkedése |
|---|---|
| Megerősített COMMIT | Cache `MarkTaken`, a tényleges lefoglalt ID-jű tárgy a válaszba kerül |
| Konfliktus / hiba / elveszett COMMIT-válasz | Nincs automatikus újra-INSERT; aktuális, zároló award-olvasás következik |
| Az egyeztetés Taken eredményt ad | Csak a cache lesz átvett; a nyugta alapján nem készül új inventory-tárgy |
| Az eredmény nem megerősített | Egyszeri friss item SELECT, további award-claim nélkül; a régi snapshot nem kerül publikálásra |
| Nincs hely | Az award érintetlen, későbbi nyitáskor ismét átvehető |

Az egyeztetés megvárja az eredeti award-írót, így egy még futó tranzakció
mellett nem állíthatja idő előtt, hogy a jutalom még átvehető. A receipt
Taken marad akkor is, ha a játékos később elmozgatja vagy elfogyasztja a
tárgyat. A tárgy hiánya nem engedélyezi annak újragyártását.

Az `ItemAwardManager::MarkTaken` kizárólag cache-művelet. A régi, később
végrehajtott külön taken UPDATE megszűnt; egyetlen másik hívója sem volt.

A meglévő Database-betöltési ág közvetlen SQL-t használt; ezt a javítás
egy közvetlen, teljes tranzakcióra cseréli. Ez továbbra is blokkoló munka a
Database folyamatban, díjanként több ellenőrzéssel. Az új Mission dispatchernek
az owned tervet aszinkron workerrel kell végrehajtania; a GameServer
főszálába ilyen közvetlen várakozás nem kerülhet.

## 5. Valódi sémás ellenőrzés

A `ItemAwardDeliveryTests` a `db/schema.sql` eredeti item, item_award és
safebox DDL-jét olvassa ki egy friss, kizárólag a teszt által létrehozott
adatbázisba. Nem egyszerűsített marker-tárgytáblát használ. A runner saját
MariaDB 11.4.13 folyamatot, adatkönyvtárat és loopback portot indít.

Lefedés:

- Nem AUTO_INCREMENT item ID, konkrét socket/attribute/lockedattr értékek.
- Ismételt kérés azonos és más javasolt tárgyazonosítóval.
- Később törölt/elfogyasztott tárgy receiptje, újragyártás tiltása.
- Hiba az item INSERT és a receipt UPDATE között, illetve mindkettő után,
  még COMMIT előtt: teljes rollback.
- Megváltozott award-tartalom, ütköző tárgyazonosító, elavult elhelyezési snapshot.
- Telt raktár, majd egy felszabadított cellába sikeres, egyszeri átvétel.
- Ugyanazért az awardért és ugyanazért az üres celláért versenyző két kapcsolat.
- Az item INSERT utáni SQL-kapcsolat kilövése és újrapróbálás.
- Az eredeti írót megváró külön egyeztető worker.
- A valódi COMMIT OK-válasz hálózati eldobása, majd deduplikált retry.
- Kényszerített MariaDB-leállítás, InnoDB crash recovery és új C++ folyamatban
  visszaellenőrzött receipt/tárgy darabszám.
- Táblapostfix, MALL/SAFEBOX célzás, MyISAM/letiltott FK/trigger elutasítása.
- A szezonális Mission Pass tábláját képviselő sentinel változatlansága.

A hibainjektáló teszt a PROCESSLIST-ben kizárja a saját kapcsolatát, és
pontos SELECT-prefixet vár. Enélkül a SLEEP-et kereső mérőlekérdezés saját
szövegét is megtalálhatta, ami az ASAN-körben tesztszinkronizációs hibát okozott.
A végleges normál és ASAN próbák már ezzel a javított szinkronizációval futottak.

| Ellenőrzés | Eredmény |
|---|---|
| Database és GameServer Windows x64 / RelWithDebInfo, normál és ASAN | Lefordult |
| Célzott normál regresszió | 30/30 |
| ASAN célzott CTest | 8/8 |
| Új tiszta terv-/határellenőrzések | 41, normál és ASAN |
| Valódi új MariaDB-suite, végleges normál és ASAN | 302 + 111 cold-process ellenőrzés |
| Védett szezonális / meglévő Unity-fájlok SHA-256 | 34 / 8 változatlan |

A polling miatt az ellenőrzések száma eltérhet. A 30-as és a 8-as kör után
a tesztszinkronizáció változott; a végleges új unit és DB-suite külön újrafutott
normál/ASAN alatt is. A production kézbesítő kód ezalatt nem változott.

Helyi, ignorált naplók: `build/mission-item-delivery-final-build.log`,
`build/mission-item-delivery-asan-build.log`, `build/mission-item-delivery-regression-tests.log`,
`build/mission-item-delivery-final-tests.log`, `build/mission-item-delivery-asan-unit-tests.log`,
`build/mission-item-delivery-asan-final-unit.log`, `build/mission-item-delivery-asan-db-tests.log`.

## 6. Következő függőség

A Mission-jogosultság és az átvehető award létrehozása kerüljön egy közös
tranzakcióba, stabil action/payload → award mappinggel, hiteles karakter/account
összerendeléssel és ismételt kérésnél ugyanazokat az awardokat visszaadó protokollal.
A kevert XP/pénz/cost bundle nem bontható csendben külön tárgyjutalmakra.

Az award-cache jelenlegi, csak növekvő ID-ra épülő betöltése külön auditot és
próbát igényel az új dispatcher előtt: az auto-increment kiosztási sorrend nem
azonos a párhuzamos tranzakciók commit-sorrendjével. A később commitoló kisebb
ID-jű awardot sem szabad kihagyni. A jelen rész a már ismert awardok tényleges
materializálását javította; ezt a forráslétrehozási/cache problémát nem állítja késznek.

Ezután következhet a többi erőforráshatás és a hiteles session-/core-koordinátor.
Csak a teljes út igazolt próbája után nyitható a Mission Gameplay binding kapuja.
