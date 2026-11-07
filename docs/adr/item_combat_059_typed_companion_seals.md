# ADR IC-059 — Tartós típusos követőpecsétek

2026-11-06, C26b. Szerverág: `20261103`. Unity-ág: `/main/unity63-integration-20261103`. Az aktív munkaterületeket az [AGENTS.md](../../AGENTS.md) sorolja fel.

2026-11-07-i kiegészítés: az állandó hátaspecsét tartós felülése és
leszállása a [C27a-ban](item_combat_061_typed_mount_riding.md) elkészült,
szerverrel és Unityvel együtt, sikeres helyi újrabelépéssel. A fejlődőpet-
csomagot az [IC-060](item_combat_060_typed_pet_hatching.md) már lezárta.
Az alábbi bekezdések a C26b eredeti határát rögzítik.

A 15-ös kategória állandó pet- és hátaspecsétjei tartós példányként kioszthatók. A Unity leltárból a **Megidézés** és **Visszahívás** művelet a pecsét példányazonosítóját küldi. Egyszerre egy típusos követő lehet aktív. A hátaspecsét ebben a lépésben követőt idéz. A típusos felülés, időkorlát, bónuszok, fejlődő pet és tojáskeltetés későbbi lépések.

Az `010_companion_seals.sql` a példányhoz adja az idézési szándékot. A tulajdonos munkamenete és revíziója védi a tranzakciót. A pecsét azonosítója, darabszáma és helye idézéskor megmarad. Aktív szándéknál a példány a tulajdonos leltárában marad. A szerver mentés után hozza létre vagy hívja vissza a követőt. Újrabelépéskor a szerver az érvényes szándékból új szereplőt hoz létre, amint a karakter és a térkép készen áll. Elérhetetlen megjelenítéskor a pecsét függő állapotot mutat. A visszahívás törli a szándékot.

A CG249 a normál szekvenciabájttal küldi a két külön műveletet. Azonos művelet ismétlése visszaadja az aktuális állapotot. A GC225 v7 a kért és a ténylegesen követő állapotot külön jelzi. Pecsét nélküli leltár a korábbi verziók egyikét használja. A követő megjelenése a szokásos szereplőbeszúrás és -törlés csomagokkal jut el a közeli kliensekhez.

A 20261103-as Unity-ág gyorsrepülése a helyi típusos protokollban a 221-es csomagon működik. Az új kliens a játékfázis kézfogásában jelzi a gyors mód támogatását. A szerver a gyors állapotjelzőt az ezt kérő klienseknek küldi, a függőleges sebességkorlát minden módban közös.

A helyi Unity kapcsolati profilban az **Enable Typed Item Backend** kapcsoló engedélyezi a 221-es és típusos tárgycsomagokat. A legacy profil a célág meglévő protokollját használja.

## Helyi ellenőrzés

A Windows Release GameServer build, hat natív tesztcsomag és a MariaDB 1917 ellenőrzése átment. Unity 6.3 alatt a pecsétcsomag 333 ellenőrzése, a korábbi típusos leltár és a TERA-tartalom regressziója is sikeres.

A 92 ellenőrzéses helyi TLS-próba két klienssel futott. Mindkét pecsétnél lefedte a leltárgombot, az ismételt kéréseket, a második aktív pecsét elutasítását, a megfigyelő csomagjait, az aktív újrabelépést és a visszahívás utáni újrabelépést. A két pecsét azonosítója és darabszáma megmaradt. A teszt után mindkettő nyugalmi állapotban volt, a korábbi 180 példányrekord hash-e egyezett.

A 30 ellenőrzéses repülési TLS-próba lefedte a gyors mód egyeztetését és elengedését, a korábbi kliens jelzőbitjeit, a 221-es és 225-ös csomagok együttes működését és a meglévő petpecsétet. A Unity változásai a célág 71–73-as changesetjeiben szerepelnek.
