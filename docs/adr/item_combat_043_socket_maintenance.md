# ADR IC-043 — Foglalatkarbantartás szerveren és Unityben

2026-11-02, C15, F4/F5. Alap: [IC-030](item_combat_030_socket_services.md),
[közös munkaterv](item_combat_034_runtime_integration_plan.md).

## Működés és határ

A fogyasztható SocketService profiljának három zárt művelete fut az új úton:

- **CleanBroken:** minden törött hely ugyanott üres lesz, minősége megmarad.
- **ExtractAll:** minden ép kő külön példányként visszakerül a leltárba;
  eredeti helyén azonos minőségű törött foglalat marad.
- **ExtractLast:** csak a legnagyobb foglalt indexű kő kerül vissza;
  helyén azonos minőségű üres foglalat marad.

Nincs tömörítés, szintvesztés, minőségcsökkentés, összeolvasztás, részleges
visszaadás vagy földre dobó fallback. Egy visszakapott példány darabszáma 1,
ItemKey-je és fejlesztési szintje a beépített érték; új tartós ID-t kap.
A céltárgy ID-je, szintje, kötése és teljes bónuszállapota megmarad.

Mindkét bemenet saját leltárbeli tárgy. A célt előbb le kell venni; fegyver
és testpáncél karbantartható. A cél viselhetősége nem feltétel, az eszköz
minimumszintje/kaszt-/nem-/harcpolitikája ellenőrzött. Idegen kötés, escrow,
azonos source/target vagy egyedi eszközállapot elutasított.
Hatástalan művelet és kevés eszköz nem fogyaszt.

Kőbehelyezés, ékszerfoglalat-unlock/charge, bónuszváltás és időzített hatás
továbbra is külön bekötések.

## Tranzakció és kivezetett kövek

A `MakeSocketMaintenanceCommit` a meglévő tiszta `PlanSocketService`
szemantikáját használja. A source fogyása után, a teljes saját leltár
lábnyomait ellenőrizve foglal helyet minden eredménynek. A teljesen elfogyó
eszköz cellája használható; részhalomé nem. Bármelyik kő helyhiánya az egész
műveletre NoRoom. Tisztításhoz nem kell új cella.

A source debit, a target teljes foglalatcseréje és minden visszakapott
példány a közös item-writer egy SQL-tranzakciója. Wallet nem változik.
Outcome `0x1501`, payload `SOC\x01`, művelet és foglalateredmény;
a teljes before/after képek és kőpéldányok a commit digest részei.
CAS-hiba mindent visszagörget, nincs sikerreceipt. CommitUnknown a meglévő
store-egyeztetésen fut.

Ismert, azóta retire-olt beépített kő visszanyerhető. A közös író általános
új-retired-tárgy tiltása megmarad; kivétele csak egy teljes, pontosan
újraellenőrzött C15 kiemelés. Követelmény az eszköz pontos fogyása, az
embedded before image megfelelő eltávolítása és az összes új kő pontos
típus/szint/darab/állapot-egyezése. Sérült későbbi before image elutasított,
a write set átrendezése támogatott. Ez nem általános retired-grant engedély.

## Runtime és kliens

Kriptográfiai nonce és operation ID, owner session/revision fence, egy
megerősítés és cached replay védi a műveletet. Új célelőnézet lecseréli a
korábbi nonce-ot. Módosított cél, mozgatás vagy relog után a régi kérés nem
írhat. Confirmkor az admission, actor és teljes terv újraellenőrzött.
ItemUse permit és TypedItemBusy védi az aszinkron írást. Az output ID-k a
közös tartós allocatorból származnak; preview nem költi el őket.

Unity: **I → karbantartó eszköz → Foglalatok**, szerveres céltárgylista,
foglalatonkénti előtte/utána állapot, visszakapott kövek és megerősítés.
Pending alatt nincs optimista fogyasztás vagy állapotcsere. Elutasításkor
a szöveg nem állítja, hogy a tervezett debit megtörtént. Inventory-változás
és karakterváltás érvényteleníti az előnézetet.

- GC227 action `Socket=128`, csak 5-ös kategórián.
- CG235: header/action u8, nonce/source/target u64, sequence u8; 26+1 bájt.
  Preview action1 nonce0, target0 = szerveres alapválasztás; confirm action2
  pontos nonce/source/target; close action3.
- GC235 v1: legalább 129 bájt. Státusz, nonce, source/target ID, owner revision,
  művelet, eszközdarab, forráshalom, típusok/szintek, három előtti és utáni
  foglalat, visszakapott kőlista, céllista, GC227 metaadat.
- A kliens az összes sikeres foglalatátmenetet, megmaradó minőséget és
  hiánytalan kő-visszaadást is ellenőrzi.

Nincs új séma, vnum-adapter vagy legacy item-írás. A lokális próbapad
`grant` opcionális explicit foglalatállapotot fogad, ugyanazzal a teljes
validálással és gazdasági writerrel; az élő fixture nem kézi SQL-lel
módosítja a tárgyakat.

## C16 protokollbővítés

[IC-044](item_combat_044_socket_insertion.md) a közös ablakot és nonce-ot
kőbehelyezéssel bővíti. A szerver már GC235 v2-t küld, 162 bájtos terminal
envelope-pal; a kliens a korábbi v1 golden-t is olvassa. Az 128-as action
az 5-ös karbantartó mellett a 7-es behelyezhető kőre is érvényes.
