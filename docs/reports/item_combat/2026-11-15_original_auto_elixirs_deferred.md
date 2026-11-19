# Hiányzó eredeti automataelixír-kezelők

A 39037–39042 protók `USE_SPECIAL` tárgyak, kapacitásra emlékeztető `value0` adattal. Az aktív használati kapcsolóban és a factory töltetinicializálásában egyik azonosító sem szerepel. A működő ág kizárólag a 72723–72730, 76004/76005 és 76021/76022 változatokat kezeli. Az aktív `ori_to_new_table.json` üres, tehát a hat protó más eredeti kezelőre sem irányítódik át. A régi F:/SF_V3_5 szerverforrás `char_item.cpp` fájljában sincs használati águk.

A 39040–39042 előfordulása a PvP engedélyezési listában önmagában nem hoz létre használati funkciót. A tárgynevekből és kapacitásból nem találok ki új működési profilt: a felhasználó kérésére mind a hat sort a teljes repository-merge utánra halasztottam. Nem konvertált és nem kivezetett sorok; az eredeti adataik és hashük a bizonyítékfájlban megmaradnak.

A halasztási nyilvántartás így 566 sort tartalmaz. A három teljes visszatöltő étel konverziója ettől külön, működő eredeti kód alapján folytatódik.
