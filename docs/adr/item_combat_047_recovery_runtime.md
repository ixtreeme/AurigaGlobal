# ADR IC-047 — Tartalékos és folyamatos HP/SP-visszatöltés

2026-11-03, C19. Előzmény: IC-010 consumable profilok, IC-034 runtime terv,
IC-045 online időelszámolás és IC-046 bónuszszolgáltatások.

## Viselkedés

A tartalék HP-n és SP-n külön működik. Egy használat legfeljebb a jelenlegi
hiányból a már elfogadott tartalék és összegyűlt törtérték felett megmaradó
részt fogadja el. Tele egyik pool mellett a másik még használható. Mindkét
hatástalan pool esetén nem fogy tárgy. A potion bonus csak a profil szerinti
határig növeli a tartalék mennyiségét.

Poolonként legfeljebb nyolc forrásadag várhat FIFO-sorrendben. Azonos forrás,
időköz és sebesség feltöltése összevonható, megtartja az adag ütemét és új
hatásazonosítót kap. Más profil külön sorba kerül; a tartalékok nem töltenek
párhuzamosan és nem veszik át egymás sebességét vagy hely/PvP-szabályát.
Adagonként a maximális pool profilban megadott százaléka tölthető vissza.
Az új sor az előző kifogyása után a saját teljes időközével indul.

A folyamatos hatás HP-ra és SP-re egy-egy csoportot használ. Új használat az
azonos pool korábbi folyamatos profilját teljesen lecseréli; a másik pool és
a tartalék megmarad. Tele poolnál is elindítható. Az első adag az első teljes
időköz végén, az utolsó a hatás időtartamának végén jár. A már eltelt online
adagok összegezhetők, a tényleges jóváírás mindig a hiányra korlátozott.

Használatkor a közös C5 admission ellenőrzi a szintet, kasztot, nemet,
harcot, helyet/PvP-t, arénakorlátot, darabszámot és cooldown-t. Tickkor a
halál, kábítás, megfigyelő mód, aktuális hely/PvP és aréna potion-tilalom
ismét ellenőrzött. Tiltott időből később nem jár visszamenőleges gyógyulás;
a tartalék megmarad, a folyamatos hatás ideje tovább fogy. Halál külön jelzi
a törlést, ezért a gyors újjáéledés sem őrzi meg az előző adagokat.
A cooldown halál és kézi megszakítás után is megmarad. Azonnali és ütemezett
ital ugyanazt a tartós cooldown-profilt használhatja.

A live karakter egész HP/SP pontot kap. A már megszerzett 1 pont alatti
maradék poolonként megmarad; nem veszítjük el csendben a kicsi adagokat.
A tartalék tele poolnál törlődik, a folyamatos hatás tovább időzik.

## Tartós állapot és idő

A `004_recovery_schedule.sql` additív migráció új `item_recovery_schedule`
táblát hoz létre. Telepítése megelőzi az új szerver indítását. A blob `RCS1`,
legfeljebb 4096 byte; revision, cancellation-generation, két törtmaradék,
legfeljebb 18 adag és 64 közös cooldown. A hash ownerhez kötött. Az owner és
az utolsó operation receipt idegen kulcsokkal kapcsolódik.

Az `ItemCommit.recovery` opcionális CAS-változás: az item fogyása, az új
ütemezés/cooldown, a tulajdonosi fence és receipt **egy SQL-tranzakció**.
Tick és megszakítás önálló recovery commit is lehet. A receipt digest csak
az új mező jelenlétében bővül, a korábbi commitok digestje megmarad. Sérült,
más ownerhez tartozó vagy elavult blob nem tölthető be. Replay nem fogyaszt
újra, és egy megtörtént tick nem adható jóvá újra.

Monoton online órát és mentett hátralévő időt használunk. Karakterválasztás
és offline idő nem számít. Logout és shutdown az utolsó töredékidőt is
menti; ugyanazon core új loginja megvárja a lezárt órát. Általános item
commit alatt az óra követi a pending állapotot, de saját időhorgonyát csak
recovery commit viszi előre. Így az accessory időelszámolása és a közben
végrehajtott item műveletek nem veszítik el a visszatöltési időt.

A jelenlegi game heartbeat másodpercenként dolgozza fel az ütemezést.
Rövidebb profiltickek is összegezve számolódnak el; ez nem milliszekundum
pontosságú vizuális gyógyulás. Az offline időre nincs catch-up. Kereskedés
szerkesztése közben a tick működik, commit alatt vár. A kizárólag passzív
recovery/accessory revision átvezethető a kereskedés és más előnézetek
fence-ére, gazdasági módosítás nem.

**Határ:** a debit mentése megelőzi a live HP/SP jóváírását. Az HP/SP továbbra
is a legacy karaktermentés része. Crash a két rendszer között elveszett
visszatöltést okozhat; a teljes HP/SP + item crash-atomikusság és többcore-os
handoff/hibainjektálás külön kiadási kapu. Ezt a C19 nem tekinti megoldottnak.

## Protokoll és Unity

- **CG242:** 26 byte + sequence, header/action:u8, session/source/target:u64.
  1: forrástárgy előnézete; 2: preview nonce megerősítése; 3: ablak bezárása;
  4: megszakítás login-control nonce + generation + hatás-ID (0: mind);
  5: aktív állapot megnyitása. A 237 legacy time-sync, ezért nem használható.
- **GC242 v1:** u8 header, u32 hossz, u8 verzió/státusz/hiba, három u64
  azonosító, HP/maxHP/SP/maxSP raw, profile delivery/units/amounts/interval/
  duration/rate/cooldown, forrás ItemKey, hosszazott RCS1 és GC227 metadata.
  Az elfogyott források ItemKey-jének adatai is benne vannak.
- **GC227 v2:** ütemezett recovery jelenlétében is u16 action, új Recovery
  bit 512. Automatikus recovery nem kap használható actiont.

Unity: **I → ital → Visszatöltés → Használat áttekintése → megerősítés**.
Az inventory **Visszatöltések** gombja a forrás elfogyása után is elérhető.
Az ablak külön mutatja HP/SP-t, a tartalékot, sebességet, következő adagot,
időtartamot és cooldown-t. Egy hatás vagy minden hatás külön megerősítéssel
megszakítható. A source neve GC227-ből jön. A kliens nem fogyaszt és nem
gyógyít helyben. A szerver friss inventoryt küld az explicit eredmény előtt.

A preview nonce egy próbát enged. Régi login/control/generation/forrás nem
irányíthat új hatást. A passzív időfrissítés érvényes megerősítést megőriz,
a forrás eltűnése vagy megváltozott darabszáma érvényteleníti.

## Ellenőrzések

Lásd a [C19 tesztjelentést](../reports/item_combat/2026-11-03_recovery_runtime.md).
Automatikus ital, időzített/toggle hatások és további item-használatok külön
szeletek maradnak.
