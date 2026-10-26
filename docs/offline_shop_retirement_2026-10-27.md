# A jelenlegi offline shop kivezetése

Dátum: 2026-10-27. Ág: `codex/mission-engine`.

A tulajdonosi döntés szerint a jelenlegi offline shop megszűnik. Nem vezetjük
át a Mission writer-handoff rendszerébe, és nem javítjuk tovább a régi
aukciós/bolti raktáros megoldást. Az új offline shop későbbi, önálló tervezés
és implementáció lesz. A normál NPC-bolt és az online személyes bolt megmarad.

## A szerverforrásból eltávolított részek

- A GameServer manager, üzleti objektumok, aukció, ajánlatok és bolti raktár.
- A Database offline bolti cache-ei, induláskori betöltése, periodikus munkái,
  SQL-válaszkezelői és GameServer–Database csomagjai.
- A ShopState offline pointerei és időbélyegei, az OfflineShopSystem, a
  világban megjelenő boltok entitásregisztere és térképi packet builderei.
- Az offline bolti klienscsomag, GM-parancsok és chatlink-kezelők; a belépés,
  kilépés, warp, csere, rendezés, cube és kiegészítő-refine offline feltételei.
- Az offline feature definíciói, csomagtípusai, forrásai és kizárólagos tesztje.
- A hat offline SQL-assembly szerződés és a hozzájuk tartozó SQL-generátor.

Ez tényleges forráskivezetés, nem egy bekapcsolható feature-flag átállítása.
A régi CG 119, GC 214, GD 153 és DG 190 kódját nem osztottuk újra más célra.
Az actual CPacketInfoCG-t futtató regresszió ellenőrzi, hogy a 119 már nincs
regisztrálva, miközben a normál shop és online shopnyitás regisztrációja megmarad.
A régi protokollt küldő kliens ehhez a szerverhez már nem kompatibilis offline
shop szempontból; nincs kompatibilitási gazdasági handler.

Az offline-shopra kizárólagos spatial tesztesetek kikerültek. Az általános
nem-karakter láthatósági/reencode próba tárggyal megmarad; a karakter-,
item-, building-, mozgás- és térképi életcikluspróbák tovább futnak.

## Adatmegőrzés

Nem készült és nem futott DROP, TRUNCATE, DELETE vagy adat-visszaosztó
migráció. A tényleges helyi és távoli játékadatbázis változatlan.

A sémafájlban a történeti offline táblák, a korábbi SQL-migrációban a régi
mezőnevek megmaradnak, hogy a mentések fogadhatók és ellenőrizhetők legyenek.
A Database item-window névtáblájának két történeti SQL ENUM-címkéje is
megmarad; ezekhez nincs gameplay-ablak vagy hálózati művelet. A meglévő
INVENTORY–GROUND sorszámok nem tolódtak el.

A régi boltokba/raktárba tett tárgyak, bevételek és ajánlatok automatikus
átvezetése nincs megvalósítva. Egy későbbi telepítési/adatrendezési fázisban
a tényleges mentés alapján kell dönteni a tulajdonosi visszaadásról. A régi
táblák megőrzése önmagában nem jelenti, hogy a játékos az új szerveren eléri
az ott archivált vagyont. Az új offline shop sem veheti át ellenőrzés nélkül
a régi állapotot.

## Kliensforrások és a telepítés határa

Ebben a Git-repóban a régi C++/Python launcher offline modulja, küldője,
GUI-fájljai, készletgombjai és chatlink-belépései kikerültek. A Python app
egy nulla értékű kompatibilitási feature-jelzést még exportál régi külső
root-csomagokhoz; ebből nem lehet boltot nyitni. A fordítás nélküli,
használaton kívüli régi UI-backupok és fordítási szövegkulcsok nem runtime
szolgáltatások.

A felhasználó pontosítása alapján a külön Unity Version Control workspace
kivezetése is része ennek a munkának. Kikerültek az offline küldők, dekódolók,
runtime események, kezelő/builder/aukció/raktár komponensek, inventory dropok,
hotkey, mobil menü és Wiki-belépés. A desktop és mobil UI-katalógusból 14 régi ablak, valamint
a kizárólagos generált prefab/layout fájlok is kikerültek. A régi inventory
prefabban megmaradó placeholder gombok inaktívak, és nem kapnak eseményt.

Az online személyes boltok feliratai közös szolgáltatásként megmaradnak:
`Metin2ShopWorldSigns`, `Metin2ShopSignArt` és `ShopSign`. A két átnevezett
script megtartja korábbi meta GUID-ját. A megosztott prémium rajzelemek és
fordítások nem törlődnek. A teleportpontok input-/kérdésdialógusa közös,
offline shoptól független helperre és a normál inputdialog prefabjára került.

A Unityben már meglévő hét helyi módosítás hash-e változatlan; ezek nem részei
a saját checkinnek. A Unity ág `/main/codex-mission-engine`, a workspace
`D:/Auriga-Unity/SRC/AurigaUnity`. A szerver új buildje nem lett telepítve vagy elindítva.
Éles frissítés előtt az archivált régi javak sorsát a fenti külön
adatmegőrzési lépésben rendezni kell. Távoli gépen semmilyen módosítás nem történt.

## Ellenőrzés

A célzott normál és ASAN körben a packet string/registráció, spatial,
entity network dispatch és GM-parancs suite mindkét konfigurációban 4/4
sikeres. A packet builder tesztharness régebben hiányzó gathering- és
üreswear-szolgáltatáshatárai is frissültek, hogy a jelenlegi tényleges
packet builder forduljon és fusson. Ezek kontrollált tesztfüggőségek.

