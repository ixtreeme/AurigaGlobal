# ADR IC-060 — Típusos pettojás-keltetés

## C26m — Teljes állapotú szállítódoboz

A párosított Unity changeset `cs:84`. A Pet tárgyai felület csomagolja és
kiveszi a visszahívott petet. A doboz a teljes eredeti pecsétet tárolja,
idő- és képességvesztés nélkül. A SQL-migráció `013_pet_supplies.sql`,
a leltárcsomag GC225 v10. Natív MariaDB-, codec- és Unity-validálás,
valamint 300 ellenőrzéses helyi TLS-próba sikeres; részletek a
[jelentésben](../reports/item_combat/2026-11-07_pet_carrier.md).

C26c a szerver `20261103` és a Unity `/main/unity63-integration-20261103` ágán. A párosított Unity changeset `cs:74`.

A típusos leltár **Keltetés** művelete megmutatja a katalógusban megadott árat és a jelenlegi típusos aranyegyenleget. A szerver két percig érvényes megerősítést ad a tojás, a pénztárca és a katalógus állapotához. A név legfeljebb 25 Unicode-karakter és 64 UTF-8 bájt lehet. A szerver ellenőrzi a nevet és a tiltott szavakat. A név a példányhoz tartozó megjelenítési név, több pet használhatja ugyanazt a nevet.

Sikeres megerősítéskor a szerver sorsolja a három születési tulajdonságot a katalógus megadott lépéseiből. Egy tranzakció von le egy tojást és az árat, majd az új pecsétet a leltárba helyezi. Az utolsó tojás helye felhasználható. Tele leltár, elavult megerősítés és kevés arany esetén az eredeti erőforrások megmaradnak. Az ismételt megerősítés ugyanazt az eredményt kapja az adott munkamenetben.

Az `011_growth_pet_birth.sql` a teljes születési állapot verziózott mezőjét adja a példányhoz. A név, születési idő, szint, evolúció, életidő, három tulajdonság és a négy kezdetben lezárt képességhely tartósan mentett. Újrabelépés után a **Pet adatai** művelet a szervertől olvassa az adatlapot. Ebben a lépésben a fejlődő pet pecsétje a tulajdonos leltárában mozgatható. Az idézés, etetés, fejlődés és képességhasználat külön bekötés.

A CG250 kezeli a lekérdezést, megerősítést, bezárást és a pet adatlapját. A GC253 az ár, eredmény és adatlap válasza. A GC225 v8 csak fejlődő petet tartalmazó leltárnál szükséges. A normál ládanyitás és a rögzített társak idézési útja külön marad.

## Helyi ellenőrzés

A Windows Release GameServer build és 15 natív, MariaDB- és próbapad-teszt sikeres. Az elkülönített Unity 6.3 projektben 362 keltetési, 335 pecsét- és 4179 típusos leltárellenőrzés ment át.

A 60 ellenőrzéses helyi Unity TLS-próba a leltárgombbal indult. Elavult token és hibás UTF-8 név után két tojás és 150 000 arany maradt. A sikeres keltetés után egy tojás, 50 000 arany és egy új pecsét maradt. Az ismételt megerősítés ugyanazt a pecsétet mutatta. Újrabelépés után egyezett a születési állapot és az egyenleg, a második keltetést az elégtelen arany miatt a felület letiltotta.

Az SQL-utóellenőrzés egyetlen keltetési tranzakciót és új pecsétet talált. A natív próbapad visszaolvasta az életidőt, a születési tulajdonságokat és mind a négy lezárt képességhelyet. A 186 korábbi példány hash-e és revíziója, az öt meglévő pénztárca, a tanulási adatok és a korábbi katalógussorok egyeztek a mentéssel.

## C26d — A fejlődő pet követése

A fejlődő pecsét a meglévő CG249 művelettel idézhető és visszahívható. A fix pecsétek és a fejlődő pet egy közös követőhelyen osztoznak. A szerver a pet evolúciós fokozatához tartozó alakot jeleníti meg. Aktív idézési szándéknál az új munkamenet visszaállítja a követőt, amint a játékállapot ezt megengedi. A **Pet adatai** a követő állapotától függetlenül elérhető.

