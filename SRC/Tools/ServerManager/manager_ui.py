"""Shared visual language for the native server manager."""
from PySide6.QtCore import QPointF, QRectF, QSize, Qt
from PySide6.QtGui import QColor, QFont, QIcon, QPainter, QPen, QPixmap, QPolygonF
from PySide6.QtWidgets import QCheckBox, QLabel, QMessageBox, QStyle, QStyleOptionButton, QStyledItemDelegate, QVBoxLayout, QWidget


# Accent, panel, border and selected-surface colors share readable light text.
ACCENTS = {
    'blue': ('#86baff', '#182d49', '#365b86', '#244b75'),
    'violet': ('#c3adff', '#292440', '#5a4a83', '#493766'),
    'gold': ('#f6ce81', '#332d22', '#73603d', '#594425'),
    'teal': ('#72e0cc', '#173331', '#356c63', '#20574d'),
    'pink': ('#f0a8d7', '#35263a', '#77516f', '#5a3553'),
    'green': ('#83e0b3', '#1c332e', '#3b705b', '#28553f'),
    'red': ('#ffabae', '#38262e', '#814650', '#61313e'),
}
PAGE_ACCENTS = {0: 'blue', 1: 'blue', 2: 'teal', 3: 'pink', 4: 'blue',
                5: 'violet', 6: 'red', 7: 'green', 8: 'gold', 9: 'violet', 10: 'teal',11:'gold'}
ACCENT_ROLE = int(Qt.ItemDataRole.UserRole) + 2


