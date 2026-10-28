# Küldetéstárgyak és valódi adatkonverziós próba — 2026-10-28

Munkapéldány: `D:/AurigaGlobal/LiveWork/AurigaGlobal`,
ág: `codex/mission-engine`, HEAD: `45e76b230f83a9adc2a244881f488786679ef7e7`.
A munkafa már a folytatás előtt is módosított volt. Ebben a körben nem
készült commit, Unity-módosítás, távoli telepítés vagy helyi játékadat-átállás.

## Elkészült

- Natív 10-es tárgykategória, token/első quest/összes quest használati mód,
  explicit csoportkód, környezet- és megfigyelőszabály. A használati terv
  nem fogyaszt tárgyat, nem ad jutalmat, nem futtat Lua-t.
- V13 fejlesztői katalógus: `019_quest_items.sql`, SQL-snapshot/betöltő,
  validátor, kanonikus hash és tranzakciós szerkesztői mentés.
- Qt szerkesztőűrlap, másolás, módváltás, tokennél inaktív használati mezők.
  A földre dobott tárgy modellje megőrizhető, felszerelésbónusz nélkül.
- A meglévő felhasználói katalógus v7→v13 offline felkészítése. Az egyetlen
  kiadott tárgy, profilok és kategóriaszámlálók megmaradnak. Ez fájlba írt
  jelölt, nem az aktív adatbázis migrációja.
- `convert_quest_tokens.py`: szigorú, offline, valódi Unity-asseteket és
  lokalizációt is ellenőrző részkonverter. Bemenetként csak a korábban mentett,
  hash-ellenőrzött adatokat és az abból újraszámítható teljes megfeleltetést
  fogadja. A megőrzendő katalógus minden celláját/számlálóját újra összeveti
  a mentéssel. Nem ad ki runtime vnum-feloldót és SQL-végpontot nem fogad.

## Valódi helyi adatokkal ellenőrzött eredmény

A teljes mentés 10 714 régi prototípust tartalmaz. A mostani célzott próba
a régi ITEM_NONE és ITEM_QUEST típus 725 sorát vizsgálta: 724 nem nulla
azonosító és egy archivált nullahelykitöltő. Ez nem a teljes katalógus
elkészült konverziója.

| Eredmény | Darab |
|---|---:|
| Teljesen leképezhető passzív token, ellenőrzött assetekkel | 2 |
| Ebben a részkonverterben blokkolt forrássor | 723 |
| Ebből scripted használat, külön kezelőportolást igényel | 479 |
| Tisztán assethiány miatt blokkolt passzív token | 21 |
| Konvertált tokenek lokalizációs felülvizsgálati hibája | 0 |

A blokkolási okok átfedhetik egymást. A teljes, soronkénti jelentés a
csomag privát `run/item-migration/20261028-quest-token-final-v2.json`
fájljában van. Nem került játékosadat Gitbe, nem töröltünk blokkolt sort.
Számos másik forrássor nem nulla generikus értéket, apply-t, időlimitet,
extra protót, speciális flaget vagy családszintet hordoz. Ezeket a megfelelő
domainbe kell átvinni; egyszerű tokenné alakításuk hibás volna.

A Lélekgömb (`30135` → `10:128`, `quest.lelekgomb`) és az Ismeretlen
aranygyűrű (`40001` → `10:220`) új, natívan validált prototípusként
visszaolvasható. A régi azonosító kizárólag az offline auditban található.
A halmozható tokenek maximuma a régi szerver `ITEM_MAX_COUNT` értékével
egyezően 50 000, nem az új szerkesztő 200-as mintabeállítása.

A Unity importált nevében talált hibás helyettesítő karakter helyett a
szerver helyes magyar neve szerepel. Az ikon és a földmodell fájlja létezik,
GUID-ja egyezik a szerializált Unity-hivatkozással; a fájlok és meták hash-e
rögzített. A 21 hiányos passzív token Unity-bejegyzésében üres ikonút és
nulla ikon/modellhivatkozás van. Nem rendeltünk hozzájuk találomra assetet.

