# IC-069 — Tartós Mission- és Lua-quest item-integráció (C34)

## Közös típusos gazdasági út

A C12 egyszeri NPC-s tárgyleadása megmarad. Mellette a meglévő natív
`MissionRuntime` általános gráfja most élő, karakterhez kötött adaptert kap:
indítás, részhaladás, NPC-cél, választás, határidő, jutalom és megszakítás
közös szerver/Unity-útvonalon működik. A checkpoint, definícióarchívum,
outbox és entitlement a már meglévő, külön `mission` adatbázist használja.
A szezonális Mission Pass ettől továbbra is külön rendszer.

A reward payload **2-es sémája** kizárólag explicit `category/local/level`
kulcsokat használ. Három listája: `typed_requirements` (megmaradó feltétel),
`typed_consume_items` (költség), `typed_items` (jutalom); a Yang-költség
`gold_cost`, a jutalom `gold`. A vnumos 1-es séma bytepontos formátuma és
lenyomatai megmaradnak, de az új élő adapter nem fogadja el őket. A régi
resource-writer szintén elutasítja a 2-es payloadot, így nem hagyhatja
figyelmen kívül annak típusos költségeit. Nincs vnum-csomagolás vagy
automatikus prototípus-konverzió. Az új kötelező natív típusok: 0x5005–0x5008.

Egy listában legfeljebb 16, összesen 32 tétel lehet, explicit finomítási
szinttel, tételenként legfeljebb 50000 darabbal. A katalógus és a gyártható
persistent állapot induláskor ellenőrzött. A karakter saját, alapállapotú
inventory-tárgyai használhatók; felszerelt, aktív, egyedi bónusz-/foglalat-
állapotú vagy más karakterhez kötött példány nem fogyasztható automatikusan.

A konkrét `ActionKey` pontos archivált definícióhoz és payloadhoz kötött.
Az adapter ebből állítja elő a natív `ItemMission` szerződést. A költséget
nem finanszírozhatja ugyanannak az akciónak a jutalma. **Egy InnoDB-
tranzakció** írja az entitlementet, a tárgyakat és Yangot, az item receiptet,
valamint a reward node nyugtázott checkpointját. Késői SQL-hiba mindent
visszagörget. Elveszett COMMIT-válasznál a koordinátor a tartós Mission-save
operationt zároló olvasással egyezteti; nem ad jutalmat az ECS állapotából.
A kliens és Lua kizárólag rendezett eredményt kap.

## Session és haladás

A typed karakterbetöltés után az adapter külön Mission-owner epochot és
tokent foglal. A claim és minden Mission-írás az aktuális typed item-login
generációját is SQL-ben ellenőrzi. Régi belépés nem veheti vissza a Mission
írót és nem menthet új állapotot. Callback csak az újra megkeresett,
azonos PID/login/session komponensre alkalmazhat eredményt.

A checkpoint az exact régi definíciót az adatbázis csomagarchívumából is
visszatöltheti; nincs latest-version fallback. A döntési runtime csak új
eseménynél vagy lejárt határidőnél épül újra. Tétlen tickben nincs teljes
küldetésgráf-bejárás; az itemesemény-poll indexelt, legfeljebb 32 sort olvas.

A `021_mission_events.sql` két additív táblát ad. Az első Mission-indítás
bekapcsolja a karakter itemesemény-figyelését. Az elfogadott natív Grant és
a másik tulajdonostól felvett földi tárgy növekménye az eredeti item-
tranzakcióban kap gyűjtési outboxsort. Saját tárgy eldobása/visszavétele nem
ad új mennyiséget. A crafting `CRF v2` receiptje explicit eredménykulcsot,
szintet és gyártott darabszámot is tárol; csak a sikeres kimenet számít,
azonos bemeneti/kimeneti tárgynál is. Replay és rollback nem ír új eseményt.

