# ADR IC-040 — NPC-s tárgyleadó Mission az új tárgytáron (C12)

2026-11-02. A C12 első játszható szelete a karakterenként egyszer teljesíthető,
NPC-nél leadható tárgyküldetés. A feltétel, fogyasztás, jutalom és Yang ugyanazt
az ItemKey-alapú állapotot használja, mint az inventory, bolt és crafting.

## Mission és tartós eredmény

A meglévő `MissionRuntime` egy korlátozott gráfot futtat:
`NpcInteracted` cél → függő `items` jutalomakció → siker. Az akció csak a
tartós tárgyreceipt megerősítése után kap acknowledgement-et. Az írást a
meglévő `ItemStore` végzi; bizonytalan commitnál annak receipt-reconcile útja
dönt. A kliens helyi állapota nem igazol teljesítést.

Ebben a gráfban nincs köztes, mentendő játékoshaladás: az egyetlen sikeres
gazdasági akció receiptje a teljes tartós teljesítés. Újrabelépéskor és az NPC
újbóli megnyitásakor ebből áll elő a teljesített lista. A Mission általános
checkpoint/outbox/resource-writer infrastruktúráját ez a szelet nem aktiválja.
Többlépcsős történet, ölés-/időzítőcél, XP/szintjutalom és a régi Lua-questek
automatikus átkötése továbbra is nyitott munka. Nincs vnumadapter vagy kettős írás.

A stabil `ActionIdentity` a meglévő Mission `ActionKey`-ből készül:
karakter, küldetéskód, verzió, 1-es példány, `deliver` node, `items` akció.
Az item operation ID külön domain-tagelt SHA-256 első 16 bájtja a tulajdonossal
és a teljes akcióazonosítóval. Nem függ a mindenkori inventorytól vagy a
katalógus kiadásától. A 77 bájtos `IMQ` v1 receipt a tulajdonost, a teljes
akcióazonosítót és a teljes gazdasági szerződés SHA-256 hashét tárolja.

Az olvasó a jelenlegi katalógus-pint és login-session kerítést is ellenőrzi,
de a régi inputokat nem követeli meg újra, és nem próbálja újraépíteni a régi
commitot. Így az elfogyott tárgy, módosult wallet és új login után is felismeri
a már kiadott jutalmat. Eltérő szerződés ugyanazon kód/verzió alatt hiba;
gazdasági módosításkor új verziót kell kiadni. A név, leírás, megjelenítési ID
és listák sorrendje nem változtatja meg a szerződést. A verzió emelése új
teljesítési lehetőséget ad, ezért ez tudatos tartalmi kiadási döntés.

## Atomi tervezés

A küldetés három külön listát ad: nem fogyó tárgyfeltételek, leadandó tárgyak,
kapott tárgyak. Minden bejegyzés explicit `ItemKey`, finomítási szint és darab.
A minimumszint, összes feltétel és teljes költség a művelet ELŐTTI állapoton
ellenőrzött. A jutalom sem tárgy-, sem Yang-költséget nem finanszírozhat előre.

Csak a karakter saját, inventoryban levő, alap példányállapotú tárgyai
számíthatók és fogyaszthatók. A saját karakterhez kötött küldetéstárgy is
használható; idegenhez kötött, felszerelt, escrowban levő, bónuszolt, kövezett
vagy aktív ékszerállapotú példány nem. A forráshalmak kiválasztása determinisztikus.
A teljesen elfogyó halom felszabaduló helyére jutalom kerülhet. Azonos
alapanyagból kapott jutalom összevonható a maradékkal, a végső revízió egyszer nő.

Egy `ItemCommit` írja a fogyasztást, jutalmat, walletet és teljesítési receiptet.
Ha kevés a pénz, hiányzik a tárgy, megtelt a táska vagy a CAS-kerítés elavult,
nincs részleges levonás és nincs teljesítési receipt. A meglévő tranzakciós
korlát legfeljebb 32 változó példány; a tartalom betöltése a jutalomhalmak
számát előre is korlátozza. A kiosztott, fel nem használt instance ID-k nem
kerülnek újra felhasználásra.

