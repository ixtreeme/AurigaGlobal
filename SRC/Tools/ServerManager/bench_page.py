"""Tárgyteszt: the new item system on a dedicated local test character.

Every action goes through ItemBenchTool, i.e. the game's own planners and the
transactional persistence writer; this page builds no SQL and decides no
outcome itself. Only the local package's own database is used.
"""
import copy

from PySide6.QtCore import Qt
from PySide6.QtWidgets import (QAbstractItemView, QComboBox, QDialog, QDialogButtonBox, QFormLayout, QHBoxLayout, QHeaderView,
    QInputDialog, QLabel, QLineEdit, QMessageBox, QPlainTextEdit, QPushButton, QSpinBox, QSplitter, QTabBar, QTableWidget,
    QTableWidgetItem, QVBoxLayout, QWidget)

import backend as api
import item_bench as bench
import item_catalog as catalog
from item_page import BONUS_LABELS, UNITS, Task, recipe_text

KEY_ROLE = int(Qt.ItemDataRole.UserRole)
STONE_STATES = {0: 'üres', 1: 'kő', 2: 'törött'}


class RefineDialog(QDialog):
    """Pick one of the item's recipes for its current level, and the roll."""

    def __init__(self, parent, title, recipes, describe):
        super().__init__(parent); self.setWindowTitle(title); self.resize(620, 260)
        layout = QVBoxLayout(self); form = QFormLayout()
        self.recipe = QComboBox()
        for recipe in recipes: self.recipe.addItem(describe(recipe), int(recipe['recipe_id']))
        self.roll = QComboBox()
        for label, value in (('Véletlen dobás (mint a játékban)', None), ('Kényszerített siker', 1), ('Kényszerített kudarc', 10000)):
            self.roll.addItem(label, value)
        form.addRow('Recept', self.recipe); form.addRow('Kimenetel', self.roll); layout.addLayout(form)
        hint = QLabel('A díj és a hozzávalók sikertelenség esetén is elfogynak. 0% és 100% esélynél nincs dobás, '
                      'ott a kényszerítés nem számít.')
        hint.setWordWrap(True); hint.setObjectName('FieldHint'); layout.addWidget(hint); layout.addStretch()
        buttons = QDialogButtonBox(QDialogButtonBox.StandardButton.Ok | QDialogButtonBox.StandardButton.Cancel)
        buttons.button(QDialogButtonBox.StandardButton.Ok).setText('Fejlesztés')
        buttons.button(QDialogButtonBox.StandardButton.Cancel).setText('Mégse')
        buttons.accepted.connect(self.accept); buttons.rejected.connect(self.reject); layout.addWidget(buttons)

    def choice(self):
        fields = dict(recipe=self.recipe.currentData())
        if self.roll.currentData() is not None: fields['roll'] = self.roll.currentData()
        return fields


