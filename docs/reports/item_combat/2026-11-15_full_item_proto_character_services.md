# Teljes item protó: rangponttárgyak és kapcsolódó ládák

2026-11-15. **16 további forrássor**, a maradék **749 → 733**.
Összesen **9762 konvertált, 491 kivezetett, 733 blokkolt** a 10986 forrássorból.
A jelölt **6844 natív definíció**, **41-es katalógusformátum**.
Hash: `af5266e1b8292a94708290dd580cf8787bdd3d7179c9ea5e245a6fe82192dbfb`.

A hat új fogyóeszköz eredeti belső rangpontértékei:

| Forrás | Egyszeri összeg | Alsó határ | Felső határ |
| --- | ---: | ---: | ---: |
| 70102 Zen-bab | 5000 | 0 | 2500000 |
| 70100 Arany zen-bab | 50000 | 0 | 25000000 |
| 39032, 71107 Élet Gyümölcse | 20000 | 0 | 25000000 |
| 72100 Arany gyümölcs | 100000 | 0 | 25000000 |
| 72101 Királyok Gyümölcse | 300000 | 25000000 | 50000000 |

A játékban látható rangpont a belső érték tizede. A Unity ezzel a skálával mutatja
a jelenlegi rangot, a határokat és a tényleges jóváírást. A felső határ közelében
csak a hiányzó mennyiséget adja; a határon és az alsó határ alatt nem fogyaszt tárgyat.
A használat a tényleges leltárgombon és a szerver előnézetéhez kötött megerősítésen át működik.

A C5 fogyóeszköz új, explicit karakter-szolgáltatás kötést kapott, saját SQL táblával.
A 049-es katalógusmigráció és a natív publikáló pontosan egy működést enged tárgyanként.
A **BOK3** karaktercheckpoint opcionálisan rangot is birtokol; a régi BOK1/BOK2
adatok nem nulláznak rangot. A tárgy fogyása, a rang, a karakter résztvevőállapota és
az ismételt kérés nyugtája egy SQL tranzakció. A késői teljes karaktermentés rangmezője
ugyanazt a revíziós védelmet használja. A rangváltozások a függő checkpoint mögött várnak.
A meglévő **GC244 v3** válasz új, explicit szolgáltatásformátumot visz;
a korábbi könyv- és segédtárgyformátumok változatlanok.

Tíz láda jutalomhivatkozása is feloldódott: **30893, 38052, 38057, 50063, 50098,
50204, 71150, 71159, 71160, 78315**. Mind a tíz megnyílt a valódi tárgykezelőn.

Lefuttatott ellenőrzések:

- GameServer, Database, CatalogEditorTool, ItemBenchTool és a natív tárgykezelő fordítása;
  az érintett 48 meglévő tesztcél újrafordítása.
- **76 konverziós teszt**, **64/64 CTest legutóbbi állapota**,
  **5252 könyv-/karaktercheckpoint-ellenőrzés**, **280 könyvsegéd-ellenőrzés**.
  A kezdeti három hiba az új formátumhoz újrafordítandó teszteszközből,
  a bővített mentési mezőt nélkülöző tesztmintából és a régi katalógushashből jött;
  javítás után mindhárom érintett teszt sikeres. A négy csomagmintában kizárólag
  a 32 bájtos katalógushash változott, ezt bájtonként ellenőriztem.
- Saját MariaDB-n **597 tényleges aszinkron tárgykezelő-ellenőrzés**:
  alsó/felső határ, egy belső pontig csökkentett jóváírás, egyszeri debit,
  ismételt kérés, új munkamenet, késői régi karaktermentés és szándékos
  karakterrevízió-ütközés teljes visszagördülése.
- Unity **6000.3.27f1**, kétszeri sikeres futás a kapcsolódó meglévő csomagokkal;
  **54628 szolgáltatásellenőrzés**, **76 valódi szervercsomag-sorozat**,
  eredeti ikonok, valódi használati és ládanyitó gombok, minden TCP-darabolási
  pont és csonkolt csomag elutasítása.
- Pontos konverziós reprodukció. Az eredeti 1574 definíció **29204** sora és az
  előző jelölt **105196** sora változatlan jelentéssel megmaradt.

A felhasználói döntést követő külön merge-jegyzékben **542** hiányos/hibás eredeti
adatú sor vagy rájuk váró függőség marad. Ezek nem lettek kivezetve.
Ettől külön még **191** működési vagy szabálybeli blokkolás van: a teljes konverzió
még nem kész. Jegyzék: `config/item_combat/full_proto_merge_deferred.json`.

**Nincs telepítve a futó helyi játékba.** Teljes auth/TLS/bejelentkezés/ECS
világfolyamat és Addressables játékosbuild ezzel a jelölttel nem volt tesztelve.
Régi account-, karakter- vagy tárgyadat nem lett visszaállítva.

[Protójegyzék](2026-11-15_full_item_proto_character_services.csv) · [Gépi bizonyíték](2026-11-15_full_item_proto_character_services.evidence.json)
