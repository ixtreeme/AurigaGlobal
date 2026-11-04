# ADR IC-055 — Gyűjtőhelyes fogás, gyűjtés és szerzett gyakorlat

2026-11-04, C25c-1. Előzmény: [IC-054](item_combat_054_tools_runtime.md).
Alapmodell: [IC-023](item_combat_023_gathering_tools_and_fishing.md).

Elkészült az időzített, kijelölt világentitáshoz kötött halászat, bányászat,
favágás és növénygyűjtés közös szerveres–Unity útja. A befejezett művelet
tényleges típusos tárgyat és a profil szerint gyakorlópontot ad. Ez játszható
gyűjtőhelyes alapfolyamat; a legacy szabadvízi halászati minijáték átvezetése
C25c-2 marad. A normál legacy leltár migrációja továbbra is vár.

## Világtartalom

A `data/typed_gathering.json` v1 fájl az explicit gyűjtőhelyeket adja meg,
a típusos crafting műhelyekhez hasonlóan. Hiányzó fájl vagy üres `sites` lista
nem engedélyez gyűjtőhelyet. Hibás létező fájl esetén a core nem indul el.
Nincs automatikus legacy fogás-/érctábla-konverzió.

Egy helyhez ID, map, világentitás-race, név, eszközfajta és botnál első/második
fogási készlet tartozik. A range legfeljebb 1000, az idő 1000–120000 ms.
Az eszköz minimális gyűjtési ereje, az esély alapja, az erőlépés, az egy
erőlépésért és egy csalierőért adott esély külön szerkeszthető. A sikeresély:

`min(10000, alapBp + eszköz.gatherBonusBp + floor(eszköz.power / powerStep) * powerChanceBp + csali * baitChanceBp)`.

A számítás egész számos, u64 köztes értékkel. Csali nélküli bot nem indítható;
más eszköznél nincs csali vagy halászati készlet. A jutalmak explicit
ItemKey/count/weight sorok. Egy siker egy súlyozott sort választ; minden
kulcsnak létező, nem kivont, menthető tárgyra kell mutatnia. A súlyösszeg u32,
egy jutalom mennyisége legfeljebb 10000. Legfeljebb 256 hely és helyenként
32 különböző jutalomkulcs engedett. A natív validátor ellenőrzi a profilokat,
a hivatkozásokat, a korlátokat és az egyértelmű map/race/kind/pool párosítást.

A JSON-szerkezet leírása a `docs/schemas/typed_gathering.schema.json`, a
csomagkészítő üres példányt és sémát ad a friss csomagokhoz. A telepített
helyi próba hat külön szabályt használ; ezek tesztadatok, nem végleges balansz.
A race a világentitást azonosítja; tárgy-vnum adapter nem készült.

## Indítás és időzítés

A saját felszerelt eszköz ablakából a **Gyűjtés előnézete** nyitja a
gyűjtőablakot. A célzott világentitást ellenőrzi a szerver; cél nélkül a
legközelebbi megfelelő helyet választja, azonos távolságnál a kisebb VID-et.
A cél élő nem-PC, a szabály mapján áll, és hatótávon belül van.

A preview rögzíti a teljes eszközpéldányt, owner-sessiont, login tokent,
katalógushasht, világentitás-generációt és a szabályt. A külön **Gyűjtés
indítása** kérés ezeket újra ellenőrzi. A szerver monotonic órával méri az
időt. Egy karakter egyszerre egy műveletet végezhet; a második indítás nem
indítja újra az órát. Futás alatt a typed item busy sorompó zárolja a
tárgyműveleteket, a grantok várakoznak.

A szervertick újra ellenőrzi a karaktert, tárgyat, sessiont és a világhelyet.
Az indulási helytől tengelyenként 50 egységnél nagyobb elmozdulás, eltűnt vagy
hatótávon kívüli cél, tiltott karakterállapot és kijelentkezés megszakítja a
folyamatot. A kézi megszakítás ugyanezt teszi. Befejezés előtti megszakításkor
nincs jutalom, pont vagy csalilevonás. Publikus befejezés-/gyakorlópont-RPC nincs.

## Atomi eredmény

A lejárt idő után a szerver ismét admission permitet kér, és a friss owner
állapotából tervet készít. Minden lehetséges jutalom teljes helyigényét és
ID-foglalását ellenőrzi az első véletlen dobás előtt. A cél maxStackje szerint
több halom is keletkezhet; mennyiség nem vész el. A teljes inventory nem
választhat másik véletlen eredményt: a művelet RNG nélkül elutasított.

Siker esetén a C25b védett practice API-ja a befejezett szerveres eseménnyel
legfeljebb egy pontot ad, a pontos N/D arány szerint. Teljes gyakorlatnál és
maximális szinten nincs gyakorlásdobás. Sikertelen fogás/gyűjtés nem ad pontot.
A befejezett halászati próbálkozás siker és kudarc esetén is törli a bot
csalierő-állapotát. A következőhöz új csalit kell alkalmazni.

A jutalom, a gyakorlat, a bot csalija, az összes érintett teljes before-state
CAS és az operation receipt egy tranzakció. A kudarc is kap receiptet és
új eszközrevíziót. A receipt az eredmény mellett a tényleges RNG-boundokat és
dobásokat is rögzíti. Kriptográfiai RNG-hibánál nincs mentés; a 0/100%-os
döntések, az egyetlen jutalomsor és a determinisztikus gyakorlat nem húz RNG-t.

A mentést az ItemStore végzi; bizonytalan commitot a meglévő tartós receipt-
egyeztetés rendez. A callback csak az eredeti login tokenjére alkalmazható.
Hiányzó authoritative reload bontja a kapcsolatot. A siker megismételt startja
csak az eredményt küldi vissza, új jutalom nélkül. Új loginban a korábbi nonce
nem indíthatja újra a folyamatot. A futó időzítő nem perzisztens; szerverújraindítás
előtti befejezetlen próbálkozás nem ad jutalmat és nem költ csalit.

## Protokoll és Unity

CG254 a meglévő 26 bájtos recovery szerkezet + sequence:
10 preview (nonce=0, saját tool ID, opcionális cél-VID), 11 start,
12 cancel, 13 állapotlekérés. Az utóbbiak nonce/tool/cél hármast igényelnek.

GC254 v3, minimum 83 bájt + név/jutalom/metaadat, little-endian:
header/size/version/status/error; nonce; tool ID/revision/ItemKey/level/points/bait;
site ID/target VID/kind/duration/remaining/chance; névhossz:u16 és UTF-8 név;
outcome:u8/afterPoints:u32/afterBait:u32; yieldCount:u8 és {ItemKey,count:u32};
metadataLength:u32 és GC227. Az outcome csak Applied állapotban jelenik meg.
A v1 halfeldolgozási és a v2 eszközcsomagok megmaradtak. Négy natív golden
és a szigorú Unity-kodek rögzíti az új szerződést.

Unity a szervertől kapott hátralévő időt csak megjeleníti; a poll nem fejezi
be a műveletet. A start, a cancel és a frissítés külön gomb. A kliens nem ad
jutalmat, nem von le csalit és nem ír pontot helyben. Megváltozott eszköz
érvényteleníti a preview-t. A mentett eredmény olvasható marad; logout törli
a kliensállapotot.

Tényleges próbák, helyi adatok és korlátok:
[C25c-1 jelentés](../reports/item_combat/2026-11-04_resource_gathering_runtime.md).