STYLE = '''
QWidget { background: #0c1423; color: #ecf2fc; font-family: "Segoe UI"; font-size: 13px; }
QMainWindow, QStackedWidget, QScrollArea { background: #0c1423; }
QLabel, QCheckBox, QWidget#Transparent { background: transparent; }
QLabel#Title { font-size: 25px; font-weight: 650; }
QLabel#Brand { font-size: 21px; font-weight: 750; color: #f5f7ff; letter-spacing: 2px; }
QLabel#BrandMark { font-size: 24px; font-weight: 750; color: #a6d9ff; background: #23486d; border: 1px solid #4b80ad; border-radius: 11px; }
QLabel#Subtle { color: #a1aec2; }
QLabel#Eyebrow { color: #8c9ab0; font-size: 10px; font-weight: 650; letter-spacing: 1px; }
QLabel#SectionTitle { font-size: 17px; font-weight: 650; }
QLabel#FieldHint { color: #92a2ba; font-size: 11px; }
QLabel#InlineError { color: #ffc1b9; background: #3a2830; border: 1px solid #79505e; border-radius: 6px; padding: 9px 12px; }
QLabel#Metric { font-size: 23px; font-weight: 650; background: transparent; padding: 0; }
QLabel#Metric[tone="good"] { color: #79dbb0; }
QLabel#Metric[tone="warning"] { color: #f1c77a; }
QLabel#Metric[tone="danger"] { color: #ffaaa9; }
QLabel#Metric[tone="muted"] { color: #a7b5ca; }
QLabel#TargetBadge { color: #a7bbd5; background: #243149; border-radius: 5px; padding: 5px 9px; font-size: 10px; font-weight: 650; }
QLabel#SavedBadge { background: #1d3b32; color: #8bdbc0; border-radius: 5px; padding: 5px 9px; }
QLabel#SavedBadge[dirty="true"] { background: #4a3b25; color: #ffda95; }
QWidget#Sidebar { background: #121e33; border-right: 1px solid #354966; }
QTreeWidget#Navigation { background: transparent; border: none; padding: 0; outline: none; }
QTreeWidget#Navigation::item { padding: 10px 8px; margin-bottom: 2px; border-radius: 5px; }
QTreeWidget#Navigation::item:selected { background: #343451; color: #eeeaff; }
QTreeWidget#Navigation::item:hover { background: #202c3f; }
QLabel#PageDescription { color: #a7b5ca; padding: 0 0 3px 0; }
QLabel#PageBreadcrumb { color: #8e9eb5; font-size: 11px; }
QWidget#QuickLinks { background: transparent; border: none; }
QPushButton#QuickLink { text-align: left; padding: 12px; border-radius: 7px; font-weight: 600; }
QToolButton#UnsavedChanges { background: #4a3b25; color: #ffda95; border-color: #746041; }
QWidget#Card, QWidget#FormCard { background: #1a2940; border: 1px solid #3b506c; border-radius: 10px; }
QWidget#FormField { background: transparent; }
QWidget#Guidance { background: #1b304a; border: 1px solid #426d98; border-left: 4px solid #86baff; border-radius: 9px; }
QWidget#Guidance[tone="good"] { background: #1a352e; border-color: #3e765e; border-left-color: #83e0b3; }
QWidget#Guidance[tone="good"] QLabel#GuidanceIcon { color: #83e0b3; }
QWidget#Guidance[tone="warning"] { background: #382f20; border-color: #8e703b; border-left-color: #f6ce81; }
QWidget#Guidance[tone="warning"] QLabel#GuidanceIcon { color: #f6ce81; }
QWidget#Guidance[tone="danger"] { background: #3c2730; border-color: #91515c; border-left-color: #ffabae; }
QWidget#Guidance[tone="danger"] QLabel#GuidanceIcon { color: #ffabae; }
QLabel#GuidanceIcon { color: #c1c6ff; font-size: 22px; font-weight: 700; }
QLabel#CrashAlert { background: #38272e; color: #ffbdb9; border: 1px solid #71424e; border-radius: 7px; padding: 10px 14px; }
QListWidget#Navigation { background: transparent; border: none; padding: 0; outline: none; }
QListWidget#Categories { border: 1px solid #354a66; background: #142239; }
QListWidget#Categories::item { padding: 12px 10px; border-radius: 5px; margin-bottom: 3px; }
QListWidget#Categories::item:selected { background: #303454; color: #ffffff; }
QListWidget#ItemSections { background: #152030; border: 1px solid #2b384b; padding: 6px; }
QListWidget#ItemSections::item { padding: 11px 8px; margin-bottom: 3px; border-radius: 5px; }
QListWidget#ItemSections::item:selected { background: #373754; color: #f3efff; }
QListWidget#ItemSections::item:hover { background: #27364b; }
QPushButton { background: #293d58; padding: 8px 13px; border: 1px solid #506887; border-radius: 6px; font-weight: 500; }
QPushButton:hover { background: #365273; border-color: #88aace; }
QPushButton:pressed { background: #1d293b; }
QPushButton:disabled { color: #8995a8; background: #202a39; border-color: #303d50; }
QPushButton#Primary { background: #3565ae; border-color: #78a6ed; color: #ffffff; font-weight: 650; padding: 10px 18px; }
QPushButton#Primary:hover { background: #427bcf; }
QPushButton#Stop, QPushButton[role="danger"], QPushButton#Primary[role="danger"] { background: #4a2936; border-color: #a85b6d; color: #ffc4ca; }
QPushButton#Stop:hover, QPushButton[role="danger"]:hover, QPushButton#Primary[role="danger"]:hover { background: #713547; border-color: #e38a9b; }
QPushButton[role="success"], QPushButton#Primary[role="success"] { background: #23654e; border-color: #59b48e; color: #f0fff7; font-weight: 650; }
QPushButton[role="success"]:hover, QPushButton#Primary[role="success"]:hover { background: #2c8060; border-color: #83e0b3; }
QPushButton[role="reload"] { background: #203f52; border-color: #4b91a4; color: #a4e8ed; }
QPushButton[role="reload"]:hover { background: #2c5c6a; border-color: #72d7db; }
QPushButton[role="warning"], QPushButton#Primary[role="warning"] { background: #4b3920; border-color: #ac8446; color: #ffdc9e; }
QPushButton[role="warning"]:hover, QPushButton#Primary[role="warning"]:hover { background: #6c4e24; border-color: #e6b86b; }
QPushButton#Primary:disabled, QPushButton#Stop:disabled, QPushButton[role]:disabled, QPushButton#Primary[role]:disabled { background: #202a39; border-color: #303d50; color: #8995a8; }
QPushButton#Quiet { background: transparent; border-color: #344157; color: #b9c5d8; }
QPushButton:focus, QToolButton:focus, QComboBox:focus, QLineEdit:focus { border: 1px solid #a6d9ff; }
QLineEdit[invalid="true"], QPlainTextEdit[invalid="true"] { border: 1px solid #e98383; }
QLineEdit, QPlainTextEdit, QTreeWidget, QListWidget, QTableWidget, QTableView, QComboBox {
 background: #111e31; border: 1px solid #3c526e; border-radius: 6px; padding: 6px; selection-background-color: #315885;
}
QComboBox { padding: 7px 10px; min-height: 18px; }
QSpinBox { background: #141f2d; border: 1px solid #304057; border-radius: 6px; padding: 6px; min-height: 20px; }
QComboBox QAbstractItemView { background: #1c2a3c; selection-background-color: #444267; padding: 6px; }
QTreeView#NameSearchPopup { background: #111e31; alternate-background-color: #1c2e46; border: 1px solid #52749b; padding: 4px; }
QTreeView#NameSearchPopup::item { padding: 6px; color: #ecf2fc; }
QTreeView#NameSearchPopup::item:selected { background: #315885; color: #ffffff; }
QTreeView#RewardItemLibrary { background: #111e31; alternate-background-color: #1c2e46; border: 1px solid #3c6477; border-radius: 6px; }
QTreeView#RewardItemLibrary::item { padding: 5px; color: #ecf2fc; }
QTreeView#RewardItemLibrary::item:selected { background: #24596b; color: #ffffff; }
QHeaderView#RewardRowHandles::section { padding: 2px; color: #8ed6e4; background: #203f51; }
QTreeWidget::item, QListWidget::item { padding: 5px; }
QTreeWidget::item:selected, QListWidget::item:selected, QTableWidget::item:selected, QTableView::item:selected { background: #353c62; color: #ffffff; }
QTableWidget, QTableView { alternate-background-color: #1c2e46; gridline-color: #31465f; }
QTableWidget#CoreTable { border: 1px solid #3b506c; background: #132239; alternate-background-color: #1e324b; padding: 0; }
QTableWidget#CoreTable::item { padding-left: 12px; border: none; }
QHeaderView::section { background: #2a4160; padding: 10px 12px; border: none; border-bottom: 1px solid #52749b; color: #d6e6fb; font-weight: 600; }
QTabBar::tab { background: #1a293f; color: #b3c4db; padding: 9px 18px; border-bottom: 3px solid transparent; }
QTabBar::tab:hover { background: #2c405b; color: #ffffff; }
QTabBar::tab:selected { background: #263e60; color: #b8d7ff; border-bottom-color: #86baff; }
QTabWidget::pane { border: 1px solid #3b506c; }
QTabWidget#ItemEditorTabs::pane { border: none; }
QCheckBox { padding: 5px; spacing: 7px; }
QCheckBox#Channel::indicator { width: 16px; height: 16px; border: 1px solid #72829b; border-radius: 4px; background: #182332; }
QCheckBox#Channel::indicator:checked { background: #2f8a72; border-color: #8be2c7; }
QCheckBox#Channel::indicator:disabled { border-color: #465369; background: #243044; }
QCheckBox#Channel { background: #202c40; border: 1px solid #3d4c63; border-radius: 6px; padding: 8px 10px; }
QCheckBox#Channel:checked { background: #204539; border-color: #63ba9b; color: #bbf5d9; }
QCheckBox#Channel:disabled { color: #8995a8; border-color: #303d50; }
QToolButton { background: #243249; border: 1px solid #3a4a61; border-radius: 6px; padding: 7px 11px; }
QToolButton:hover { background: #32445e; }
QToolButton:checked { background: #373653; border-color: #8982c9; }
QToolButton#Disclosure { background: transparent; border: none; padding: 4px 0; color: #b5c2d7; text-align: left; }
QMenu { background: #1b2939; border: 1px solid #46546c; padding: 6px; }
QMenu::item { padding: 8px 18px; }
QMenu::item:selected { background: #3b4265; }
QSplitter::handle { background: #29364b; }
QScrollArea { border: none; }
QScrollBar:vertical { background: #131d2b; width: 10px; margin: 0; }
QScrollBar::handle:vertical { background: #45546d; min-height: 30px; border-radius: 4px; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
QScrollBar:horizontal { background: #131d2b; height: 10px; }
QScrollBar::handle:horizontal { background: #45546d; min-width: 30px; border-radius: 4px; }
QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }
QProgressBar { background: #142238; border: 1px solid #405976; border-radius: 5px; text-align: center; }
QProgressBar::chunk { background: #367f9a; border-radius: 4px; }
QStatusBar { color: #b2c5df; font-size: 11px; background: #142238; border-top: 1px solid #30455f; }
QToolTip { background: #26354b; color: #f1f4fa; border: 1px solid #65718a; padding: 7px; }
'''