Az aktív idejű pet életideje a tényleges követés alatt fogy. A szerver öt másodpercenként elszámolja az eltelt időt, a követés megszakadásakor pedig elmenti a maradékot. Azonos szerverfolyamaton belül a következő belépés megvárja a függő elszámolást. Lejáratkor a követő eltűnik, az idézési szándék törlődik, és a pecsét megmarad. A GC225 v9 jelzi a fejlődő pet követő, függő vagy lejárt állapotát. A már elmentett fejlődő pecsétek ugyanazt a születési állapotot használják.

A párosított Unity changeset a `cs:75`. A Windows GameServer build, kilenc natív és MariaDB-teszt, valamint a Unity 6.3 validátorai sikeresek. A 113 ellenőrzéses helyi TLS-próba a keltetéstől az idézésen és az aktív újrabelépésen át a visszahívás utáni újrabelépésig futott. A kliens megkapta a követő megjelenését és törlését. A mentett életidő az offline várakozás alatt és a visszahívás után változatlan maradt. Az SQL-utóellenőrzés 190 korábbi példányt és hét pénztárcát egyeztetett a próba előtti állapottal.

## C26e — A fejlődő pet etetése

A **Pet adatai → Etetés** felületen a szerver listázza az adott állapothoz illő leltári ételeket. A követő pet arányos életidőt vagy tárgytapasztalatot kaphat a forráshoz rendelt szabály szerint. A visszahívott pet a közvetlen ételtől teljes életidőt kap. Lejárt petnél ugyanez az étel a katalógusban megadott arányra éleszti újra a pecsétet.

Az előnézet a forráspéldányt és a levonandó mennyiséget rögzíti. A szerver a megerősítéskor ismét ellenőrzi a feltételeket, elszámolja a követés során eltelt időt, majd egy tranzakcióban menti az étel levonását és a pet új állapotát. Hatás nélküli vagy elavult műveletnél az étel megmarad. Az ismételt megerősítés ugyanazt a műveletet adja vissza. A CG250 az etetési kéréseknél is a meglévő keretet használja. A GC253 v2 az etetési válaszokban adja át a listát, az ajánlatot és az eredményt.

A párosított Unity changeset a `cs:76`. A Windows GameServer build, nyolc natív és MariaDB-teszt, valamint a Unity 6.3 validátorai sikeresek. A 175 ellenőrzéses helyi TLS-próba lefedte a követő és a visszahívott pet etetését, a tele életidőnél visszautasított ételt és az ismételt megerősítést. Újrabelépéskor ugyanaz a pecsét 259 200 000 ms életidőt mutatott, a három ételből egy maradt. A MariaDB-próba ellenőrizte az újraélesztést és a felszerelésből nyert tárgytapasztalatot is.

## C26f — Szintlépés és evolúció

A követő fejlődő pet a gazda számított harci EXP-jének 45%-ával megegyező külön jutalmat kap. A szerver a harci jutalmakat a megszerzésük sorrendjében számolja el akkor is, ha közös SQL-tranzakcióban menti őket. A profilszabály dönti el, hogy a szintlépéshez harci EXP vagy harci és tárgy-EXP együtt szükséges.

A **Pet adatai** megmutatja a két EXP-számlálót és az evolúciós kaput. A kapunál a követő peten az **Evolúció** egy szinttel és egy fokozattal léptet, és nullázza a számlálókat. A kérés az előző fokozatot is megadja, így a megismételt kérés nem léptet tovább. A fokozat és a szint ugyanabban a példányban marad. Az új alak a mentés után jelenik meg, és újrabelépéskor a szerver a mentett fokozatból állítja vissza. Az adatlap és az evolúció eredménye a GC253 v3-on érkezik.

A párosított Unity changeset a `cs:77`. A Windows GameServer build, nyolc natív és MariaDB-teszt és a Unity 6.3 validátorai sikeresek. A 113 ellenőrzéses helyi próba valódi harccal érte el a 2. szintet, majd újrabelépés után az adatlapról indította az evolúciót. A második újrabelépéskor az adatlap 3. szintet és 1. fokozatot mutatott, és a szerver a 32000-es követőalakot állította vissza. Az SQL-utóellenőrzés visszaolvasta a szintet és fokozatot. A 177 ellenőrzéses etetési regresszió és annak SQL-utóellenőrzése is sikeres.

