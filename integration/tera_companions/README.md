# TERA társak és hátasok

C26a a szerver `codex/mission-engine` és a Unity `/main/codex-mission-engine` ágán. A tartalom forrása az `ECS_+_BGFX` kiadása és az AurigaUnity `/main` cs52 assetjei.

| Tartalom | Pecsét VNUM | Modell race |
| --- | --- | --- |
| 203 hátas | 630000–630202 | 32000–32202 |
| 69 pet | 631000–631068 | 32500–32568 |

A `release.json` rögzíti a megfeleltetést és a 19 kiegészítő modell 33 csatolását. A Unity eredeti GUID-okkal tárolja a prefabokat, mozgásokat, anyagokat, effekteket és hangokat, tíz nyelvű nevekkel és pecsétleírásokkal. A 756 hanggráf az eredeti súlyozást, ismétlést és távolságfüggést használja. A `particle-mesh-fix.json` a hat korábban összevont Kirin részecskemesh adatait tartalmazza.

## Helyi telepítés és próba

1. Állítsd le a játékszervert és mentsd az adatbázist. A `prototypes.sql` egyszeri tranzakcióban szúrja be a 272 új sort mindhárom táblába. Azonosítók ütközésekor vizsgáld meg a meglévő rekordokat. A futtatás sorrendjét a [repülés leírása](../mount_flight/README.md) adja meg.
2. Másold a `tera_companions.lua` fájlt a `data/quest/Quest/system` alá, regisztráld egyszer `Quest/system/tera_companions.lua` néven a `data/quest/quest_list` fájlban, majd futtasd a csomag `manage.py apply-quests` parancsát.
3. Indítsd a párosított szervert és a Unity 6.3 (`6000.3.27f1`) projektet. A hátaspecsét a jelmezes hátashelyre kerül. A petpecsét használata megidézi, újbóli használata visszahívja a petet. A meglévő quest egyszerre egy aktív klasszikus petet enged, és megőrzi a pecsétet.

Az ellenőrzéshez futtatható a `python tests/server/TeraCompanionReleaseTests.py` és a kliens `Metin2TeraCompanionPortValidator` ellenőrzője. A Unity tartalomszerkesztő újragenerálásához a `release.json`, `approved-settings.json` és `audio-cues.json` a kliens `Migration/TeraCompanions` könyvtárába kerül. A szerver- és kliensoldali működés az [IC-058](../../docs/adr/item_combat_058_tera_companion_port.md) leírásában követhető.
