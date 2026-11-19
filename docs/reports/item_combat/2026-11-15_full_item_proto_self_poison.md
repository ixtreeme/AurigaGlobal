# Méregpalack: eredeti saját mérgezés

A 27996 protó új kulcsa C10:1586. Megőrzi az eredeti működést: egy palack elfogy, a karakter saját mérgezést kap a szerver meglévő ApplyPoison szabályai szerint. A fogyasztás akkor is megtörténik, ha a már fennálló mérgezés vagy a szerver szabálya miatt új mérgezés nem indul.

A natív tulajdonos/élet/map/hely/használati ellenőrzések megelőzik az SQL-fogyasztást. A hatás kizárólag sikeres, nem ismételt írás után, ugyanazon bejelentkezett és élő karakteren, ugyanazon a térképen indul. Sem új jutalom, sem kliens által megadható hatás nem került be.

Összesen 9778 konvertált, 491 kivezetett, 717 blokkolt forrássor; 6860 natív definíció, 44-es séma. Jelölt SHA-256: `675408939e17b56eea0998973429b70a5054df2704dd1b1af4ac7bbdb85ef669`. Katalógushash: `37807fd37ab4dd1253af9b8cab2235c24c2bcce7e892dcb8b5b1f75d7d6b0dd2`.

Futtatva: GameServer/Database fordítás; 64/64 CTest; a tényleges SQL item worker 43 ellenőrzése, egyszeri debit/ismétlés/utolsó használat/új SQL-munkamenet; Unity 6000.3.27f1 fordítás és 290 ellenőrzés öt valódi csomagpárral, három tényleges inventory használati gombbal. A jelölt bájtra azonosan újragenerálható.

Az aktív helyi szerverre nincs telepítve. Teljes ECS mérgezési tick, TLS/login és Addressables player build még nincs ellenőrizve; ezeket a szerkesztői és SQL próbák nem helyettesítik.