## C26g — Képességtanulás és rangfejlesztés

A **Pet adatai → Képességek** mutatja a négy helyet, a megtanult képességeket és rangjukat. A szerver a pet életkora és evolúciós fokozata alapján nyitja meg a helyeket. A követő pet a katalógusban engedélyezett könyvet használhatja a kiválasztott nyitott helyen. Az **Ismert fejlesztése** út a könyvhöz beállított esélyt és várakozási időt alkalmazza.

Az előnézet a könyvpéldányt, a helyet, a következő rangot és a sikeresélyt rögzíti. A megerősítéskor a szerver ismét ellenőrzi a petet, a könyvet és az életidőt. Egy tranzakcióban von le egy könyvet és menti a petet. Ismert képesség sikeres könyves fejlesztésénél ugyanaz a tranzakció menti a karakterhez és képességhez tartozó várakozási időt. Sikertelen dobásnál egy könyv fogy. A `012_pet_study_cooldown.sql` a várakozási idő tábláját telepíti. A CG250 és a GC253 v4 viszi a képességlistát, az előnézetet és az eredményt.

A párosított Unity changeset a `cs:78`. A Windows GameServer build, tíz natív és MariaDB-teszt, valamint a Unity 6.3 validátorai sikeresek. A 165 ellenőrzéses helyi próba visszaolvasta az első rangot, majd a könyves fejlesztés után új belépésben a második rangot és a várakozási időt. Három tesztkönyvből egy maradt. Az SQL-utóellenőrzés 203 korábbi példányt és 12 pénztárcát egyeztetett. A sikertelen dobást az izolált MariaDB-próba fedte le.

## C26h — A megtanult képességek passzív bónuszai

A párosított Unity changeset a `cs:79`.

A karakter a ténylegesen követő fejlődő pet megtanult képességeinek aktuális rangértékeit kapja meg. Az egész forráscsomag cserélődik, ezért az ismételt idézés és statújraszámítás nem halmozza a bónuszt. Visszahívás, lejárat vagy a követést tiltó világállapot megszünteti a forrást. Újrabelépéskor a mentett képességek és idézési szándék alapján épül vissza, a követő létrejötte után. Az életkorból és születési tulajdonságokból eredő bónuszokat a C26i kapcsolja ehhez a forráshoz.

A 18 képességhatás a maximális HP-t, a fizikai támadási bónuszt, a megfelelő PvE-, főellenfél- és metinsebzést, a szörny-/játékos-/kasztellenállásokat, a PvE kritikus esélyt, a blokkolást, a nyílkikerülést, a harci EXP-t és az aranydobási esélyt módosítja. A százalékos értékek bázispontosak; az egész HP-bónusz külön egységet használ. Az újraszámítás nem gyógyítja fel a karaktert.

A GC231 v3 a megidézett pecsét azonosítóját és az aktív képességhatásokat adja át. A pet nélküli v1/v2 csomagok megmaradnak. A Unity **Harci statok** ablaka a karakter tényleges statjai mellett külön felsorolja a követő pet képességbónuszait. Új tartós adat vagy SQL-migráció nem szükséges: a már mentett képességállapotból származtatott értékek kerülnek a karakterre.

A mentett karakterbuffok és a típusos leltár betöltése tetszőleges sorrendben fejeződhet be. A buffbetöltés végén a szerver új GC231-et küld, így a visszahívott pet melletti statablak sem marad a korábbi, részleges belépési értékeken.

A Windows GameServer build és hat natív teszt sikeres: `pet_lifecycle`, `equipment_stats`, `point_calculation`, `combat_state`, `affect_lifecycle`, `item_persistence_mariadb`. A natív próbák mind a 18 képességhatás rangértékeinek forrásképzését, a HP ismételt újraszámítását, a sebzés- és ellenállásfeltételeket ellenőrizték. A Unity 6.3 fordítás, 390 petcsomag-, 1118 statcsomag-, 340 pecsét- és 4179 leltárellenőrzés sikeres. A statcsomag-próba a TCP-feldarabolást, hibás petforrást, ismétlődő hatást és a régi csomagra váltáskor törölt petbónuszokat is lefedte.

