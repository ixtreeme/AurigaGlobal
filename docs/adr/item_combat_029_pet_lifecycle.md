# IC-029 — A fejlődő pet teljes példányállapota és tárgyműveletei

2026-10-29. Kezdő HEAD: `e3693f7f`; helyi fejlesztés, IC-012 F2/F3-előkészítés.

A következő egység együtt kezeli a kort, a négy skillhelyet, a passzív
hozzájárulásokat, a tanulást/resetet, az időnövelést/enchantot, a szállítást
és a pet megjelenési tárgyát. Az idézés/visszahívás külön terv, az első
idézés (`evocation`) és a visszaidézési szándék megőrzött adat. A fejlődési
szint és a hívó +N szintje külön adat.
Az új összetett petállapot nem újabb írható másolata a részállapotoknak:
ugyanazon példány aggregátuma, explicit, verziózott mentési codec-kel.

## Forrásaudit

- `New_PetSystem.cpp::UpdateActorTime`: a `minAgeStart` UTC-korkezdet;
  az életkor valós idő, offline is nő. Az első három hely evolúciókor,
  a negyedik 1296000 másodperces korban nyílik. A kor nem az aktív életidő.
- `GiveBuff`: korhatárok 950400/2246400/4147200 mp; HP 500/1200/2100/3000,
  szörny- és emberellenállás 2/4/7/10 százalék. A születési tulajdonságok
  egész `/10` csonkolása megmarad. A 12 jelenlegi skill passzív;
  `ActorDoPetSkill` az aktív buildben azonnal visszatér.
- `ActorIncreasePetSkill`: kiválasztott nyitott helyre biztos tanulás vagy
  fejlesztés, maximum 10; más helyen azonos skill tiltott.
  `ActorIncreasePetSkillByBook`: csak már ismert skill, 71% siker
  (`number(1,100)<30` a kudarc), siker után 3 óra owner+skill várakozás.
  A várakozást átugró affect a dobás előtt fogy; sikertelen könyv is elfogy.
  Ez két külön engedélyezhető tanulási út, nem véletlenül választott API.
- `ActorResetSkill(s)`: tanult skillek ürülnek, zárolt helyek zároltak maradnak.
  Az 55003..55005 közvetlen ITEM_SKILLBOOK ága tiltott; questben történő
  meghívásuk nem igazolható automatikusan. Nincs kitalált tartalomimport.
- `ActorIncreasePetEvolution`: az új szint 5/7/4 oszthatósága szerint
  1..6 tizedes traitnövekmény, clamp 32767. A szokásos 41/61/81 szinteknél
  egyik sem aktiválódik; az egyszerű EXP-szintlépés nem növel traitet.
- `UseItemEx::USE_PET_REVIVE`: inaktív pet maradékához hozzáadott napok,
  az új kapacitás is ez az összeg; korlátos és külön korlátlan tárgy.
  A `USE_PET_ENCHANT` 30% sikerrel +5 tized, HP cap150, másik kettő cap100;
  a forrástárgy kudarc esetén is fogy.
- `UseItemEx` 55002 szállítódoboz: pet-sor ID-ja és szűkített attr/socket
  projekció kerül a dobozba; kivételkor a régi kód kapacitásra töltheti az
  életidőt. Az új megoldás a teljes állapotot és ugyanazt a hívópéldányt
  tartja meg, nem generálja vissza a petet a megjelenítési mezőkből.
- `New_PetSystem.cpp::PetSkin`: felszerelt külön petkostüm ad formát.
  Az új skin csak a megjelenést módosítja, nem a pet prototípusát/statját.

A forrásban ismert 5 tojáscél- és 11 quest/native dispatch-ütközés továbbra
is offline tartalmi döntést igényel. A rögzített mentésben nincs valódi
fejlődő petállapot; fixture-próba nem jelent valós adatkonverziót.

## Tulajdon és végrehajtás

A teljes petállapot részei: név/születési idő/kor, progression, trait, négy
skillhely, életidő, szállítóhely. A hívó tartós instance ID-ja megmarad.
Szállításkor a példány helye a hordozó tárgy lesz; kivételkor a tulajdonos
lefoglal egy inventory-helyet. A doboz és a pet egy tulajdonoshoz tartozik,
trade és fogyasztás csak az egész aggregátum konzisztens állapotában történhet.
Nincs socketbe csomagolás, azonosító-szűkítés vagy életidő-visszatöltés.

A kor monoton megőrzött alsó korlát; UTC-visszalépés nem zár vissza helyet
és nem csökkenti az életkort. A képességnyitás latch. A passzív terv egy
forrásazonosítóhoz tartozó teljes cserélendő hozzájáruláscsomag; visszahívott,
lejárt, szállított, tiltott térképen levő vagy halott gazdához tartozó pet
nem ad aktív bónuszt. Az effektusok névvel és egységgel rögzített új domain-
értékek, nem régi APPLY-számok. A statíró bekötése továbbra is F4-kapu.

A könyv késleltetése owner+skill állapot, nem egy másik pet választásával
megkerülhető petmező. A bypass-affect, könyv és skill/cooldown ugyanabban a
receiptelt erőforrás-commitban változik. A tervezők nem hívnak RNG-t; a
szerver rögzített 0..9999 döntését kapják. Nincs kliens által megadott eredmény.

A korlátlanító tárgy külön, tartós engedélyt rögzít az instance életidején;
ez engedélyezett műveleti óraváltás, nem protofrissítésből eredő újraértelmezés.
Betöltés soha nem futtatja az új példány factoryját, és a codec sem old fel
régi itemazonosítót. A blob nem SQL-prototípus, JSON/EAV vagy klienspacket:
explicit byte-szerződés a teljes petpayload megőrzésére, F3-tesztelési alap.

Az új katalógus és Qt-szerkesztő a regisztrált petellátmány-viselkedéseket
típusos mezőkkel kezeli; ismeretlen különleges viselkedés nem publikálható.
Az ehhez használt 17-es kategóriában kizárólag az 1-es, nevesített
petellátmány-viselkedés támogatott; ez nem általános `valueN` helyettesítő.
A tanulható képesség azonosítója/kódja/hatása egy kiadáson belül egységes
a különböző petprofilok között. F3/F4 kiadási kapu a már kiadott skill-ID-k
jelentésének és minden mentett rangnak a megőrzése is; a jelen szerkesztő
és snapshot nem bizonyítja egy későbbi tartalomkiadás instance-kompatibilitását.
A forrástartalom automatikus élesítése, a GameServer/Database writer,
Unity-protokoll és teljes offline import továbbra is a későbbi F3..F7 feladat.
