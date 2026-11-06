# ADR IC-060 — Típusos pettojás-keltetés

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