for _name, (_color, _surface, _border, _selected) in ACCENTS.items():
    STYLE += f'''
QWidget#Card[accent="{_name}"], QWidget#FormCard[accent="{_name}"] {{ background: {_surface}; border-color: {_border}; border-top: 3px solid {_color}; }}
QWidget#Card[accent="{_name}"] QLabel#Eyebrow {{ color: {_color}; }}
QPushButton#QuickLink[accent="{_name}"] {{ background: {_surface}; border: 1px solid {_border}; color: {_color}; }}
QPushButton#QuickLink[accent="{_name}"]:hover {{ background: {_selected}; border-color: {_color}; }}
QLabel#Title[accent="{_name}"] {{ color: {_color}; }}
QWidget#Workspace[accent="{_name}"] QLabel#SectionTitle {{ color: {_color}; }}
QWidget#Workspace[accent="{_name}"] QTabBar::tab:selected {{ background: {_surface}; color: {_color}; border-bottom-color: {_color}; }}
QWidget#Workspace[accent="{_name}"] QListWidget#Categories::item:selected,
QWidget#Workspace[accent="{_name}"] QListWidget#ItemSections::item:selected {{ background: {_selected}; color: #ffffff; border-left: 3px solid {_color}; }}
'''


def accent(widget, value):
    """Refresh descendants too: their section/tab rules depend on this page."""
    if widget.property('accent') != value:
        widget.setProperty('accent', value)
        for child in [widget, *widget.findChildren(QWidget)]:
            child.style().unpolish(child)
            child.style().polish(child)
            child.update()


