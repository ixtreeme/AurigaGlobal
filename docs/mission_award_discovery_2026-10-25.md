# Item-award discovery: commit-order safe cache

Dátum: 2026-10-25. Ág: `codex/mission-engine`.

Az AUTO_INCREMENT kiosztási sorrendje nem commit-sorrend. A korábbi globális
`id > lastCachedID` vízjel kihagyhatta a később véglegesített kisebb azonosítót.

Az új `ItemAwardScan` egy kör elején rögzíti az aktuális legnagyobb ID-t,
majd a még átvehető awardokat rendezett, legfeljebb 256 soros oldalakon olvassa.
A kör végén nulláról indul újra. Az új beszúrások nem nyújthatják végtelenre
az aktuális kört. Egyszerre csak egy lekérdezés lehet úton; hibánál ugyanaz az
oldal következik. A ceiling után az első oldal azonnal indul, a további oldalak
a meglévő öt másodperces ütemet használják. Az észlelési késés legfeljebb a
következő sikeres körben megtett út; tartós SQL-hiba ezt természetesen kitolja.
Az eredeti sémában a taken_time index InnoDB alatt az elsődleges ID-t is hordozza.

A cache csak egy teljes sikeres kör után töröl nem látott rekordokat. Részleges
vagy hibás oldal miatt nem ürül ki. Az átvett awardok így nem maradnak bent a
folyamat teljes életére. Az ajándékikon nyugtázása megjelöli az ismert rekordot;
nem törli a még átvehető jutalmat, ezért az ismételt felderítés nem értesít újra.
A bejelentkezési értesítés kihagyja az átvett és a nyugtázott ikonokat.

A közös `ItemAwardDeliveryTests` a tényleges Scan SQL-t és nyers MYSQL_RES-t
használja: két kapcsolat fordított commit-sorrendje, több mint két oldal,
közben érkező új sorok, ismételt kör, taken rekord kihagyása, hibás SQL és
hibás eredmény utáni azonos kurzoros retry. A cache dekódolója ugyanazt az
ellenőrzött, visszatekert eredményt kapja. A meglévő atomi tárgykézbesítési
és hideg újraindításos próbák is lefutnak.

Normál és ASAN Database / ItemAwardDeliveryTests build sikeres. Normál CTest:
2/2; izolált MariaDB és kényszerített újraindítás ASAN alatt is sikeres.
Futó szerver, valódi játékosadat és Unity nem változott.
