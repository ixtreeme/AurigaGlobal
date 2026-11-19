# A lómedál hiányzó eredeti fogyasztója

Az 50050 lómedálhoz az aktív szerverforrásban, az archivált eredeti játékforrásban és az elérhető questekben nincs működő tárgykezelő vagy NPC-/questfogyasztó; csak a drop_item API kommentpéldája hivatkozik rá. Az eredeti NPC-/questadat nélkül nem adok neki kitalált lótanítási vagy jutalmazási működést.

A felhasználó kérésének megfelelően ez az egy sor a merge-ig várakozik, forrássorhash-sel és a keresés bizonyítékával. Nem töröltem, és nem számítottam konverziónak vagy kivezetésnek. A C47 csomag mellett 630 hiányos eredeti sor vár merge-re, és 54 további sor bekötése maradt. A négy, hiányzó világmodellre váró natív idéző külön nyilvántartásban marad.
