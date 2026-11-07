# Fejlődő pet: a passzív képességek helyi harci próbája

A C26h szerver (`340ed66b`, Windows Release) párosított Unity 6.3
(`6000.3.27f1`) regressziója a `20261103` és
`/main/unity63-integration-20261103` ágakon.

Párosított Unity changeset: `cs:80`.

## A mérés módja

Az elkülönített Unity Editor öt valódi TLS-munkamenetet használ. Az első
karakter minden hatáshoz külön petet keltet, majd a típusos felület
képességkönyvével megtanítja az adott képességet. A másik négy karakter
harcos, nindzsa, Sura és sámán. Mindegyik saját fiókot, tartós tárgyakat
és szerver által számított statokat használ.

Minden képesség ugyanazt az öt állapotot vizsgálja:

1. Visszahívott pet: alapérték és tényleges találat vagy ölési jutalom.
2. Megidézett pet: az adott pecsét egyetlen, pontos rangértéke a GC231-ben,
   és a hozzá tartozó harci hatás.
3. Aktív újrabelépés: ugyanaz a pecsét, hatásérték és harci eredmény.
4. Visszahívás: üres petbónusz-forrás és visszaállt alapérték.
5. Visszahívott újrabelépés: az eredeti karakterstatok és harci eredmény.

Az aktív bónuszt a Unity **Harci statok** ablakában is ellenőrzi. A
találatokat a GC135-ből, az EXP-t és aranyat a tényleges karakterpont-
frissítésekből olvassa. A teszt a rendes támadási műveletet ismétli, ha a
belépés átmeneti védelme vagy a támadás időzítése miatt még nem jött találat.
A sebzés összehasonlítása legfeljebb két egész egység eltérést enged a
végső legacy szorzások és kerekítések miatt; a kritikus, blokkolt és
kikerült találati jelzőknek pontosan egyezniük kell.

A normál szörny, főellenfél, metinkő, közelharci támadó és íjász külön
ideiglenes prototípust kap. A `NOMOVE` szabály rögzíti a helyüket; a
közelharci és íjász AI-t a normál támadási kérés indítja. A beérkező
sebzés maga a szerver szokásos harci útján keletkezik. A jutalompróbák
egy külön, kis HP-jú szörny ismételt megölését használják.

A kritikus, blokkoló és nyílkikerülő tesztképesség 100%-os értékű. Az
aranypróba hat ölésenként mér: aktívan mind a hat ölésnek adnia kell
aranyat, visszahívva legalább egy kimaradt dobást követel meg. A teszt
saját katalógussorai külön tartalmi kódokat használnak.

## Futtató és adatvédelem

- `tests/server/PetPassiveUnityLiveFixture.py`: saját helyi csomag,
  leállított stack, teljes player/protos mentés, új tesztfiókok és
  katalógussorok. A korábbi katalógussorokat és más tulajdonosok tartós
  példányait, pénztárcáit, tanulási adatait összehasonlítja.
- `tests/server/PetPassiveUnityLiveSmoke.py`: a projekt által rögzített
  Unity Editor, elkülönített validációs projekt és a `pet-combat-live` mód.
- `Metin2TypedPetCombatLiveValidator`: valódi Unity felület, hálózati
  műveletek, harci megfigyelések, aktív és visszahívott újrabelépések.

A privát fixture és a keltetési checkpoint a `build` mappába kerül.
A jelszavak a privát JSON-ban maradnak. A `verify` művelet megköveteli
mind a 18 külön sikeres eredményt és a teljes futás sikerjelzőjét;
visszaolvassa a mentett petképességeket. A `restore` a saját ideiglenes
prototípusokat és az eredeti regenfájlt ellenőrzi, majd helyreállítja a
próba előtti világot. A tesztfiókok és az új tárgysorok megmaradnak a
későbbi regressziókhoz.

## Eredmények

Mind a 18 eset sikeres, összesen 1412 élő ellenőrzéssel. Minden sorra
átment az aktív és visszahívott újrabelépés, valamint az alapérték
visszaállítása.

| Hatás | Visszahívva | Idézve | Mért érték |
|---|---:|---:|---|
| Maximális HP (egész) | 31260 | 31510 | max. HP |
| Maximális HP (%) | 31260 | 39075 | max. HP |
| Szörnyek elleni sebzés | 417 | 626 | sebzés |
| Normál PvE-sebzés | 417 | 626 | sebzés |
| Szörnyek elleni ellenállás | 433 | 217 | sebzés |
| Játékosok elleni ellenállás | 409 | 205 | sebzés |
| Harcos elleni ellenállás | 409 | 205 | sebzés |
| Sura elleni ellenállás | 487 | 243 | sebzés |
| Nindzsa elleni ellenállás | 386 | 193 | sebzés |
| Sámán elleni ellenállás | 386 | 193 | sebzés |
| Főellenfelek elleni sebzés | 417 | 626 | sebzés |
| Metinkövek elleni sebzés | 417 | 626 | sebzés |
| PvE kritikus esély | 417 | 835 | sebzés |
| Harci EXP | 100 | 200 | EXP / ölés |
| Aranydobási esély | 5 | 0 | kimaradt aranydobás / 6 ölés |
| Fizikai támadási bónusz | 417 | 626 | sebzés |
| Blokkolási esély | 409 | 0 | sebzés |
| Nyílkikerülési esély | 433 | 0 | sebzés |

A kritikus találat GC135-jelzője 33, a blokkolté 8, a kikerült
nyílé 4 volt. A normál találatok jelzője 1. Az aranybónusz aktív és
aktív újrabelépéses hat-hat ölésében minden ölés adott aranyat.

A hat natív regresszió és a Unity fordítása sikeres. A statcsomag-
validátor 1118 ellenőrzése is átment. Az SQL-utóellenőrzés mind a
18 megtanult képességet 1. rangon, külön pecsétben, visszahívott
állapotban olvasta vissza. Mind a 243 korábbi példány,
22 pénztárca és a korábbi tanulási adatok egyeztek a
mentéssel.

Az ideiglenes prototípusok törlése és a regenfájl visszaállítása sikeres;
az eredeti fájl és a csomagmanifest hash-e egyezik. A szokásos öt helyi
szerverfolyamat újra fut. A helyreállítás utáni SQL-ellenőrzés is sikeres.
