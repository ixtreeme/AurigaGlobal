# C23 — Könyvbónusz és várakozáselhagyó segédtárgyak

2026-11-04. Szerződés: [IC-051](../../adr/item_combat_051_book_aids_runtime.md).

## Elkészült

Kétféle segédtárgy, többtöltetes csomag, tárolási korlát, közös atomi
forrás-/töltetmentés, megerősítés, receipt replay és relog. A szerver és Unity
együtt változott; a tárgyszerkesztőben létrehozható és másolható az új profil.
A külön típusos leltár használja; a normál legacy leltár bekötése továbbra is vár.

## Ténylegesen lefutott ellenőrzések

| Ellenőrzés | Eredmény |
|---|---|
| Release GameServer / Database / LoginServer | sikeres |
| Célzott item-CTest | 20/20 csoport |
| BookAidRuntimeTests | 280 ellenőrzés |
| CatalogSnapshotTests | 126 ellenőrzés |
| Külön saját MariaDB, ItemPersistenceSqlTests | 1599 ellenőrzés |
| Külön saját MariaDB, CatalogSqlReaderTests | 158 ellenőrzés |
| Katalógusséma, benne új CHECK/FK/exkluzivitás | 41 teszt |
| Tárgyszerkesztő modell, Qt-felület és külön SQL | 90 teszt, mind sikeres |
| ItemBenchToolTests, külön saját MariaDB | sikeres |
| Független Python/C++ v26 alapkatalógus-hash | egyezik |
| Érintett Python-szintaxis, git diff check | sikeres |
| Unity 6000.3.27f1 scriptfordítás | sikeres |
| Új GC244 v2 golden, csonkítás/TCP split/dispatcher/UI | 2924 ellenőrzés |
| C22 könyves validátor | 2929 ellenőrzés |
| Korábbi C4–C21 Unity-validátorok | mind sikeres |
| Valódi Unity → TLS → GameServer → SQL/UI | 37 ellenőrzés |
| Kanonikus forrásokból ellenőrzött snapshot | 5472 fájl, 1732 byteazonos C# |
| Helyi korábbi tárgyak / C22 learning | mind a 94 állapot és a checkpoint változatlan a próba visszaállítása után |
| Új helyi szerkesztő build és telepített hash-ek | egyeznek |

SQL-ben ellenőrzött a forrás-CAS és a karakter-revíziós CAS hibájának teljes
rollbackje, az egyszeri jóváírás, a többtöltetes korlát, a könyvbe továbbvitt
bónusz/várakozáselhagyás és az új owner-session. A C22 karaktermentési védelme
változatlanul érvényes a segédtárgy tranzakciójára is.

Az élő próba a tényleges **Könyvsegéd** inventorygombot és a külön megerősítést
használta. Egy bónusz- és egy várakozáselhagyó töltet újrabelépés után is megmaradt.
A 0%-os alapesélyű, 100%-os bónuszesélyű könyv valóban 0→1 szintet adott, és a
felesleges várakozáselhagyó töltetet megtartotta. A következő, bónusz nélküli
olvasás garantáltan sikertelen volt, de a várakozást feloldotta és a jelzett
tárgyat, XP-t és töltetet együtt fogyasztotta. A következő olvasást már tiltotta
a várakozás. Új segédtárgyak után mindkét töltet felhasználásával 1→2 szint,
700→625 XP és három könyvfogyás történt. A kéttöltetes csomag utolsó darabja
eltűnt, az eredmény a felületen megmaradt. Az újrabelépés ezt az állapotot is
visszaadta. Teli/alacsonyabb korlát, minimumszint és régi session-control tiltása
nem fogyasztott újabb tárgyat.

## Helyi csomag és tesztadatok

Csak az igazolt `codexitem` próbakliens karaktere kapott új tárgyakat.
A katalógus teljes SQL-mentése, régi binárisok, manifest, karakter és sidecar
a csomag `run/deployments/book-aids-*` könyvtárában vannak. A katalógus v26-ra
frissült; az öt régi szerverfolyamat leállítása után a párosított Game/DB/Login
és natív segédek kerültek a helyi csomagba. A friss szerkesztő teljes régi
könyvtára külön mentésbe került. A felhasználó kanonikus Unity Editora nyitva maradt.

A próba után a dedikált PC pontos C22 learning-/karakterállapota visszaállt,
a 94 régi tárgy minden mezője egyezik. Az új bónusz/várakozáselhagyó segédből
5–5, csomagból és szinttiltott próbából 1–1, az új könyvből 5 darab elérhető.
A helyi stack öt saját folyamata fut; telepített binárishash-ek egyeznek.
A csomag a helyi próbákhoz készült, nem távoli vagy éles telepítés.

Unity: a mission engine ágon **cs62**, kizárólag a tizenkét saját
fájllal. A jelenet, kapcsolati asset, Packages és felhasználói ProjectSettings
módosításai kimaradtak; a ProjectSettings.asset ellenőrzött hash-e változatlan.

## Nyitott korlátok

A teszt a forrássnapshot item-folyamatát fordította és futtatta. A hiányzó
SpeedTree/VFX/Crest világassetek miatt ez nem teljes jelenet- vagy playerbuild.
A fagyasztott helyi szerkesztő új buildje elkészült és telepítése hash-ellenőrzött;
a Qt-lap tesztje a forrásból futott. FreeBSD, több core közti handoff,
teljes játékmeneti hibainjektálás/stressz és éles migráció nem futott.
A legacy affectek automatikus átalakítása és a normál leltár átállítása nincs
ebben a szeletben. A C19 HP/SP crash-atomikussági és F7 kiadási kapui nyitottak.
