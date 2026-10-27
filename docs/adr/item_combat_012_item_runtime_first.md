# ADR IC-012 — A tárgyrendszer élesítése a harcmag előtt

2026-10-27. Forrás: a felhasználó döntése a jelen munkamenetben.
Állapot: **elfogadott; a megvalósítás folyamatban**.

## Döntés

A felhasználó a következő célállapotot választotta: a GameServer és a Database
az új `protos` katalógusból fusson. A tárgypéldány ItemKey-t és saját
fejlesztési szintet hordoz. A mentés, a fejlesztés, az inventory, a trade,
a bolt, a drop, a quest és a Mission jutalom erre áll át, és a Unity kliens is
ezt kezeli. A meglévő tartalom és a játékosok tárgyai **egyszeri offline
konverzióval** kerülnek át (IC-006 szerint: a +0…+9 családból egy kulcs lesz,
a példányszint megmarad).

A régi harcképletek egyelőre maradnak, de a tárgyakból származó statokat az új
katalógusból kapják. Az új determinisztikus harcmag (terv P3) egy későbbi,
külön kiadás.

## Viszony a korábbi döntésekhez

- **IC-001** „koherens release” szabálya a harcmagra nézve módosul: a
  tárgyrendszer a harcmag nélkül is kiadható. Minden más IC-001 tilalom
  változatlan: nincs runtime vnum-adapter, kompatibilitási proto, dual write
  vagy régi tárgymodellre visszakapcsoló flag. A régi `item_proto` formátumot
  csak az offline konverter ismeri.
- **IC-006** sorrendje érvényes. Ez az ADR a „4. átállítás” lépést előbbre hozza
  a harcmagnál, és rögzíti, hogy a tárgystatok a régi képletek bemenetei lesznek.
- A DragonSoul, a régi BattlePass és az offline shop kivezetése (IC-010 és a
  korábbi döntések) érvényben marad.

## A konverzió alapelve

A konverzió a **mai futó viselkedést** őrzi meg. A régi tartalomból az kerül át
működő tárgyként, amit a mai szerver ténylegesen kezel. Lehet, hogy egy
`item_proto` sort a mai build nem tud használni, például mert a kódja ki van
kapcsolva (`ENABLE_PENDANT`, ismeretlen altípus), vagy mert kivezetett
rendszerhez tartozik. Az ilyen sor karanténlistára kerül, indoklással és
darabszámmal. Csendes adatvesztés nincs: a karanténban lévő játékospéldányok
külön jelentésben szerepelnek, és felhasználói döntés nélkül nem törlődnek.

## Fázisok és elfogadási kapuk

| Fázis | Tartalom | Kapu |
|---|---|---|
| F1 | Régi tartalom leltára, régi típus → új kategória leképezés, karantén | Minden `item_proto` típus/altípus besorolva vagy indokoltan karanténban |
| F2 | Hiányzó kategóriák: domain, SQL, betöltő, szerkesztő | A leképezés minden célkategóriája tölthető és szerkeszthető |
| F3 | Új példánymodell: `player` tábla, Database cache, mentés/betöltés | ItemKey + szint + tulajdon/hely visszatöltése azonos |
| F4 | GameServer: katalógus induláskor, tárgygyártás, statok, használat, fejlesztés, drop, bolt, trade, quest, Mission | A régi item_proto/vnum út nincs a runtime-ban; a meglévő tesztek zöldek |
| F5 | Protokoll és Unity kliens | Kliens ItemKey-t és szintet jelenít meg; régi kliens elutasítva |
| F6 | Offline konverter: protók, példányok, boltok, receptek, dropok, questek, itemshop | 100% besorolás; darab/tulajdon/szint egyezés; ismételt futás azonos |
| F7 | Helyi átállás a Server2-Local csomagon, regresszió és smoke | Bejelentkezés, inventory, fejlesztés, bolt, trade, drop, quest működik |

Minden fázis saját jelentést kap a `docs/reports/item_combat/` alatt. Távoli
gépen és éles adatbázison nem történik művelet; az éles átállás külön döntés.
