# ADR IC-042 — Pénzjegyek futásidejű beváltása

2026-11-02. F4/F5, C14. Alap: [IC-014](item_combat_014_use_items.md),
[közös szerver–Unity terv](item_combat_034_runtime_integration_plan.md).

## Működés

A 16-os kategória explicit pénznemét és egységértékét használjuk. A Unity
**I → pénzjegy → Beváltás** útja a szervertől kér előnézetet. Alapból a
teljes halmot választja, a darabszám módosítása új előnézetet igényel.
A megerősítés előtt látható a ténylegesen elfogyó darab, jóváírás,
megmaradó halom, egyenleg előtte/utána és a korlát.

A tiszta tervező a meglévő `PlanTokenRedeem` függvényre épül:
`elfogy = min(kért, (korlát - egyenleg) / egységérték)`.
Csak egész, teljes értékükben jóváírt darabok fogynak. A részhalom megtartja
azonosítóját és helyét; revíziója egyszer nő. Teljes beváltáskor tombstone
keletkezik. Teli leltárban is beváltható, mert nem hoz létre jutalomtárgyat.

## Tulajdon és tranzakció

Ez a szelet a karakter **Yang/Gold** pénztárcáját kezeli. A typed író meglévő,
előjeles 64 bites korlátja `INT64_MAX`, ugyanaz, mint a boltnál és a ládáknál;
ez nem a régi runtime `GOLD_MAX` értéke. Nem vezetünk be új konfigurációt.
Az egységérték és a szorzat ellenőrzése osztással történik a szorzás előtt.

A **Sárkányérme/DragonCoin** fiókpénztárcájának íróját még a legacy út
birtokolja. Utalványa szállítható és előnézete megnyitható, de Unsupported:
nem fogy, nem keletkezik párhuzamos karakteregyenleg, és nincs legacy
`SetDragonCoin`/vnum-adapter. E pénznem bekötése külön account-writer átadás.

A művelet saját kriptográfiai nonce-ot és operation ID-t kap. A karakter
session-generation/owner-revision fence-e, a kért darabszám és a teljes
gazdasági quote kötött. Confirmkor az aktuális actor/policy/szint és a
teljes terv újraellenőrzött. Mozgatás, új preview vagy új belépés elavítja
a korábbi megerősítést. Azonos confirm a tárolt eredményt adja vissza;
módosított darabszám nem indít új beváltást.

ItemUse resource permit és TypedItemBusy védi az aszinkron írást. A forrás
fogyása, pénztárca-revízió/egyenleg és az immutable receipt ugyanaz a natív
SQL itemtranzakció. CAS-hiba mindent visszagörget. A callback csak az eredeti
karakter aktuális belépését frissíti. Saját kötés engedett; idegen kötés,
escrow, egyedi példányállapot vagy hibás policy elutasított.

## Protokoll

- GC227 `Redeem=64`, csak CurrencyToken definícióra; GC225 a tényleges
  pénztárca és leltár forrása.
- CG234: header u8, action u8, nonce u64, instance u64, darab u32,
  normál sequence u8. Preview action1 nonce0, darab0 = aktuális teljes halom;
  confirm action2 pontos kért darabbal; close action3 darab0.
- GC234 v1: legalább 88 bájt; status, nonce, forrás ID/revízió/típus/szint,
  pénznem, elérhető/kért/elfogyó darab, egységérték/jóváírás/egyenleg/korlát,
  GC227 metaadat. A sikeres eredmény a beváltás előtti forrást tartalmazza,
  így a teljesen elfogyott halom neve és eredménye is megjeleníthető.
- A kliens ellenőrzi a hosszt, verziót, státuszt, kategóriát, metaadatot,
  mennyiségeket és a teljes pozitív Gold quote számtani azonosságát.
- Receipt outcome `0x1401`, payload `TKN\x01`, forrás ID, kért/elfogyó darab,
  pénznem, egységérték, jóváírás és korlát. Az item/wallet before/after képek
  a közös commit digest részei.

Nincs új séma, runtime vnum-leképezés vagy kettős pénz-/tárgyírás.
Ellenőrzések és helyi próba a kapcsolódó futási jelentésben.
