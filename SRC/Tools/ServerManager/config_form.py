"""Category-based configuration forms; edits preserve sparse server overrides."""
import copy
import math
import re

from PySide6.QtCore import Qt, Signal
from PySide6.QtWidgets import (QCheckBox, QComboBox, QFormLayout, QFrame, QHBoxLayout,
    QLabel, QLineEdit, QListWidget, QListWidgetItem, QPlainTextEdit, QPushButton,
    QScrollArea, QSizePolicy, QSplitter, QStackedWidget, QVBoxLayout, QWidget)

import backend as api

NAMES = {
    'common': 'Közös beállítások', 'database': 'Database', 'login': 'Beléptetés',
    'game_defaults': 'Game core alapértékek', 'server': 'Általános működés',
    'network': 'Hálózat és portok', 'sql': 'Adatbázis-kapcsolatok', 'tls': 'TLS és tanúsítvány',
    'gameplay': 'Játékszabályok', 'events': 'Mentés és időzítések', 'logs': 'Naplózás',
    'quest': 'Questek', 'guild': 'Céhek', 'cache': 'Gyorsítótár', 'proto': 'Prototípusok',
    'version': 'Kliensverzió', 'map_allow': 'Engedélyezett mapok',
    'working_directory': 'Futási mappa', 'locale': 'Karakterkódolás', 'table_postfix': 'Táblák utótagja',
    'interserver_key': 'Szerverek közötti kulcs', 'hostname': 'Szerver neve',
    'channel': 'Csatorna száma', 'server_id': 'Core azonosítója', 'max_level': 'Maximális szint',
    'port': 'Kapcsolódási port', 'bind_port': 'Figyelőport', 'p2p_port': 'Core-ok közötti port',
    'bind_ip': 'Figyelő IP-cím', 'db_addr': 'Database címe', 'db_port': 'Database portja',
    'host': 'Szervercím', 'user': 'Felhasználónév', 'password': 'Jelszó',
    'cert_file': 'Tanúsítvány fájlja', 'key_file': 'Privátkulcs fájlja',
    'item_id_range': 'Kiosztható tárgyazonosítók', 'data_path': 'Játékadatok mappája',
    'client_heart_fps': 'Database frissítése másodpercenként', 'passes_per_sec': 'Game frissítése másodpercenként',
    'player_id_start': 'Játékosazonosítók kezdete', 'test': 'Tesztüzemmód',
    'log': 'Naplózás engedélyezése', 'from_db': 'Prototípusok betöltése SQL-ből',
    'mirror2db': 'Prototípusok visszaírása SQL-be', 'view_range': 'Látótávolság',
    'stone_chance': 'Metinkő esélye', 'save_second_cycle': 'Automatikus mentés időköze (mp)',
    'ping_second_cycle': 'Kapcsolatellenőrzés időköze (mp)', 'check_server': 'Kliensverzió ellenőrzése',
    'value': 'Elvárt kliensverzió', 'db_level': 'SQL-napló részletessége',
    'keep_days': 'Naplók megőrzése (nap)', 'item_count_limit': 'Tárgyak halmozási korlátja',
    'item_bonus_change_time': 'Bónuszváltás várakozási ideje (mp)',
    'status_point_get_level_limit': 'Státuszpontszerzés szintkorlátja',
    'status_point_set_max_value': 'Státuszpontok felső határa', 'shout_limit_level': 'Kiáltás minimális szintje',
    'global_shout_enable': 'Globális kiáltás', 'disable_prism_need': 'Kiáltás prizmakövetelményének kikapcsolása',
    'disable_emotion_mask': 'Emóciómaszk követelményének kikapcsolása',
    'disable_empire_language_check': 'Birodalmi nyelvi korlátozás kikapcsolása',
    'empire_shop_price_triple_disable': 'Más birodalom háromszoros árának kikapcsolása',
    'user_limit': 'Játékoslimit', 'block_login': 'Belépési dátumkorlát',
    'block_char_creation': 'Karakterlétrehozás tiltása', 'no_regen': 'Újraszületés tiltása',
    'no_wander': 'NPC-k bolyongásának tiltása', 'skill_disable': 'Képességek tiltása',
    'player_flush_seconds': 'Játékosadatok kiírása (mp)', 'item_flush_seconds': 'Tárgyadatok kiírása (mp)',
    'pricelist_flush_seconds': 'Árlisták kiírása (mp)', 'flush_limit_per_second': 'Kiírások felső korlátja / mp',
    'dir': 'Questek mappája', 'object_dir': 'Quest object mappák', 'mark_min_level': 'Céhjel minimális szintje',
    'mark_server': 'Céhjelek kiszolgálása', 'database_link': 'Database-kapcsolat',
    'account_sql': 'Fiókadatbázis', 'spam_block': 'Spamvédelem', 'item_destroy': 'Eldobott tárgyak eltűnése',
}
ROLES = {'player': 'Játékosadatok', 'account': 'Fiókok', 'common': 'Közös adatok', 'log': 'Naplóadatok'}
HELP = {
    'common': 'Minden szerver közös beállításai. Az itt megadott értékeket a szerverek örökölhetik.',
    'database': 'A szerveroldali Database folyamat beállításai.',
    'login': 'A belépést kezelő szerver saját és közösen örökölt értékei.',
    'game_defaults': 'A game core-ok közös alapértékei. A core-on megadott saját érték elsőbbséget kap.',
    'network': 'A kiválasztott szerver elérhetősége és a folyamatok közötti kapcsolatok.',
    'tls': 'A titkosított klienskapcsolathoz használt tanúsítvány és kulcs elérési útja.',
    'map_allow': 'Ezeket a mapazonosítókat szolgálja ki a kiválasztott core. Vesszővel vagy soronként add meg őket.',
    'sql': 'A kiválasztott adatkör kapcsolati adatai. A jelszó rejtve marad.',
}


