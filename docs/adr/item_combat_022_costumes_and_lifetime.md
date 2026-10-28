# ADR IC-022 — Hagyományos kosztümök, megjelenési változatok és élettartam

2026-10-28. Állapot: az IC-012 F2 részeként megvalósítva, helyi tesztelés.
Kezdő Git HEAD: `da184bd4`, ág: `codex/mission-engine`.

## Kategória és megjelenés

A 4-es kategória `ItemCostumeRow` sora test-, haj- vagy fegyverkosztümöt ír le.
A fegyverkosztümhöz kötelező a típusos, ténylegesen felszerelt fegyver altípusa;
testhez és hajhoz ez NULL. A közös `WeaponSubtype` egyetlen definíciója a
`WeaponTypes.hpp` fejlécben van. A kosztüm nem halmozható.

A `CostumeVisualRow` és `protos.item_costume_visual` természetes kulcsa:
`(category_id, local_id, character_class, sex)`. Minden használati policy által
engedélyezett osztály/nem kombinációhoz kell modell. Egyéb, szerkezetileg érvényes
változat megmaradhat, így a policy szűkítése nem töröl modellt. Hiányzó modellnél
nincs másik osztályra vagy régi vnumra épülő fallback.

A base appearance továbbra is külön, opcionális profil (például földre tett
tárgy). A felszerelt kosztüm megjelenését az explicit változatsor adja.
Az editor modellútvonalai szerkesztői hivatkozások; a tényleges Unity assetek
létezésének és szemantikájának ellenőrzése a konverter publikálási kapuja.

A `RefinementKind::Costume` csak szintbónuszt hordozhat, nyers fegyver- vagy
páncélalapértéket nem. Egy prototípus marad, a maximum 0..255 közötti profiladat;
a tesztben ugyanaz az ItemKey +30-ig használható. Az aktuális szint kumulatív
bónuszprofilját vesszük, korábbi szintek profiljait nem adjuk még egyszer hozzá.

## Élettartam és írási felelősség

Négy explicit óra: korlátlan; létrehozástól; első használattól; csak viselés
közben fogyó idő. A definícióban milliszekundumos időtartam, az instance-ban
elindult/lejárt jelző és abszolút UTC-határidő vagy fennmaradt viselési idő van.
Az értékek felső határa INT64_MAX; összeadás előtt overflow-ellenőrzés történik.

A `MakeCostumeLifetime` kizárólag új instance gyártására használható.
Betöltéskor és belépéskor a tartós állapotot kell visszaadni, különben újratöltődne
az élettartam. A `AdvanceCostumeLifetime` tiszta tervet ad, órát nem olvas.
UTC-határidőnél `now >= deadline` lejárat; a már commitolt lejárt jelzőt későbbi
óravisszaállítás sem oldja fel. Viselési időnél a szerver monoton eltelt idejét
csak az előző intervallumban viselt tárgyról vonjuk le, nulláig telítve.

A kiadott instance hátralévő idejét a prototípus időtartamának módosítása nem
tölti fel és nem csonkolja. Óratípus megváltoztatásakor a régi állapot átírása
külön migráció; hibás állapotból nincs automatikus újragyártás.

**A régi első használati szabály külön figyelmet igényel.**
`ItemSystem_LegacyBridge.cpp::UseItemEx` az első használati időzítőt a későbbi
felszerelési eredmény előtt indítja el. Például hiányzó valódi fegyvernél is
elindulhat. Az új item-use ownernek ezért a befogadott használat élettartamtervét
akkor is commitolnia kell, ha a felszerelési terv később elutasít. Ezt külön
teszt bizonyítja: az első kísérlet határidejét a későbbi sikeres felvétel megőrzi.
A tiszta felszerelési planner nem helyettesíti ezt az owner-útvonalat.

A `PlanCostumeEquip` ellenőrzi a szerveres befogadást, katalógust, kivezetést,
halmot, osztályt/nemet, minimumszintet, fejlesztést, policy-t és valódi fegyvert.
Birtokolt modell/hátralévő állapot tervet és katalógusrevisiont/hasht ad.
Lejárt tárgynál sikeresen visszaadott, `equip=false` eltávolítási terv őrzi meg
a lejárt jelzőt. Hibaként eldobott lejárat óravisszaállításkor újjáéledést okozna.

Ez nem ECS-, felszerelés- vagy SQL-író. Az F3/F4 owner feladata marad a konkrét
instance/revision, session, tulajdon, hely, zárolás, csere, displaced slot és
az atomi commit. A régi játékútvonalat ebben az F2 lépésben nem kapcsoljuk át.

## Formátum és konverziós határ

A natív séma v15, kanonikus tartomány `auriga.item_catalog.v15`, két új
hash-szekcióval (32: definíciók, 33: változatok). A teljes friss létrehozási
lánc 000..021. Ez nem a telepített v7-en futtatható helybeni frissítő.
A megőrzendő v7 felhasználói katalógus offline exportból, új saját teszt-DB-n
lett ellenőrizve; az aktív 33 táblás v7 és a telepített app érintetlen.

A forrás 1392 kosztümsorából 917 hagyományos (358 test, 415 haj, 144 fegyver),
454 más viselkedésű (mount, acce, stole, skinek, effektek, rúnák), 21 altípusa
pedig a jelenlegi enumon kívüli. Utóbbiakat nem tesszük működő hagyományos
kosztümmé, és nem töröljük. A Rune rendszer nem DragonSoul, továbbra is megmarad.

A régi `LIMIT_UNIQUE_UNLIMITED` 1-es értéke jelölés, nem egymásodperces
időtartam. A 917 sorból 96 ilyen, 146 simán korlátlan, 424 létrehozási,
251 első használati idővel rendelkezik. 116 sor nem nulla apply-t hordoz;
ezek teljes bonus/policy/ár/extra-proto/asset és instance-konverziója még kapu.
Egyik auditált sor sincs pusztán ettől a munkától konvertáltnak minősítve.
