# IC-070 — Mission XP-, szint- és fiókvaluta-jutalom (C35)

A C34 általános Mission-játékútja a tárgyak és Yang mellett fejlődést és
közvetlen típusos fiókvalutát is adhat. Ugyanezt az utat használja a Unity
Mission-napló és a `pc.typed_mission_action` coroutine API.

## Szerződés és számítás

A reward **3-as sémája** a 2-es típusos tárgyformátumot három mezővel
bővíti: `experience` (XP), `levels` (közvetlen szintnövekedés) és
`account_coins` (a típusos fiókpénztárca Dragon Coin egyenlege). Mindhárom
opcionális, alapértéke nulla; tárgy nélküli jutalom is megengedett.
A régi 1-es és 2-es payloadok bájtsorozata és lenyomata változatlan.
Ezek az új mezőket elutasítják, nem hagyják figyelmen kívül.
A szint- és fiókvaluta-payload natív típusazonosítója 0x5009 és 0x500a.
A gameplay csomagaktiválás külön ellenőrzött összeg- és szintkötést igényel.

A közvetlen `levels: N` először N alkalommal befejezi az aktuális szintet;
az utolsó ilyen szintlépés után az XP nulla. Ezután kerül hozzáadásra az
`experience`. A szerver aktuális XP-táblája, szintkorlátja, statpontszabálya,
képességcsoportja és kasztonkénti HP/SP-növekménye érvényesül. Az XP
átvihet több szinten, maradéka megmarad. A három negyedszint statpontjai,
a szintenként járó képességpontok és a véletlen HP/SP-növekmények is
részei az eredménynek. A könyvköltség miatt a jelenlegi XP-nél magasabb
mentett negyedszintet nem jutalmazzuk ismét.

A jutalom szerző által meghatározott fejlődés; a normál ölési XP tiltója
nem kapcsolja ki. A teljes szint-, XP-, pont-, HP/SP- vagy pénztárcatúllépés
elutasítja az átvételt. Nincs csonkítás, részleges jutalom vagy elveszett
függő akció. A szintnövekedés rendes szintértesítést és pontcsomagot küld;
a kliens nem növel XP-t, szintet vagy pénztárcát előre.

## Egyetlen tartós átvétel

A pinned Mission-akció adja az összegeket. A kliens és Lua továbbra is
csak a küldetés azonosítóját és a műveletet küldheti.
Egy tranzakció tartalmazza az entitlementet, tárgy-/Yang-műveletet,
fejlődési checkpointot, fiókpénztárcát és a nyugtázott Mission-mentést.
Elveszett COMMIT-válasz esetén a C34 egyeztetője ugyanazt a mentési
operationt olvassa vissza. Az ismételt kérés nem jutalmaz újra.

A fejlődés a meglévő `item_book_learning` ellenőrzött snapshotját és
`typed_book_revision` kerítését használja. A BOK2 snapshot az XP és
képességpontok mellett szintet, negyedszintet, statpontot és véletlen HP/SP-t
is tárol. A BOK1 visszaolvasható és változatlanul kódolható. Új tábla vagy
adatbázis-migráció nem szükséges. A normál XP-/szint-/statmutáció és az új
login a folyamatban lévő checkpoint végéig vár. A késői teljes karakter-
mentés és az adatbázisszerver cache-e mind a tíz érintett mezőt a revízióval
védi. A magasabb checkpoint újrabelépéskor a régi cache-adatot felülírja;
szintjutalmat vagy szintquest-eseményt nem játszik újra.

A fióktulajdonos kizárólag a hitelesített descriptorból származik. A közös
tranzakció SQL-ben is zárolja és ellenőrzi a karakter fióktagságát, a fiók
owner fence-ét és a pénztárca előző revízióját. Más fiókot a kliens nem
jelölhet meg. Párhuzamos pénztárcaművelet konfliktus, nem felülírás.
Ez a C33 típusos pénztárcája; a régi cash mező importja nem része ennek.

## Unity és ellenőrzés

A GC202 **2-es verziója** a jelenlegi szintet, XP-t és fiókegyenleget,
valamint küldetésenként az új jutalmakat is közli. A CG202 kérése változatlan.
A Unity az archivált GC202 v1-et is olvassa. A napló a megerősítés előtt
feltünteti az összes jutalmat; a rendes játékos pontállapotát a szerver
GC16/17 frissíti. A natív/SQL-, Unity- és helyi újrabelépési eredményeket a
[C35 jelentés](../reports/item_combat/2026-11-08_mission_progression_rewards.md)
rögzíti.
