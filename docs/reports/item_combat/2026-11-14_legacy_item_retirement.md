# A rĂ©gi itemrendszer kivezetĂ©se â€” elsĹ‘ mĹ±kĂ¶dĹ‘ lĂ©pĂ©s

2026-11-16-i folytatás: a kizárólagos natív runtime, régi DB-cache és a 17 régi állapottábla kivezetése elkészült; [friss állapot és pontos korlátok](2026-11-16_native_item_retirement.md). Az alábbi szöveg a korábbi checkpoint történeti állapota.

2026-11-16-i folytatás: a kizárólagos natív runtime, régi DB-cache és a 17 régi állapottábla kivezetése elkészült; [friss állapot és pontos korlátok](2026-11-16_native_item_retirement.md). Az alábbi szöveg a korábbi checkpoint történeti állapota.

Az Ăşj rendszerre mĂˇr ĂˇtĂˇllĂ­tott karakter most kizĂˇrĂłlag a tĂ­pusos
tĂˇrgyĂˇllapotĂˇt hasznĂˇlja. Ez a lĂ©pĂ©s a kĂ©t runtime keveredĂ©sĂ©t szĂĽnteti meg;
a teljes rĂ©gi rendszer Ă©s a rĂ©gi tĂˇblĂˇk eltĂˇvolĂ­tĂˇsa mĂ©g nem fejezĹ‘dĂ¶tt be.
A UI megjelenĂ©se nem vĂˇltozott.

FelhasznĂˇlĂłi dĂ¶ntĂ©s: az Ă¶sszes rĂ©gi fiĂłk, karakter Ă©s hozzĂˇjuk tartozĂł
tĂˇrgyĂˇllapot tĂ¶rlendĹ‘; ezekbĹ‘l semmit nem viszĂĽnk Ăˇt az Ăşj rendszerbe.
EzĂ©rt a rĂ©gi karakterek konverziĂłja nem rĂ©sze a tovĂˇbbi ĂˇtĂˇllĂˇsi tervnek.
A felhasznĂˇlĂł kĂĽlĂ¶n engedĂ©lyezte az azonnali tĂ¶rlĂ©st kizĂˇrĂłlag a helyi
szerveren. A `D:/AurigaGlobal/Server2-Local` sajĂˇt, `127.0.0.1:3307`
MariaDB-pĂ©ldĂˇnyĂˇban 117 ĂˇllapottĂˇbla kiĂĽrĂĽlt: 3704 fiĂłk, 416 karakter,
3216 rĂ©gi tĂˇrgysor Ă©s 1516 natĂ­v pĂ©ldĂˇnysor tĂ¶rlĹ‘dĂ¶tt a kapcsolĂłdĂł
jĂˇtĂ©kosadatokkal egyĂĽtt. A rĂ©gi cĂ©hjelvĂ©nyek a 14 helyi runtime/default
jelvĂ©nykĂ¶nyvtĂˇrbĂłl Ă©s kĂ©t szimbĂłlumfĂˇjlbĂłl is eltĹ±ntek.
ĂšjraindĂ­tĂˇs utĂˇn a jĂˇtĂ©kosĂˇllapot tovĂˇbbra is ĂĽres; a helyi stack fut.
A katalĂłgus- Ă©s vilĂˇgdefinĂ­ciĂłk megmaradtak. RĂ©szletes bizonyĂ­tĂ©k:
[helyi adatĂĽrĂ­tĂ©s](2026-11-14_local_player_reset.evidence.json).

## ElkĂ©szĂĽlt

- A rĂ©gi DB-leltĂˇrcsomag megvĂˇrja az aszinkron tulajdonosfeloldĂˇst. NatĂ­v
  tulajdonosnĂˇl csak a session betĂ¶ltĂ©si kapuja teljesĂĽl: nem keletkezik
  rĂ©gi tĂˇrgyentitĂˇs, felszerelĂ©s, affect vagy visszamentĂ©s. A megmaradt rĂ©gi
  SQL-sorokat a betĂ¶ltĂ©s Ă©rintetlenĂĽl hagyja. A mĂˇsik Ă©rkezĂ©si sorrend is
  ugyanide vezet; kijelentkezĂ©skor a tokenhez kĂ¶tĂ¶tt fĂĽggĹ‘ callback elvĂ©sz.
- A szerver 25 rĂ©gi item-/bolt-/csere-/raktĂˇr-/fejlesztĂ©si csomagtĂ­pust
  elutasĂ­t natĂ­v vagy mĂ©g feloldĂˇsra vĂˇrĂł karakternĂ©l. A kapcsolat lezĂˇrul;
  vĂˇltozĂł hosszĂşsĂˇgĂş csomag maradĂ©ka sem hajthat vĂ©gre Ăşj jĂˇtĂ©kparancsot.
- A rĂ©gi tĂˇrgybehelyezĂ©si, felszerelĂ©si, automatikus jutalom- Ă©s felvĂ©teli
  belĂ©pĂ©si pontok szintĂ©n tiltottak ezeknĂ©l a karaktereknĂ©l. A mĂ©g Ăˇt nem
  Ă­rt vnumos jutalom nem hozhat lĂ©tre egy mĂˇsodik, rejtett leltĂˇrt.
