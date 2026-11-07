# C26m — Fejlődő pet szállítódoboza

A szerver `20261103` és a Unity `/main/unity63-integration-20261103` ágán.
A Pet tárgyai ablakban a visszahívott pet dobozba helyezhető, majd a
leltári doboz **Tárolt pet adatai** műveletéből kivehető. A csomagolás az
eredeti pecsétet külön, 1×1-es belső tárolóba mozgatja; nem készít másolatot.
Minden születési adat, életidő, fejlődés és képesség megmarad. Kivételkor
a doboz egyszer elfogy, felszabadított helye felhasználható, a belső tároló törlődik.

A `013_pet_supplies.sql` példányállapotot és a karakterhez kötött belső
tárolót vezeti be. A kétirányú doboz–pecsét kapcsolat SQL-betöltéskor és
a GC225 v10 klienscsomagban is ellenőrzött. A mentés, mozgatás és dobozfogyasztás
egy CAS-védett tranzakció. A normál tárgyfogyasztás nem törölhet teli dobozt.
A dedikált item writernek az `item_storage` táblára INSERT/DELETE jogosultság
is kell; az `item_owner_fence` továbbra is csak SELECT/UPDATE, tulajdonost nem hozhat létre.

Ellenőrzések:

- Windows GameServer és ItemBenchTool fordítás sikeres.
- `pet_lifecycle`, `typed_inventory_wire`, `item_persistence_mariadb` sikeres.
  Az SQL-próba a csomagolás és kivétel ismétlését, újratöltését, tiltott
  normál fogyasztását és a belső tároló eltávolítását is ellenőrzi.
- Unity 6.3 offline: 3403 pet-, 1640 felszerelési, 340 companion- és
  4179 leltárellenőrzés sikeres.
- Valódi helyi TLS-kapcsolat Unityből: **300 ellenőrzés sikeres**.
  Képességtanulás után csomagolás, dobozban újrabelépés, teljes adatlap és
  képességek visszaolvasása, kivétel, ismételt megerősítés, újrabelépés,
  idézés, visszahívás és végső visszahívott újrabelépés történt.
- Az SQL-utóellenőrzés 465 korábbi tárgyat és a korábbi pénztárca/tanulási
  adatokat változatlannak találta. A tesztvilág helyreállt.

Bizonyítékok: `build/c26m-native-green.log`, `build/c26m-game-sync2-build.log`,
`build/c26m-unity-offline-final.log`, `build/c26m-unity-live2.log`.
Privát fixture: `build/c26m-carrier3.private.json`, nem része a commitnak.
A grafikai snapshot nem a teljes játékvilág művészeti ellenőrzése; a
hálózati UI-folyamatot teszteli. Szerver-újraindítás a későbbi közös pet
regresszió része; ebben a próbában kliens-újrabelépések futottak.