A végső, 289 ellenőrzéses helyi Unity TLS-próba megtanulta, majd második rangra fejlesztette a tesztképességet. A maximális HP visszahívva 1424, idézve 1674 volt. Három ismételt idézés változatlanul 1674-et adott. Az aktív, majd a visszahívott újrabelépés az összes eredeti karakterstatot helyreállította a megfelelő petbónusszal. Az SQL-utóellenőrzés a 2. rangot, egy megmaradt könyvet és a várakozási időt olvasta vissza; mind a 215 korábbi példány és 16 pénztárca egyezett a mentéssel. Az első helyi élő próba a HP-hatást fedte le.

### Teljes helyi harci regresszió

A párosított Unity changeset a `cs:80`.

A külön Unity teszt mind a 18 képességhez külön petet keltet és könyvet használ. Négy további karakterrel vizsgálja a kasztellenállásokat, külön próbaszörnyekkel a normál, főellenfél-, metin- és beérkező sebzést, íjász AI-val a nyílkikerülést, valódi ölésekkel az EXP-t és az aranydobást. Minden esetben megméri az alapértéket, az idézett állapotot, az aktív újrabelépést, a visszahívást és a visszahívott újrabelépést. Az aktív hatást a Unity statablakában is ellenőrzi.

Mind a 18 eset és 1412 élő ellenőrzés sikeres. A hat natív regresszió, a Unity 6.3 fordítás és a statcsomag 1118 ellenőrzése is átment. Az SQL-utóellenőrzés mind a 18 képességet visszaolvasta a saját pecsétjéből; a 243 korábbi példány, 22 pénztárca és a korábbi tanulási adatok megmaradtak. Az ideiglenes próbaszörnyek és a regenfájl helyreállítása sikeres. A mérési táblázat és a futtatók a [jelentésben](../reports/item_combat/2026-11-07_pet_passive_live_combat.md) találhatók.

## C26i — Életkori és születési bónuszok

A párosított Unity changeset a `cs:81`, a `/main/unity63-integration-20261103` ágon.

Az életkori profil egész maximális HP-t, szörnyellenállást és játékosok elleni ellenállást ad. A születési tulajdonságok a maximális HP százalékát, a szörnyek elleni sebzést és a normál PvE-sebzést módosítják. A már definiált egész százalékra lefelé kerekítés megmarad. A szerver külön gyűjti az életkori, születési és tanult forrásokat, és az ellenőrzött összegüket egyszer alkalmazza. Az életkori küszöb követés közben, új idézés nélkül is frissíti a statokat; a gyorsítótár a küszöböt őrzi, nem a folyamatosan változó életkort.

A GC231 v4 a teljes bónusz mellett a három forráscsoportot és az életkori küszöböt is átadja. A Unity ellenőrzi a csoportok összegét, és a **Harci statok** külön jeleníti meg a forrásokat. A pet nélküli v1/v2 és a csak tanult bónuszos v3 út megmarad. Új SQL-migráció nem szükséges.

A Windows Release GameServer build, hat natív regresszió és a Unity 6.3 fordítás sikeres. A Unity 1640 statcsomag-, 390 petcsomag-, 340 pecsét- és 4179 leltárellenőrzése átment. A 365 ellenőrzéses helyi Unity TLS-próba mind a hat természetes hatást, a követés közbeni életkori fokozatváltást és a tanult HP-bónusszal együtt alkalmazást vizsgálta. Az idézés, aktív újrabelépés, visszahívás és visszahívott újrabelépés HP-, PvE-, PvP- és szörnyellenállási mérései sikeresek. Az SQL-utóellenőrzés a megőrzött életkort, születési értékeket és tanult rangot visszaolvasta; a 312 korábbi példány, 32 pénztárca és korábbi tanulási adatok megmaradtak. Az ideiglenes világ helyreállt, a szokásos öt szerverfolyamat fut. A pontos mérések és a futtatók a [jelentésben](../reports/item_combat/2026-11-07_pet_age_birth_bonuses.md) találhatók.

## C26j — Képességek elfelejtése

A párosított Unity changeset a `cs:82`, a `/main/unity63-integration-20261103` ágon.

