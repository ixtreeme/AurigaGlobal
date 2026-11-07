# ADR IC-064 — Stóla, rúna és speciális bónuszműveletek runtime

2026-11-07. C29. Szerver: `20261103`; Unity:
`/main/unity63-integration-20261103`, Editor `6000.3.27f1`.

## Adatok és felszerelés

A v28 katalógus két új, változatlan azonosítójú kategóriát vezet be:
19 `Regalia`, 20 `BonusTool`. A stóla a hat saját bónuszos
`COSTUME_STOLE` viselkedés; az önálló Acce-felszerelés elnyelése külön
rendszer. Nincs futásidejű vnum-következtetés vagy kettős írás.

A stóla a 12-es helyre kerül. Négy fokozata és hat különböző bónusztípusa
van, típusonként négy katalógusértékkel. A létrehozott példány a tényleges
hat értéket tárolja. A gyári dobás a szerveres operation ID és instance ID
SHA256 lenyomatából készül; az újrabűvölés külön szerveres dobásokat használ.
Katalógusérték átírása nem változtatja meg a már kiadott hat értéket.

A hét rúnahely a 13–19. felszerelési hely. Az első hat rúna töltése és
aktív állapota példányadat, a két bónusz a megmaradt töltéshez tartozó
legmagasabb elért katalógusküszöbből számolódik. A hetedik rúna két
készletbónusza akkor érvényesül, ha mind a hat másik rúna fel van szerelve,
aktív és legalább 50%-os. A hetediknek nincs saját órája vagy kapcsolója.
A stóla és a rúnák ugyanazon felszerelési, kötési és tulajdonosi
ellenőrzéseket használják, mint a korábbi típusos tárgyak.

## Tartós óra és állapot

A rúnakeret csak online, aktívan és felszerelve fogy. Öt másodperces
checkpoint, levétel, kikapcsolás és kijelentkezés rendezi az időtartozást.
Az új munkamenet megvárja a korábbi tartozás rendezését. Inaktívan,
leltárban és offline nem fogy; a kiadott kapacitás katalógusmódosításkor is
megmarad. A stat- és megjelenésprojekció a még ki nem írt időt is figyelembe
veszi, így a fél töltés vagy a nulla elérésekor nem marad régi bónusz.

Nullánál a rúna inaktív lesz, azonos példányként megmarad és tölthető.
Az üres példányhoz nem gyűlhet további óratartozás. Felszerelésből
leltárba mozgatás és csere kikapcsolja a levett rúnát; a végső tartozás
utána is rendeződik. A korábbi checkpointos crash/handoff korlát megmarad.

A `017_regalia_special_bonuses.sql` hozzáadja a két nullable állapotoszlopot
és a régi equipment tárolókat legalább 19 helyre bővíti. A 93 bájtos RGL1
a kapcsolót, kiadott kapacitást, maradékot és hat opcionális bónuszt
tárolja. A 18 bájtos BTL1 a donor bónuszát vagy az üveg maradék
bázispontját őrzi. Hibás/hiányzó kötelező állapot betöltési hiba, nem
újragyártott állapot. A korábbi kategóriák új mezői NULL-ok; állapot nélküli
régi példányok kanonikus hash-e változatlan.

## Hat külön szolgáltatás

| Művelet | Szabály |
|---|---|
| Stólabűvölés | Leltárban lévő stóla mind a hat értéke egyszerre sorsolódik a saját négy változatából; az eredmény az előzővel egyezhet. |
| Rúnatöltés | Felszerelt 1–6. rúna aktívan vagy inaktívan tölthető. Először az eltelt idő rendeződik, utána a saját kiadott kapacitás arányos része töltődik vissza. A nem halmozható üveg fel nem használt része ugyanazon példányon marad; csak a teljesen elfogyott üveg tűnik el. |
| Jelmezreset | Leltári test-/haj-/fegyverjelmez meglévő, nem zárolt normál csoportját pontosan három új sorra cseréli. Mindhárom húzásnak saját szinteloszlása van; a jelöltek súlyozva, visszatevés nélkül választódnak. Ritka sor, időzítő és fejlesztés megmarad. |
| Jelmezdonor | A nem halmozható donor példányán tárolt pontos bónuszt az első üres ritka helyre írja. A ritka/extra csoportban már jelen lévő azonos típus vagy két teli ritka hely elutasítás; donor nem fogy. |
| Ritka sor törlése | A kiválasztott két ritka hely egyikét törli, csak a ritka csoportot tömöríti. Üres hely nem fogyaszt eszközt. |
| Kapcsolt addon | Leltári fegyver normál és skill sebzésbónuszát kapcsolt Gauss-dobással cseréli. A többi normál sor megmarad; védett kapcsolt sor elutasítás. |