class BenchPage(QWidget):
    def __init__(self, owner):
        super().__init__(); self.owner = owner
        self.task = self.packet = self.bench = self.status_info = self.cached = None; self.bases = {}
        self.target = None; self.attempted = None; self.state = None; self.page_index = 0
        layout = QVBoxLayout(self); layout.setContentsMargins(0, 0, 0, 0)
        bar = QHBoxLayout(); title = QLabel('Tárgyteszt'); title.setObjectName('SectionTitle'); bar.addWidget(title, 1)
        self.start_db_button = QPushButton('Adatbázis indítása'); self.start_db_button.clicked.connect(self.start_database)
        self.reload_button = QPushButton('Frissítés'); self.reload_button.clicked.connect(self.reload)
        self.install_button = QPushButton('Mentési táblák telepítése…'); self.install_button.clicked.connect(self.install)
        self.setup_button = QPushButton('Próbapad létrehozása'); self.setup_button.clicked.connect(self.setup)
        self.gold_button = QPushButton('Yang beállítása…'); self.gold_button.clicked.connect(self.set_gold)
        for widget in (self.start_db_button, self.reload_button, self.install_button, self.setup_button, self.gold_button): bar.addWidget(widget)
        layout.addLayout(bar)
        self.status = QLabel('Válassz helyi szervercsomagot.'); self.status.setWordWrap(True); self.status.setObjectName('Subtle')
        layout.addWidget(self.status)
        hint = QLabel('Az új tárgyrendszer próbája a #%d tesztkarakteren, a játék saját döntési és mentési kódjával. Csak az új '
                      'mentési táblákat használja; a régi player.item és a karakterek változatlanok. A katalógus (Tárgyak lap) '
                      'módosítása után a próbapad ellenőrzi és átveszi az új katalógust.' % bench.BENCH_OWNER)
        hint.setWordWrap(True); hint.setObjectName('FieldHint'); layout.addWidget(hint)
        split = QSplitter()
        left = QWidget(); side = QVBoxLayout(left); side.setContentsMargins(0, 0, 0, 0)
        side.addWidget(QLabel('Tárgy adása'))
        filters = QHBoxLayout()
        self.category = QComboBox(); self.category.addItem('Minden kategória', 0)
        for category, name in catalog.CATEGORIES.items(): self.category.addItem(name, category)
        self.search = QLineEdit(); self.search.setPlaceholderText('Keresés…')
        filters.addWidget(self.category); filters.addWidget(self.search, 1); side.addLayout(filters)
        self.catalog_list = QTableWidget(0, 3); self.catalog_list.setHorizontalHeaderLabels(['Kulcs', 'Kategória', 'Tartalomkód'])
        self.catalog_list.horizontalHeader().setSectionResizeMode(2, QHeaderView.ResizeMode.Stretch)
        self.catalog_list.verticalHeader().hide(); self.catalog_list.setEditTriggers(QAbstractItemView.EditTrigger.NoEditTriggers)
        self.catalog_list.setSelectionBehavior(QAbstractItemView.SelectionBehavior.SelectRows)
        self.catalog_list.setSelectionMode(QAbstractItemView.SelectionMode.SingleSelection)
        self.catalog_list.itemSelectionChanged.connect(self.grant_limits)
        side.addWidget(self.catalog_list, 1)
        give = QHBoxLayout()
        self.count = QSpinBox(); self.count.setRange(1, 1); self.count.setPrefix('Darab: ')
        self.level = QSpinBox(); self.level.setRange(0, 0); self.level.setPrefix('Szint: +')
        self.grant_button = QPushButton('Adás'); self.grant_button.setObjectName('Primary'); self.grant_button.clicked.connect(self.grant)
        give.addWidget(self.count); give.addWidget(self.level); give.addWidget(self.grant_button); side.addLayout(give)
        for signal in (self.category.currentIndexChanged, self.search.textChanged): signal.connect(lambda *_: self.fill_catalog())
        split.addWidget(left)
        right = QWidget(); body = QVBoxLayout(right); body.setContentsMargins(8, 0, 0, 0)
        self.summary = QLabel(''); self.summary.setObjectName('SectionTitle'); body.addWidget(self.summary)
        self.pages_bar = QTabBar(); self.pages_bar.currentChanged.connect(self.page_changed); body.addWidget(self.pages_bar)
        self.grid = QTableWidget(0, 0); self.grid.verticalHeader().hide(); self.grid.horizontalHeader().hide()
        self.grid.setEditTriggers(QAbstractItemView.EditTrigger.NoEditTriggers)
        self.grid.setSelectionMode(QAbstractItemView.SelectionMode.SingleSelection)
        self.grid.cellClicked.connect(self.grid_clicked); self.grid.itemSelectionChanged.connect(self.controls)
        body.addWidget(self.grid, 2)
        self.items = QTableWidget(0, 7); self.items.setHorizontalHeaderLabels(['ID', 'Tárgy', 'Szint', 'Darab', 'Cella', 'Bónuszok', 'Kövek'])
        self.items.horizontalHeader().setSectionResizeMode(1, QHeaderView.ResizeMode.Stretch)
        self.items.horizontalHeader().setSectionResizeMode(5, QHeaderView.ResizeMode.Stretch)
        self.items.verticalHeader().hide(); self.items.setEditTriggers(QAbstractItemView.EditTrigger.NoEditTriggers)
        self.items.setSelectionBehavior(QAbstractItemView.SelectionBehavior.SelectRows)
        self.items.setSelectionMode(QAbstractItemView.SelectionMode.SingleSelection)
        self.items.itemSelectionChanged.connect(self.controls)
        body.addWidget(self.items, 2)
        actions = QHBoxLayout()
        self.refine_button = QPushButton('Fejlesztés…'); self.refine_button.clicked.connect(self.refine)
        self.attribute_button = QPushButton('Bónuszszolgáltatás…'); self.attribute_button.clicked.connect(lambda: self.service('attribute'))
        self.socket_button = QPushButton('Foglalatszolgáltatás…'); self.socket_button.clicked.connect(lambda: self.service('socket'))
        self.move_button = QPushButton('Áthelyezés a kijelölt cellára'); self.move_button.clicked.connect(self.move)
        self.destroy_button = QPushButton('Megsemmisítés'); self.destroy_button.clicked.connect(self.destroy)
        for widget in (self.refine_button, self.attribute_button, self.socket_button, self.move_button, self.destroy_button): actions.addWidget(widget)
        actions.addStretch(); body.addLayout(actions)
        self.log = QPlainTextEdit(); self.log.setReadOnly(True); self.log.setMaximumBlockCount(500); self.log.setPlaceholderText('Műveletnapló')
        body.addWidget(self.log, 1)
        split.addWidget(right); split.setSizes([360, 760]); split.setStretchFactor(1, 1)
        layout.addWidget(split, 1)
        self.render()

    # --- owner integration ------------------------------------------------------
    def active(self):
        return self.task is not None

    def local(self):
        return not self.owner.is_remote() and bool(self.owner.root)

    def tick(self, state):
        if self.target != self.owner.target_id():
            self.target = self.owner.target_id(); self.attempted = None
            self.packet = self.bench = self.status_info = self.cached = None; self.bases = {}; self.render()
        if not self.local():
            self.status.setText('A tárgyteszt csak helyi szervercsomagon érhető el.')
        elif state and not state.get('database_running') and not self.active():
            self.status.setText('A helyi adatbázis nem fut. Indítsd el az „Adatbázis indítása” gombbal (a játékszerver ettől nem indul el).')
        elif self.isVisible() and state and state.get('database_running') and self.status_info is None and self.attempted != self.target \
                and not self.active() and not self.owner.busy():
            self.attempted = self.target; self.reload()
        self.state = state; self.controls()

    def controls(self):
        state = self.state or {}
        local = self.local(); busy = self.active() or self.owner.busy(); db = bool(state.get('database_running'))
        info = self.status_info or {}; ready = self.bench is not None
        self.start_db_button.setVisible(local and not db and state.get('database_kind') != 'external')
        self.start_db_button.setEnabled(local and not busy and bool(state.get('database_initialized')))
        self.reload_button.setEnabled(local and db and not busy)
        self.install_button.setVisible(bool(self.status_info) and not info.get('installed'))
        self.install_button.setEnabled(local and db and not busy and not state.get('running'))
        self.setup_button.setVisible(bool(info.get('installed')))
        self.setup_button.setText('Próbapad visszaállítása…' if info.get('owner') else 'Próbapad létrehozása')
        self.setup_button.setEnabled(local and db and not busy)
        self.gold_button.setEnabled(local and db and not busy and ready)
        self.grant_button.setEnabled(local and db and not busy and ready and self.chosen_catalog() is not None)
        item = self.selected_item()
        for button in (self.refine_button, self.destroy_button): button.setEnabled(not busy and ready and item is not None)
        self.attribute_button.setEnabled(not busy and ready and item is not None and bool(self.tools('attribute_service_profile_id')))
        self.socket_button.setEnabled(not busy and ready and item is not None and bool(self.tools('socket_service_profile_id')))
        cell = self.grid_cell()
        self.move_button.setEnabled(not busy and ready and item is not None and cell is not None and cell not in self.occupied(item['id']))

    def start_database(self):
        if not self.local() or self.owner.busy(): return
        self.owner.job(['start-db'], lambda ok: (setattr(self, 'attempted', None), self.reload()) if ok else None)

    # --- background work ---------------------------------------------------------
    def run(self, function, done, message):
        if self.active(): return
        task = Task(function, self); self.task = task; outcome = {}
        self.status.setText(message)
        task.ready.connect(lambda value: outcome.update(value=value)); task.failed.connect(lambda text: outcome.update(error=text))
        def finished():
            if self.task is task: self.task = None
            task.deleteLater()
            if 'error' in outcome:
                self.status.setText(outcome['error']); self.write('Hiba: ' + outcome['error']); self.owner.error(outcome['error'])
            elif 'value' in outcome:
                try: done(outcome['value'])
                except Exception as error: self.status.setText(str(error)); self.owner.error(str(error))
            self.controls(); self.owner.refresh()
        task.finished.connect(finished); task.start(); self.controls()

    def write(self, text):
        self.log.appendPlainText(text)

    def reload(self):
        if not self.local() or self.active(): return
        root = self.owner.root
        def fetch():
            packet = catalog.request(root)
            info = bench.call(root, 'status'); info.pop('state', None)
            loaded = None
            if info.get('installed') and info.get('owner'):
                try: loaded = bench.call(root, 'load')
                except api.EditorError as error: info['load_error'] = str(error)
            return packet, info, loaded
        self.run(fetch, self.loaded, 'Katalógus és próbapad olvasása…')

    def loaded(self, value):
        self.packet, self.status_info, loaded = value
        self.cached = catalog.Catalog(self.packet)
        self.bases = {(int(r['category_id']), int(r['local_id'])): r for r in self.cached.tables['item_base']}
        self.bench = loaded['state'] if loaded else None
        info = self.status_info
        if not info.get('installed'): text = 'Az új mentési táblák még nincsenek telepítve a helyi player adatbázisba.'
        elif not info.get('owner'): text = 'A mentési táblák telepítve. Hozd létre a próbapadot.'
        elif info.get('load_error'): text = info['load_error']
        else: text = 'Próbapad kész • katalógus-hash %s…' % self.packet['hash'][:12]
        if loaded and loaded.get('catalog_followed'): self.write('A katalógus változott; a próbapad ellenőrizte a tárgyait és átvette az új katalógust.')
        self.status.setText(text)
        self.fill_catalog(); self.render()

    def act(self, action, message, describe=None, **fields):
        root = self.owner.root
        def done(result):
            self.bench = result['state']
            if result.get('catalog_followed'): self.write('A katalógus változott; a próbapad átvette az új katalógust.')
            self.write(describe(result) if describe else message.rstrip('…') + ': kész.')
            self.status.setText('Próbapad kész • katalógus-hash %s…' % self.packet['hash'][:12] if self.packet else '')
            self.render()
        self.run(lambda: bench.call(root, action, **fields), done, message)

    # --- catalog side ------------------------------------------------------------
    def base(self, category, local):
        return self.bases.get((int(category), int(local)))

    def fill_catalog(self):
        self.catalog_list.setRowCount(0)
        if not self.packet: return
        category = self.category.currentData(); needle = self.search.text().strip().lower()
        rows = [r for r in self.cached.items() if r['retired'] != '1' and (not category or int(r['category_id']) == category)
                and (not needle or needle in (r['content_code'] + ' ' + r['category_id'] + ':' + r['local_id']).lower())]
        self.catalog_list.setRowCount(len(rows))
        for index, row in enumerate(rows):
            key = (int(row['category_id']), int(row['local_id']))
            for column, value in enumerate(('%d:%d' % key, catalog.CATEGORIES.get(key[0], str(key[0])), row['content_code'])):
                cell = QTableWidgetItem(value); cell.setData(KEY_ROLE, key); self.catalog_list.setItem(index, column, cell)
        self.catalog_list.resizeColumnToContents(0); self.catalog_list.resizeColumnToContents(1)

    def chosen_catalog(self):
        cells = self.catalog_list.selectedItems()
        return cells[0].data(KEY_ROLE) if cells else None

    def grant_limits(self):
        key = self.chosen_catalog(); base = self.base(*key) if key else None
        self.count.setRange(1, int(base['max_stack']) if base else 1)
        top = self.cached.target_levels(key) if base else 0
        self.level.setRange(0, top or 0); self.controls()

    def name(self, item):
        return '%s  (%d:%d)' % (item.get('content_code') or '?', item['category'], item['local'])

    def bonus(self, bonus_id, value):
        d = next((d for d in (self.packet or {}).get('bonuses', []) if str(d['id']) == str(bonus_id)), None)
        if d is None: return '#%s: %s' % (bonus_id, value)
        scale, unit = UNITS.get(int(d['unit']), (1, ''))
        return '%s %s %s' % (BONUS_LABELS.get(d['code'], d['code']), catalog.display(value, scale), unit)

    def tools(self, field):
        """Bench items whose prototype is a consumable with this service."""
        if not self.bench or not self.packet: return []
        services = {}
        for row in self.cached.tables['item_consumable']:
            if row.get(field): services[(int(row['category_id']), int(row['local_id']))] = True
        return [i for i in self.bench['items'] if not i['destroyed'] and (i['category'], i['local']) in services]

    # --- bench side --------------------------------------------------------------
    def layout_info(self):
        layout = (self.bench or {}).get('layout') or dict(columns=5, rows=9, pages=4)
        return layout['columns'], layout['rows'], layout['pages']

    def footprint(self, item):
        base = self.base(item['category'], item['local'])
        width, height = (int(base['inventory_width']), int(base['inventory_height'])) if base else (1, 1)
        columns = self.layout_info()[0]
        return [item['cell'] + y * columns + x for y in range(height) for x in range(width)], width, height

    def occupied(self, except_id=None):
        cells = set()
        for item in (self.bench or {}).get('items', []):
            if item['destroyed'] or item['cell'] is None or item['id'] == except_id: continue
            cells.update(self.footprint(item)[0])
        return cells

    def render(self):
        columns, rows, pages = self.layout_info()
        while self.pages_bar.count() < pages: self.pages_bar.addTab('%d. oldal' % (self.pages_bar.count() + 1))
        self.grid.clearSpans(); self.grid.clear(); self.grid.setColumnCount(columns); self.grid.setRowCount(rows)
        self.grid.horizontalHeader().setSectionResizeMode(QHeaderView.ResizeMode.Stretch)
        self.grid.verticalHeader().setSectionResizeMode(QHeaderView.ResizeMode.Stretch)
        size = columns * rows; first = self.page_index * size
        for r in range(rows):
            for c in range(columns):
                cell = QTableWidgetItem(''); cell.setData(KEY_ROLE, None); cell.setToolTip('Cella %d' % (first + r * columns + c)); self.grid.setItem(r, c, cell)
        live = [i for i in (self.bench or {}).get('items', []) if not i['destroyed']]
        for item in live:
            if item['cell'] is None or not first <= item['cell'] < first + size: continue
            offset = item['cell'] - first; r, c = divmod(offset, columns); _, width, height = self.footprint(item)
            label = (item.get('content_code') or '?').split('.')[-1]
            text = label + (' +%d' % item['level'] if item['level'] else '') + (' ×%d' % item['count'] if item['count'] > 1 else '')
            cell = QTableWidgetItem(text); cell.setData(KEY_ROLE, item['id']); cell.setToolTip(self.name(item) + '\nCella %d' % item['cell'])
            cell.setTextAlignment(Qt.AlignmentFlag.AlignCenter)
            self.grid.setItem(r, c, cell)
            if width > 1 or height > 1: self.grid.setSpan(r, c, min(height, rows - r), min(width, columns - c))
        selected = self.selected_item()
        self.items.setRowCount(len(live))
        for index, item in enumerate(live):
            attributes = ', '.join(self.bonus(a['bonus'], a['value']) + (' (zárolt)' if item.get('locked_slot') == a['slot'] else '') for a in item['attributes'])
            stones = ', '.join('kő %d:%d +%d' % (s['stone'][0], s['stone'][1], s['level']) if s['stone'] else STONE_STATES.get(s['state'], '?') for s in item['stones'])
            values = (item['id'], self.name(item), '+%d' % item['level'], str(item['count']), '' if item['cell'] is None else str(item['cell']), attributes, stones)
            for column, value in enumerate(values):
                cell = QTableWidgetItem(value); cell.setData(KEY_ROLE, item['id']); self.items.setItem(index, column, cell)
            if selected and item['id'] == selected['id']: self.items.selectRow(index)
        self.items.resizeColumnToContents(0)
        if self.bench:
            wallet = next((w for w in self.bench['wallets'] if w['currency'] == 1), None)
            self.summary.setText('#%d tesztkarakter • %s Yang • %d tárgy' % (self.bench['owner'], format(wallet['balance'] if wallet else 0, ',').replace(',', ' '), len(live)))
        else: self.summary.setText('')
        self.controls()

    def page_changed(self, index):
        self.page_index = max(index, 0); self.render()

    def grid_cell(self):
        cells = self.grid.selectedIndexes()
        if not cells: return None
        columns, rows, _ = self.layout_info()
        return self.page_index * columns * rows + cells[0].row() * columns + cells[0].column()

    def grid_clicked(self, row, column):
        identity = self.grid.item(row, column).data(KEY_ROLE) if self.grid.item(row, column) else None
        if identity is not None:
            for index in range(self.items.rowCount()):
                if self.items.item(index, 0).text() == identity: self.items.selectRow(index)
        self.controls()

    def selected_item(self):
        cells = self.items.selectedItems()
        if not cells or not self.bench: return None
        identity = cells[0].data(KEY_ROLE)
        return next((i for i in self.bench['items'] if i['id'] == identity and not i['destroyed']), None)

    # --- actions -----------------------------------------------------------------
    def install(self):
        if QMessageBox.question(self, 'Tárgyteszt', 'A helyi player adatbázisba 9 új tábla kerül (a meglévőkhöz nem nyúl), előtte teljes '
                                'mentés készül a player adatbázisról. Folytatod?') != QMessageBox.StandardButton.Yes: return
        root = self.owner.root
        def done(result):
            self.write('Mentési táblák telepítve. Mentés: ' + result.get('folder', '—')); self.attempted = None; self.status_info = None; self.reload()
        self.run(lambda: bench.install(root), done, 'Mentési táblák telepítése…')

    def ask_gold(self, title, default):
        text, ok = QInputDialog.getText(self, title, 'Yang (egész szám):', text=str(default))
        if not ok: return None
        try: value = int(text.strip().replace(' ', ''))
        except ValueError: QMessageBox.warning(self, title, 'Érvénytelen összeg.'); return None
        if not 0 <= value <= 9223372036854775807: QMessageBox.warning(self, title, 'Az összeg kívül esik a tartományon.'); return None
        return value

    def setup(self):
        reset = bool((self.status_info or {}).get('owner'))
        if reset and QMessageBox.question(self, 'Tárgyteszt', 'A próbapad minden tárgya és a pénze törlődik, és üres próbapad indul az aktuális '
                                          'katalógussal. Folytatod?') != QMessageBox.StandardButton.Yes: return
        gold = self.ask_gold('Próbapad', 1000000000)
        if gold is None: return
        root = self.owner.root
        def done(result):
            self.bench = result['state']; self.status_info = dict(self.status_info or {}, owner=True, pin_matches=True); self.status_info.pop('load_error', None)
            self.write('Próbapad %s, %s Yang.' % ('visszaállítva' if reset else 'létrehozva', format(gold, ',').replace(',', ' ')))
            self.status.setText('Próbapad kész • katalógus-hash %s…' % self.packet['hash'][:12] if self.packet else ''); self.render()
        self.run(lambda: bench.call(root, 'setup', reset=True, gold=gold), done, 'Próbapad előkészítése…')

    def set_gold(self):
        wallet = next((w for w in self.bench['wallets'] if w['currency'] == 1), None)
        gold = self.ask_gold('Yang beállítása', wallet['balance'] if wallet else 0)
        if gold is not None: self.act('wallet', 'Yang beállítása…', lambda _: 'Yang: %s.' % format(gold, ',').replace(',', ' '), gold=gold)

    def grant(self):
        key = self.chosen_catalog()
        if key is None: return
        base = self.base(*key)
        self.act('grant', 'Tárgy adása…', lambda r: 'Kapott: %s ×%d +%d (ID %s).' % (base['content_code'], self.count.value(), self.level.value(), r.get('granted')),
                 category=key[0], local=key[1], count=self.count.value(), level=self.level.value())

    def move(self):
        item, cell = self.selected_item(), self.grid_cell()
        if item and cell is not None: self.act('move', 'Áthelyezés…', lambda _: '%s → cella %d.' % (self.name(item), cell), item=item['id'], cell=cell)

    def destroy(self):
        item = self.selected_item()
        if not item: return
        if QMessageBox.question(self, 'Tárgyteszt', 'Megsemmisíted: %s?' % self.name(item)) != QMessageBox.StandardButton.Yes: return
        self.act('destroy', 'Megsemmisítés…', lambda _: 'Megsemmisítve: %s.' % self.name(item), item=item['id'])

    def recipe_label(self, recipe):
        target = 'ugyanez' if recipe['target_category_id'] == recipe['source_category_id'] and recipe['target_local_id'] == recipe['source_local_id'] else \
            ((self.base(recipe['target_category_id'], recipe['target_local_id']) or {}).get('content_code', '?'))
        ingredients = []
        for row in self.cached.tables['refinement_ingredient']:
            if row['recipe_id'] == recipe['recipe_id']:
                part = self.base(row['category_id'], row['local_id']) or {}
                ingredients.append('%s ×%s' % (part.get('content_code', '?'), row['amount']))
        text = '→ %s +%s • %s' % (target, recipe['target_level'], recipe_text(dict(recipe, ingredients=ingredients)))
        return text + ('  [' + ', '.join(ingredients) + ']' if ingredients else '')

    def refine(self):
        item = self.selected_item()
        if not item: return
        recipes = [r for r in self.cached.tables['refinement_recipe'] if r['source_category_id'] == str(item['category'])
                   and r['source_local_id'] == str(item['local']) and r['source_level'] == str(item['level'])]
        if not recipes:
            QMessageBox.information(self, 'Fejlesztés', 'Ehhez a tárgyhoz +%d szinten nincs fejlesztési recept. A Tárgyak lap Fejlesztés '
                                    'fülén, a „Recept” oszlopban adhatsz meg.' % item['level']); return
        dialog = RefineDialog(self, 'Fejlesztés: ' + self.name(item) + ' +%d' % item['level'], sorted(recipes, key=lambda r: int(r['recipe_id'])), self.recipe_label)
        if dialog.exec() != QDialog.DialogCode.Accepted: return
        def describe(result):
            after = next((i for i in result['state']['items'] if i['id'] == item['id']), None)
            outcome = 'SIKER' if result['success'] else 'KUDARC'
            now = 'megsemmisült' if after is None or after['destroyed'] else '%s +%d' % (after.get('content_code'), after['level'])
            return 'Fejlesztés %s (dobás %s): %s → %s.' % (outcome, result['roll'] or '—', self.name(item), now)
        self.act('refine', 'Fejlesztés…', describe, item=item['id'], **dialog.choice())

    def service(self, kind):
        item = self.selected_item()
        tools = self.tools('attribute_service_profile_id' if kind == 'attribute' else 'socket_service_profile_id')
        if not item or not tools: return
        labels = ['%s ×%d (ID %s)' % (t.get('content_code'), t['count'], t['id']) for t in tools]
        title = 'Bónuszszolgáltatás' if kind == 'attribute' else 'Foglalatszolgáltatás'
        label, ok = QInputDialog.getItem(self, title, 'Eszköz erre: ' + self.name(item), labels, 0, False)
        if not ok: return
        tool = tools[labels.index(label)]
        def describe(result):
            after = next((i for i in result['state']['items'] if i['id'] == item['id']), None)
            text = '%s %s: %s.' % (title, 'SIKER' if result.get('success', True) else 'KUDARC', self.name(item))
            if after and kind == 'attribute': text += ' Bónuszok: ' + (', '.join(self.bonus(a['bonus'], a['value']) for a in after['attributes']) or 'nincs')
            return text
        self.act(kind, title + '…', describe, tool=tool['id'], target=item['id'])
