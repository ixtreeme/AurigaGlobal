# A félretett küldetéses petpecsétek újratervezése

**Megvalósítás jóváhagyva. A helyi kiadás külön tervezetben vár jóváhagyásra. 2026. november 25.**

A javaslat szerint a játékos a leltárból egyetlen pecsétet vált be egy típusos, megidézhető petre. A forráspéldány elfogyasztása és a jutalom egy közös tranzakció. Megismételt kéréskor ugyanaz az eredmény jár.

A régi küldetés 29 bejegyzéshez ad lényazonosítót. Az időtartamok és a hatások az egykori Lua-megjegyzések alapján tett **javaslatok új példányokra**. Az eredeti tárgyprototípusok hiányoznak. Új példánynál az idő az első használatkor indulna. Az új kiosztás külön tartalmi döntést igényel.

| Javasolt pet | Régi pecsét | Lény | Új példány ideje | Javasolt hatás | Unity-megjelenés |
| --- | ---: | ---: | ---: | --- | --- |
| Sarkvidéki Kán | 53019 | 34019 | 30 nap | 1500 TP, 15% sebzés | Prefab megtalálva |
| Sarki Porky | 53020 | 34017 | 30 nap | 1500 TP, 15% sebzés | Prefab megtalálva |
| Fagyos Rufusz | 53021 | 34018 | 30 nap | 1500 TP, 15% szörnyek ellen | Prefab megtalálva |
| Sheldon | 53022 | 34021 | 30 nap | 1500 TP, 15% sebzés | Prefab megtalálva |
| Cooper | 53023 | 34022 | 30 nap | 1500 TP, 15% sebzés | Prefab megtalálva |
| Párduckölyök | 53024 | 34023 | 7 nap | 1500 TP, 5% átható találat, 5% készségellenállás | Prefab megtalálva |
| Párduckölyök | 53218 | 34023 | 30 nap | 1500 TP, 5% átható találat, 5% készségellenállás | Prefab megtalálva |
| Párduckölyök | 53219 | 34023 | 120 nap | 1500 TP, 5% átható találat, 5% készségellenállás | Prefab megtalálva |
| Leopárdkölyök | 53220 | 34024 | 30 nap | 1500 TP, 5% kritikus találat, 10% átlagkár-ellenállás | Prefab megtalálva |
| Leopárdkölyök | 53221 | 34024 | 120 nap | 1500 TP, 5% kritikus találat, 10% átlagkár-ellenállás | Prefab megtalálva |
| Tökfej | 53222 | 34026 | 30 nap | 1500 TP, 15% sebzés | Prefab megtalálva |
| Kis seprű | 53223 | 34027 | 30 nap | 1500 TP, 15% sebzés | Prefab megtalálva |
| Hópehely | 53224 | 34028 | 30 nap | 1500 TP, 15% sebzés | Prefab megtalálva |
| Száncsengő | 53225 | 34029 | 30 nap | 15% sebzés, 5% kritikus sebzés, 15% mágikus sebzés | Prefab megtalálva |
| Cupido | 53226 | 34030 | 120 nap | 1500 TP, 5% átható találat | Prefab megtalálva |
| Tomboló szív | 53227 | 34031 | 120 nap | 1500 TP, 5% kritikus találat | Prefab megtalálva |
| Kék holdsarló | 53228 | 34033 | 30 nap | 1500 TP, 15% fizikai és mágikus sebzés | Prefab megtalálva |
| Piros holdsarló | 53229 | 34032 | 30 nap | 5% kritikus találat, 15% fizikai és mágikus sebzés | Prefab megtalálva |
| Eweriel | 53230 | 34034 | 30 nap | 1500 TP, 5% átható találat | Prefab megtalálva |
| Ramblue | 53231 | 34035 | 30 nap | 1500 TP, 5% kritikus találat | Prefab megtalálva |
| Baby Huashin | 53232 | 34039 | 30 nap | 1500 TP, 5% átható találat | Prefab megtalálva |
| Bruce | 53233 | 34055 | 30 nap | 1500 TP, 5% kritikus találat, tárgyfelvétel | Prefab megtalálva |
| Wayne | 53234 | 34056 | 30 nap | 1500 TP, 5% átható találat, tárgyfelvétel | Prefab megtalálva |
| Robin | 53235 | 34057 | 1 nap | 1500 TP, 5% kritikus találat, tárgyfelvétel | Prefab megtalálva |
| Valentino | 53236 | 34060 | 30 nap | 1500 TP, 15% szörnyek ellen, csokoládéamulettel 50% tapasztalatbónusz | Prefab megtalálva |
| Valentinia | 53237 | 34061 | 30 nap | 1500 TP, 5% kritikus találat, csokoládéamulettel 50% tapasztalatbónusz | Prefab megtalálva |
| Holdlámpás (Kék) | 53240 | 34063 | 30 nap | 1500 TP, 5% kritikus találat | Eredeti modell és textúra ellenőrizve |
| Holdlámpás (Narancs) | 53241 | 34062 | 30 nap | 1500 TP, 15% szörnyek ellen | Eredeti modell és textúra ellenőrizve |
| Mergezett Szellem Sarkany | 53284 | 34116 | 3600 nap | Bónusz nélküli társ | Prefab megtalálva |
| Választható petcsere | 53297 | – | Meglévő lejárat | A kiválasztott típus szerint | A listában elérhető modell |

A két Holdlámpás az eredeti animált modellt és a hozzá tartozó kék, illetve narancs textúrát kapta. Az 53297-es pecséthez csak használati esemény tartozik, a régi küldetésben hiányzik a lény. Ehhez egyszeri választást javaslunk a megjeleníthető típusok közül, a választott pet nevének és hatásának előzetes megjelenítésével. Megszakításkor a pecsét a leltárban marad. Bruce, Wayne és Robin a típusos zsákmányfelvételt használja. Valentino és Valentinia +50% tapasztalatbónuszának feltétele a felszerelt csokoládéamulett. Az amulett jelenleg hiányzik a helyi típusos katalógusból.

**Meglévő játékostárgyak.** A mentésből egyeztetni kell a tulajdonost, a tárgypéldányt, az elhelyezést, a kötést, a bónuszokat, az óra típusát és az abszolút lejáratot vagy a hátralévő viselési időt. A már lejárt példány lejárt marad. Ismeretlen állapot esetén a beváltás várakozik, az eredeti tárgy megmarad. A fenti napok kizárólag új példányokra vonatkoznak.

**Jóváhagyás tárgya.** A 29 típus időtartama és bónuszai, a két különleges hatás, a Holdlámpás-modellek pótlása és a választható csere. Kiadás előtt minden érintett típusnál ellenőrizni kell a szerver bónuszazonosítóit, a megjelenést, az egyszeri kiosztást és a lejárat megőrzését.

A megfeleltetések forrása a szerver pet_system.lua küldetése, a Unity magyar lénynevei és a Races/Prefabs modelljegyzék.
