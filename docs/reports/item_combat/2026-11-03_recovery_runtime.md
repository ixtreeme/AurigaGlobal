# C19 — Tartalékos és folyamatos HP/SP-visszatöltés

2026-11-03. Szerződés: [IC-047](../../adr/item_combat_047_recovery_runtime.md).

## Eredmény

Tartalékos és folyamatos HP/SP-ital használható a szerveren és Unityben.
A profil adja az összeget, sebességet, időközt, időtartamot, közös cooldown-t
és használati feltételeket. A tárgyfogyás és a tartós ütemezés egy SQL-commit.
Az aktív állapot a forrás elfogyása után is megnyitható és megszakítható.
Offline idő nem fogyaszt időt és nem ad gyógyulást; halál törli az adagokat.

## Lefutott ellenőrzések

| Ellenőrzés | Eredmény |
|---|---|
| Release GameServer és érintett natív célok | sikeres |
| CTest célzott regresszió | **13/13 csoport** |
| RecoveryScheduleTests | **664 ellenőrzés** |
| Külön saját MariaDB, ItemPersistenceSqlTests | **1399 ellenőrzés** |
| Külön saját MariaDB, ItemBenchToolTests | sikeres |
| Unity 6000.3.27f1 scriptfordítás | sikeres |
| Új golden, RCS1/GC242 codec, TCP-framing, metadata, UI és dispatcher | **3212 ellenőrzés** |
| Kereskedő UI, beleértve passzív mentés miatti Stale/retry-t | **4558 ellenőrzés** |
| Korábbi C4–C18 Unity validátorok | mind sikeres |
| Valódi Unity → TLS → GameServer → SQL | **3/3 fázis, 112 ellenőrzés** |
| Két valódi kliens kereskedése aktív visszatöltés mellett | **2 sikeres csere**, eredeti partnerkészlet visszaállt |
| Canonical Unity snapshot összevetése | **5440 fájl, 1720 byteazonos C# fájl** |
| UI-képernyőkép vizuális ellenőrzése | forrásnév, HP/SP, adagok, idő és megerősítések elférnek |

A natív csoportok: recovery, consumable, persistent items, inventory/ground
wire, accessory/attribute runtime, player trade, equipment stats, container,
token, socket maintenance és insertion. A külön SQL-próba forrás + schedule
rollbackot, before-state CAS-t, receipt/replay védelmet, tick-only mentést,
relogot és sérült checksum elutasítását is ellenőrizte. A próbapad új sémája,
telepítési listái és owner resetje a recovery táblát is kezeli.

## Élő fázisok

1. **Hiányzó HP/SP és időzítés.** A valódi inventory `Visszatöltés` gombja és
   megerősítése két poolhoz tartalékot indított. Egy darab fogyott; a live
   tick HP/SP-t adott és ugyanakkor a megfelelő tartalékot csökkentette.
   HP/SP folyamatos profilok együtt éltek, HP újraalkalmazás csak a HP-csoportot
   cserélte. Nonce replay nem fogyasztott újra. Közös, tartós cooldown alatt
   a C5 azonnali ital is megmaradt. Megszakítás nem törölte a cooldown-t.
   Logout és 3,5 másodperc offline után az azonos hatás ID-ja és hátralévő
   ideje folytatódott; új control nonce keletkezett, a régi login törlési
   kérése elutasított. Elavult cancellation-generation sem törölhetett hatást.
2. **Tele pool és elutasítás.** Tele HP/SP-n a tartalék nem fogyott. A magas
   minimumszint és tiltott kontextus sem fogyasztott. Folyamatos profil tele
   HP-n is indítható volt; a valódi hatásonkénti megszakító megerősítés törölte.
3. **Kereskedés melletti checkpoint.** Két ellenőrzött helyi kliens között
   egy +1 fegyver átadása és visszaadása commitolt, miközben a hosszú recovery
   hatás időcheckpointjai működtek. A checkpoint nyitva tartotta az ajánlatot,
   a következő ajánlatmódosítás friss fence-en ment. A partner eredeti három
   tárgya és 500 Goldja byte szerint megmaradt; pozíciója visszaállt.

A natív próbák ezen felül FIFO-t, eltérő ütemű tartalékokat, nyolcsoros
korlátot, kifogyást, potion bonus/clamp működést, törtpont-megőrzést,
halált és cooldown-megtartást, tiltott idő eldobását, pontos maradékidőt és
nagyon nagy elapsed érték túlcsordulásmentes számítását is lefedték.

## Helyi állapot és újrafuttatás

A PID 1610 dedikált tesztkarakter **49 tárgyállapotot**, 1000 Goldot, minden
korábbi C4–C18 próbatárgyat és tizenegy új recovery-próbát kapott 20–20
példányban. Három üres +1 bónuszcél és a kilenc bónuszeszköz ×60 megmaradt.
A recovery-készlet HP/SP/vegyes tartalékot, HP/SP folyamatos hatást,
HP-cserét, hosszú időt, közös cooldown-os tartalékot és azonnali italt,
valamint szint- és helytiltásos próbát tartalmaz.

**I → ital → Visszatöltés**; az aktív hatásokhoz az inventory felső
**Visszatöltések** gombja használható. A kézi próba kiinduló HP/SP-je
1000/20. A szervercache kiürítése után állítottuk be, majd az öt saját
szerverfolyamatot újraindítottuk. A saját MariaDB fut; a kőbehelyezés korábbi
85%-os beállítása megmaradt. A telepített GameServer, manager helper és
004-es migráció hash-e egyezik a manifesttel és az elkészült fájlokkal.

Újrafuttatás: `SeedRecoveryUnityLive.py` leállított saját csomaggal,
`PrepareUnityValidationSnapshot.py`, `RecoveryUnityLiveSmoke.py` és a
`recovery-live` Editor bridge. **Minden ismétlés előtt újra kell indítani a
saját játékszerver-stackot:** a legacy DatabaseServer HP/MP cache miatt egy
korábban betöltött karakter állapotát a közvetlen tesztadat-SQL önmagában
nem állítja vissza. A fixture csak karakterválasztásban, azonosított két
codexitem tesztfióknál ír. A privát fixture és az ellenőrzési logok a nem
követett `build` könyvtárban vannak; a telepítési és owner mentések a saját
csomag `run/deployments` és `run/recovery-live` könyvtáraiban.

A canonical Unity jelenet-, kapcsolat-, Packages- és ProjectSettings-
módosításai megmaradtak és kimaradnak a checkinből. A snapshot hiányzó
világ/VFX/Ocean assetjeinek jelzései nem akadályozták az item validációt.

## Nyitott kiadási kapuk

Teljes Unity player build, FreeBSD, többcore-os terhelés és hibainjektálás
nem futott. A legacy HP/SP és az item-adatbázis közötti teljes crash-
atomikusság nincs megoldva: a mentett debit utáni, live jóváírás előtti
crash elveszett visszatöltést okozhat. A feldolgozás jelenlegi heartbeatje
másodperces; a profil rövidebb tickjeit összegezve számolja el.
Automatikus ital és időzített/toggle hatások következő szeletek maradnak.
