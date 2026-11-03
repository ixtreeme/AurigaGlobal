# C21 — Időzített és kapcsolható tárgyhatások

2026-11-03. Szerződés: [IC-049](../../adr/item_combat_049_item_effects_runtime.md).

## Eredmény

Az időzített italok és kapcsolható harmatok használhatók a futó szerver és
a külön típusos Unity-leltár közös útján. A normál legacy leltár átállítása
a felhasználó kérésére vár; a régi kliensablak megjelenése önmagában nem
jelenti az új rendszer használatát.

Időzített csomagoknál online/falióra, Reject/Replace, teljes csomagcsere,
lejárat, halálszabály és megerősített törlés készült. A harmatok pontos
megmaradó aktív ideje, ki-/bekapcsolása, variánscseréje, szerepfeltétele,
zárolása és kifogyása mentett állapot. A bónuszok a tényleges statprojekcióba
kerülnek, nem csak az ablakban jelennek meg.

## Lefutott ellenőrzések

| Ellenőrzés | Eredmény |
|---|---|
| Release GameServer és érintett natív célok | sikeres |
| CTest célzott regresszió | **16/16 csoport** |
| TimedEffectTests / ToggleEffectTests | **65 / 1065 ellenőrzés** |
| Új EffectRuntimeTests | **163 ellenőrzés** |
| Külön saját MariaDB, ItemPersistenceSqlTests | **1504 ellenőrzés** |
| Külön saját MariaDB, ItemBenchToolTests | sikeres |
| Érintett Python fájlok szintaxisa, git diff check | sikeres |
| Unity 6000.3.27f1 scriptfordítás | sikeres |
| GC243 native golden, minden csonkítás/TCP split, UI, dispatcher | **5445 ellenőrzés** |
| Korábbi C4–C20 Unity-validátorok | mind sikeres |
| Valódi Unity → TLS → GameServer → SQL és UI | **50 ellenőrzés, sikeres** |
| Canonical source snapshot | **5456 fájl, 1726 byteazonos C# fájl** |
| Korábbi helyi próbakészlet | **mind az 55 tárgyállapot változatlan** |

A natív tesztek ellenőrizték az online és UTC órák eltérését, halálszabályt,
teljes csomagcserét és statösszegzést, RCS2 roundtripet, hibás/trailing
adatokat, pontos gyors kapcsolást, inaktív idő megőrzését, aktív mozgatás és
fogyasztás tiltását, szerepfeltétel elvesztését, kifogyási tombstone-t,
GC225 v3 roundtripet és generációval védett egyedi törlést.

SQL: forrás és RCS2 egy tranzakciója, source/timer CAS rollback, receipt
replay, falióra-határidő, pontos ezredmásodpercek, online maradék, relog és
sérült példányhash elutasítása. A korábbi hash- és inventory-goldenek
változatlanok maradtak.

Az élő próba valódi UI-gombbal indított és erősített meg műveleteket.
Egy darab levonása +5 tényleges Erőt adott; Reject nem fogyasztott újabb
darabot. Egy másik csoport teljes lecserélése +14 összesített Erőt adott,
a törlés az alapértéket állította vissza. A gyors harmat-kikapcsolás pontos
időt fizetett, a kikapcsolt idő nem fogyott, azonos variáns elutasított,
eltérő variáns atomi csere volt. A tényleges házassági/csoportvezetői
hiány tiltott; a rövid harmat kifogyott és eltűnt, bónusza megszűnt.
13 másodperc offline után a faliórás hatás eltűnt, az online és aktív harmat
maradéka megmaradt, a bónuszok helyesen visszaépültek. Régi login-control
elutasított. Végül minden próbából származó hatás megszűnt.

Az első élő kör későn érkező passzív állapotcsomagot talált az új tárgy
előnézete közben. A kliens most nem cseréli le erre az új előnézetet;
külön regressziós ellenőrzés és sikeres teljes újrafuttatás védi.

## Helyi állapot és újrafuttatás

Aktív Unity: `D:/Auriga-Unity6/SRC/AurigaUnity`,
`/main/codex-mission-engine`, **cs58**, pinned Editor `6000.3.27f1`.
A checkin csak a húsz C21-fájlt tartalmazza. A külön batch
source snapshot fordította a teljes aktuális C# készletet. A felhasználó
Editorát nem zártuk be; a jelenet-, kapcsolat-, Packages- és ProjectSettings
módosításai megmaradtak. A hiányzó világ/VFX/Ocean assetek ismert snapshot
jelzései nem akadályozták a vizsgált itemutakat. Teljes jelenet/player-build
ellenőrzésként ez nem értelmezhető.

A saját `D:/AurigaGlobal/Server2-Local` öt szerverfolyamata és saját MariaDB-je
fut. PID 1610 dedikált codexitem tesztkarakter: **69 tárgyállapot**, ebből
az 55 korábbi változatlan; 10 új katalógustípus a próbákhoz, pótló italhalmokkal.
Az összes harmat kikapcsolva, részben elhasznált maradékuk megőrizve; a kis
kifogyási próba új példányt kapott. Az italokból típusonként összesen öt darab
rendelkezésre áll. HP/SP alap 1000/20. A GameServer, helper és 006-os migráció
hash-e egyezik a csomag manifestjével és az elkészült fájlokkal.

Újrafuttatás: saját stack stop és leállás megvárása után
`SeedEffectsUnityLive.py`, az automata vagy effects privát fixture-rel;
ez csak az azonosított tesztfiókhoz ad hiányzó próbatárgyat, korábbiakat megőriz.
Stack start, majd `PrepareUnityValidationSnapshot.py` és
`EffectsUnityLiveSmoke.py`. Bridge módok: `effects-offline`, `effects-live`.
Privát fixture és logok: nem követett `build` könyvtár. Telepítési mentések:
`Server2-Local/run/deployments/effects-*`. Közvetlen HP/SP tesztadat-SQL után
a saját stack újraindítása kell a DatabaseServer cache miatt.

## Nyitott kapuk

Teljes Unity player build, FreeBSD, többcore-os handoff, terhelés és
crash-injection nem futott. Az IC-047/048 HP/SP-mentési crash-rés változatlan.
A pozitív házassági/csoportvezetői esetet és szerepvesztést natív teszt
ellenőrizte; élő karakterrel a tiltás futott. A normál leltár- és teljes
adatkonverzió külön munka. Következő szelet: **C22 — képességkönyvek**.
