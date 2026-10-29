# ADR IC-031 — Példánybónuszok, hozzáadás, váltás és zárolás

2026-10-29. Kiinduló HEAD `0c04033b`, F2 és F3 adatmodell-előkészítés.
Csak helyi fejlesztés, friss tesztséma; az aktív v7 nem frissíthető e lánccal.

## Ellenőrzött forrás

`ItemAttributeSystem.cpp::RollNormal/PrepareReroll`: öt normál hely,
súlyozott típus, külön szintdobás és típusonkénti szintmaximum. Az alap- és
extra proto apply-k általában kizárják ugyanazt a normál bónuszt, de vannak
külön vnum/altípus-kivételek. `RollRare/RollRare8`: két ritka és egy extra
hely, egymás között közös duplikációtilalom, egyenletes típusválasztás és
legmagasabb engedélyezett érték. A normál és ritka csoport ismétlődhet.

`UseItemAttributeLock`: tele normál csoport kell az első védelemhez;
áthelyezéskor csak másik foglalt normál hely választható. `PrepareReroll`
a védett sort a helyén hagyja, és a normál sorok darabszámát őrzi meg.
`ItemAttributeRules.hpp::RollLevel` a hiányos valószínűség maradékát nem
normalizálja: az ilyen dobás a jelenlegi szerződés szerint sikertelen
előkészítés, fogyasztás nélkül.

`ApplyAddon` összekapcsolt normál/skill bónuszt állít elő Gauss- és egész
dobásokból. Ezt nem alakítjuk át független választásokra. Stóla, rúna,
jelmezreset és a példányadatos jelmezbónusz-adók további külön viselkedések.
Az ilyen tartalom konverziója nyitott, nem kerül automatikusan publikálásra.

## Döntés

Külön `Instance` bónuszforrás kerül a regiszterbe (4, maszk 8). A kiadott
BonusId-k és egységeik nem változnak. Az explicit tartalomlisták csak az új
regiszterben engedélyezett bónuszokat használhatják; nincs runtime APPLY-kód.
A statikus `BonusProfile` továbbra sem tárolhat példányforrású profilt.

A példányállapot nyolc opcionális, típusos `BonusEntry` és egy opcionális
normál zárolt index. Nem prototípus és nem második stat-cache. Normál, ritka
és extra csoport kapacitását a tárgyhoz kötött profil mondja meg (legfeljebb
5/2/1). Lehetnek lyukak, a zárolt index foglalt normál hely kell legyen.
Normálon belül és a ritka+extra tartományban külön-külön tilos az ismétlés.
A tárolt érték a regiszter szerinti int64; az új kiadás sorsolási értékeinek
változása nem írja át a már megszerzett értéket. Ismeretlen BonusId, tiltott
forrás, nulla érték vagy tartományhiba elutasított.

A tárolt bónusz típusának szerepelnie kell a megfelelő csoport jelöltlistáján,
de értéke lehet korábbi kiadásból származó, jelenleg nem sorsolható érték is.
F3/F4 publikálási kapu: jelölt, csoportkapacitás, binding vagy védelem
eltávolítása előtt az összes érintett tartós példányt ellenőrizni kell. Olyan
változás nem publikálható, amely meglévő állapotot olvashatatlanná tenne;
nincs csendes értéktörlés vagy automatikus újrasorsolás. Ez a teljes
példány-writerrel együtt megvalósítandó release preflight, nem élő ellenőrzés
a jelenlegi, még nem aktivált szerkesztőben.

Típusos katalógus: profil, csoportkapacitás/statikus kizárás, bónuszjelölt
súly, szintenkénti érték és tárgy–profil kapcsolat. A csoportok választási
súlya tartalmi döntés; ritkánál az egyenletes választás explicit 1-es súly.
A fogyasztható külön szolgáltatásprofilja: hozzáadás, teljes csoportváltás,
védelem, védelem áthelyezése, védelem törlése. Célkategória-maszk,
cél-szintkorlát, darab/kísérlet, előzetes darabszám és hozzáadott darab
szabályok adatok. A profiloknak nincs hallgatólagos esélye vagy value-tömbje.

A sorsolás csak rögzített szerveres döntéseket dolgoz fel. Minden új sorhoz
szintdobás és az addig kizárásokkal csökkentett halmaz súlyösszegéhez tartozó
típusdobás kell. A szint a kiválasztott bónusz legmagasabb engedélyezett
szintjén clampelhető, érték maga soha. A kihagyott szinttartomány elutasítás,
nem újradobás. Fölösleges vagy hiányzó döntés nem elfogadható. Védelemnél
egyetlen, korlátos jelöltindex választás van; nincs újradobó ciklus.

A teljes bemenet, katalóguspin, session, operation ID és gazdasági revisionök
a birtokolt terv részei. Nincs RNG, ECS-, SQL-, stat- vagy inventory-írás.
Sikertelen, fogyasztó hozzáadási próba külön sikeresen előkészített eredmény;
hibás/hatástalan terv nem fogyaszt. F3/F4 egy receipt, anyagfogyasztás és
teljes bónuszállapot-csere tranzakcióját végzi majd, statpublikálással utána.
CommitUnknown esetén tartós operation ID szerinti egyeztetés szükséges.

A codec explicit mezőnkénti little-endian v1, szigorú verzió/kulcs/EOF és
teljes állapotvalidálás. Ez nem bekötött DB-writer vagy klienspacket.

V24 friss katalógus és Qt-űrlap készül, nem balance-jóváhagyás vagy teljes
APPLY-migráció. A megmaradó, még nem regisztrált bónuszok és különleges
műveletek F2/F6 kapuk maradnak. Unity és távoli gép nem módosul.
