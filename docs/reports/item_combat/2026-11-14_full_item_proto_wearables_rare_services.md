# Teljes item protó konverzió kosztümök és ritka bónuszok

2026-11-14. **110 további protó** egy csomagban: 103 frizurakosztüm,
2 ruhakosztüm, 4 ritkabónusz-szolgáltatástárgy és 1 feloldott ládadefiníció.
**9 677 konvertált, 491 kivezetett, 818 akadályos** sor a 10 986 forrásból.
6 759 natív definíció; a formátum változatlanul 39-es.
Katalógushash: `4b1c3aeba3c23a84df6532d187e4d55a2b6b4cf3770a87e4a230b9822e032e36`.

## Helyreállított modellek és működés

Az aktív Unity-projektben megvolt a hiányzó frizurák és egy ruhamodell
eredeti definíciója, mesh-e és textúrája. A futás közben betölthető
kapcsolat hiányzott. **213 pontos assetkapcsolat** került vissza a
futásidejű katalógusba és az Addressables-regisztrációba, az eredeti
karakterváltozat és megjelenésazonosító alapján. A helyreállító nem választ
szomszédos frizurát, nem cserél modellt és nem generál pótló grafikát.

A 105 kosztüm mindegyike felszerelhető és levehető. Pontos bónusza,
elhelyezése és időállapota mentődik; ismételt kérés nem duplázza és nem
tölti újra a tárgyat. A lejárat eltávolítja a példányt és a bónuszt,
a kliens megjelenéscsomagja pedig törli a külsőt.

Az eredeti, kiadott 1 574 definíció és **29 204 tartalmi sor**, továbbá
az előző jelölt **102 692 tartalmi sora** megmaradt. Az azonosítószámlálók
nem csökkentek. Az assetkapcsolatok bővülése a Unity-katalógusokban történt.

## Ritka bónusz szolgáltatások

- **30617**: hozzáadás a 6–7. bónuszhelyek csoportjához, legfeljebb két sor.
- **30618**: a meglévő 6–7. bónuszok újrasorsolása, a darabszám megtartásával.
- **30619**: hozzáadás az önálló 8. bónuszhelyhez.
- **30620**: a meglévő 8. bónusz újrasorsolása.

Mindegyik a szerver által meghatározott natív profilt használja. A forrás
egyenletes választását a rögzített, egyező súlyok biztosítják; a konvertáló
elutasítja az ettől eltérő adatcsomagot. Az érték a bónusz legmagasabb
engedett szintje, legfeljebb az 5. szint. A normál és a másik ritkabónusz-csoport
megmarad, a ritka/extra helyek közös ismétléstiltása érvényesül.

A meglévő GC236 előnézet és megerősítés kezeli a műveletet Unityben.
Csak sikeres szerveres művelet fogyaszt egy egységet. Az elavult előnézet
véletlensorsolás és fogyasztás előtt elutasítódik; az ismételt művelet a
mentett eredményt adja vissza. A kliens nem választ értéket és nem módosít
helyben bónuszt vagy készletet.

## A láda külön állapota

Az **50082** jutalomlistájának hiányzó tárgyfüggősége feloldódott,
ezért a definíció konvertálható. **A láda még nem nyitható**: a listában
65/70%-os véletlen kezdőbónusz-kiosztás is van, amelynek általános
kiadási szabálya még nincs bekötve. A valódi szerver-worker `Unsupported`
eredménnyel, sorsolás és fogyasztás nélkül utasítja el. A Unity nem enged
nyitást megerősíteni. Ez az egy sor definíciós részeredmény, nem kész
játékmeneti funkció.

## Lefutott ellenőrzések

- A szerveres item-worker új próbáinak fordítása sikeres. A termelési
  natív runtime és a csomagformátum ebben a csomagban nem változott;
  az új profilok a már megvalósított műveletekhez kapcsolódnak.
- **60 konverziós teszt**, köztük a két ritka csoport, a legmagasabb
  engedett érték, az ismeretlen paraméterek és az eltérő forrássúlyok.
- **9 CTest-cél sikeres**: attribute runtime, példánybónuszok, kosztümök,
  felszerelési statisztika, ládanyitás, teljes protókonverzió, Qt-szerkesztő,
  SQL-szerkesztő és a valódi katalógus-runtime MariaDB-próbája.
- A teljes **6 759 definíciós** jelölt saját MariaDB-be írva és natívan
  visszaolvasva; a hash és a tartalom egyezik. **4 876 valódi item-worker
  ellenőrzés** mind a 110 új protóval, mentéssel és új owner-sessionnel.
- Unity **6000.3.27f1** fordítás és **13 validátorsorozat** sikeres.
  **539 valódi worker-állapot** feldolgozva a leltárban és szolgáltatásablakokban;
  **216 pontos modellkapcsolat** ténylegesen betöltve a **8 karakterváltozat**
  felszerelési rigjére. A skinnelt modellek csontkötése hiánytalan.
  Megjelenésváltás, levétel/lejárat miatti törlés, ikonok, megerősítési
  kérés és protokoll-darabolás ellenőrzött.

A teljes belépés/auth/TLS/ECS-világfolyamat, valamint az Addressables
játékoscsomagjának buildje és távoli terítése nem futott le ezzel a jelölttel.
`release_ready=false`; a teljes katalógus **nincs a játékba telepítve**.

## Fennmaradó akadályok

| Akadálycsoport | Forrássor |
|---|---:|
| Hiányzó ikon/modell/vizuális asset | 262 |
| Hiányzó vagy hibás jutalomadat, jutalomfüggőség | 283 |
| Hiányzó viselkedés vagy képességdefiníció | 200 |
| Forrásadat, korlát vagy más szabály | 73 |

A helyi játékadatbázis 1 574 definíciós maradt. Account, karakter,
régi/natív tárgypéldány, céh és chatelőzmény: 0; **94 játékállapot-tábla üres**,
frissen visszaellenőrizve. A tesztkatalógus a C: meghajtó saját adatbázisában futott.
A Unity alacsony lemezterület miatti megállása után négy korábbi, igazoltan
leállított benchmark-adatbázis adatkönyvtára visszaállíthatóan a C:-re került;
a fájlszám és minden fájl bájtmérete egyezett az áthelyezés után.

## Folytatási alap

- [2026-11-14_full_item_proto_wearables_rare_services.csv](2026-11-14_full_item_proto_wearables_rare_services.csv): minden protó és a 110 új konverzió.
- [2026-11-14_full_item_proto_wearables_rare_services.evidence.json](2026-11-14_full_item_proto_wearables_rare_services.evidence.json): tesztállapotok, eredeti modellek és fájlhash-ek.
- `build/full-proto-wearables-v39-final.json`: ellenőrzött következő jelölt.
- `tests/server/SourceWearableCatalogTests.py`: a szerveres próba indítója.
- `Assets/Metin2/Editor/Metin2SourceWearableRecovery.cs`: pontos assethelyreállító.
- `Assets/Metin2/Editor/Metin2SourceWearableValidator.cs`: a tényleges klienspróba.
- `C:/Users/ixtre/AppData/Local/Temp/AurigaItemValidation/wearables-v39-final/`: SQL és worker-csomagok.

Folytatás: [lélekmagok, bónuszadók és új ládanyitások](2026-11-14_full_item_proto_souls_costume_grants.md) — 21 további konverzió, 797 maradék sor.