A kapcsolt addon skill értéke a legacy képlettel egyezően
`clamp(int(BoxMuller * 5 + .5), -30, 30)` százalék. A normál érték
`-2 * skill` plusz a két, -8..8 közti zaj abszolút összege és 1..4 közti
tag, ha `abs(skill)<=20`; azon kívül csak 1..5 közti tag. A tárolt értékek
bázispontok. Negatív érték megengedett, nulla sor nem kerül be.

A céltárgy nem lehet lejárt jelmez. A teljes előfeltétel- és tárolóvalidálás
a tényleges RNG előtt fut. A forrásfogyás és a teljes célállapot cseréje egy
atomi itemcommit. A SBR1 bizonylat a műveletet, két példányt, időtartozást,
ritka választást, szereplői kontextust és legfeljebb 12 dobást rögzíti.
A writer pontos visszajátszással ellenőrzi az eredményt és anyaglevonást;
oda rejtett más tárgyváltozás nem engedett. A meglévő, kizárólag viselt
ékszeridőt rendező járulékos levonás továbbra is elfogadható.

## Hálózat és kliens

- **CG214:** 28 bájt + sequence. Action preview/confirm/close; operation
  activate/deactivate/service; nonce, source ID, target ID, ritka választás.
  A kliens nem küld bónuszértéket vagy dobást.
- **GC220 v1:** legalább 35 bájt. Státusz, nonce, két ID, operation,
  választás és text16 üzenet. Azonos nonce ismétlése az eltárolt választ
  adja, új dobás és fogyás nélkül. Megváltozott forrás/cél új preview-t igényel.
- **GC225 v14:** a korábbi sidecarok után regalia- és eszközállapot,
  definíció, küszöbök és húzásonkénti szinteloszlás. A régi csomagok formája
  megmarad, ha nincs új kategória.
- **GC227 v3:** az action mező u32, SpecialBonus bit 65536; a korábbi
  v1/v2 metadata továbbra is olvasható.
- **GC241 v2:** a korábbi öt modellhivatkozás után explicit stólamodell és
  rúnamód (0: nincs typed rúna, 1: viselt, 2: aktív készlet). A megfigyelő
  is megkapja. A mount-flight támogatási bitek közül a 4-es kéri a v2-t;
  csak jelmezt ismerő kliens továbbra is v1-et kap.

A CG214 külön ütközéstesztet kapott a hexadecimális legacy azonosítókkal
együtt. A kezdeti CG241 a legacy `CLIENT_VERSION2=0xf1` miatt nem használható.

A külön típusos leltárban a stóla felszerelhető, a hét rúna külön helyre
kerül, a **Bónuszok/Rúna** gomb nyitja az új ablakot. Céltárgy-választás,
ritkahely-választás, áttekintés és megerősítés valós szerveres műveletet
indít. A UI kizárólag a GC225-ből változtat inventoryt, és a teljesen
elfogyott eszköz eredményét is megőrzi a válaszhoz.

A stóla explicit GameObject assetet tölt a tényleges sash rendererbe.
A teljes aktív rúnakészlet a tényleges rune effektet kapcsolja. Levételkor
a korábbi megjelenés visszaáll, a mounted pozícióváltás a typed modellen is
működik. A Qt-kezelő külön stóla-/rúna- és eszközűrlapot, szerkeszthető
szabálytáblát, kezdő sorokat és natív validálást használ.

Az ellenőrzések és a telepítés részletei a
[C29 jelentésben](../reports/item_combat/2026-11-07_regalia_special_bonuses.md).
Ez nem teljes APPLY-/tartalomkonverzió vagy a normál legacy leltár átállítása.