def label(path):
    if path[-1] == 'database':
        return 'Adatbázis neve'
    return NAMES.get(str(path[-1]), str(path[-1]).replace('_', ' ').capitalize())


def contains(document, path):
    try:
        api.value_at(document, path)
        return True
    except (KeyError, TypeError, IndexError):
        return False


def assigned(document, path, value):
    parent = document
    for key in path[:-1]:
        parent = parent.setdefault(key, {})
    parent[path[-1]] = value


def discard(document, path):
    parents, current = [], document
    for key in path[:-1]:
        parents.append((current, key))
        current = current[key]
    del current[path[-1]]
    for parent, key in reversed(parents):
        if parent[key] == {}:
            del parent[key]
        else:
            break


def inherited(document, scope):
    if scope == ('common',):
        return {}
    common = document.get('common', {})
    role = scope[0]
    base = {}
    for key in ('locale', 'table_postfix', 'interserver_key'):
        if key in common:
            path = (key,) if role == 'login' or (key == 'interserver_key' and role != 'database') else ('server', key)
            if role == 'login' and key == 'table_postfix':
                continue
            assigned(base, path, common[key])
    sql = common.get('sql', {})
    if role == 'login':
        if 'account' in sql:
            base['account_sql'] = copy.deepcopy(sql['account'])
    else:
        roles = ('player', 'account', 'common') if role == 'database' else ('player', 'common', 'log')
        if any(key in sql for key in roles):
            base['sql'] = {key: copy.deepcopy(sql[key]) for key in roles if key in sql}
    if role != 'database' and 'tls' in common:
        assigned(base, ('tls',) if role == 'login' else ('network', 'tls'), copy.deepcopy(common['tls']))
    if role not in ('common', 'database'):
        network = document.get('database', {}).get('network', {})
        host = network.get('bind_ip', '127.0.0.1')
        link = api.merge(dict(host='127.0.0.1' if host in ('', '0.0.0.0', '::') else host,
                              port=network.get('bind_port', 5300)), common.get('database_link', {}))
        if role == 'login':
            base['database_link'] = link
        else:
            assigned(base, ('network', 'db_addr'), link['host'])
            assigned(base, ('network', 'db_port'), link['port'])
    if role == 'game_servers':
        base = api.merge(base, document.get('game_defaults', {}))
    return base


