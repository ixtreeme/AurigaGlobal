# Mission Engine – bolti admission és ajánlatellenőrzés

Dátum: 2026-10-26. Részfázis: **4/D–4l**. Csak helyi fejlesztés és tesztelés.

## Hatókör

A hetedik követett műveletcsoport a `Shop`. A natív cache-előkészítés a
korábbi hat csoport mellett ezt is pause-olja. A változás nem hoz létre új
Mission SQL-submitot, tartós writer-lease-t vagy teljes SourceStopped nyugtát.

A tényleges `ShopSystem::Buy`, `MultipleBuy` és a manager vásárlás/eladás
belépései tartják az admissiont a hívás, előkészítés és publikálás idejére.
Normál/extended NPC-boltnál a vevő, online játékosbolt esetén a vevő és az
eladó atomikusan együtt kap engedélyt. Az egyikük pause-a egyik oldalt sem
engedheti részlegesen belépni. A komponenskonstrukció után a boltot és a
tulajdonost frissen ellenőrizzük; az engedély birtokolt actor-state-et tart,
nem ECS-pointert. Az offline boltok DB-alapú, aszinkron adásvétele külön feladat.

Az admission aktivitáskövetés és célzott pause, nem általános reentrancy-lock.
Az előző részek tíz másodperces, pre-submit recovery szerződése változatlan;
ez későbbi CommitUnknown helyzetek feloldására továbbra sem használható.

## Javított hibák

- A normál bolt negatív árat fogadhatott el, illetve nulla/negatív darabszámot
  továbbíthatott a gyárnak. Ezek a gyártás és terhelés előtt elutasulnak.
  A normál bolt szándékos, nulla aranyárú ajánlata továbbra is megengedett.
- A batch teljes aranyára, az extended másik birodalomra érvényes háromszoros
  ára és az eladási egységár × darabszám előbb szorzódott, mint ahogy a korlátot
  ellenőriztük. A közös ellenőrző segéd most a szorzás előtt zárja ki az
  előjeles 64 bites túlcsordulást. Ismeretlen extended pénznem elutasul.
- Azonos tárgypénznem több ártételének külön ellenőrzése nem igazolta a teljes
  árat. Most a közös egyenlegre összesítünk, a batch szorzójával együtt;
  a legacy `int` alapú számláló fölötti igény és a hibás ártétel elutasul.
- A kiválasztott ajánlat/tulajdonos és extended tab másolat. A gyártás és
  üreshely-keresés után újra ellenőrizzük az aktuális boltot, vendégjogot,
  ajánlatot, session-admissiont, előállított mennyiséget és egyenleget.
  A megszűnt/átárazott ajánlat első terhelése nem indulhat el.
- Előkészítési hiba után csak a még tulajdonos nélküli, RESERVED gyártmány
  takarítható. Callback által már karakterhez adott tárgyat nem törlünk.
- A batch korábban több nem sikeres válasznál is sikeres vásárlást számolt.
  Most minden nem-OK eredménynél megáll és azt adja vissza.
- Az NPC-nek történő teljes eladás a stack méretét a kérés 16 bites típusára
  csonkolhatta. A tényleges mennyiség külön 32 bites érték; nincs 65535-ös
  csonkolás. Idegen, lockolt, pending vagy cserélt tárgy nem adható el ezen az úton.
- Az eladás újra ellenőrzi a nyitott bolt vendégjogát és az NPC mapját/távolságát.
  A cooldown csak sikeres fogyasztás után frissül, eltelt idővel, uint32-wrap
  mellett is. Sikertelen eladás nem naplóz megtörtént pénzmozgást.
- A manager hibaválaszának küldése null descriptorral nem dereferál pointert.

## Fontos fennmaradó határ

Ez a belépés és az első mutáció előtti érvényesség védelme, **nem a teljes
adásvétel atomikus átírása**. A legacy kód a pénzt/tárgypénznemet, a célba
helyezést/merge-et, a forrásfogyasztást és az eladói jóváírást még külön
szolgáltatáshívásokkal módosítja. Nem szabad ezek közötti callback-,
elhelyezési vagy mentési hibára új rollback-/crash-biztonságot állítani.
A már első mutáción túljutott művelethez külön commit- és recovery-szerződés kell.

Következő feladat a tényleges NPC-adásvétel: előkészített tárgy-, költség-,
pénz- és slotterv, közös memóriabeli commit, majd publikálás. Ezután következik
az online játékosbolt két szereplős átadása és az offline boltok DB-koordinációja.
A raktár, pet, timer/quest/harc/XP/pénz, közvetlen SQL és az alacsony szintű
primitívek miatt a teljes producer-stop továbbra sem igazolt.

## Helyi ellenőrzés

A ShopLifecycleTests a tényleges shop.cpp és shop_manager.cpp implementációkat
futtatja, kontrollált item/point/session/DB/inventory szolgáltatásokkal.
A ResourceAdmissionTests a tényleges Core és ECS admission kódját használja,
mind a hét művelet és a 127 lehetséges nemüres pause-maszk ellenőrzésével.
Ez nem teljes inventory-commit, két folyamat közötti vagy Unity-játékpróba.

A végső célzott CTest-kör normál és ASAN konfigurációban **2/2** sikeres:

- Core/ECS admission: **2018** ellenőrzés mindkét konfigurációban.
- Tényleges shop/manager lifecycle és ajánlathibák: **50207** ellenőrzés mindkettőben.
- A szorzási segéd szélsőértékeket és 50000 kis érvényes ár/mennyiség párt is vizsgál.
- Vevő/eladó pause, callback alatti aktivitás, factory/space közben megszűnő,
  kiürülő vagy átárazott bolt, hiányzó vendégjog, csökkenő egyenleg,
  hibás ár/quantity/coin, duplikált itemprice, sikertelen retirement,
  nagy teljes stack eladása és cooldown-wrap is próbált.

A végső kód teljes helyi ellenőrzése is sikeres:

| Ellenőrzés | Eredmény |
|---|---|
| GameServer, Database, LoginServer | Mindhárom lefordult normál és ASAN konfigurációban |
| Normál CTest-regresszió | **45/45**, benne öt izolált MariaDB-integráció |
| Natív ASAN CTest-regresszió | **33/33**, új ASAN MariaDB-kör nélkül |
| Védett fájlok SHA-256 ellenőrzése | 34 szezonális Mission Pass- és nyolc korábbi Unity-fájl változatlan |

A korábbi, egyszer észlelt ASAN történeti hatásnyugta-visszaolvasási eltérés
oka továbbra is nyitott; ez a részfázis nem igazol rá javítást.
A tényleges Server2-Local játékadatbázis és a távoli gép változatlan;
nincs sémafrissítés, telepítés, FreeBSD-build vagy Unity/socket végpontok
közötti játékpróba. A fordítás `_CL_=/MP2`, `--parallel 2` mellett futott.

A megőrzött helyi naplók a `build` könyvtárban:

- `mission-shop-admission-native-unit-build.log`, `mission-shop-admission-asan-unit-build.log`
- `mission-shop-admission-native-unit-tests.log`, `mission-shop-admission-asan-unit-tests.log`
- `mission-shop-admission-native-build.log`, `mission-shop-admission-asan-build.log`
- `mission-shop-admission-native-regression.log`, `mission-shop-admission-asan-regression.log`
- `mission-shop-admission-protected-hashes.json`
