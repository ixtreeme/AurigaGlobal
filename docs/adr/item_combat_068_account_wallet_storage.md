# IC-068 — Fiókpénztárca és típusos tárolók (C33)

## Tulajdon és jogosultság

A meglévő Character, Account és Guild tulajdonosokat használjuk. A bank
(Safebox) és a Mall fiókszintű; a céhraktár Guild, a beérkező tárgylevél
és a függő jutalom Character tulajdonoshoz tartozik. Az új
`020_shared_storage.sql` csak a céhraktár bezárási kapuját adja hozzá.
A katalógus v30 és a példánypayload változatlan.

A futó szerver hiányzó tárolóelrendezést és nulla egyenlegű pénztárcát
hozhat létre. A művelet ismételhető, a már létező méretet és egyenleget
megőrzi. A fiók és a céh közös session-generációját karakterbelépéskor
nem foglaljuk újra. A közös revízió CAS-védett; minden játékosírás az
azonosított karakter aktuális session-fence-ét is tartalmazza.

Az SQL-olvasás és -írás a tényleges karakter–fiók kapcsolatot ellenőrzi.
Céhműveletnél a tényleges tagságot, vezetői műveletnél a guild.master
mezőt is zároltan ellenőrzi. Egy tag olvashat és betehet; kivenni és
átrendezni a vezető tud. A céh feloszlatását a nem üres raktár megállítja.
Az üres raktár lezárása és a későbbi betétek ugyanazon SQL-kapun
szerializáltak, a többi magról indított régi műveletet is kizárva.

A bank mozgásai kijelölt, azonos térképen 2000-es távolságon belüli
9005-ös raktáros NPC-t igényelnek. A saját fiók bankjának olvasása
NPC nélkül is engedélyezett, ilyenkor a mozgatás letiltott. A Mall és
a függő jutalom csak átvehető; a játékos nem tehet beléjük tárgyat.

## Tárgy és valuta

Teljes mozgatásnál az ID és a teljes példányállapot megmarad. Részleges
stack-mozgatás új, előre lefoglalt ID-t kap, a darabszám megmarad.
A rácsfoglalás és a teljes módosítás egy tranzakció. Felszerelt vagy
aktív hatású tárgy, megidézett társ, fejlődő pet pecsétje és kapcsolt
hordozó-/skinállapot ezen az általános útvonalon elutasított.

Karakterhez kötött tárgy bankba tehető, de csak az eredeti karakter
veheti ki. Fiókhoz kötött kézbesítés első átvétele a karakterhez köti a
tárgyat. Céhbe és másik karakternek levélben csak kötetlen, kereskedhető
tárgy küldhető. A címzettet a szerver név alapján oldja fel. A küldő
nem kapja meg a címzett leltárát vagy postaládáját.

A típusos DragonCoin pénztárca fiókszintű. Pénzjegy beváltásakor a fizikai
darabszám csökkenése és a fiók jóváírása együtt történik. Visszaváltáskor
a választott katalóguscímlet azonos értékű pénzjegyei kerülnek a karakter
leltárába, az egyenleg levonásával együtt. Túlcsordulás, fedezethiány,
régi előnézet vagy helyhiány nem fogyaszt. Az ismételt nonce/operation
nem ismétli meg a pénz- vagy tárgymozgást.

Az új típusos pénztárca nulla egyenleggel indul; a meglévő típusos értéket
megőrzi. A legacy account.cash/SÉ automatikus importja és a régi shopok
átirányítása nem része ennek a szeletnek. A típusos NPC-bolt továbbra is
Yangot használ. A levél itt tárgykézbesítés, külön szöveg, utánvét és
postadíj nélkül. A Mall/jutalom szolgáltatói feltöltéséhez a natív
admineszköz storage_grant művelete használható.

## Szerver és Unity

A CG/GC 201 külön protokoll. A kérés 57 byte + sequence, azonos műveleti
adatokkal az előnézetben és a nonce-os megerősítésben. A GC 201 v1 uint32
méretű, tartalmazza a státuszt, műveletet, tulajdonost, tényleges
rácspozíciókat, közös pénztárcát és címleteket, valamint beágyazott
GC 225 leltárprojekciót és GC 227 típusos leírásokat. A projekció kizárólag
megjelenítésre szolgál; a kliens nem küld vissza példánypayloadot.

A külön típusos Unity-leltár **Fiók és tárolók** gombja hat lapot nyit:
bank, Mall, céhraktár, levél, jutalom, pénztárca. A művelet, tárgy,
darabszám, opcionális célcella és levélcímzett kiválasztható. Az
Áttekintés után a Megerősítés a szerver tartós eredményére vár;
helyi tárgy- vagy egyenlegjóslás nincs. Az eredeti nulla/automatikus
értékek is változatlanul szerepelnek a megerősítésben. A pénzjegy meglévő
Beváltás gombja fiókvalutánál erre az ablakra irányít.

A normál legacy leltár átkapcsolása továbbra is külön, későbbre hagyott
konverziós feladat.