## Szervertartalom és jogosultság

Új `data/typed_item_missions.json`, formátumverzió 1. Hiánya vagy az üres
`boards` lista kikapcsolja ezt a funkciót. Hibás, duplikált vagy nem támogatott
tartalom megállítja a core indulását. Az új csomag üres mintafájlt kap.
A szerverkezelő meglévő adatfájl-szerkesztője kezeli ezt a JSON-t is.

```json
{
  "version": 1,
  "boards": [{
    "npc_race": 9003,
    "name": "Kereskedő küldetései",
    "missions": [{
      "id": 1, "version": 1, "code": "merchant.leather",
      "name": "Bőrszállítmány", "description": "Hozz négy bőrt.",
      "min_level": 1,
      "requirements": [{"category": 6, "local": 1, "level": 0, "count": 10}],
      "costs": [{"category": 6, "local": 1, "level": 0, "count": 4}],
      "rewards": [{"category": 5, "local": 1, "level": 0, "count": 3}],
      "gold_cost": 25, "gold_reward": 100
    }]
  }]
}
```

A példában szereplő kulcsokat a saját katalógusban létező kulcsokra kell cserélni.
Maximum 256 NPC-tábla, táblánként 32 küldetés, listánként 16 külön kulcs/szint,
bejegyzésenként 1–50000 darab. A küldetéskód globálisan egyedi, a numerikus ID
táblánként egyedi. A minimumszint 0–255, a Yang legfeljebb `INT64_MAX`.

A szerver ellenőrzi a tényleges NPC-t, faját, térképet, távolságot, élő/game
állapotot, itemjogosultságot és az ütköző quest/bolt/csere/széf/műhely állapotot.
Az `ItemUse` resource permit és `TypedItemBusy` a teljes aszinkron műveletet
védi. Visszahíváskor a karaktert PID és login-token alapján újra felkeresi.
NPC-nonce és növekvő kéréssorszám védi a felületet; ugyanaz a kérés az előző
választ adja vissza. Új NPC-munkamenet sem adhatja ki még egyszer a jutalmat.

## Unity és protokoll

**NPC kijelölése → I → Küldetések**. A kliens külön mutatja a feltételeket,
leadandó és kapott tárgyakat, szinteket, minimumszintet és Yangot.
Megerősítés előtt ellenőrizhető minden tétel; függő kérés alatt nincs újabb
leadás, és az inventoryfrissítés érvényteleníti a korábbi megerősítést.
A teljesített küldetés gombja tiltott. Karakterváltás törli a helyi állapotot.

`CG_TYPED_ITEM_MISSION = 232`: padding nélküli 26 bájt, utána a normál
sequence bájt. `header:u8, action:u8, npc:u32, session:u64, request:u64,
mission:u32`. Akció 1 megnyitás (utolsó három mező 0), 2 leadás,
3 bezárás (request és mission 0).

`GC_TYPED_ITEM_MISSION = 232`, v1: 38 bájt–1 MiB dinamikus packet.
`header:u8, size:u32, version:u8, status:u8, npc:u32, session:u64,
request:u64, selected:u32, boardName:text16, missionCount:u8`.
Küldetésenként `id:u32, version:u32, code/name/description:text16,
minLevel:u16, completed:u8, goldCost:u64, goldReward:u64`, majd a három lista:
`count:u8, N × (category:u16, local:u32, level:u16, count:u32)`.
A végén `detailsSize:u32` és a szükséges GC227 katalógusrészlet.
Minden szám little-endian; a text16 UTF-8 bájthosszal jelzett szöveg.

Státuszok: 0 Closed, 1 Ready, 2 Completed, 3 AlreadyCompleted, 4 Pending,
5 Busy, 6 Invalid, 7 Denied, 8 Requirements, 9 NoMoney, 10 NoRoom,
11 Failed, 12 Stale, 13 Level. Natív golden fixture ellenőrzi a Unity kodeket,
a teljes TCP-darabolást és a klienskérés byte-elrendezését.

Ellenőrzések és helyi próba: [C12 jelentés](../reports/item_combat/2026-11-02_item_missions_runtime.md).
