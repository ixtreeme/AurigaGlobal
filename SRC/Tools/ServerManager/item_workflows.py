"""Human-readable catalog presentation and guided, unsaved item creation."""
import copy
import re
import unicodedata

from PySide6.QtCore import Qt
from PySide6.QtWidgets import (QComboBox, QDialog, QDialogButtonBox, QFormLayout,
    QHBoxLayout, QLabel, QLineEdit, QSpinBox, QVBoxLayout)

import item_catalog as catalog
import backend as api


CATEGORY_HELP = {
    1: ('Felszerelés', 'Fizikai vagy mágikus támadást ad. Beállítható fegyvertípus, hatótáv, bónusz és fejlesztés.'),
    2: ('Felszerelés', 'Testpáncél, sisak, pajzs vagy cipő. Védekezés, ellenállások és fejlesztési szintek.'),
    3: ('Felszerelés', 'Karkötő, nyaklánc vagy fülbevaló. Bónuszok, ellenállások és foglalatok.'),
    4: ('Felszerelés', 'Külsőt módosító felszerelés, választható időkorláttal és bónuszokkal.'),
    5: ('Használható tárgy', 'Gyógyítás, időzített hatás, kapcsolható harmat vagy tárgymódosító szolgáltatás.'),
    6: ('Hozzávaló', 'Fejlesztéshez és receptekhez felhasználható, halmozható alapanyag.'),
    7: ('Használható tárgy', 'Felszerelésbe helyezhető bónuszkő vagy nem használható kőszilánk.'),
    8: ('Használható tárgy', 'Jutalmakat tartalmazó láda, questdoboz vagy fejlődő pet tojása.'),
    9: ('Használható tárgy', 'Képességek tanulását és fejlesztését szabályozó könyv.'),
    10: ('Küldetés', 'Gyűjthető küldetéstárgy vagy questet aktiváló tárgy.'),
    11: ('Gyűjtögetés', 'Bányászathoz vagy horgászathoz használható, fejleszthető eszköz.'),
    12: ('Gyűjtögetés', 'Hal, csali és a halakhoz tartozó jutalmak vagy feldolgozás.'),
    13: ('Használható tárgy', 'Átváltozást biztosító tárgy és az átváltozás fejlesztése.'),
    14: ('Használható tárgy', 'Lélek, lélekfejlesztés és a hozzá tartozó tekercsek.'),
    15: ('Társak', 'Pet vagy hátas megidézése, időtartama, képességei és fejlődése.'),
    16: ('Használható tárgy', 'Használatkor Yangra vagy Sárkányérmére váltható tárgy.'),
    17: ('Társak', 'Pet tanítására, élettartamára, képességeire vagy külsejére ható ellátmány.'),
    18: ('Használható tárgy', 'Az azonos zárazonosítójú láda kinyitásához szükséges kulcs.'),
}
SECTION_HELP = {
    'Áttekintés': 'A tárgy működése és a következő szerkesztési lépések, egy helyen.',
    'Alapbeállítások': 'Szintkövetelmény, leltárméret, halmozás és bolti árak.',
    'Használat és csere': 'Ki használhatja a tárgyat, és mi történhet vele kereskedéskor?',
    'Állandó bónuszok': 'Ezek a bónuszok a tárgy minden fejlesztési szintjén érvényesek.',
    'Fejlesztés és receptek': 'A +szintek értékei, fejlesztési költségei és hozzávalói.',
    'Véletlen bónuszok': 'Az egyes tárgypéldányokra kerülő, generálható bónuszok szabályai.',
    'Pet táplálása': 'Milyen etetési hatást kap a pet ennek a tárgynak a felhasználásakor?',
    'Azonosítók és megjelenés': 'Belső azonosítók, kliensoldali fordítási kulcsok, ikon és modell.',
    'Mentés áttekintése': 'Nézd át a változtatásokat, majd ellenőrizd és mentsd a tárgytípust.',
}
TYPE_SECTION = {1: 'Fegyver és támadás', 2: 'Páncél és ellenállások', 3: 'Ékszer és foglalatok',
    4: 'Kosztüm és időkorlát', 5: 'Használati hatás', 6: 'Alapanyag fajtája',
    7: 'Kő és behelyezés', 8: 'Nyitás és jutalmak', 9: 'Képességtanulás',
    10: 'Küldetéskezelés', 11: 'Eszköz és fejlődés', 12: 'Hal, csali és feldolgozás',
    13: 'Átváltozási hatás', 14: 'Lélek és fejlesztés', 15: 'Pet vagy hátas',
    16: 'Beváltás és érték', 17: 'Petellátmány hatása', 18: 'Kulcs és zárazonosító'}
