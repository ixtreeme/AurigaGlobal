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
