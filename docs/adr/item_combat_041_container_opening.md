# ADR IC-041 — Dobozok és kulcsos ládák atomi nyitása (C13)

2026-11-02. Az [IC-013](item_combat_013_containers.md) katalógus- és
sorsolási alapjának első szerver–Unity bekötése. A láda, a kulcs és a jutalom
végig ItemKey/példányazonosító alapú; nincs `special_item_group` vagy vnumadapter.

## Támogatott állapot és eredmény

A tartós tároló a sima Use/Locked/Scripted konténereket is hordozza.
A Scripted fajta külön questet igényel, a PetEgg saját keltetési állapota
továbbra sem nyitható ezen az úton. A nyitás kizárólag saját, inventoryban
levő, alap példányállapotú forrást és kulcsot fogad el. A saját karakterhez
kötött forrás engedett, az idegenhez kötött, felszerelt, escrowban levő vagy
egyedileg módosított forrás nem. A két instance ID különböző.

A Use önmagában, a Locked pontos zárazonosítóval egyező kulccsal nyílik.
Mindkettőből egy darab fogy. A szerver ellenőrzi a forrás és a kulcs
minimumszintjét, kaszt-/nempolitikáját, valamint a harc közbeni használatot.
A futó karakternek életben, játékfázisban és normál tárgyműveletre képesnek
kell lennie; quest, csere, bolt, széf, crafting, observer és stun ütközés tiltott.

A C13 teljes profilja **tartósan kiadható, +0 szintű tárgyakat és Yangot**
tartalmazhat, ritka példánybónusz-sorsolás nélkül. Ha akár egy sor XP-t,
szörnyt/csoportot, státuszhatást vagy nem támogatott ritka bónuszt kér, az
egész profil elutasított a sorsolás és fogyasztás előtt. Nem marad el
csendben a nem támogatott sor, és nincs kedvező sorra történő újrasorsolás.
Ezek a hatások, a petkeltetés és a régi tartalom teljes konverziója későbbi munka.

## Befogadás a sorsolás előtt

A pusztán kisorsolt eredmény helyellenőrzése kihasználható lenne: megtelt
leltárral addig lehetne próbálkozni, amíg a tárgyjutalom helyett Yang esik.
Ezért a tervező **a hívó RNG-je előtt minden lehetséges tárgyeredményt
ellenőriz**, a fogyasztás után felszabaduló helyeket is felhasználva.

- WeightedOne: minden lehetséges egyetlen jutalom külön ellenőrzött,
  legfeljebb a katalógus 256 soros határáig.
- IndependentChance: a biztos tárgysorok minden jelöltben benne vannak,
  a bizonytalan tárgysorok minden kombinációját ellenőrzi. Legfeljebb
  **8 bizonytalan tárgysor** engedett, azaz maximum 256 kombináció.
  A teljes lehetséges tárgysorszám plusz a két input nem lépheti túl a
  meglévő 32 változó példányos tranzakciós keretet.
- Yang: a teljes lehetséges maximum ellenőrzött `INT64_MAX` ellen;
  az independent Yang-sorok maximuma minden jelöltben szerepel.

Az independent ág minden részhalmazát ténylegesen megtervezi. Csak a
legnagyobb csomag ellenőrzése nem lenne elég: a first-fit elhelyezés egy
kisebb részhalmaznál más helyeket választhat. A nyitás nem kap részleges
kiosztást, földre dobó fallbacket vagy a mintától függő ingyenes újrapróbálást.
ID-, hely-, pénztárca- vagy szabályhiba előtt nincs érdemi lootdobás.

A tényleges eredményt a már meglévő `PlanContainerOpen` választja ki.
A runtime OpenSSL `RAND_bytes` forrást használ; a bounded draw rejection
samplinggal torzításmentes. A nyilvános preview-nonce nem árul el a loothoz
használt, kiszámítható PRNG-állapotot. Entropiahiba elutasítást ad.

## Egy gazdasági commit

A forrás-/kulcsfogyasztás, az összes tárgyjutalom, a Yang és a `BOX` v1
eredményreceipt egy `ItemCommit`. Az eredmény az input instance ID-ket,
a nyertes sorszámokat és a Yang-összeget rögzíti. A közös `MakeGrantCommit`
tervezi a helyeket és a halmozást; az eredeti példány egyszer kap új revíziót.
Az új példányok a közös SQL-azonosító-kiosztó game-thread pooljából származnak.

A preview rögzíti a tulajdonosi/session kerítést. Inventoryváltozás után
a régi preview elavult, még a sorsolás előtt. A megerősítés egylövetű:
függő műveletre Pending, kész kérés ismétlésére a korábbi eredmény érkezik,
új dobás és írás nélkül. Egy másik preview régi nonce-a elutasított.
A közös ItemStore a bizonytalan SQL-commitot a receipt alapján egyezteti.
Új belépés a már mentett eredményt tölti vissza; nem kezd új nyitást.

A teljes aszinkron művelet `ItemUse` resource permitet és `TypedItemBusy`
állapotot tart. A callback PID/login-token alapján újrakeresi a karaktert.
A globális giftbox-várakozás és a régi 17-es rangsorszámláló csak igazolt
siker után frissül. Ezek a meglévő karakter-runtime járulékos adatai,
nem részei az atomi item/wallet receiptnek; általános külsőhatás-outbox
aktiválása nem történt.

## Unity és hálózat

**I → láda kiválasztása → Nyitás.** Az új GC227 action bit **32 = Open**;
a sima Használ gomb helyét ilyenkor a Nyitás veszi át. A preview külön
mutatja a lehetséges jutalmakat, esélyeket, a kulcsválasztást és az üres
independent eredmény lehetőségét. Csak a szerver által felsorolt megfelelő
kulcsok választhatók. A megerősítés előtt világos az egy-egy darab fogyasztás.
Inventoryfrissítés törli a korábbi megerősítést, pending alatt nincs új kérés,
és a kliens nem módosít optimistán tárgyat vagy pénzt. Elfogyott kulcs után
az új preview egy megmaradt megfelelő kulcsot keres. Karakterváltás töröl.

**CG233**: 26 bájt + sequence. Little-endian `header:u8, action:u8,
session:u64, box:u64, key:u64`. Akció 1 preview (session 0; key 0 esetén
szerveres alapkulcs-választás), 2 megerősítés, 3 bezárás. Egy nonce egy nyitás.

**GC233 v1**: 69 bájt–1 MiB. `header:u8, size:u32, version:u8, status:u8,
session/box/key/ownerRevision:u64`, majd `boxKey+level` és `keyKey+level`
(2 × category:u16, local:u32, level:u16; hiánynál nullák),
`kind:u8, lock:u16, mode:u8`, `keyCount:u16, IDs:u64[]`,
`entryCount:u16, entries[]`, `winnerCount:u16, winners:u16[]`,
`detailsSize:u32, GC227`. Egy jutalomsor: `ordinal:u16, kind:u8,
itemCategory:u16, itemLocal:u32, amount:u64, weight:u32, rareChance:u16`.

Státusz: Closed 0, Ready 1, Opened 2, Pending 3, Busy 4, Invalid 5,
Denied 6, KeyRequired 7, WrongKey 8, NoRoom 9, WalletLimit 10,
Unsupported 11, Stale 12, Failed 13, Level 14, Cooldown 15.
A GC227 v1 a 32-es bitet csak Container kulcson engedi. A szerver és Unity
közös natív golden alapján ellenőrzi a packetet és a teljes TCP-darabolást.

[Ellenőrzések és helyi próba](../reports/item_combat/2026-11-02_container_opening_runtime.md).