def tone(widget, value):
    if widget.property('tone') != value:
        widget.setProperty('tone', value)
        widget.style().unpolish(widget)
        widget.style().polish(widget)
        for child in widget.findChildren(QLabel):
            child.style().unpolish(child)
            child.style().polish(child)


def confirm_action(parent, title, message, action, destructive=False):
    dialog = QMessageBox(QMessageBox.Icon.Warning if destructive else QMessageBox.Icon.Question,
                         title, message, parent=parent)
    apply = dialog.addButton(action, QMessageBox.ButtonRole.DestructiveRole if destructive else QMessageBox.ButtonRole.AcceptRole)
    cancel = dialog.addButton('Mégse', QMessageBox.ButtonRole.RejectRole)
    dialog.setDefaultButton(cancel)
    dialog.exec()
    return dialog.clickedButton() is apply


class ChannelBox(QCheckBox):
    def paintEvent(self, event):
        super().paintEvent(event)
        if self.isChecked():
            option = QStyleOptionButton()
            self.initStyleOption(option)
            rect = self.style().subElementRect(QStyle.SubElement.SE_CheckBoxIndicator, option, self)
            painter = QPainter(self)
            painter.setRenderHint(QPainter.RenderHint.Antialiasing)
            painter.setPen(QPen(QColor('#ffffff' if self.isEnabled() else '#99a6bb'), 1.8,
                                Qt.PenStyle.SolidLine, Qt.PenCapStyle.RoundCap, Qt.PenJoinStyle.RoundJoin))
            painter.drawPolyline(QPolygonF([QPointF(rect.x() + 4, rect.y() + 8),
                                            QPointF(rect.x() + 7, rect.y() + 11),
                                            QPointF(rect.x() + 13, rect.y() + 5)]))
            painter.end()


