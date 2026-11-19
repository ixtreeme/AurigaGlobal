# Eredeti összetett fogyóeszközök, C49

A 50183 és 50216 eredeti szezonális tárgy egyenként 30 percre mozgás-, közvetlen prémium támadás- és tapasztalatbónuszt ad. A külön csoportok együtt használhatók, saját csoportjuk ismétlése fogyasztás nélkül elutasított. A prémium támadás külön natív 144-es bónusz: a tényleges sebzési útvonal az eredeti 20%-os összesített és 300 sebzéses korlátot alkalmazza. Az APPLY64 korábbi felszerelésbónusza változatlan maradt.

A 61400 valódi prototípusa USE_FISH: 50 másodpercre ember elleni sebzést, ember elleni ellenállást és 3000 HP-t ad. A tárgy neve és nem olvasott további értékei nem határozzák meg a működését. Az eredeti kód utolsó APPLY pont szerinti, sorrendfüggő ütközését az összes 32 halétel explicit módon hordozza. A 31 meglévő halétel azonosítói, bónuszai és időtartamai megmaradtak. A szerver a régi aktív affectekkel való ütközést is ellenőrzi.

A katalógusformátum 49, migráció 057. GC243 v4 az explicit családot és ütközési pontot is közli, a korábbi verziók változatlanok. A Unity a tényleges leltár, áttekintés és megerősítés gombokat használja, megjeleníti az összes bónuszt és az ütközés okát.

Ellenőrzés: 291 natív effect-runtime és 65 timed-effect ellenőrzés; 49 sémateszt; 2157 tényleges ItemStore/SQL-ellenőrzés 34 forrással és 211 csomagállapottal; a teljes 66 tesztes regresszió sikeres. GameServer és Database fordítás sikeres. A Unity 6.3 fordítása és 14259 ellenőrzése 102 tényleges gombot próbált. Mindkét halételsorrend, ismétlés, újrabelépés, online idő, halál és két szezonális bónusz együttes működése ellenőrzött. A 15 független digest és négy metadata fixture változását külön felülvizsgálat rögzíti. Az SQL-jelölt, a végleges és az újrafuttatott konverzió bájtpontosan azonos.

Állapot: 9820 konvertált, 491 korábban kivezetett, 630 merge-re váró hiányos eredeti sor és 45 további bekötendő sor; 6902 natív definíció. Négy külön idéző hiányzó világmodellre vár. Az aktív helyi játékra nem történt telepítés: 94 játékosadat-tábla üres, a katalógus 1574 definíciós. Teljes TLS-belépéses ECS-játékpróba és Addressables player build nem futott. A konverzió folytatódik.