SUBTYPES = {
    1: ('weapon_subtype', [('1', 'Kard'), ('2', 'Tőr'), ('3', 'Íj'), ('4', 'Kétkezes fegyver'), ('5', 'Csengő'), ('6', 'Legyező'), ('7', 'Lovas lándzsa')]),
    2: ('equip_slot', [('2', 'Testpáncél'), ('3', 'Sisak'), ('4', 'Pajzs'), ('5', 'Cipő')]),
    3: ('equip_slot', [('6', 'Karkötő'), ('7', 'Nyaklánc'), ('8', 'Fülbevaló')]),
    6: ('material_kind', [('1', 'Bőr'), ('2', 'Vér'), ('3', 'Gyökér'), ('4', 'Tű'), ('5', 'Ékkő')]),
    16: ('wallet_currency', [('1', 'Yang'), ('2', 'Sárkányérme')]),
}


def readable_name(base):
    """A label derived from the code; never pretend it is a localized name."""
    code = base.get('content_code') or ''
    tail = code.split('.', 1)[-1].replace('.', ' ').replace('_', ' ').strip()
    if re.fullmatch(r'new item \d+', tail) and base.get('category_id'):
        return '%s #%s' % (catalog.CATEGORIES.get(int(base['category_id']), 'Tárgy'), base.get('local_id', '?'))
    return (tail[:1].upper() + tail[1:]) if tail else 'Névtelen tárgy'


def summary_lines(document):
    base, child = document['base'], document['child']
    category = int(base['category_id'])
    lines = [catalog.CATEGORIES[category] + ' · ' + CATEGORY_HELP[category][0],
             'Használat: %s. szinttől · leltár: %s × %s mező · legfeljebb %s darab egy helyen' %
             (base['min_level'], base['inventory_width'], base['inventory_height'], base['max_stack'])]
    if category in SUBTYPES:
        field, choices = SUBTYPES[category]
        value = next((label for key, label in choices if key == str(child.get(field))), None)
        if value: lines.append('Típus: ' + value)
    stats = []
    for field, label in (('physical_attack_base_raw', 'fizikai támadás'), ('magical_attack_base_raw', 'mágikus támadás'),
                         ('physical_defense_base_raw', 'fizikai védekezés'), ('magical_defense_base_raw', 'mágikus védekezés')):
        if field in child: stats.append('%s %s' % (catalog.display(child[field]), label))
    if stats: lines.append('Alapértékek: ' + ' · '.join(stats))
    if category == 5:
        effect = ('Bónuszkezelés' if document.get('attribute_service') else 'Foglalatkezelés' if document.get('socket_service') else
                  'Kapcsolható harmat' if document.get('toggle') else 'Időzített bónuszhatás' if document.get('timed') else
                  {'1': 'Azonnali gyógyítás', '2': 'Adagolt visszatöltés', '3': 'Időszakos regeneráció', '4': 'Automata palack'}.get(str((document.get('recovery') or {}).get('delivery_kind')), 'Gyógyítás'))
        lines.append('Használati hatás: ' + effect)
    maximum = (document.get('refinement') or {}).get('max_level', '0')
    lines.append('Fejlesztés: +0 … +%s · állandó bónuszok: %d · receptek: %d' %
                 (maximum, len(document.get('bonuses') or []), len(document.get('recipes') or [])))
    trade = document.get('trade') or {}
    allowed = [label for field, label in (('allow_trade', 'játékosok között cserélhető'), ('allow_drop', 'eldobható'), ('allow_sell', 'NPC-nek eladható')) if trade.get(field) == '1']
    lines.append('Kereskedelem: ' + (', '.join(allowed) if allowed else 'nem cserélhető, nem dobható el és nem adható el'))
    return lines


def slug(text):
    ascii_text = unicodedata.normalize('NFKD', text).encode('ascii', 'ignore').decode().lower()
    return re.sub(r'[^a-z0-9]+', '_', ascii_text).strip('_')