A **Pet adatai → Képesség elfelejtése** a katalógusban `ResetOne` vagy `ResetAll` típusú ellátmányt használ. Egy kiválasztott tanult képesség vagy az összes tanult képesség törölhető. A feloldott helyek nyitva, a zároltak zárolva maradnak. Az üres hely ismét tanítható. A pet neve, születési adatai, szintje és evolúciója megmarad; az életidőből a követés alatt eltelt rész elszámolódik. Az életkori és születési bónuszok nem törlődnek.

A CG250 új `ResetList`, `ResetPreview`, `ResetConfirm` műveletei a meglévő rögzített keretet használják. A GC253 v5 különbözteti meg az elfelejtést a v4-es tanulástól; a közös adatblokk a törlés hatókörét és a négy képességhely szerveres állapotát adja át. Az előnézet kiválasztott törlésnél megnevezi a képességet, teljes törlésnél az összes tanult képességet jelöli. A megerősítés egy ellátmányt fogyaszt. Üres képességhelyre vagy visszahívott petre nincs fogyasztás. A megerősítés a pet tényleges követését, az ellátmány teljes példányállapotát, a képességhelyeket, a katalógust és a kétperces ajánlatot újra ellenőrzi.

A szerver egy receiptelt SQL-tranzakcióban menti a törlést és az ellátmány levonását. A `PSR` műveleti bizonyíték csak az adott katalógusszabályból megengedett változást fogadja el. Ismételt megerősítés vagy receipt-visszajátszás nem von le újabb tárgyat. A karakterhez és képességhez kötött tanulási várakozási idő változatlanul megmarad. Új SQL-migráció nem szükséges.

A Windows Release GameServer build és hat natív regresszió sikeres, beleértve a MariaDB-visszaolvasást és a második rangú képesség törlését. A Unity 6.3 fordítás, 1189 petcsomag-, 1640 statcsomag-, 340 pecsét- és 4179 leltárellenőrzés átment. A 268 ellenőrzéses helyi TLS-próba a kiválasztott és teljes törlést, az újratanulást, a hibás tokent, az üres célpontot és az ismételt megerősítést is vizsgálta. Mindkét törlés utáni újrabelépés megőrizte a képességeket és üres helyeket; a tanult bónuszok eltűntek, a természetes bónuszok megmaradtak. Az SQL-utóellenőrzés a végső állapotot és a 359 korábbi példány, 38 pénztárca változatlanságát igazolta. A mérési táblázat és a pontos próbahatókör a [jelentésben](../reports/item_combat/2026-11-07_pet_skill_reset.md) szerepel.

## C26k–l — Életidő és születési tulajdonságok fejlesztése

A párosított Unity changeset a `cs:83`.

A **Pet adatai → Pet tárgyai** felület a visszahívott peten külön kezeli az időhosszabbító, korlátlanító és tulajdonságfejlesztő ellátmányokat. A növelés a megmaradt időhöz adja a katalógusban megadott mennyiséget; ez lesz az új kapacitás is. A korlátlanítás explicit tartós engedélyt és állandó órát ment. Korlátlan peten egyik életidő-tárgy sem fogyhat el újra.

Az enchant a három születési tulajdonság egyikét fejleszti, a profil esélyével, lépésével és felső határával. Elfogadott sikertelen próbánál egy tárgy fogy; értékhatáron, hibás tokennél és elavult állapotban nincs levonás. A növelés és az egy tárgy levonása egy `PSU` bizonyítékkal ellenőrzött SQL-tranzakció. A visszahívás utolsó aktív idejének elszámolása megelőzi a kapacitás módosítását. A CG250 három új művelete és a GC253 v6 viszi a forráslistát, előnézetet és eredményt. Új SQL-migráció nem szükséges.

A Windows GameServer build, `pet_lifecycle` és `item_persistence_mariadb`, a Unity 6.3 fordítás és 2811 petcsomag-, 1640 statcsomag-, 340 pecsét-, 4179 leltárellenőrzés sikeres. A 371 ellenőrzéses helyi TLS-próba az időhosszabbítást, korlátlanítást, három sikeres és egy sikertelen enchantot, az ismételt megerősítést, értékhatárt és újrabelépést ellenőrizte. Az SQL-visszaolvasás 387 korábbi példány és 43 pénztárca megőrzését is igazolta. A részletes mérés a [jelentésben](../reports/item_combat/2026-11-07_pet_life_enchantment.md) található.
