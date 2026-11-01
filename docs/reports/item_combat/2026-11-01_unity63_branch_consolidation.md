# Unity 6.3 a mission engine ágon

2026-11-01. Felhasználói kérés: a szerver és a Unity együtt fejlődjön,
helyben tesztelhető munkaszeletekben; a külön 6.3-as ág szűnjön meg.

## Eredmény

- Aktív Unity-workspace: `D:\Auriga-Unity6\SRC\AurigaUnity`.
- UVCS-ág: `/main/codex-mission-engine`, az eredeti ágazonosítóval
  (`518848`), fej: **cs:39**.
- Projektverzió: **6000.3.27f1**, revízió: `672d562a42a7`.
- A `/main/unity-6.3` ág törölve. Changeset nem lett törölve.
- A workspace tiszta; a 6.3-as ág korábbi cs:39 fájlállapota maradt meg.

## Előzmények megőrzése

A 6.3-as ág cs:35 changesetje már tartalmazta a mission engine cs:33
állapotát és a main cs:34 változásait. A cs:39 a main cs:37 változásait is
beolvasztotta. Az előzetes összehasonlítás alapja és célja egyaránt cs:33
volt, konfliktus nélkül: a forrás már tartalmazta a cél teljes történetét.

Az UVCS csak üres ágat enged törölni a changesetek törlése nélkül, és a
changesetek áthelyezésének is üres célág kell. Ezért:

1. A régi mission cs:30 és cs:33 a
   `/main/codex-mission-engine-2022-archive` ágra került.
2. A 6.3-as cs:35, cs:36, cs:38 és cs:39 az eredeti
   `/main/codex-mission-engine` ágra került, meglévő azonosítóival és
   merge-kapcsolataival.
3. A tiszta 6.3-as workspace erre az ágra váltott.
4. A kiürült `/main/unity-6.3` ág törölve, az archív ág elrejtve.

Az archív ág ideiglenes megjelenítésével a cs:39 őseinek lekérdezése mind
a **40 changesetet (0–39)** visszaadta, köztük a cs:30 és cs:33 tételeket.
Az archív ág ezt követően ismét rejtett állapotba került. Nem történt
changesettörlés, újraimportálás vagy a fájlok újramentése.

## Ellenőrzés Unity 6.3 alatt

A projektet a telepített `6000.3.27f1` Editor fordította és ellenőrizte
batch módban. Mind az öt belépési pont **0 kilépési kóddal** végzett:

| Ellenőrzés | Eredmény |
| --- | --- |
| `Metin2ProtocolCodecValidator.RunFromCommandLine` | Protokoll és typed inventory/ground golden minták sikeresek |
| `Metin2OfflineShopRetirementValidator.Run` | 30 ellenőrzés sikeres |
| `Metin2ClientBootstrapValidator.RunFromCommandLine` | Kliensindítás ellenőrzése sikeres |
| `Metin2ClientSceneValidator.RunFromCommandLine` | Főjelenet és katalóguskapcsolatok sikeresek |
| `Metin2EquipmentPlayerSplitValidator.RunFromCommandLine` | Felszerelés- és játékosadatok ellenőrzése sikeres |

A naplók a szerverrepo figyelmen kívül hagyott `build` könyvtárában vannak:
`unity63-protocol-validation.log`, `unity63-offline-shop-validation.log`,
`unity63-bootstrap-validation.log`, `unity63-scene-validation.log`,
`unity63-equipment-validation.log`.

A branchműveletek után a workspace továbbra is cs:39-en áll, helyi
változás nélkül. A régebbi workspace-ek függő fájljairól a műveletek előtt
és után készült SHA-256 ellenőrzés: **15 + 10 142 útvonal, 0 eltérés**.
Ezeket a helyi módosításokat és a megnyitott régi Editort nem módosítottuk.
A régi workspace-ek selectorát sem írtuk át; a további fejlesztéshez a
fenti aktív Unity 6.3 workspace használható.

## Következő fejlesztés

A saját tárgy eldobásának C7d-1 natív/SQL alapja már kész, a játékbeli
szerver- és Unity-bekötése továbbra is a következő, közös C7d-2 munkaszelet.
A munkamenetet a gyökér `AGENTS.md` és az IC-034/035 terv rögzíti.
Ebben az ágösszevonásban helyi, bejelentkezett szerver–kliens játékpróba
nem történt; a fenti ellenőrzések nem helyettesítik azt.
