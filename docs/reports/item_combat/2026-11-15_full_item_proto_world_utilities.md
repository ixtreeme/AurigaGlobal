# Eredeti világműveletek – ellenőrzött konverziós részállapot

Tíz további forrásprotó konvertálva: 27124 kötés; 70008 fehér zászló; 70057 bátorságköpeny; 50103–50106 négy fix tűzijáték; 50107 kínai tűzijáték; 50108 pörgettyű; 71013 véletlen tűzijáték. Ezek tényleges fordított natív utility-kezelők, nem passzív helyettesítő tárgyak. Az inventory a szokásos CG226 használati kérést küldi konkrét példányazonosítóval.

A szerver ellenőrzi a tulajdonost, inventoryhelyet, aktuális darabszámot, karakterállapotot, szintet, osztályt/nemet, kontextust és a forrás tiltásait. A köpeny map 1-en tiltott; az eredeti 70057-es ág mindkét aggregálási hívását megőrzi. A kínai tűzijáték és pörgettyű az eredeti aréna/PvP-korlátot használja. Pontosan egy tárgy fogy atomikus SQL-műveletben; a világművelet sikeres, nem újrajátszott írás után fut. A 71013 nem halmozható, a próba ezt is megőrzi.

A Unityben a GC114 9/10 eddig eldobott eredeti effektjei is bekötve. A helyes fájlok a régi kliens regisztrációja szerint `newyear_firecracker.mse` és `paing_i.mse`. A speciális effektek a használat helyén, a karakter akkori világtranszformációjával jönnek létre; nem követik a mozgását. A fly-tűzijátékok hat normál és egy karácsonyi indexe már regisztrált; ezt a validátor ellenőrzi.

A 72305 láthatatlanság-kezelése és 300 másodperces forráshű időtartama elkészült és célzott natív tesztben ellenőrzött, de a forrás Unity-katalógusbejegyzésben nincs ikonútvonal és az ikonreferencia nulla. A felhasználó kérésére ezt a protót a merge utánra halasztottam; más tárgy ikonját nem rendeltem hozzá.

Ellenőrzések: GameServer/Database és natív célok fordítása sikeres, 64/64 regressziós teszt sikeres, a kibővített quest/utility egységteszt 183 ellenőrzése sikeres. A valódi aszinkron SQL-munkafolyamat 347 ellenőrzéssel végigvitte mind a tíz forrást: kiadás, egy darab használata, receipt-újrapróbálás, új SQL-munkamenet, utolsó használat és üres visszatöltés. Unity 6000.3.27f1 alatt 50 tényleges csomagpár, 28 valódi használati gombnyomás/CG226-kódolás és két eredeti világban maradó részecskeeffekt; összesen 521 célzott ellenőrzés sikeres. A konverzió bájtpontosan reprodukálható.

Összesítés: 9773 konvertált, 491 korábban kivezetett, 722 blokkolt forrássor; 6855 natív definíció. Ebből 560 a merge utánra halasztott, 162 további forrássor feldolgozása még hátravan. Ez részállapot, nem a teljes konverzió befejezése.

A jelölt nem került az aktív helyi játékszerverre. Mind a 94 ellenőrzött játékosállapot-tábla üres, a helyi aktív katalógus 1574 definíciós. Az SQL-munkafolyamat és az EditMode-klienshasználat ellenőrzött; a teljes belépéses ECS-világban a szörnyek tényleges behúzását, a rossz hatások eltávolítását és az arénabeli játékmenetet még nem próbáltam ki. Addressables játékosbuild sem futott ebben a részben.
