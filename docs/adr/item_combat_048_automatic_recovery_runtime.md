# ADR IC-048 — Automatikus HP/SP-italok a futó szerveren és Unityben

2026-11-03, C20. Előzmény: IC-016 profilok és tiszta tervezők, IC-047 tartós
visszatöltési óra. A korábbi domain-alap most szerver–kliens funkciót kapott.

## Viselkedés és mentés

Az automatikus ital önálló, nem halmozható példány. Bekapcsolásakor nem fogy
darab; a véges töltet csak az elfogadott visszatöltéssel csökken. A végtelen
ellátás explicit profilérték, tárolt töltete nulla. Egy karakternek egy HP-
és egy SP-palackja lehet aktív. Másik azonos poolú palack bekapcsolása az
előzőt ugyanabban a tranzakcióban kikapcsolja, megőrizve annak töltetét.

Az aktív palack nem mozgatható, adható át, dobható el, adható el vagy
fogyasztható el más művelet részeként. A közös commit-validátor is védi,
nem csak a kliens. Kikapcsolás után a megmaradt töltet a példány részeként
megmarad. A grant-tervező és a helyi próbapad új palackot teljes, inaktív
állapotban hoz létre. A kifogyott véges palack tartós tombstone-ná válik.

Az additív `005_automatic_recovery.sql` három nullable oszlopot ad a
`player.item_instance` táblához: töltet, aktivitás és következő adagig hátralévő
idő. Telepítése megelőzi az új szerver indítását. A korábbi példányok NULL
mezői és digestjei megmaradnak. A töltetes példány hash-e az új állapotot is
fedi; teljes before-state CAS, owner-fence és receipt védi a módosítását.
A betöltés hibás vagy többszörösen aktív pool esetén elutasít.

A source/lecserélt palack, az idő és a közös cooldown egy SQL-tranzakcióban
változik. Az aktív maximum-bónusz a hiteles itemállapotból épül a statokhoz;
nem ismételten hozzáadott affect. A jelenlegi egész HP/SP maximumhoz a
profil basis-point értéke egyszer, lefelé kerekítve adódik hozzá. Kikapcsolás
és kifogyás eltávolítja, és a szokásos maximum-clamp érvényesül. Kifogyáskor
előbb a régi maximum szerinti jóváírás, utána a statfrissítés történik.

## Idő, tiltások és együttműködés

Az IC-047 monoton online órája kezeli a palackok idejét is. Offline és
karakterválasztás közben nincs töltetfogyás vagy felzárkózó gyógyítás.
Logout/shutdown a töredékidőt menti; a same-core login megvárja a lezárást.
Egy elmaradt időszakból legfeljebb egy automatikus tick teljesülhet. Az
egyéb recovery-commitok az automata időzítését is elszámolják; esedékes tick
ilyenkor a következő feldolgozásra marad, töltet levonása nélkül.

A game heartbeat másodperces: egy rövidebb profilintervallum sem jelent
ennél gyakoribb élő feldolgozást. Halál, kábítás és megfigyelő mód szüneteltet;
tiltott kontextus, megváltozott hozzáférési feltétel vagy retire kikapcsol.
Arénában automata nem engedett. A világ/párbaj/céhháború döntést az owner
adja, ezen felül a profil PvP- és a use policy harci/szint/kaszt/nem feltétele
érvényesül. A kikapcsolás halott/kábult vagy tiltott helyen álló karakternek
is engedett, a közös tárgyműveleti munkamenet szabályain belül.

Az elfogadott tartalék és az összegyűlt törtérték levonódik a szabad hiányból:
ugyanazt a hiányt az automata nem terheli újra töltetre. A tört gyógyítás az
IC-047 tartós kreditjébe kerül, nem vész el a live egészpont-határon.

A késve érkező legacy affect-betöltés korábban teljesen feltöltötte a
HP/SP-t. Typed karakteren ez most csak maximum-clampot végez; nem írja felül
a mentett poolokat és a közben elindított visszatöltést. A hagyományos
karakterek korábbi feltöltése változatlan.

## Protokoll és Unity

- CG242 marad 26 byte + sequence; előnézet és egyszer használható nonce-os
  megerősítés vezérli a palack ki-/bekapcsolását is.
- GC242 v2 automata esetén supply/maximum-bónusz profilt és az önálló
  palackokat adja: példányazonosító, ItemKey, töltet, aktivitás, idő,
  célpool, ellátás, kapacitás, intervallum, sebesség és bónusz. GC227 metadata
  biztosítja a neveket. A többi használat v1 csomagja megmarad.
- GC225 v2 a 4-es flaggel opcionális automata blokkot visz: supply:u8,
  töltet:u64, aktív:u8, következő tick:u64. Automata nélkül v1 marad.
- GC227 v2 Recovery action automatikus italhoz is jár. Az aktív palack
  kliensoldalon is zárolt, a kezelési művelet továbbra is elérhető.

Unity: **I → palack → Visszatöltés → Használat áttekintése → Állapotváltás
megerősítése**. Az ablak megmutatja a véges/végtelen töltetet, sebességet,
maximum-bónuszt és aktuális állapotot. Az inventory felső **Visszatöltések**
gombjából az aktív palack **Kezelés** gombbal nyitható meg. A közös adagok
megszakítása a tartalékos/folyamatos hatásokra vonatkozik; a palack külön
állapotváltást kap. A kliens nem gyógyít és nem von le töltetet helyben.

## Ellenőrzés és határ

Lásd a [C20 tesztjelentést](../reports/item_combat/2026-11-03_automatic_recovery_runtime.md).
Az item-adatbázis és a legacy HP/SP-mentés közötti crash-atomikussági rés
IC-047 szerint továbbra is nyitott. FreeBSD, teljes Unity player build,
többcore-os handoff, terhelés és hibainjektálás külön kiadási kapu.
