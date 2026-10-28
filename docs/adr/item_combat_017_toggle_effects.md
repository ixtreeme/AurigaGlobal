# ADR IC-017 — Tárgyhoz kötött, kapcsolható harmatok

2026-10-28. IC-014 következő izolált szelete. Nem futó szerverre telepített migráció.

A régi `UseItemEx::USE_NEW_POTIION` affect-típusonként egy tárgyat zárol.
Azonos típussal másik példányt elutasít; a 19/20 és 21/22 párok kizárják
egymást. 24–29 házasságot, 31 csoportvezetést követel. A bónusz az első
apply mezőből, a tartós kapcsolat az item ID-ból származik. `LoadAffect`
visszatöltéskor ellenőrzi a tárgyat. `real_time_expire_event` socket0 időt
csak socket1=aktív esetén csökkent, 60 másodperces lépésekkel. Az item
factory a kezdő időt limit0-ból állítja be. Ez aktív online idő, nem
elfogyasztott, fix végidejű buff és nem automata gyógyító töltet.

A régi stackbontás a sikeres factory/inventory művelet előtt fogyaszt.
A házasság/vezető feltétel a kikapcsolás előtt fut, ezért az elvesztett
feltétel a tisztítást is tilthatja. Házasság megszűnésére és party elhagyására
külön takarítás van (`marriage::CManager::Remove`, `SocialSystem::SetParty`).
A típusegyezéses korai elutasítás miatt egy későbbi csereág elérhetetlen.
Ezeket a hibás sorrendeket az új domain nem veszi át.

Új `ToggleEffectGroup` (stabil ID/kód, halálkor levétel) és típusos
`ToggleEffectProfile`: csoport, variáns, TimedEffect forrású bónusz (ez is időhöz kötött aktív hatás), véges aktív idő,
házasság/vezető feltételmaszk, környezet/PvP, opcionális cooldown. Azonos
csoport+variáns másik példánya elutasított, eltérő variáns ugyanabban a
csoportban atomikusan cserélhető. Nem kizáró hatások külön csoportot kapnak.
A csoport szabályai kiadás után tárgyszerkesztéssel nem módosíthatók.

A harmat nem halmozható. Egy példány egy pozitív ItemInstanceId, saját
remaining_ms és active állapot. Nincs stackbontás, socket- vagy vnum-adapter,
végtelen időt utánzó konstans, aktiváláskori darabfogyasztás. Kezdő töltet
csak megszerzéskor inicializálható; visszatöltés nem tölti újra.

Kikapcsolás halál, retire, cooldown vagy elvesztett használati feltétel
esetén is lehetséges. Az admission owner által feloldott, alapból tiltó
globális/map engedélyt kap. Aktív állapot listája egyedi példányt és
csoportot tartalmaz; sérült vagy hiányos binding nem jelent automatikus
újraaktiválást. A visszatöltés feltételeit ugyanígy ellenőrizni kell.

Az advance a szerver által mért aktív online időt pontos egész ms-ban
fogyasztja; legfeljebb a maradékot vonja le. Az owner minden használat,
csere, logout és szabályváltás előtt elszámolja az eltelt aktív intervallumot.
Így a gyors ki/be kapcsolás sem ad ingyenes időt. A határon előbb az eltelt
intervallum terhelése, utána az új feltétel miatti kikapcsolás történik.
Halálkor a csoportszabály dönt: levétel, vagy az idő tovább fogy aktívan.
Kábulat nem állítja meg a passzív buffot. Offline idő nem fogy; nincs catch-up.
Nulla maradék levétel és példánytörlés javaslat. Az aktiváló terv birtokolja
a bónuszokat; az advance nem adja hozzá őket ismét.

Az owner egyetlen revision/receipt commitban kezeli a zárolást, cserét,
aktív bindingot, időt, bónuszokat, cooldown-t és törlést. Ezek runtime,
ECS/persistence/Unity bekötése későbbi fázis; a tiszta tervező nem állít
tartós idempotenciát. V10 friss draft séma: nem helyben futó v7 frissítője.
Valódi régi tartalom csak teljes APPLY/feltétel megfeleltetés után importálható;
az új profilok tesztadatai nem jóváhagyott balance-adatok.
