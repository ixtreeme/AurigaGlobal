# Kategóriás itemmodell — profilból vezérelt fejlesztési maximum

2026-10-27. A felhasználói pontosítás alapján a fő cél a kategóriabontás és a
rögzített +9-es plafon megszüntetése. A további munka sorrendjét az
[IC-006 döntés](../../adr/item_combat_006_category_refinement_priority.md) rögzíti.

Az új domain `NextRefinementLevel` függvénye a tárgy fejlesztési profiljából
oldja fel a következő szintet. Nincs +9-es külön elágazás vagy prototípuscsere.
Az aktuális profilon túli szint hibát ad; a maximum elérése külön eredmény.
A meglévő +9-es referenciafixture továbbra is szabályos adat, nem globális plafon.

Egyetlen fegyverprototípus, ugyanaz az ItemKey a tesztben:

| Fejlesztés | Fizikai alap+növekmény | Mágikus alap+növekmény |
|---|---:|---:|
| +0 | 120 | 30 |
| +9 | 210 | 66 |
| +10 | 220 | 70 |
| +15 | 270 | 90 |
| +30 | 420 | 150 |

Ezek értelmezési tesztadatok, nem jóváhagyott balance. A páncél ugyanebben a
katalógusban külön +15-ös maximumot kap; nem a fegyver +30-as határát használja.
Hiányzó +10-es lépcsővel a katalógus publikálása sikertelen. A 0..max görbének
teljesnek kell lennie. Az aktuális technikai profiltartomány 0..255.

Friss helyi ellenőrzés: **8/8 célzott normál CTest, 7/7 az ASAN buildből**.
Az SQL suite **10/10** esetet futtatott saját ideiglenes MariaDB-példányban.
A natív suite-okban 451 refinement-, 96 katalógus-, 229 bónusz- és 197442
ItemKey-ellenőrzés sikeres. A refinery suite minden szintlépést végigjár
+15, +30, +100 és +255 maximumú profilnál, köztük a +9→+10 átmenetet.
A digest változatlan v2 fixture-jét a független Python encoder továbbra is igazolja.

A részletes naplók a `build/item-refinement-20261027/` alatt vannak,
a forrás- és tesztlelet a jelentés melletti evidence JSON-ban.

Ez a következő szint és a hozzá tartozó statok domain-/katalógusbizonyítéka.
A kész fejlesztési tranzakció, item-instance mentés, újratöltés és Unity
megjelenítés még hátravan. A futó szerver +9-es útvonalát ezzel még nem cseréltük le.
Csak helyi kód/build és saját ideiglenes DB-próba történt; meglévő játék-DB-n,
Unityben vagy távoli gépen nincs módosítás.