class DashboardContent(QWidget):
    """Let the process table shrink before scrolling the entire dashboard."""
    def minimumSizeHint(self):
        return QSize(super().minimumSizeHint().width(), 1)

    def heightForWidth(self, width):
        return self.layout().minimumHeightForWidth(width)


def line_icon(name, color='#aab8ce'):
    """Small, code-native icons stay crisp on high-DPI desktop displays."""
    pixmap = QPixmap(40, 40)
    pixmap.fill(Qt.GlobalColor.transparent)
    painter = QPainter(pixmap)
    painter.setRenderHint(QPainter.RenderHint.Antialiasing)
    painter.scale(2, 2)
    painter.setPen(QPen(QColor(color), 1.5, Qt.PenStyle.SolidLine,
                        Qt.PenCapStyle.RoundCap, Qt.PenJoinStyle.RoundJoin))
    paths = {
        'overview': [[(3, 3), (8, 3), (8, 8), (3, 8), (3, 3)], [(12, 3), (17, 3), (17, 8), (12, 8), (12, 3)],
                     [(3, 12), (8, 12), (8, 17), (3, 17), (3, 12)], [(12, 12), (17, 12), (17, 17), (12, 17), (12, 12)]],
        'settings': [[(3, 5), (17, 5)], [(3, 10), (17, 10)], [(3, 15), (17, 15)], [(7, 3), (7, 7)], [(13, 8), (13, 12)], [(7, 13), (7, 17)]],
        'data': [[(3, 5), (10, 2), (17, 5), (10, 8), (3, 5), (3, 15), (10, 18), (17, 15), (17, 5)], [(10, 8), (10, 18)]],
        'code': [[(6, 5), (2, 10), (6, 15)], [(14, 5), (18, 10), (14, 15)], [(11, 3), (9, 17)]],
        'logs': [[(4, 2), (13, 2), (17, 6), (17, 18), (4, 18), (4, 2)], [(7, 7), (11, 7)], [(7, 11), (14, 11)], [(7, 14), (13, 14)]],
        'remote': [[(2, 3), (18, 3), (18, 14), (2, 14), (2, 3)], [(6, 18), (14, 18)], [(10, 14), (10, 18)], [(5, 6), (8, 8), (5, 10)], [(11, 10), (14, 10)]],
        'warning': [[(10, 2), (19, 17), (1, 17), (10, 2)], [(10, 7), (10, 11)], [(10, 14), (10, 14.3)]],
        'backup': [[(3, 6), (17, 6), (17, 18), (3, 18), (3, 6)], [(2, 2), (18, 2), (18, 6), (2, 6), (2, 2)], [(8, 10), (12, 10)]],
        'items': [[(10, 2), (18, 7), (15, 17), (5, 17), (2, 7), (10, 2)], [(2, 7), (18, 7)], [(10, 2), (6, 7), (10, 17), (14, 7), (10, 2)]],
        'drops': [[(10, 2), (10, 11)], [(6, 7), (10, 11), (14, 7)], [(3, 13), (3, 17), (17, 17), (17, 13)]],
        'bench': [[(7, 2), (13, 2)], [(8, 2), (8, 8), (3, 16), (4, 18), (16, 18), (17, 16), (12, 8), (12, 2)], [(6, 12), (14, 12)]],
        'play': [[(6, 3), (16, 10), (6, 17), (6, 3)]],
        'stop': [[(4, 4), (16, 4), (16, 16), (4, 16), (4, 4)]],
        'check': [[(3, 10), (8, 15), (17, 5)]],
        'refresh': [[(17, 8), (17, 3), (13, 3)], [(3, 12), (3, 17), (7, 17)], [(17, 3), (11, 2), (5, 4), (3, 8)], [(3, 17), (9, 18), (15, 16), (17, 12)]],
        'folder': [[(2, 5), (8, 5), (10, 7), (18, 7), (18, 17), (2, 17), (2, 5)]],
    }
    for points in paths.get(name, paths['overview']):
        painter.drawPolyline(QPolygonF([QPointF(x, y) for x, y in points]))
    painter.end()
    pixmap.setDevicePixelRatio(2)
    return QIcon(pixmap)