A megváltoztatott tizenkét legacy Python/UI-forrás a tényleges helyi
Python 2.7 fordítójával végrehajtás nélkül sikeresen lefordult. A korábbi
tíz érintett fájl az átírás előtt is szintaktikailag helyes volt.
Ez nem a legacy DirectX-launcher teljes buildje vagy UI-elfogadása.

A kódszintű SQL-leltárban a 421 korábbi lekérdezésből 386 maradt; az offline
táblákat megnevező 32 lekérdezésből nulla maradt. A HEAD-ből elkülönített
forrásarchívumon és a jelenlegi forráson azonos eszközzel készült összehasonlítás.
A teljes régi
SQL-audit külön, korábbról elavult nem-offline assembly azonosítókat is
tartalmaz (`player_save`, régi ClientManager item-award); ezek nem kaptak
automatikus újraminősítést. Az offline szerződések kikerültek, a valóban
módosított Cache/ClientManager/Boot fájlok fennmaradó builder-pinjei frissültek.
Ez a részfázis nem állít teljes, új SQL-forrásauditot.

A szezonális Mission Pass 34 védett fájljának hash-e változatlan. A négy
célzott suite eredménye: packet strings/registry 2413, spatial 1118,
entity network dispatch 1409, GM-parancs 47561 ellenőrzés mindkét buildben.

A GameServer, Database és LoginServer normál és ASAN buildje elkészült.
A lezárt célzott regresszió **49/49 normál**, öt izolált helyi MariaDB-próbával,
és **37/37 ASAN** eredményű. Új ASAN MariaDB-kör és FreeBSD build nem futott.
A korábbi Mission történeti ASAN DB-nyugtaeltérés oka ettől még nyitott tétel.

A Unity runtime teljes Windows-, Android- és iOS-kódágának, valamint az Editor
assemblynek a fordítási ellenőrzése sikeres; ez a mobil ágakon compile-only,
nem eszközbuild. A tényleges Unity 2022.3.62f3 Editorban az új kivezetési próba
**30 ellenőrzéssel sikeres**: régi csomagok elutasítása, hiányzó régi API-k,
NPC-/online bolti küldő/felirat, inventory és közös teleportpárbeszédek.
A meglévő protokollvalidátor és a desktop/mobil hozzáférési validátor is
sikeres, nulla kilépési kóddal. Utóbbi 13 desktop sidebar- és 38 mobil
menübejegyzést ellenőriz. Két korábbi tesztfeltételezés javult: a drag-over
próba után visszaáll a tárgyat tartalmazó inventorylap, és Windows alatt
az F12 továbbra is a runtime meglévő platformfeltételének megfelelően inaktív.
Az elvárt működéshez a runtime inventoryhasználatát és F12-kiosztását nem
kellett megváltoztatni. A törölt assetek GUID-jára nem maradt hivatkozás.

Ezek célzott próbák, nem teljes játékbeli vagy grafikai elfogadás. Az első,
sikeres protokollvalidálás után nyitva maradt batch Editorban egy korábbi
automatikus SpeedTree-atlasz-migráció külön hibát jelzett. Ez az offline shop
kivezetésétől független, nem javított grafikai tétel; a lezárt protokollpróba
`-quit` kapcsolóval, a UI-/kivezetési próbák saját kilépéssel futottak.

Megőrzött helyi bizonyítékok:

- `build/offline-shop-removal-native-build.log`, `build/offline-shop-removal-asan-build.log`.
- `build/offline-shop-removal-native-regression.log`, `build/offline-shop-removal-asan-regression.log`.
- `build/offline-shop-python2-final.log`, `build/offline-shop-source-sql-comparison.json`.
- `build/unity-offline-retirement/unity-protocol-final.log`.
- `build/unity-offline-retirement/unity-access-complete.log`.
- `build/unity-offline-retirement/unity-retirement-final.log`.
- `build/unity-offline-retirement/compile/final-results.json` és a négy `*-final.log`.
- `build/unity-offline-retirement/final-protection-check.json`.

A részletes Unity-jegyzet helyben a `Docs/OFFLINE_SHOP_RETIREMENT_2026-10-27.md`
fájlban is megtalálható. Az új helyi Docs-jegyzeteket a Unity workspace
meglévő ignore-szabálya privátként kezeli; a közös lezárási dokumentum ez a fájl.

A Unity-munka külön, pontos fájllistával rögzített changesetje **cs:33** a
`/main/codex-mission-engine` ágon. A négy közös script/meta átnevezést a
Version Control valódi move-ként követte. A hét korábbi helyi változás
kimaradt a checkinből.

## Mission folytatási sorrend

1. Online játékosbolt vevő/eladó közös memóriabeli resource/inventory commit.
2. Raktár/pet és a fennmaradó timer/quest/harci/XP/pénz- és külön SQL-írók.
3. Teljes source-stop, tartós writer-átadás és hiteles ECS/cache-rebase.
4. Új Mission jutalmazás/session-integráció és a már tervezett későbbi rétegek.

Az új offline shop önálló jövőbeli fejlesztés. A majdani gazdasági műveleteinek
a készülő erőforrás-koordináció szerződését kell használniuk; a régi offline
shop jelenlegi forrása nem marad a Mission Engine aktiválásának függősége.