- A Unity ugyanazokat a rĂ©gi csomagokat elkĂĽldĂ©s elĹ‘tt elutasĂ­tja, a
  kapcsolati sorszĂˇm elhasznĂˇlĂˇsa nĂ©lkĂĽl. A natĂ­v kĂ©rĂ©s ezt kĂ¶vetĹ‘en is
  mĹ±kĂ¶dik. A csomaglista egyezĂ©sĂ©t kĂ¶zĂ¶s protokollteszt ellenĹ‘rzi.
- A fĂ¶ldi felvĂ©tel kĂ¶zĂ¶s VID-csomagja megmaradt: a tĂ­pusos loot tovĂˇbbra
  is felvehetĹ‘; a rĂ©gi tĂˇrgyrendszerbe tĂ¶rtĂ©nĹ‘ visszaesĂ©s tiltott.

A kĂłdvĂˇltoztatĂˇs nem vĂ©gzett globĂˇlis backendvĂˇltĂˇst vagy adatkonverziĂłt.
A kĂĽlĂ¶n engedĂ©lyezett helyi adatĂĽrĂ­tĂ©s a fent leĂ­rtak szerint elkĂ©szĂĽlt.
A natĂ­v Ăˇllapot nĂ©lkĂĽli karakterekhez az explicit Ăˇtmeneti Ăşt a kĂłdban
mĂ©g megmaradt. A `catalog=off` konfigurĂˇciĂł kivezetĂ©se is tovĂˇbbi feladat.

## Az indulĂł helyi adatĂˇllapot

Az alĂˇbbi tĂ¶rtĂ©neti, adatĂĽrĂ­tĂ©s elĹ‘tti adatok az e munkĂˇban lĂ©trehozott kĂ©t
kĂĽlĂ¶n tesztkaraktert nem tartalmazzĂˇk. Ezeket a jĂˇtĂ©kosadatokat azĂłta tĂ¶rĂ¶ltĂĽk.

| Adat | MennyisĂ©g |
|---|---:|
| RĂ©gi tĂˇrgysor | 3214 |
| RĂ©gi tĂˇrgyak darabszĂˇma | 68079 |
| RĂ©gi tĂˇrgyakkal rendelkezĹ‘ tulajdonos | 216 |
| KĂĽlĂ¶nbĂ¶zĹ‘ tĂˇrolt rĂ©gi vnum | 411 |
| NatĂ­v karakterfence nĂ©lkĂĽli karakter | 235 |
| NatĂ­v karakter mellett megmaradt rĂ©gi sor | 72 sor / 4 tulajdonos |
| NatĂ­v katalĂłgusdefinĂ­ciĂł | 1574 |
| NatĂ­v mobdrop-bejegyzĂ©s | 0 |

A helyi konfigurĂˇciĂł `items.catalog=required`; a `drops` nincs megadva,
ezĂ©rt mĂ©g a `legacy` alapĂ©rtĂ©k aktĂ­v. Az ĂĽres natĂ­v drop-tĂˇblĂˇra tĂ¶rtĂ©nĹ‘
egyszerĹ± ĂˇtkapcsolĂˇs megszĂĽntetnĂ© a rĂ©gi vilĂˇgdropokat.

Az ĂˇtnĂ©zett C44 konverziĂłs tervbĹ‘l Ă©s a jelenlegi katalĂłgusbĂłl 277 tĂˇrolt
forrĂˇs-vnumhoz talĂˇlhatĂł hasznĂˇlhatĂł megfeleltetĂ©s. 134 tovĂˇbbi forrĂˇs-vnum
(723 sor) nincs lefedve ezzel a tervvel; kĂ¶ztĂĽk valĂłdi tartalom Ă©s korĂˇbbi
tesztprototĂ­pusok is vannak. A rĂ©gi jĂˇtĂ©kosadatok tĂ¶rlĂ©sĂ©rĹ‘l szĂłlĂł dĂ¶ntĂ©s
miatt ezeket a pĂ©ldĂˇnyokat nem kell importĂˇlni. A vilĂˇg tartalmi
definĂ­ciĂłinak Ă©s drop-/bolt-/questhivatkozĂˇsainak ĂˇtĂˇllĂ­tĂˇsa kĂĽlĂ¶n feladat.

## A teljes kivezetĂ©s fennmaradĂł kapui

1. **Helyileg elkĂ©szĂĽlt:** teljes jĂˇtĂ©kosadat-ĂĽrĂ­tĂ©s: minden rĂ©gi fiĂłk,
   karakter, rĂ©gi Ă©s natĂ­v tĂˇrgypĂ©ldĂˇny, pĂ©nztĂˇrca, megosztott tĂˇrolĂł,
   tĂˇrsĂˇllapot, guild-/social-/chat-/quest-/mission-/ranglistaadat Ă©s
   jĂˇtĂ©koshoz kapcsolĂłdĂł importarchĂ­vum/nyugta. Sem rĂ©gi karaktert, sem
   rĂ©gi tĂˇrgyĂˇllapotot nem migrĂˇlunk Ăˇt. A katalĂłgus Ă©s a vilĂˇg definĂ­ciĂłi
   nem karakterhez tartozĂł tĂˇrgypĂ©ldĂˇnyok.
