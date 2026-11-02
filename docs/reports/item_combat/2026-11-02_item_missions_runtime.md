# NPC-s tárgyleadó Mission — C12

2026-11-02. Szerverág `codex/mission-engine`, Unity 6.3 `6000.3.27f1`,
klienság `/main/codex-mission-engine`, **Unity changeset 45**.

## Eredmény és határ

Az új tárgytáron futó NPC-küldetés külön kezeli a darabszámfeltételeket,
elfogyó tárgyakat, jutalomtárgyakat és a Yangot. A teljes gazdasági eredmény
és a teljesítési receipt egy tranzakcióban kerül mentésre. Az egyszer már
teljesített kód/verzió újranyitással és újrabelépéssel sem jutalmaz újra.
Az eredeti darabszámfeltételnek ehhez már nem kell teljesülnie.

A meglévő MissionRuntime NPC-cél/jutalom/siker gráfja a tárgyreceipt után
kap acknowledgement-et. Ez az egyszeri, köztes haladást nem tároló
tárgyleadás készül el; az általános Mission checkpoint/outbox/resource-writer
aktiválás, XP/szintjutalom, többlépcsős célok és a legacy Lua-quest konverziója
továbbra is külön munka. A teljes item-rendszer átállása ezzel még nem kész.
Részletes döntések és tartalomformátum: [IC-040](../../adr/item_combat_040_item_missions.md).

## Ellenőrzések

- Release GameServer, SQL-, Mission- és csomagellenőrző programok lefordultak.
- CTest **19/19**: `item_missions`, `equipment_stats`, `point_calculation`,
  `combat_state`, `persistent_items`, `typed_inventory_wire`, `typed_ground`,
  `item_persistence_completion`, `item_persistence_mariadb`, `npc_shops`,
  `player_trades`, `crafting`, `shop_lifecycle`, `exchange`, `typed_item_grants`,
  `quickslots`, `spatial_lifecycle`, `gm_commands`, `server_package_tools`.
- Külön a meglévő `mission_core` tesztcsoport is sikeres.
- **23** új tiszta domain/Mission ellenőrzés: valódi függő jutalomakció,
  NPC-azonosság, tulajdonos és verzió szerinti akcióazonosító, szerződéshash,
  input/jutalom összevonás, finomítás, kötés, escrow, egyedi állapot kizárása,
  minimumszint, Yanghiány, túlcsordulás és közös natív packet fixture.
- A MariaDB-s tesztprogram **1194** ellenőrzése tartalmazza az új atomi
  leadás/jutalom/receipt utat, szerződésütközést, wallet-CAS rollbacket,
  sessionváltást és az eredeti inputfeltétel megszűnése utáni receiptolvasást.
- A nyitott Unity Editor **3871** új offline ellenőrzést futtatott: natív
  golden, teljes TCP-darabolás/csonkolás, mezőellenőrzés, pontos CG232,
  nem fogyó feltételek, saját kötés, minimumszint, megerősítés, ismétlés,
  szerveres teljesítési jel, nonce-váltás és karakterváltás.
- Az előző Unity-csoportok is sikeresek: equipment **708**, crafting **3699**,
  csere **4553**, NPC-bolt **4044**, inventory **4179**. A küldetésablak
  `Logs/typed-item-mission-preview.png` képe vizuálisan ellenőrzött.

Az élő TLS-próba végleges eredménye még rögzítendő. A nyitott Editor a
legutóbbi sikeres scriptfordítás után nem folytatta az update ciklust;
az Assets → Refresh lépést kértük, mert az alkalmazásvezérlő inicializálása hibás.
A legutolsó kliensjavítás (halott/kábult karakter elküldetlen kérésének
várakozását törli) Editor-újrafordítása is ezzel együtt ellenőrzendő.

## Helyi csomag és újrafuttatás

Saját csomag: `D:\AurigaGlobal\Server2-Local`. A demó a 9003-as kereskedőnél
ad külön próbaküldetéseket: normál leadás, teli táska felszabaduló hellyel,
pénzhiány, tényleges helyhiány, nem fogyó tárgyfeltétel, minimumszint és
jutalomból előre nem finanszírozható input. Új adatfájl:
`data/typed_item_missions.json`; a szerverkezelő adatfájl-szerkesztőjével szerkeszthető.

A dedikált #1610 karakter állapota és pozíciója minden fixture-reset előtt
a `run/item-mission-live/` alatt mentendő. Az eredeti bináris, manifest és
adatfájl a `run/deployments/item-missions-*` mentésben marad.
A teszt kizárólag ellenőrzött saját fiókot épít át a karakterválasztóban,
és nem töröl történelmi teljesítési receipteket. Új, már jutalmazó tesztfutás friss, explicit
demóküldetés-verzióval indul; ez nem production adatvisszaállítási eljárás.

Újrafuttatás: `tests/server/ItemMissionUnityLiveSmoke.py --package ...
--unity-project ... --fixture <privát-json> --bench-tool <ItemBenchTool>`.
A fixture jelszót és TLS-beállításokat tartalmaz, ezért nem része a commitnak.
A segéd az Editor opt-in fájlbridge-én keresztül valódi TLS-kliensfolyamatot
indít, négy fázist készít elő, és siker után kézzel próbálható tárgyakat ad.

Naplók: `build/item-missions-final-build.log`, `build/item-missions-final-ctest.log`,
`build/item-missions-kernel-ctest.log`, `build/item-missions-live-harness-final.log`,
Unity `METIN2_TYPED_ITEM_MISSION_*`.

A korábbi felhasználói ProjectSettings-, csomagmanifest/lock- és AI-beállítások
nem részei ennek a munkának. FreeBSD- és standalone Unity-player build,
teljes production tartalom-/játékoskonverzió és általános Mission-aktiválás
nem futott ebben a szeletben.