class ConfigForm(QWidget):
    changed = Signal()
    error = Signal(str)

    def __init__(self, advanced_factory):
        super().__init__()
        self.value, self.scope, self.groups, self.controls, self.errors, self.drafts = {}, ('common',), [], {}, {}, {}
        self.rendering = False
        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(12)
        selector = QHBoxLayout()
        selector.addWidget(QLabel('Szerkesztett szerver'))
        self.scopes = QComboBox()
        self.scopes.setMinimumHeight(36)
        self.scopes.currentIndexChanged.connect(self.scope_changed)
        selector.addWidget(self.scopes, 1)
        self.mode = QComboBox()
        self.mode.addItems(['Űrlapnézet', 'Haladó szerkezet'])
        self.mode.currentIndexChanged.connect(self.mode_changed)
        selector.addWidget(self.mode)
        layout.addLayout(selector)
        self.stack = QStackedWidget()
        normal = QWidget()
        normal_layout = QVBoxLayout(normal)
        normal_layout.setContentsMargins(0, 0, 0, 0)
        self.search = QLineEdit()
        self.search.setPlaceholderText('Keresés a kijelölt szerver beállításaiban…')
        self.search.textChanged.connect(self.build_groups)
        normal_layout.addWidget(self.search)
        split = QSplitter()
        self.categories = QListWidget()
        self.categories.setObjectName('Categories')
        self.categories.setMinimumWidth(160)
        self.categories.setMaximumWidth(235)
        self.categories.currentRowChanged.connect(self.render_group)
        split.addWidget(self.categories)
        self.scroll = QScrollArea()
        self.scroll.setWidgetResizable(True)
        self.scroll.setHorizontalScrollBarPolicy(Qt.ScrollBarPolicy.ScrollBarAlwaysOff)
        self.scroll.setFrameShape(QFrame.Shape.NoFrame)
        split.addWidget(self.scroll)
        split.setStretchFactor(0, 0)
        split.setStretchFactor(1, 1)
        split.setSizes([205, 760])
        normal_layout.addWidget(split, 1)
        self.stack.addWidget(normal)
        self.advanced = advanced_factory()
        self.advanced.mode.setCurrentIndex(1)
        self.advanced.mode.hide()
        self.advanced.changed.connect(self.advanced_changed)
        self.stack.addWidget(self.advanced)
        layout.addWidget(self.stack, 1)

    def set_value(self, value):
        if not isinstance(value, dict) or not isinstance(value.get('game_servers', {}), dict):
            raise api.EditorError('Az űrlapnézethez konfigurációs objektum és game_servers objektum szükséges.')
        self.value = copy.deepcopy(value)
        self.errors.clear()
        self.drafts.clear()
        previous = self.scopes.currentData()
        self.scopes.blockSignals(True)
        self.scopes.clear()
        for key in ('common', 'database', 'login', 'game_defaults'):
            if key in value:
                self.scopes.addItem(NAMES[key], key)
        for name, core in value.get('game_servers', {}).items():
            channel = core.get('server', {}).get('channel', '?')
            self.scopes.addItem('CH%s  ·  %s' % (channel, name), 'game_servers/' + name)
        index = self.scopes.findData(previous)
        self.scopes.setCurrentIndex(max(0, index))
        self.scopes.blockSignals(False)
        self.advanced.set_value(value)
        self.scope_changed()

    def checked_value(self):
        if self.mode.currentIndex() == 1:
            self.advanced.flush()
        if self.errors:
            first = next(iter(self.errors.values()))
            raise api.EditorError(first)
        return self.value

    def scope_changed(self):
        self.scope = tuple((self.scopes.currentData() or 'common').split('/'))
        self.build_groups()

    def effective(self):
        return api.merge(inherited(self.value, self.scope), api.value_at(self.value, self.scope))

    def build_groups(self):
        if not self.value or not contains(self.value, self.scope):
            return
        current = self.categories.currentItem()
        previous = current.data(Qt.ItemDataRole.UserRole) if current else None
        groups, general = [], []
        def collect(value, path):
            rows = []
            for key, child in value.items():
                if isinstance(child, dict):
                    rows += collect(child, path + (key,))
                else:
                    rows.append((path + (key,), child))
            return rows
        for key, child in self.effective().items():
            if key == 'sql' and isinstance(child, dict):
                for role, connection in child.items():
                    groups.append(('sql.' + role, 'SQL · ' + ROLES.get(role, role), collect(connection, ('sql', role))))
            elif isinstance(child, dict):
                groups.append((key, NAMES.get(key, key.replace('_', ' ').capitalize()), collect(child, (key,))))
            elif key == 'map_allow':
                groups.append((key, NAMES[key], [((key,), child)]))
            else:
                general.append(((key,), child))
        if general:
            groups.insert(0, ('general', 'Alapbeállítások', general))
        query = self.search.text().casefold().strip()
        filtered = []
        for key, title, rows in groups:
            if query and query not in title.casefold():
                rows = [(path, value) for path, value in rows if query in label(path).casefold()
                        or query in '.'.join(path).casefold()
                        or (not any(api.secret_field(part) for part in path) and query in str(value).casefold())]
            if rows:
                filtered.append((key, title, rows))
        self.groups = filtered
        self.categories.blockSignals(True)
        self.categories.clear()
        selected = 0
        for i, (key, title, rows) in enumerate(filtered):
            item = QListWidgetItem(title)
            item.setData(Qt.ItemDataRole.UserRole, key)
            item.setToolTip('%d beállítás' % len(rows))
            self.categories.addItem(item)
            if key == previous:
                selected = i
        self.categories.setCurrentRow(selected if filtered else -1)
        self.categories.blockSignals(False)
        self.render_group(selected if filtered else -1)

    def render_group(self, index):
        self.rendering = True
        self.controls = {}
        self.origin_labels, self.reset_buttons = {}, {}
        card = QWidget()
        card.setObjectName('FormCard')
        layout = QVBoxLayout(card)
        layout.setContentsMargins(22, 20, 22, 22)
        layout.setSpacing(16)
        if not 0 <= index < len(self.groups):
            layout.addWidget(QLabel('Nincs a keresésnek megfelelő beállítás.'))
        else:
            key, title, rows = self.groups[index]
            heading = QLabel(title)
            heading.setObjectName('SectionTitle')
            layout.addWidget(heading)
            description = QLabel(HELP.get(key.split('.')[0], HELP.get(self.scope[0],
                'A saját értékek felülírják a közös és a game-alapértékeket.')))
            description.setObjectName('Subtle')
            description.setWordWrap(True)
            layout.addWidget(description)
            form = QFormLayout()
            form.setHorizontalSpacing(24)
            form.setVerticalSpacing(18)
            form.setFieldGrowthPolicy(QFormLayout.FieldGrowthPolicy.AllNonFixedFieldsGrow)
            form.setRowWrapPolicy(QFormLayout.RowWrapPolicy.WrapLongRows)
            raw = api.value_at(self.value, self.scope)
            fallback = inherited(self.value, self.scope)
            for path, value in rows:
                field_label = QLabel(label(path))
                field_label.setWordWrap(True)
                field_label.setFixedWidth(205)
                field_label.setToolTip('.'.join(self.scope + path))
                field = QWidget()
                field.setObjectName('FormField')
                field_layout = QVBoxLayout(field)
                field_layout.setContentsMargins(0, 0, 0, 0)
                field_layout.setSpacing(5)
                row = QHBoxLayout()
                control = self.make_control(path, value)
                self.controls[self.scope + path] = control
                row.addWidget(control, 1)
                if contains(fallback, path):
                    reset = QPushButton('↶')
                    reset.setFixedWidth(34)
                    reset.setToolTip('Saját érték törlése; örökölt alapérték visszaállítása')
                    reset.clicked.connect(lambda checked=False, p=path: self.reset(p))
                    reset.setVisible(contains(raw, path))
                    self.reset_buttons[self.scope + path] = reset
                    row.addWidget(reset)
                field_layout.addLayout(row)
                origin = 'Saját beállítás' if contains(raw, path) else 'Örökölt alapérték'
                origin_label = QLabel(origin)
                origin_label.setObjectName('FieldHint')
                self.origin_labels[self.scope + path] = origin_label
                field_layout.addWidget(origin_label)
                origin_label.setVisible(self.scope[0] != 'common')
                form.addRow(field_label, field)
            layout.addLayout(form)
        layout.addStretch()
        old = self.scroll.takeWidget()
        if old:
            old.deleteLater()
        self.scroll.setWidget(card)
        self.rendering = False

    def make_control(self, path, value):
        full = self.scope + path
        if type(value) is bool:
            control = QCheckBox('Bekapcsolva')
            control.setChecked(value)
            control.toggled.connect(lambda state, p=path: self.commit(p, state))
        elif isinstance(value, list):
            control = QPlainTextEdit()
            control.setMinimumHeight(100)
            control.setMaximumHeight(155)
            numbers = all(type(part) is int for part in value) and (bool(value) or path[-1] in ('map_allow', 'item_id_range'))
            control.setPlainText(self.drafts.get(full, ', '.join(map(str, value)) if numbers else '\n'.join(map(str, value))))
            control.setPlaceholderText('Például: 1, 21, 41' if numbers else 'Soronként egy érték')
            def changed():
                text = control.toPlainText()
                try:
                    result = [int(part) for part in re.split(r'[,;\s]+', text.strip()) if part] if numbers else text.splitlines()
                    self.commit(path, result)
                except ValueError:
                    self.invalid(path, text, control, 'Egész számokat adj meg, vesszővel vagy soronként elválasztva.')
            control.textChanged.connect(changed)
        else:
            control = QLineEdit()
            control.setMinimumHeight(34)
            control.setText(self.drafts.get(full, value if isinstance(value, str) else str(value)))
            if any(api.secret_field(part) for part in path):
                control.setEchoMode(QLineEdit.EchoMode.Password)
            if type(value) in (int, float):
                def changed(text):
                    try:
                        parsed = int(text) if type(value) is int else float(text.replace(',', '.'))
                        if type(parsed) is float and not math.isfinite(parsed):
                            raise ValueError()
                        self.commit(path, parsed)
                    except ValueError:
                        self.invalid(path, text, control, 'Egész szám szükséges.' if type(value) is int else 'Érvényes szám szükséges.')
                control.textEdited.connect(changed)
            else:
                control.textEdited.connect(lambda text, p=path: self.commit(p, text))
        control.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Fixed)
        control.setProperty('invalid', full in self.errors)
        return control

    def commit(self, path, value):
        if self.rendering:
            return
        full = self.scope + path
        assigned(self.value, full, value)
        self.errors.pop(full, None)
        self.drafts.pop(full, None)
        control = self.controls.get(full)
        if control:
            control.setProperty('invalid', False)
            control.style().unpolish(control)
            control.style().polish(control)
        if full in self.origin_labels:
            self.origin_labels[full].setText('Saját beállítás')
        if full in self.reset_buttons:
            self.reset_buttons[full].show()
        self.changed.emit()

    def invalid(self, path, text, control, message):
        full = self.scope + path
        self.errors[full] = label(path) + ': ' + message
        self.drafts[full] = text
        control.setProperty('invalid', True)
        control.style().unpolish(control)
        control.style().polish(control)
        self.changed.emit()

    def reset(self, path):
        full = self.scope + path
        discard(self.value, full)
        self.errors.pop(full, None)
        self.drafts.pop(full, None)
        self.build_groups()
        self.changed.emit()

    def mode_changed(self, index):
        try:
            if index:
                self.advanced.set_value(self.checked_value())
            else:
                if not isinstance(self.value, dict):
                    raise api.EditorError('Az űrlapnézethez konfigurációs objektum szükséges.')
                self.set_value(self.value)
            self.stack.setCurrentIndex(index)
        except api.EditorError as error:
            self.mode.blockSignals(True)
            self.mode.setCurrentIndex(1 - index)
            self.mode.blockSignals(False)
            self.error.emit(str(error))

    def advanced_changed(self):
        self.value = copy.deepcopy(self.advanced.value)
        self.changed.emit()
