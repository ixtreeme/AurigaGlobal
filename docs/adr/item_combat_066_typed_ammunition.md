# IC-066 — Típusos íj és lőszer (C31)

## Katalógus és felszerelés

A v29 katalógus új, végleges 21 `Ammo` kategóriát és `item_ammo`
táblát ad. A lőszer egy névvel ellátott, halmozható példány;
`ammo_policy_id` az íjjal közös `ConsumeStack` szabályra mutat.
A kompatibilitás pontos policyazonosság, nem legacy vnum vagy ikon.
A stack maximuma legalább a szabály `units_per_hit` mennyisége.
Nincs rejtett lőszersebzés: a támadási érték a típusos íj saját értéke.

Az új felszerelési hely 20 (nulla alapú anchor 19). A 019 migráció
csak kibővíti a korábbi kisebb felszereléstárolókat; az anchorok és
példányok változatlanok. Az íj lőszer nélkül is felszerelhető, de
lőni csak elegendő, kompatibilis, felszerelt lőszerrel lehet.
A normál játékbeli ki-/felszerelés, csere és újrabelépés a meglévő
azonosító-, revízió-, kötés- és foglaltságvédelmeket használja.

## Elfogadott lövés

A szerver ellenőrzi az élő, nem átváltozott karaktert, a típust,
célt, azonos térképet, hatótávot, felszerelt forrásokat és a használati
feltételeket. A normál íjászút az assassin osztályt követeli meg;
a képességlövés a szerver skillhasználati/hitkeretét is ellenőrzi.
Az önmagára célzás, hiányzó/távoli cél, lőszerhiány vagy eltérő
policy nem enged ingyenes sebzést. A késői harci elutasítás sem fogyaszt.
Egy elfogadott, de nulla végső sebzésű lövés fogyaszt lőszert.
Az engedélyezett PvP-lövés is fogyaszt, PvE-lélekbónuszt nem használ.

Az `AmmoShotOutcome=0x3101` Service-művelet pontosan a policy szerinti
darabszámot vonja le; az íj revízióját is zárolja. Az utolsó darab után
az elfogyott példány audit-tombstone marad. Ha van azonos ItemKey-jű,
használható inventory-tartalék, az ugyanabban az írásban felszerelődik
(storage/anchor/ID szerinti stabil kiválasztás). Másik prototípust vagy
hiányos darabszámot nem alakít át hallgatólagosan kompatibilis lőszerré.

A 019 migráció `item_ammo_shot_event` táblájának owner/event elsődleges
kulcsa új operation ID mellett is kizárja a duplafogyást. Azonos
operation ismétlése a meglévő receiptet adja vissza. A natív writer
újratervezi és ellenőrzi a teljes elvárt/utána állapotot. Általános
Service nem csökkentheti a felszerelt lőszert.

Az elfogadott sebzés a C30 közös actor-/célfoglalását használja.
Függő írás alatt újabb érintett sebzés és karaktermegsemmisítés vár;
újrabelépés csak a régi függő írás lezárása után tölthet be.
A végső HP-, damage-packet-, quest- és halálfolytatás SQL-siker után fut.
A szinkron íj-lövedék scope az elemi sebzésű íjképességeket is jelöli;
a visszaverődés és másodlagos időszakos sebzés nem új lőszerlövés.

Aktív, jogosult vörös/kék lélek esetén annak SLR1 találati döntése
ugyanabba az AMR1 receiptbe és itemírásba kerül. A lélekevent és a
lőszerevent is egy tranzakcióban keletkezik. Nincs két egymás utáni,
külön sikerülő lőszer-/lélekírás. A C30 töltési órája a beágyazott
elapsed mennyiséget is pontosan levonja.

## Receipt és Unity

Az AMR1 payload 80 byte, opcionális 84 byte SLR1-toldalékkal:
magic 4; bow/ammo/reserve/event/target/targetRevision u64; input/output
i64; units u16; level u16; class/sex/inCombat/targetIsNpc/alive/
polymorphed/channel/soulSize u8. Minden szám little endian.

GC225 v16 az eddigi kumulatív mezők után példányonként hozzáadja:
kind u8 (0 nincs, 1 íj, 2 lőszer), nemnullánál policy u32 és units u16.
A régi, íjat/lőszert nem tartalmazó csomagok korábbi verzión maradnak.
A GC227 felszerelési slot 20-at publikál; nincs új CG fejléc.

A Unity a valódi felszerelési műveletet küldi a meglévő csatornán,
megjeleníti a lőszer mennyiségét és a fogyási szabályt. A harci
vezérlő a legutóbbi hiteles GC225 felszerelt lőszerét is ellenőrzi.
Nem von le előre és nem jósol új tartalékot. A kezelőben a lőszer
saját kategóriája és kompatibilitási választója is elérhető.

## Ellenőrzési határ

Natív tervező-, valódi SQL-, Unity-fordítás/codec/felület- és helyi
TLS-játékpróba tartozik a csomaghoz. A pontos eredmények a C31
jelentésben szerepelnek. A legacy harci procok korábbi mellékhatásai
és az ECS-világ HP-ja nem vált tartós harci outboxszá. Nem állítunk
SQL utáni folyamatösszeomlással szembeni teljes világ-ACID garanciát.
Animációs ütemezés, production íjmodell-/lövedék-VFX, teljes legacy
konverzió, FreeBSD, standalone és terhelés külön ellenőrzési feladat.
