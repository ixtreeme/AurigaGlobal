# Két eredeti újrafelhasználható világművelet

A 71092 átváltozást megszüntető tárgy a kijelölt célponton, kijelölés nélkül saját magán működik, és nem fogy el. Natív átváltozáskor a tartós RCS-állapotot a meglévő szerveróra törli és a karakter eredeti alakját vetíti ki; a következő frissítés és újrabelépés nem állítja vissza az előző alakot. A régi NPC/világ-affectek meglévő eltávolítási útvonala is megmarad. A régi affectrendszer nem enged külön régi átváltozást a natív játékosállapot mellé.

A 70302 házassági gyűrű a meglévő tényleges házasságot és partnerazonosítót használja. Az eredeti idézési térképkorlát, a közös teleport-várakozási és ablakfeltételek, valamint mindkét online fél arénakorlátja ellenőrzött. Az SP-költség floor(maximum SP ×30/100), túlcsordulás nélkül. A SP-t a szerver vonja le és a meglévő WarpToPID útvonal kezeli az utazást; a tárgy megmarad. A működést a natív utility.csoport azonosítja, futásidős régi tárgyazonosító nélkül. A forrást az autentikált tulajdonosi fence-hez ellenőrzi a használat.

Mindkét tárgy a tényleges leltár Használ gombját és változatlan CG226 példányazonosítós kérését használja. Nincs helyi fogyasztás vagy kliensoldali hatásengedélyezés. A katalógusformátum49 maradt.

Ellenőrzés:278 natív quest/utility ellenőrzés;85 konverterteszt;126 tényleges ItemStore/SQL-ellenőrzés és10 csomagállapot. Saját és másik tulajdonos natív átváltozása, törlése, újrabelépése, régi nyugta ismétlése és a változatlan forrástárgy ellenőrzött. A Unity 6.3 fordítása és615 ellenőrzése100 tényleges ismételt használatot próbált. GameServer és Database fordítás, valamint a teljes66 tesztes regresszió sikeres. Az SQL-jelölt, végleges és újrafuttatott konverzió bájtpontosan azonos.

Állapot:9823 konvertált,491 kivezetett,630 merge-re váró hiányos eredeti sor és42 további bekötendő sor;6905 natív definíció. Négy már natív idéző külön hiányzó világmodellre vár. Az aktív helyi szerverre nem történt telepítés;94 játékosadat-tábla üres és1574 katalógusdefiníció maradt. Teljes TLS-belépéses ECS-játékpróba, tényleges szerverek közötti házastársi utazás és Addressables player build nem futott. A konverzió folytatódik.