Az itemesemény feldolgozott jelzője és az abból származó checkpoint közös
tranzakció. Nincs commit-sorrendtől függő „legnagyobb ID” kurzor: a később
commitoló kisebb ID is megtalálható. Az eredeti SQL-időpont megőrzi, melyik
már aktív célhoz tartozhatott az esemény. A harci/NPC callback a következő
checkpoint-commitkor válik tartóssá; a kliens ezt sem jósolja meg.

A bekötött események: typed lootgyűjtés, crafting, szerver által jóváírt
mob-/bossölés, valódi NPC-kattintás vagy naplóbeli NPC-művelet, illetve az
aktív, szerző által engedélyezett dialógusválasztás. Terület-, puzzle- és
escort-eseményhez még nincs élő adapter, ezért ilyen csomag induláskor
elutasított. XP/szint- és közvetlen fiókvaluta-jutalom nem része ennek az
item-adapternek. A banki rendezés és pénzjegyváltás nem lootgyűjtési esemény.

## Tartalom és Unity

Az engedélyező fájl `data/typed_mission_runtime.json`, 1-es formátummal:
egy helyi `.aurmission` csomagfájl és küldetésenként kód, verzió, kezdő/
átvevő NPC-faj és minimumszint. Hiánya kikapcsolja az új adaptert.
Legfeljebb 32 élő küldetés engedélyezhető. Indítás és átvétel valódi,
azonos térképen, 1000-es távolságon belüli NPC-t igényel. A napló olvasása
NPC nélkül is működik. A szerver item admissiont és függő írási védelmet
használ a jutalom teljes élettartamára.

Fordítható minták: `config/mission_engine_typed_items.example.json`,
`config/typed_mission_runtime.example.json`,
`config/typed_mission_quest.example.quest`. A mintakulcsokat a saját
katalógusban ténylegesen létező kulcsokra kell cserélni. Fordítás:
`MissionCompiler compile source.json typed_missions.aurmission`.

A típusos leltár **Mission napló** gombja külön ablakot nyit. Látható a
részhaladás, a megnyílt célok, az engedélyezett választások és a függő
jutalom feltételei/költsége/jutalma. Az Áttekintés után a Megerősítés a
szerver tartós eredményére vár. A két projekten közös új protokoll:

- **CG 202:** 216 byte + sequence; művelet, NPC VID, ablaknonce,
  kéréssorszám, 97-97 byte-os Mission-kód és választás.
- **GC 202 v1:** uint32 méret, státusz, nonce/sorszám/NPC, szűrt Mission-
  és node-állapotok, explicit típusos tételek és GC227 leírások.

A kliens nem küld gráfot, példánypayloadot, tárgydarabszámot vagy jutalom-
receiptet. Az ablak újrabelépéskor törlődik, majd szerverről áll helyre.

## Lua-quest API

- `pc.typed_item_count(category, local, level)`: használható darabszám;
  betöltetlen/függő itemállapotnál `nil`. Az akció ettől függetlenül friss
  SQL-snapshoton ellenőrzi a tényleges feltételeket.
- `pc.typed_mission_status(code)`: -1 betöltés/mentés, 0 még nincs példány,
  1 aktív, 2 jutalomra vár, 3 teljesített, 4 sikertelen/megszakított.
- `pc.typed_mission_action(code, "start"|"talk"|"claim"|"abandon")`:
  coroutine-t felfüggesztő, ugyanazt a natív utat használó művelet.
  Visszatérés: `true, "committed"`, vagy `false` és elutasítási ok.

A folytatás csak az aktuális karakter/login eredeti, még ugyanarra a
műveletre váró coroutine-jára történhet. Másik PC-blokk vagy nem yieldelhető
környezet elutasított. A Lua-kód nem állít elő saját jutalomlistát, és nem
kap sikerjelzést a puszta sorba állításra. A régi `give_item/count_item`
függvények és questforrások automatikus átírása a tartalomkonverzió része.
