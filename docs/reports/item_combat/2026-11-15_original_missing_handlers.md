# Hiányzó eredeti tárgykezelők: merge-ig félretéve

Az aktív szerver, az aktív questek és az archivált eredeti játékforrás ellenőrzése 32 további protóhoz nem talált működő használati/céltárgy-kezelőt. A forrásazonosítók és sorhash-ek megmaradnak; sem konverziónak, sem kivezetett tartalomnak nem számítanak. A másik branch merge-je után újra kell értékelni őket.

A 71068 egy kommentelt konstansban, a 90000 egy tapasztalati számsorban fordul elő; egyik sem tárgyfunkció. A jelenlegi identitás-átrendelési tábla üres. A 50067 napi láda a 71035 hiányzó működésétől függ, ezért a teljes eredeti jutalomlistával várakozik; jutalmat nem töröltünk belőle.

A 71052 archivált forrásában működési ág található: ezt külön megőriztük a konvertálandó feladatok között. A kilenc kazamatarúna, a három kazamatakulcs és az egyéb meglevő kezelők szintén kívül maradtak ezen a listán.

A pontos sorlista, megmaradt referenciák és ellenőrzött források hash-ei az evidence JSON-ban találhatók. Ez a dokumentum tervezési állapot, az aktív játékszervert nem módosítja.