2. VilĂˇgdropok, NPC-boltok, receptek, rĂ©gi Lua-questek tĂˇrgyfeltĂ©teleinek,
   fogyasztĂˇsainak Ă©s jutalmainak ĂˇtĂ­rĂˇsa. A letiltott rĂ©gi jutalomutak
   ettĹ‘l mĂ©g nem vĂˇlnak tĂ­pusos jutalommĂˇ; ezeket a tartalmakat kĂĽlĂ¶n
   Ăˇt kell ĂˇllĂ­tani Ă©s jĂˇtĂ©kban ellenĹ‘rizni.
3. Ăšj karakter lĂ©trehozĂˇsa Ă©s kezdĹ‘felszerelĂ©se kizĂˇrĂłlag natĂ­v Ăˇllapottal;
   a hiĂˇnyzĂł tulajdonoshoz vezetĹ‘ fallback eltĂˇvolĂ­tĂˇsa.
4. A globĂˇlis `drops=typed` Ă©s kĂ¶telezĹ‘ natĂ­v backend aktivĂˇlĂˇsa csak a
   tartalom- Ă©s tulajdonosellenĹ‘rzĂ©sek utĂˇn.
5. Ezt kĂ¶vetĹ‘en a rĂ©gi Database-cache/save/load, GameServer CItem/vnum/
   APPLY-Ăˇgak, Unity backendkapcsolĂł Ă©s rĂ©gi itemkĂĽldĹ‘k eltĂˇvolĂ­tĂˇsa.
   A kĂ¶zĂ¶sen hasznĂˇlt vilĂˇg-/chat-/actor-csomagok kĂĽlĂ¶n megĂ­tĂ©lendĹ‘k.
6. A rĂ©gi aktĂ­v itemtĂˇblĂˇk fizikai kivezetĂ©se. A korĂˇbbi jĂˇtĂ©kosadatok
   importarchĂ­vuma sem marad rĂ©sze az Ăşj jĂˇtĂ©kĂˇllapotnak.

## EllenĹ‘rzĂ©s

- MSVC Release GameServer-fordĂ­tĂˇs a projekt helyi toolchainjĂ©vel.
- Ă–t natĂ­v teszt: `legacy_item_boundary` (70 ellenĹ‘rzĂ©s), `item_runtime`,
  `quickslots`, `typed_ground`, `typed_inventory_wire`.
- KĂ©t Python protokollteszt; a nyugdĂ­jazott fejlĂ©cazonosĂ­tĂłk a szerver
  deklarĂˇciĂłival Ă©s az aktĂ­v Unity-munkatĂ©r szabĂˇlyĂˇval is egyeznek.
- Unity 6000.3.27f1 fordĂ­tĂˇs, `item-boundary-offline` Ă©s tĂ­pusos
  item-integrĂˇciĂłs validator.
- Helyi TLS-jĂˇtĂ©kprĂłba kĂ©t Ăşj, dedikĂˇlt fiĂłkkal. MindkettĹ‘n egyszerre van
  rĂ©gi felszerelĂ©si forrĂˇssor Ă©s natĂ­v anyag/pĂ©nztĂˇrca: natĂ­v betĂ¶ltĂ©s,
  mozgatĂˇs, helyi rĂ©gi kĂ©rĂ©s elutasĂ­tĂˇsa, kĂ¶vetkezĹ‘ natĂ­v kĂ©rĂ©s,
  rĂ©szleges eldobĂˇs, kĂ¶zĂ¶s felvĂ©tel, kijelentkezĂ©s Ă©s ĂşjrabelĂ©pĂ©s.
- A kliensvĂ©delmet megkerĂĽlĹ‘ nyers rĂ©gi hasznĂˇlati Ă©s vĂˇltozĂł hosszĂşsĂˇgĂş
  sajĂˇtbolt-csomag kĂĽlĂ¶n ellenĹ‘rzi a szerveroldali lezĂˇrĂˇst.
- LeĂˇllĂ­tott stack mellett SQL-ujjlenyomat igazolja a rĂ©gi tesztsorok,
  minden mĂˇs tulajdonos natĂ­v tĂˇrgyainak Ă©s pĂ©nztĂˇrcĂˇinak vĂˇltozatlansĂˇgĂˇt,
  a tesztkarakterek 20 darab anyagĂˇt Ă©s a fĂ¶ldi escrow kiĂĽrĂĽlĂ©sĂ©t.

A teljes vilĂˇgdrop-, rĂ©gi quest-, bolti Ă©s tĂ¶bbkarakteres tartalomkonverziĂł,
valamint FreeBSD/standalone/terhelĂ©si regressziĂł e lĂ©pĂ©sben nem lett
ellenĹ‘rizve. RĂ©szletes eredmĂ©ny: az azonos nevĹ± `.evidence.json` fĂˇjl.
