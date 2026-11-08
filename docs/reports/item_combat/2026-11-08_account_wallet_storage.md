# C33 — Fiókpénztárca és további típusos tárolók

A szerver és a Unity közös bekötése elkészült a `20261103`, illetve
`/main/unity63-integration-20261103` ágakon. A külön típusos leltár
**Fiók és tárolók** gombja megnyitja a bankot, Mall-átvételt, céhraktárt,
tárgyleveleket, függő jutalmakat és a közös DragonCoin-pénztárcát.
A pontos működést az [IC-068](../../adr/item_combat_068_account_wallet_storage.md)
rögzíti. A kapcsolódó Unity-változás: **cs:94**.

## Helyi játékpróba

A telepített helyi szerver valódi TLS/session útján, Unity 6.3-ban,
tényleges UI-gombokkal **836 ellenőrzés és öt újrabelépés** sikeres.
Három elkülönített karakter, két fiók és egy céh vett részt.

| Eset | Igazolt eredmény |
| --- | --- |
| Banki jogosultság | Raktáros nélkül olvasható, mozgatás elutasított; valódi közeli, kijelölt NPC-vel működik. |
| Teljes és részleges betét | A +9-es fegyver ID-ja, generált bónusza és három foglalatának teljes állapota megmarad; a 20-as stack 13+7-re válik. |
| Közös bank | Ugyanazon fiók másik karaktere kiveheti és visszateheti a kötetlen fegyvert; a karakterhez kötött tárgy kivétele elutasított. |
| Részleges kivét és átrendezés | A 7-es stackből 3 a másik karakterhez kerül, 4 megmarad; a maradék a 20-as célcellára helyezhető. |
| Fiókpénztárca | Két 100-as pénzjegy 200-as jóváírást ad; egy jegy kiváltása után a másik karakter 100-as közös egyenleget lát, és azt is elköltheti. A végén 4+1 pénzjegy és nulla fiókegyenleg marad. |
| Mall és jutalom | Mindkettő egyszer átvehető; a Mall fiókhoz kötött tárgya az átvevő karakterhez kötődik. |
| Céh | Vezető és tag is betehet; a tag kivétét a UI és a szerver is tiltja; a vezető mindent kivehet. A külső karakter nem olvashatja a raktárt. |
| Tárgylevél | A szerver név alapján oldja fel a címzettet. A küldő újrabelépése után is hiányzik a küldött fegyver; a címzett teljes állapottal átveheti, és újrabelépve is megtartja. |
| Ismétlés és elkülönítés | Minden megerősítés nonce-os ismétlése változatlan állapotot ad. Másik fiók bankja és pénztárcája üres marad. |

A végső natív SQL-visszaolvasás a teljes példánypayloadot, a kötés tényleges
tulajdonosát, az 5 pénzjegy és 29 anyagegység megmaradását, az üres
céhraktárt és az állandó, 1-es közös fiókgenerációt is igazolja.
**1089 másik typed tárgyrekord**, más tulajdonosok pénztárcái és egyéb
mentett adatai változatlanok. A tesztkarakterek nem kaptak legacy tárgyat.
A korábbi részpróbák saját adatai megmaradtak.
Az ideiglenes raktáros regenjét az eredeti bájtokra állítottuk vissza.

## Fordítás és regresszió

- **48 kiválasztott natív tesztcsoport sikeres.** A végső SQL-betöltési
  javítás után a storage-runtime és a két érintett MariaDB-csoport is
  ismét sikeres. A külön protokollfejléc-ütközési teszt sikeres.
- A natív SQL-próba lefedi a két karakter közös fiókegyenlegét, a régi
  revízió visszagörgetését, a tényleges céhvezető ellenőrzését, a nem üres
  céh feloszlatásának tiltását és a lezárt céhbe későn érkező betét elutasítását.
  Az autentikált tárolóolvasás mindegyik változó SQL-guard alakja külön
  regressziós eset; a hamis karakter–fiók kapcsolat elutasított.
- Unity 6.3 fordítás és a végső offline validátorcsomag sikeres.
  Az új tárolóvalidátor **4667 ellenőrzése** natív goldent, minden
  csonkolást, TCP-darabolási pontot, tényleges ablakműveletet és helyi
  jóslás nélküli megerősítést vizsgál.
- A kezelő csomagolási **6 tesztje sikeres**. Az új migrációt tartalmazó
  kezelő és a friss GameServer/ItemBenchTool telepítve van a helyi csomagba.
- A **020 migráció kétszer lefutott**, megőrizve mind az **1076 korábbi
  tárgyrekordot**, a tárolókat és a korábbi katalógussorokat.

A helyi bizonyítékok: `build/c33-native-tests-all.log`,
`build/c33-native-fix-tests.log`, `build/c33-unity-offline-final.log`,
`build/c33-unity-live3.log`, `build/c33-storage-final-verify.log`,
`build/c33-migration-evidence.json`, `build/c33-validation-evidence.json` és
`build/c33-deployment-evidence.json`. A privát fiókadatok a helyi
build-/backupkönyvtárban maradtak.

## Hatókör és további feladat

A próba a valódi hálózati és UI-komponenseket használó, forráshash szerint
egyeztetett, elkülönített Unity-projektben futott. Teljes világjelenetes
grafikai próba, standalone kliens, FreeBSD és production konverzió nem
futott. A kihagyott VFX-/Crest-/SpeedTree-eszközök indulási üzenetei a
korábbi snapshot alapállapotához tartoznak.

A legacy account.cash/SÉ egyenleget nem importáltuk; a normál legacy
leltárat és shopokat nem kapcsoltuk át. A typed NPC-bolt Yang-alapú.
A levél jelenleg tárgykézbesítés, szöveg/utánvét/postadíj nélkül.
A következő nyitott nagy munkacsomag a teljes Mission-/quest-integráció,
majd a tartalomkonverzió és a kiadási ellenőrzések.
