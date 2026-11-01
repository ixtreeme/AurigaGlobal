# ADR IC-036 — NPC-bolt az új tárgyrendszeren (C8)

2026-11-01. Az NPC-bolt a szerverrel és a Unity 6.3 klienssel együtt készül.

## Kínálat és ár

A `server.data_path` alatt lévő `typed_shops.json` explicit NPC → ItemKey
kínálatot tartalmaz. Nem épít futásidejű vnum-adaptert. A fájl hiányozhat;
ilyenkor nincs új típusos bolt. Hibás vagy a betöltött katalógussal
összeegyeztethetetlen fájlnál a katalógust használó core nem indul.

```json
{
  "version": 1,
  "shops": [
    {
      "npc_race": 9003,
      "name": "Vegyeskereskedés",
      "offers": [
        {"id": 1, "category": 5, "local": 1, "level": 0, "count": 1}
      ]
    }
  ]
}
```

A példában a `5/1` típusnak léteznie kell a saját katalógusban. A
`config/typed_shops.example.json` üres kiindulófájl; az új helyi csomagok ezt
kapják. A szerverkezelő adatszerkesztőjében a JSON is szerkeszthető.

- Egy NPC-hez egy lista tartozik, legfeljebb 128 ajánlattal; összesen 256 NPC.
- Az `id` a listán belüli állandó ajánlatazonosító. A `count` egy csomag
  darabszáma, a vásárlás mennyisége pedig a kért csomagok száma.
- Az ár darabonként a katalógus `buy_amount` / `sell_amount` értéke, Yangban.
  A szint nem szorzó. A nulla vételár nem érvényes bolti ajánlat; nulla
  eladási árhoz nincs eladási művelet. A bolt készlete ebben a szeletben korlátlan.
- Csak a már tartósan tárolható kategóriák vásárolhatók, érvényes szinten.
  Kivezetett tárgy nem lehet ajánlat. Kivezetett saját tárgy eladható, ha a
  katalógus átadási szabályának `allow_sell` mezője ezt engedi.
- Eladni csak saját inventorypéldányból lehet; felszerelés és escrow nem
  forrás. A kötés nem tulajdonosváltás: az `allow_sell` dönt az NPC-eladásról.

## Tranzakció és munkamenet

A vásárlás a `MakeGrantCommit` teljes lábnyomos helyfoglalását és azonosító-
kiosztását használja, azonos állapotú halmokat feltöltve. Ugyanabban a
tranzakcióban vonja le az új Gold pénztárcából az árat. Részleges eladáskor
a maradék az eredeti azonosítón marad; teljes eladáskor teljes állapotú
tombstone készül. A jóváírás ugyanabban a tranzakcióban történik.

A katalógus, tulajdonos, munkamenet, inventoryrevízió és pénztárcarevízió
ellenőrzése a meglévő írónál marad. Nincs kettős írás a régi Yanghoz vagy
inventoryhoz. Az elveszett COMMIT-válasz rendezéséig a karakter foglalt.

A bolt nyitásakor a szerver véletlen tokent ad. A vásárlás/eladás kérései
szigorúan növekvő azonosítót visznek. Az utolsó azonosító azonos paraméterű
ismétlése a korábbi választ küldi vissza, új írás nélkül. Más paraméter vagy
régi azonosító elavult kérés. Újranyitás új tokent ad; a régi kérés nem írhat.

Az NPC, a térkép és az 1000-nél kisebb közelítő távolság minden kérésnél
ellenőrzött. Halott, megfigyelő vagy másik gazdasági ablakban lévő karakter
nem kereskedhet. A Mission erőforrás-belépésvédelme is érvényesül. A bolt
távolodáskor, NPC-megszűnéskor vagy állapotváltáskor bezáródik.

## Protokoll és Unity

- `CG_TYPED_SHOP` **229**: 34 bájtos body a meglévő szekvenciás útvonalon.
  Művelet, NPC VID, bolttoken, kérésazonosító, ajánlat-/példányazonosító,
  mennyiség. A kliens nem küld árat.
- `GC_TYPED_SHOP` **228 v1**: 32 bites méret; státusz, NPC, token, kérés,
  név, ajánlatok, saját eladható példányok egységárai és beágyazott GC227
  katalógusrészlet. A pontos LE-formátum az `NpcShops.hpp` fejlécben van.
- A szerver minden válasz előtt irányadó inventoryt küld. A kliens nem
  módosít előre pénzt vagy tárgyat; a gombok megerősítést és függőállapotot
  használnak. Karakterváltáskor a bolti munkamenet törlődik.
- A normál `npc.open_shop` út a konfigurált NPC-knél ezt a boltot nyitja.
  Közvetlenül is elérhető: NPC kijelölése → **I → NPC-bolt**.

## Helyi teszt

A dedikált fixture seedelése: `SeedTypedItemUnityLive.py --npc-shops`,
leállított saját játékfolyamatokkal. A jelenlegi helyi kereskedő #9003,
főtérkép 41, közelében `(986600, 269100)`, core-port 30078. A privát fixture
nem kerül verziókezelésbe. Az új árak/lista csak új core-induláskor lépnek életbe.

A `NpcShopUnityLiveSmoke.py` a fixture ellenőrzött saját karakterét reseteli,
és a normál TLS-kliensútvonalon három esetet futtat: vásárlás/eladás és
újrabelépés, pénzhiány, teljes inventory. A korábbi állapotot a csomag
`run/item-shop-live` könyvtárába menti. A Unity Editor legyen edit módban;
a külön validátorfuttató ideiglenes jelenetben dolgozik, és nem zárja be az Editort.

Az ellenőrzések és telepítés konkrét eredményeit a
[beszámoló](../reports/item_combat/2026-11-01_npc_shop_runtime.md) rögzíti.
