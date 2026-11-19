"""Grouped navigation with stable destinations shared by every manager page."""
import unicodedata
from PySide6.QtCore import Qt, Signal
from PySide6.QtGui import QColor
from PySide6.QtWidgets import QTreeWidget, QTreeWidgetItem
from manager_ui import ACCENTS, ACCENT_ROLE, PAGE_ACCENTS, NavigationDelegate, line_icon

# IDs retain the existing page-stack positions; visual ordering is independent.
DROP_PAGE = 10
PENDING_PAGE = 11
PAGES = {
    0: ('Szerver', 'Áttekintés', 'overview', 'Állapot, csatornák és a leggyakoribb szerverműveletek.', 'indítás leállítás core SQL ellenőrzés karbantartás'),
    1: ('Szerver', 'Beállítások', 'settings', 'A kiválasztott szerver hálózati, adatbázis- és játékszabály-beállításai.', 'konfiguráció port jelszó szint server.json'),
    8: ('Játéktartalom', 'Tárgykatalógus', 'items', 'Tárgytípusok, bónuszok és fejlesztések szerkesztése.', 'item tárgy készítés recept bónusz fejlesztés'),
    DROP_PAGE: ('Játéktartalom', 'Dropok', 'drops', 'Mobdropok, ládatartalmak és tömeges jutalommódosítás élő frissítéssel.', 'drop szörny mob láda jutalom esély súly darabszám tömeges'),
    9: ('Játéktartalom', 'Tárgypróbapad', 'bench', 'A mentett tárgytípusok kipróbálása külön tesztkarakteren.', 'teszt karakter felszerelés próba'),
    2: ('Játéktartalom', 'Adatfájlok', 'data', 'Boltok, műhelyreceptek, térképek és további játékadatok.', 'json npc bolt hozzávaló kezdőfelszerelés gyűjtőhely'),
    3: ('Játéktartalom', 'Küldetések és Lua', 'code', 'Questfájlok keresése, szerkesztése és ellenőrzött mentése.', 'quest küldetés lua script kód'),
    4: ('Üzemeltetés', 'Szervernaplók', 'logs', 'Naplók olvasása, keresés és szűrés hibatípus szerint.', 'log hiba warning figyelmeztetés'),
    6: ('Üzemeltetés', 'Összeomlások', 'warning', 'Összeomlási jelentések és a hibakereséshez szükséges adatok.', 'crash dump hibakeresés'),
    7: ('Üzemeltetés', 'Biztonsági mentések', 'backup', 'Mentések készítése, ellenőrzése és visszaállítása.', 'backup helyreállítás adatmentés'),
    PENDING_PAGE: ('Üzemeltetés', 'Függő módosítások', 'backup', 'Ideiglenes szerkesztések összegyűjtése, áttekintése és közös jóváhagyása.', 'tervezet piszkozat ideiglenes feltöltés mentés jóváhagyás változás összesítő'),
    5: ('Kapcsolatok', 'Távoli elérés', 'remote', 'SSH-kapcsolatok, távoli build, feltöltés és telepítés.', 'ssh profil tunnel alagút fordítás csomag'),
}


def normalized(value):
    return ''.join(c for c in unicodedata.normalize('NFKD', value.casefold()) if not unicodedata.combining(c))


class NavigationItem(QTreeWidgetItem):
    def text(self, column=0):
        return super().text(column)

    def setText(self, *args):
        super().setText(*(args if len(args) == 2 else (0, args[0])))


class ManagerNavigation(QTreeWidget):
    currentRowChanged = Signal(int)
    filterResetRequested = Signal()
    resultsChanged = Signal(int)

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setObjectName('Navigation')
        self.setItemDelegate(NavigationDelegate(self))
        self.setHeaderHidden(True)
        self.setRootIsDecorated(False)
        self.setIndentation(0)
        self.setHorizontalScrollBarPolicy(Qt.ScrollBarPolicy.ScrollBarAlwaysOff)
        self.setAccessibleName('Szerverkezelő oldalai, feladat szerint csoportosítva')
        self.groups, self.destinations = {}, {}
        for page, (group, label, icon, description, keywords) in PAGES.items():
            if group not in self.groups:
                heading = QTreeWidgetItem(self, [group.upper()])
                heading.setFlags(Qt.ItemFlag.ItemIsEnabled)
                heading.setData(0, Qt.ItemDataRole.UserRole + 1, 'section')
                group_accent = {'Szerver': 'blue', 'Játéktartalom': 'gold',
                                'Üzemeltetés': 'green', 'Kapcsolatok': 'violet'}[group]
                heading.setData(0, ACCENT_ROLE, group_accent)
                heading.setForeground(0, QColor(ACCENTS[group_accent][0]))
                font = heading.font(0)
                font.setPixelSize(11)
                font.setBold(True)
                heading.setFont(0, font)
                self.groups[group] = heading
            item = NavigationItem(self.groups[group], [label])
            item.setIcon(0, line_icon(icon, ACCENTS[PAGE_ACCENTS[page]][0]))
            item.setData(0, ACCENT_ROLE, PAGE_ACCENTS[page])
            item.setData(0, Qt.ItemDataRole.UserRole, page)
            item.setToolTip(0, description)
            self.destinations[page] = item
        self.expandAll()
        self.currentItemChanged.connect(self._selected)

    def _selected(self, item, previous):
        if item is not None and item.data(0, Qt.ItemDataRole.UserRole) is not None:
            self.currentRowChanged.emit(item.data(0, Qt.ItemDataRole.UserRole))

    def item(self, page):
        return self.destinations[page]

    def count(self):
        return len(self.destinations)

    def currentRow(self):
        item = self.currentItem()
        return item.data(0, Qt.ItemDataRole.UserRole) if item is not None and item.data(0, Qt.ItemDataRole.UserRole) is not None else -1

    def setCurrentRow(self, page):
        item = self.destinations.get(page)
        if item is None:
            return
        if item.isHidden() or item.parent().isHidden():
            self.filterResetRequested.emit()
            self.filter('')
        item.parent().setExpanded(True)
        self.setCurrentItem(item)
        self.scrollToItem(item)

    def filter(self, text):
        terms = normalized(text).split()
        total = 0
        for page, item in self.destinations.items():
            haystack = normalized(' '.join(PAGES[page]) + ' ' + item.text())
            visible = all(term in haystack for term in terms)
            item.setHidden(not visible)
            total += visible
        for heading in self.groups.values():
            heading.setHidden(all(heading.child(i).isHidden() for i in range(heading.childCount())))
            if text:
                heading.setExpanded(True)
        self.resultsChanged.emit(total)

    def open_first_match(self):
        for page, item in self.destinations.items():
            if not item.isHidden() and not item.parent().isHidden():
                self.setCurrentRow(page)
                self.setFocus()
                return
