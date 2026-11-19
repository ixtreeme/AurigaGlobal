# Eredeti állapotos világműveletek

Négy elérhető visszahívó tekercs (22000,22010,22011,39043), a27600 tábortűz és a27989/76006 kőkeresők natív konverziója. A három további tekercs (22001,22020,22030) aktív Unity ikonkötése és megfelelő ikonfájlja hiányzik; a felhasználó kérésére a merge-ig halasztva. Nincs kitalált grafika vagy ikonalias.

A tekercsek explicit RCL1 térkép/XY állapotot tárolnak. Az eredeti HP-korlátok, költségek, birodalmi, térkép-, dungeon-, aréna- és teleportfeltételek érvényesek; a22011 hibás nulla-távolság ága a valódi mentett célpontra ellenőriz. A kötegből bélyegzett egység saját új natív azonosítót és szabad helyet igényel (külön doménteszt); az aktuális négy protó eredetileg nem halmozható. A tárgy és az RCS4 teleport-szándék azonos SQL-tranzakcióban mentődik. Az úti cél és a HP-költség plafonja elvesző callback vagy újrabelépés után megmarad; a szerver a megérkezést követően nyugtázza és törli. Másik login tokenre callback nem alkalmaz állapotot.

A kőkereső külön ToolKind::Detector típust és maximum_uses kapacitást használ. A hat használat saját perzisztens remainingUses mező; nem gyakorlópont, csali vagy régi socket. Az üres keresés is fogyaszt használatot, a hatodik törli a példányt és az irányjelzőt. A legközelebbi kő, a VID sorrendű döntetlen és az eredeti három távolsági fokozat szerveroldali. A Unity az autentikált StoneDetect parancs alapján a helyi karakterhez kötött irányjelzőt mutatja és az eredeti hangot játssza; nincs kliensoldali tárgyfogyasztás.

A tábortűz a karakter előtt100 egységgel, valós sektree/vízfeltétellel készül elő rejtetten. Csak a sikeresen nyugtázott SQL-mentés után jelenik meg a valódi12000 actor,40 másodperces élettartammal. Sikertelen előkészítés vagy mentés nem fogyaszt tárgyat. A60 másodperces UTC újrahasználat a fogyasztással együtt mentődik, offline is telik, és a halál nem törli. COMMIT-újraegyeztetés esetén az első igazolt callback egyszer publikálja az előkészített actort.

Katalógusformátum50, migráció058; példánymigráció035; RCS4 és visszafelé kompatibilis GC225 v27 állapot. A régi csomagverziók és az új mezőt nem használó példányok korábbi hash/payload formái megmaradtak. Négy metaadat-golden független ellenőrzése szerint csak a katalógus hash változott.

Ellenőrzés:237 natív domén-/codec-ellenőrzés,88 konverterteszt,259 tényleges ItemStore/SQL-ellenőrzés,33 kimentett csomagállapot; Unity6.3 fordítás és465 ellenőrzés22 tényleges Használ kattintással, valódi ikonkötésekkel és világ-irányjelzővel. GameServer/Database fordítás és67 teljes regressziós teszt sikeres. Az első regresszió két fixture-migrációs hiányát az új035/058 migrációval javítottuk; a friss saját SQL-környezetben minden teszt átment.

Ez ellenőrzött offline konverziós és implementációs eredmény. Teljes TLS/ECS játékmenetet, tényleges core-váltást, megszakított élő recallt, világban történő tábortűz-sütést és Addressables játékosbuildet még nem próbáltunk. A helyi aktív katalógust és játékosadatokat ez a kör nem módosította; a94 játékosállapot-tábla továbbra is üres.

Összesen9830 konvertált,491 kivezetett forrásprototípus,6912 natív definíció.633 valóban hiányos tétel a merge-re vár,32 rendelkezésre álló funkció/politika feldolgozása folytatódik.