class NavigationDelegate(QStyledItemDelegate):
    def sizeHint(self, option, index):
        return QSize(190, 34 if index.data(Qt.ItemDataRole.UserRole + 1) == 'section' else 40)

    def paint(self, painter, option, index):
        painter.save()
        painter.setRenderHint(QPainter.RenderHint.Antialiasing)
        rect = QRectF(option.rect).adjusted(0, 2, -1, -2)
        color, surface, border, selected_surface = ACCENTS.get(index.data(ACCENT_ROLE), ACCENTS['blue'])
        if index.data(Qt.ItemDataRole.UserRole + 1) == 'section':
            painter.setPen(QColor(color))
            font = QFont(option.font)
            font.setPixelSize(10)
            font.setWeight(QFont.Weight.DemiBold)
            painter.setFont(font)
            painter.drawText(rect.adjusted(12, 0, -5, 0), Qt.AlignmentFlag.AlignVCenter,
                             index.data(Qt.ItemDataRole.DisplayRole))
            painter.restore()
            return
        selected = bool(option.state & QStyle.StateFlag.State_Selected)
        if selected or option.state & QStyle.StateFlag.State_MouseOver:
            painter.setPen(QPen(QColor(border), 1) if selected else Qt.PenStyle.NoPen)
            painter.setBrush(QColor(selected_surface if selected else surface))
            painter.drawRoundedRect(rect, 7, 7)
        if selected:
            painter.setPen(Qt.PenStyle.NoPen)
            painter.setBrush(QColor(color))
            painter.drawRoundedRect(QRectF(rect.x() + 1, rect.y() + 8, 3, rect.height() - 16), 1.5, 1.5)
        icon = index.data(Qt.ItemDataRole.DecorationRole)
        if icon:
            icon.paint(painter, int(rect.x() + 12), int(rect.center().y() - 9), 18, 18)
        painter.setFont(option.font)
        painter.setPen(QColor('#ffffff' if selected else '#c1cee1'))
        painter.drawText(rect.adjusted(42, 0, -5, 0), Qt.AlignmentFlag.AlignVCenter,
                         index.data(Qt.ItemDataRole.DisplayRole))
        painter.restore()


def card_layout(parent=None, accent_name=None):
    card = QWidget(parent)
    card.setObjectName('Card')
    if accent_name:
        card.setProperty('accent', accent_name)
    layout = QVBoxLayout(card)
    layout.setContentsMargins(18, 12, 18, 12)
    layout.setSpacing(10)
    return card, layout


def metric_card(caption, initial, description, accent_name='blue'):
    card, layout = card_layout(accent_name=accent_name)
    title = QLabel(caption)
    title.setObjectName('Eyebrow')
    value = QLabel(initial)
    value.setObjectName('Metric')
    value.setWordWrap(True)
    hint = QLabel(description)
    hint.setObjectName('Subtle')
    hint.setWordWrap(True)
    layout.addWidget(title)
    layout.addWidget(value, 1)
    layout.addWidget(hint)
    return card, value, hint