class NewItemDialog(QDialog):
    """Collect only the essential choices before opening the full editor."""
    def __init__(self, parent, packet, category=1, source=None):
        super().__init__(parent)
        self.packet, self.source, self.result_document = packet, source, None
        self.setWindowTitle('Új tárgytípus meglévő alapján' if source else 'Új tárgytípus készítése')
        self.resize(660, 550)
        self.setMinimumWidth(560)
        layout = QVBoxLayout(self)
        layout.setSpacing(16)
        title = QLabel('Másolatból új tárgy' if source else 'Milyen tárgyat szeretnél készíteni?')
        title.setObjectName('SectionTitle'); layout.addWidget(title)
        intro = QLabel(('Az új tárgy átveszi a forrás beállításait. Új azonosítót kap, és külön menthető.' if source else
                        'Válassz kategóriát és egy belső elnevezést. A részletes értékeket a következő nézetben állíthatod be.'))
        intro.setWordWrap(True); intro.setObjectName('Subtle'); layout.addWidget(intro)
        form = QFormLayout(); form.setSpacing(12)
        self.category = QComboBox()
        for key, name in catalog.CATEGORIES.items(): self.category.addItem(name, key)
        self.category.setCurrentIndex(max(0, self.category.findData(int(source['base']['category_id']) if source else category)))
        self.category.setEnabled(source is None)
        form.addRow('Tárgykategória', self.category)
        self.type_help = QLabel(); self.type_help.setWordWrap(True); self.type_help.setObjectName('FieldHint')
        form.addRow('', self.type_help)
        self.subtype = QComboBox(); self.subtype_label = QLabel('Tárgy fajtája'); form.addRow(self.subtype_label, self.subtype)
        self.name = QLineEdit()
        self.name.setPlaceholderText('Például: vörös acélkard')
        if source: self.name.setText(readable_name(source['base']) + ' másolat')
        form.addRow('Belső elnevezés', self.name)
        self.code = QLineEdit(); self.code.setPlaceholderText('Például: weapon.voros_acelkard')
        self.code_edited = False
        self.code.textEdited.connect(lambda _: setattr(self, 'code_edited', True))
        form.addRow('Egyedi tartalomkód', self.code)
        self.level = QSpinBox(); self.level.setRange(0, 65535)
        self.level.setValue(int(source['base']['min_level']) if source else 0)
        form.addRow('Szükséges karakterszint', self.level)
        layout.addLayout(form)
        self.preview = QLabel(); self.preview.setWordWrap(True); self.preview.setObjectName('SavedBadge'); layout.addWidget(self.preview)
        localization = QLabel('A másolat átveszi a forrás játékbeli nevének és leírásának fordítási kulcsát, ikonját és modelljét. Ezek az Azonosítók és megjelenés részben módosíthatók.' if source else
            'A belső elnevezésből tartalomkód készül. A játékbeli névhez és leíráshoz a kliens fordítási kulcsait kell használni; ezeket külön szerkesztheted.')
        localization.setWordWrap(True); localization.setObjectName('FieldHint'); layout.addWidget(localization)
        self.error = QLabel(); self.error.setWordWrap(True); self.error.setObjectName('InlineError'); layout.addWidget(self.error)
        self.error.hide()
        layout.addStretch()
        buttons = QDialogButtonBox(QDialogButtonBox.StandardButton.Ok | QDialogButtonBox.StandardButton.Cancel)
        self.create_button = buttons.button(QDialogButtonBox.StandardButton.Ok)
        self.create_button.setText('Piszkozat létrehozása'); self.create_button.setObjectName('Primary')
        buttons.button(QDialogButtonBox.StandardButton.Cancel).setText('Mégse')
        buttons.accepted.connect(self.create); buttons.rejected.connect(self.reject); layout.addWidget(buttons)
        self.category.currentIndexChanged.connect(self.category_changed)
        self.name.textChanged.connect(self.update_code)
        self.code.textChanged.connect(self.update_preview)
        self.level.valueChanged.connect(self.update_preview)
        self.category_changed()

    def category_changed(self):
        category = self.category.currentData()
        self.type_help.setText(CATEGORY_HELP[category][1])
        self.subtype.clear()
        for key, name in SUBTYPES.get(category, ('', []))[1]: self.subtype.addItem(name, key)
        visible = self.subtype.count() > 0 and self.source is None
        self.subtype.setVisible(visible)
        self.subtype_label.setVisible(visible)
        self.code_edited = False
        self.update_code()

    def update_code(self):
        if not self.code_edited:
            tail = slug(self.name.text())
            self.code.setText(catalog.CODE_PREFIX[self.category.currentData()] + '.' + tail if tail else '')
        self.update_preview()

    def update_preview(self):
        self.preview.setText('%s · %s. szinttől\n%s\nA létrehozás szerkeszthető piszkozatot nyit. Az adatbázisba a Mentés gomb ír.' %
                             (catalog.CATEGORIES[self.category.currentData()], self.level.value(), self.code.text() or 'Adj meg egy belső elnevezést.'))
        self.error.clear()
        self.error.hide()

    def create(self):
        code = self.code.text().strip()
        if not re.fullmatch(r'[a-z][a-z0-9_.]{0,95}', code):
            self.error.setText('A tartalomkód kisbetűvel kezdődjön, és legfeljebb 96 karakterből álljon: a–z, 0–9, _ és .')
            self.error.show()
            self.code.setFocus(); return
        model = catalog.Catalog(self.packet)
        if any(base['content_code'] == code for base in model.items()):
            self.error.setText('Ez a tartalomkód már foglalt. Válassz egy másik, egyedi elnevezést.')
            self.error.show()
            self.code.setFocus(); return
        try:
            document = model.duplicate((int(self.source['base']['category_id']), int(self.source['base']['local_id']))) if self.source else model.new(self.category.currentData())
        except api.EditorError as error:
            self.error.setText(str(error)); self.error.show(); return
        document['base'].update(content_code=code, min_level=str(self.level.value()))
        if not self.source:
            document['base'].update(name_key='item.' + code + '.name', description_key='item.' + code + '.description')
            if self.category.currentData() in SUBTYPES:
                document['child'][SUBTYPES[self.category.currentData()][0]] = self.subtype.currentData()
                if self.category.currentData() == 1 and self.subtype.currentData() == '3':
                    document['ammo'].update(ammo_kind='2', units_per_hit='1')
        self.result_document = copy.deepcopy(document)
        self.accept()
