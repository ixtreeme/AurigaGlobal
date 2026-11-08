# C34 — Tartós Mission- és Lua-quest item-integráció

A `20261103` szerverág és a `/main/unity63-integration-20261103` Unity-ág
közös bekötése elkészült. A típusos leltár **Mission napló** gombja a natív
Mission-motor részhaladását, megnyílt céljait, választásait és jutalmát
mutatja. A Lua-quest ugyanazt az atomi, szerver által meghatározott tárgy-
és Yang-műveletet tudja meghívni. A párosított Unity-mentés **cs:95**. Részletek:
[IC-069](../../adr/item_combat_069_typed_mission_quests.md).

## Végső helyi játékpróba

A friss tesztkaraktereken, a végleges szerverkóddal, a valódi TLS/session
útvonalon és tényleges Unity UI-gombokkal **296 ellenőrzés és hét
újrabelépés** sikeres. Három külön natív küldetés, két elkülönített karakter
és a valódi NPC/Lua-párbeszéd vett részt.

| Eset | Igazolt eredmény |
| --- | --- |
| Tartós gyűjtés | A másik karakter valódi tárgyeldobása és felvétele 1, majd 2 darabos, egyszer rögzített gyűjtési eseményt ad. A 1/3 részhaladás megmarad újrabelépve. |
| Többlépcsős NPC-cél | A 3/3 gyűjtés után az NPC-cél következik; korai jutalomkérés elutasított. Az NPC-s lépés után a függő jutalom is megmarad újrabelépéssel. |
| Jutalom és költség | NPC nélkül elutasított. A helyes átvétel 4 anyagot és 25 Yangot fogyaszt, 2 jutalomtárgyat és 100 Yangot ad, és együtt menti a teljesítést. |
| Ismétlés | A kéréssorszám/nonce ismétlése nem fogyaszt és nem jutalmaz újra. A valódi NPC-kattintás utáni háttérmentés sem változtatja sikeresre a korábban elutasított kérés eredményét. A teljesített küldetés és az egyenleg újrabelépve is változatlan. |
| Lua-kezdés | Az eredeti NPC-menü tényleges válaszgombja `pc.typed_mission_action` hívást indít. A coroutine csak a tartós kezdés után kap `true, committed` eredményt. |
| Lua-csere | A jutalomra váró állapot újrabelépve megmarad. A claim 9-ről 6-ra csökkenti a használható anyagot és új jutalmat ad; a folytatott Lua már ezt az állapotot olvassa. Új párbeszéd nem ad második jutalmat. |
| Választás és időzítő | A valódi naplóbeli választás az erdei ágat rögzíti. Újrabelépés közben letelik az időzítő; az erdei 1 tárgy/5 Yang jutalom áll elő, nem a másik ág 9 tárgy/99 Yang jutalma. A végső átvétel is megmarad újrabelépve. |

A natív SQL-visszaolvasás **három egyedi entitlementet**, a két pontosan
egyszer feldolgozott tárgyfelvételt és a végső gazdasági állapotot igazolja:
6 anyag a küldetéses karakternél, 2 a másik karakternél, 4 jutalomtárgy és
1080 Yang. **1126 másik typed tárgyrekord** és más tulajdonosok pénztárcái/
tanulási adatai, valamint más karakterek Mission-owner/haladási fejlécei változatlanok. Legacy tárgyat a tesztkarakterek nem kaptak.

## Fordítás és regresszió

- **57 natív tesztcsoport sikeres** a végső teljes futásban. Ebben szerepel
  az item-/katalógusregresszió, a Mission-kernel, checkpoint, csomag,
  reward/resource réteg, a C12 tárgyleadás, crafting és a valódi MariaDB.
- Az új SQL-esetek a részhaladás újrabelépését, a régi login tiltását,
  a közös entitlement/item/acknowledgement késői hibájának teljes
  visszagörgetését, a reward replayt és az itemesemények egyszeri
  rögzítését ellenőrzik. A crafting sikertelen kísérlete nem ad eseményt.
- A régi Mission store teljes próbája a COMMIT-válasz elvesztését és az
  adatbázis-crash utáni cold restore-t is lefuttatja. A versenytesztek
  MariaDB 13 alatt a 1020-as, igazolt rollbacket is figyelembe veszik;
  az elvárt egyetlen revízió/receipt/ledger és a vesztes operation hiánya
  továbbra is szigorúan ellenőrzött. A read-only egyeztető olvasás
  konfliktusa újraolvasható, az eredeti hatás nem játszódik újra.
- Unity **6000.3.27f1** fordítás és a teljes végső offline validátorcsomag
  sikeres. Az új Mission-validátor **3169 ellenőrzése** natív goldent,
  minden csonkolást/TCP-darabolási pontot, valódi naplógombokat és jóslás
  nélküli megerősítést vizsgál. Aktív csomag nélküli ablak is bezárható.
- A kezelő **20 szerkesztőtesztje**, **6 csomagolási tesztje** és a
  Mission-adatbázis **13 tesztje** sikeres, utóbbiban tényleges friss
  MariaDB-telepítéssel és haladást megőrző upgrade-del. A típusos táblák
  opcionálisak: hiányzó táblára nem adunk CREATE-jogot; későbbi telepítés
  után az explicit Mission-upgrade a szükséges táblajogokat hozzáadja.
- A **021 item-migráció kétszer** lefutott, megőrizve mind az **1108
  korábbi tárgyrekordot**, tárolót és katalógussort. A helyi külön Mission-
  séma 4-ről 5-re frissült. A katalógus továbbra is v30, nem történt
  production tárgy- vagy valuta-konverzió.

A végső helyi bizonyítékok: `build/c34-native-tests-final.log`,
`build/c34-unity-offline-final.log`, `build/c34-unity-live-final2.log`,
`build/c34-local-sql-verify-final2.log`, `build/c34-mission-db-bootstrap-final.log`,
`build/c34-migration-evidence.json`, `build/c34-validation-evidence.json`
és `build/c34-deployment-evidence.json`.

## Hatókör és takarítás

A játékpróba a valódi hálózati/session útvonalat és Unity-komponenseket
használó, forráshash szerint egyeztetett, elkülönített Unity-projektben futott.
Teljes világjelenetes grafikai próba, standalone kliens, FreeBSD és
terheléses próba nem futott. A hiányzó VFX-/Crest-/SpeedTree-fájlok indulási
üzenetei a korábbi snapshot alapállapotához tartoznak.

A külön teszt-NPC és Mission-tartalom visszaállítva. A már publikált saját
tesztquestek handler nélküli előzményként maradnak meg: a quest publisher
helyesen tiltja a tartós quest-/state-azonosító egyszerű törlését.
Más questforrást vagy játékosprogresszt nem töröltünk. Az eredeti források
új bootcsomagja kiválasztva, a friss szerver és kezelő telepítve.

Ez a típusos item-integráció és az első általános, tartós Mission-játékút.
A teljes 80 küldetéses kampány, Mission Studio, XP/szint-/közvetlen
fiókvaluta-jutalom, további világ-eseményadapterek és a régi questek
tartalmi konverziója továbbra is külön munka. A normál legacy leltárat
és a régi tárgyhivatkozásokat nem kapcsoltuk át automatikusan.
