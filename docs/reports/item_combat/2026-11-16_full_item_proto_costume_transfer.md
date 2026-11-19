# Eredeti öltözékbónusz-átvitel

70065 és a függő 50066 láda: három külön saját tárgy, azonos öltözékcsalád, eredeti NPC-közelség, pontos szerveres előnézet és két percig érvényes egyszeri engedély. A normál és extra ritka bónuszok másolódnak, az eredeti 5/6 ritka hely törlődik; a fogadó zárolt helye és élettartama megmarad. A stóla első öt értéke másolódik, a hatodik kiürül; a későbbi normál újrakeverés ismét hatot ad. Az eredeti 96 kizárt öltözék kimarad, 852 valódi natív cél engedélyezett.

A donor és egy tekercs ugyanabban az ItemStore-tranzakcióban fogy. Függő művelet, pontos előállapot, munkamenet, NPC és határidő véd; elutasítás és elavult előnézet után friss szerveres engedély kérhető. Az NPC eredeti menüje és a leltár Bónuszok művelete ugyanazt a Unity kezelőt nyitja. GC214 művelet4, GC220v3 és GC225v28 együtt készült el. A több tárgyat tartalmazó klienspróba egy régebbi v27 állapotolvasási sorrendhibát is feltárt és javított.

60 natív doménellenőrzés, 97 konverterteszt, 449 tényleges SQL/ItemStore ellenőrzés 41 csomaggal, Unity6.3 fordítás és 14015 ellenőrzés 16 tényleges gombművelettel sikeres. A 69 regressziós teszt a Qt szerkesztőt és saját ideiglenes MariaDB-ket is tartalmazza. GameServer és Database fordult; a teljes konverzió megismételve bájtpontosan azonos. A korábbi 15 független hash-orákulum csak az új séma/domain és az üres új tábla miatt változott; négy hálózati referencia csak a katalógushash mezőben változott.

9848 konvertált, 496 kivezetett forrássor, 6930 natív definíció. 637 hiányos tétel a merge-re vár; öt működés feldolgozása folytatódik. Az aktív játékadatbázisra nincs telepítve. Teljes TLS/ECS NPC-beszélgetés, élő játékbeli átvitel és Addressables játékosbuild nem volt próbálva.