A két új token és a megőrzött felhasználói fegyver összesen három tárgyként
került egy **új, izolált MariaDB-be**. Mind a 20 sémalépés sikerült; a natív
visszaolvasás hash-e és kategóriaszámlálói azonosak a bemenettel. A sandbox
a próba végén leállt. Katalógushash:
`0fa135738df56a55ffdd0c65ea75dda0611a684dbc4105fde43728ff4810d99f`.

Normál és AddressSanitizer eszközzel a teljes részkonverziós kimenet
bájtról bájtra egyezik. Mindkettő `installable_release=false` jelölt.

Az aktív `Server2-Local` adatbázisból olvasott mind a 33 `protos` tábla
metaadata és cellája megegyezik az eredeti mentéssel: továbbra is v7 és
egy tárgy. A játékot még nem állítottuk át részleges katalógusra.

## Ellenőrzések

- Friss natív ServerTests build; teljes CTest: **109/109 sikeres**.
  Ez a napló az új konverter külön CTest-bejegyzése előtti teljes kör.
- A végső quest-, konverter- és független hash-tesztek: **3/3 sikeres**,
  normál és ASAN buildben is. Az új konverter **11 tesztesetet** futtat.
- Quest domain: **54 ellenőrzés**; határértékek, hiányzó/ismeretlen kulcs,
  sérült tartalom, admission, observer, környezet, szint, handler, hash és
  SQL-snapshot visszaolvasás.
- Qt/model/natív SQL szerkesztő: **42/42 sikeres**, normál és ASAN helperrel;
  másolás, hibás mentés visszagörgetése és számlálómegőrzés is ellenőrzött.
- A v13 kódolást független Python encoder ellenőrzi üres és kitöltött
  quest-táblával. A kitöltött fixture hash-e:
  `fbae0855a1b9f72438ca193f6c737c6d97edfc7517276f5af2ab6ffeabb241ed`.
- A jelenlegi aktív katalógus olvasási összevetése és a valódi adatos
  izolált import is sikeres. A kapcsolódó hash-ek az evidence JSON-ban vannak.

## Még szükséges a teljes átálláshoz

1. Az F2 többi kategóriájának és altípusának natív lefedése: kosztüm,
   foglalatkő, eszköz, halászat, polimorf, külön ITEM_SOUL, pet/mount,
   speciális tárgyak; a már létező kategóriák hiányzó viselkedései is ide
   tartoznak. A kivezetett DragonSoul nem tér vissza.
2. A régi nem nulla értékek, apply-k, flag-ek, limitek, szintenkénti árak,
   speciális fejlesztések, questhasználat és csoportok szemantikai portolása;
   az összes család jóváhagyható, teljes konverziója. A korábbi teljes
   leltár 4 532 normál kovácsrecept-tervezete még nem kész runtime tartalom.
3. F3/F4: ItemKey + példányszint/tulajdon/hely/állapot tartós modell,
   tranzakciós gazdasági író és receipt, majd Database/GameServer átállítása.
   CommitUnknown esetén tartós operation ID szerinti egyeztetés szükséges.
4. F5/F6: protokoll és Unity katalógus/UI, boltok, dropok, questek, Mission,
   jutalmak, socketek és minden megmaradó tartalomhivatkozás átvezetése.
5. F7: a teljes csomag offline migrációja és helyi játékpróba, csak az előző
   kapuk után. A korábbi leltár 49 hiányzó tulajdonosú példánya és a többi
   karanténadat megmarad; tulajdonost nem találunk ki hozzájuk.

Következő implementációs irány: az F2 fennmaradó kategóriái és a konverter
mezőinek szemantikai lefedése, nem egy részleges katalógus bekapcsolása.
Távoli gépen nem történt művelet.
