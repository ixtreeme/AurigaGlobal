"""Two-pane item library and safe, dialog-local reward drag and drop."""
import json,secrets
from PySide6.QtCore import QMimeData,QPoint,Qt,QSize,QSortFilterProxyModel
from PySide6.QtGui import QDrag
from PySide6.QtWidgets import (QAbstractItemView,QApplication,QComboBox,QHeaderView,QLabel,QLineEdit,
    QPushButton,QSplitter,QTableWidget,QTableWidgetItem,QTreeView,QVBoxLayout,QWidget,QTabWidget)
from content_names import search_text
from item_icons import ITEM_CATEGORY_ROLE,ITEM_RETIRED_ROLE

ITEM_MIME='application/x-auriga-reward-items'
ROW_MIME='application/x-auriga-reward-rows'
GROUP_MIME='application/x-auriga-reward-group'
ROW_TOKEN_ROLE=int(Qt.ItemDataRole.UserRole)+220


def decode(mime,kind,session):
    if not mime.hasFormat(kind):return None
    raw=bytes(mime.data(kind))
    if len(raw)>65536:return None
    try:
        value=json.loads(raw)
        if value['session']!=session or not isinstance(value['values'],list) or not 0<len(value['values'])<=256:return None
        if not all(isinstance(v,str) and len(v)<=96 for v in value['values']):return None
        return value['values']
    except (ValueError,KeyError,TypeError):return None


def mime_data(kind,session,values):
    mime=QMimeData();mime.setData(kind,json.dumps(dict(session=session,values=values)).encode());return mime


class LibraryFilter(QSortFilterProxyModel):
    def __init__(self,parent):
        super().__init__(parent);self.needle='';self.category=None

    def filterAcceptsRow(self,row,parent):
        index=self.sourceModel().index(row,0,parent)
        if index.data(ITEM_RETIRED_ROLE):return False
        if self.category is not None and index.data(ITEM_CATEGORY_ROLE)!=self.category:return False
        return not self.needle or self.needle in search_text(index.data() or '')

    def apply(self,text,category):
        self.needle=search_text(text.strip());self.category=category;self.invalidate()


class ItemLibraryView(QTreeView):
    def __init__(self,parent):
        super().__init__(parent);self.partner=None;self.session=None
        self.setObjectName('RewardItemLibrary');self.setAccessibleName('Tárgylista – húzd a tárgyat a tartalomba')
        self.setRootIsDecorated(False);self.setItemsExpandable(False);self.setUniformRowHeights(True)
        self.setAlternatingRowColors(True);self.setIconSize(QSize(36,36))
        self.setEditTriggers(QAbstractItemView.EditTrigger.NoEditTriggers)
        self.setSelectionMode(QAbstractItemView.SelectionMode.ExtendedSelection)
        self.setSelectionBehavior(QAbstractItemView.SelectionBehavior.SelectRows)
        self.setDragDropMode(QAbstractItemView.DragDropMode.DragDrop);self.setDefaultDropAction(Qt.DropAction.CopyAction)
        self.setDropIndicatorShown(True)

    def keys(self):
        return [str(index.data(Qt.ItemDataRole.UserRole)) for index in self.selectionModel().selectedRows(0)]

    def startDrag(self,actions):
        values=self.keys()
        if not values or len(values)>256:return
        drag=QDrag(self);drag.setMimeData(mime_data(ITEM_MIME,self.session,values))
        icon=self.currentIndex().siblingAtColumn(0).data(Qt.ItemDataRole.DecorationRole)
        if icon:drag.setPixmap(icon.pixmap(40,40))
        drag.exec(Qt.DropAction.CopyAction)

    def removable(self,event):
        if event.source() is not self.partner:return None
        return decode(event.mimeData(),ROW_MIME,self.session)

    def dragEnterEvent(self,event):
        if self.removable(event):event.setDropAction(Qt.DropAction.MoveAction);event.accept()
        else:event.ignore()

    def dragMoveEvent(self,event):self.dragEnterEvent(event)

    def dropEvent(self,event):
        tokens=self.removable(event)
        if tokens and self.partner.remove_tokens(tokens):event.setDropAction(Qt.DropAction.MoveAction);event.accept()
        else:event.ignore()


class RewardRowHeader(QHeaderView):
    def __init__(self,table):
        super().__init__(Qt.Orientation.Vertical,table);self.table=table;self.press=None
        self.setObjectName('RewardRowHandles')
        self.setSectionsClickable(True);self.setCursor(Qt.CursorShape.OpenHandCursor)
        self.setFixedWidth(32);self.setDefaultAlignment(Qt.AlignmentFlag.AlignCenter)
        self.setToolTip('Húzd vissza balra a kijelölt sorokat a tartalomból való kivételhez.')

    def mousePressEvent(self,event):
        self.press=event.position().toPoint() if event.button()==Qt.MouseButton.LeftButton else None
        row=self.logicalIndexAt(event.position().toPoint())
        if row>=0 and (event.modifiers()&(Qt.KeyboardModifier.ControlModifier|Qt.KeyboardModifier.ShiftModifier) or not self.table.selectionModel().isRowSelected(row,self.table.rootIndex())):super().mousePressEvent(event)
        else:event.accept()

    def mouseMoveEvent(self,event):
        if self.press is not None and event.buttons()&Qt.MouseButton.LeftButton and (event.position().toPoint()-self.press).manhattanLength()>=QApplication.startDragDistance():
            self.press=None;self.table.startDrag(Qt.DropAction.MoveAction);event.accept()
        else:super().mouseMoveEvent(event)

    def mouseReleaseEvent(self,event):self.press=None;super().mouseReleaseEvent(event)


class RewardDropTable(QTableWidget):
    def bind_library(self,library,on_add,on_remove):
        self.library=library;self.session=library.session;self.on_add=on_add;self.on_remove=on_remove
        self.setObjectName('RewardContents');self.setAccessibleName('Megnyitott drop vagy láda tartalma')
        self.setSelectionMode(QAbstractItemView.SelectionMode.ExtendedSelection)
        self.setDragDropMode(QAbstractItemView.DragDropMode.DragDrop);self.setDefaultDropAction(Qt.DropAction.CopyAction)
        self.setDropIndicatorShown(True);self.setVerticalHeader(RewardRowHeader(self));self.verticalHeader().show()
        self.verticalHeader().setDefaultSectionSize(44)

    def token(self,row):
        item=self.item(row,0)
        if item is None:item=QTableWidgetItem();self.setItem(row,0,item)
        value=item.data(ROW_TOKEN_ROLE)
        if not value:value=secrets.token_hex(12);item.setData(ROW_TOKEN_ROLE,value)
        return value

    def startDrag(self,actions):
        rows=sorted({index.row() for index in self.selectedIndexes()})
        if not rows or not hasattr(self,'library'):return
        drag=QDrag(self);drag.setMimeData(mime_data(ROW_MIME,self.session,[self.token(row) for row in rows]))
        drag.exec(Qt.DropAction.MoveAction)

    def remove_tokens(self,tokens):
        if len(set(tokens))!=len(tokens):return False
        rows=[row for row in range(self.rowCount()) if self.token(row) in tokens]
        if len(rows)!=len(tokens):return False
        self.on_remove(rows);return True

    def incoming(self,event):
        if not hasattr(self,'library') or event.source() is not self.library:return None
        values=decode(event.mimeData(),ITEM_MIME,self.session)
        return values if values and all(value in self.library.allowed_keys for value in values) else None

    def incoming_group(self,event):
        if not hasattr(self,'group_panel') or event.source() is not self.group_panel.list:return None
        values=decode(event.mimeData(),GROUP_MIME,self.session)
        return values if values and len(values)==2 else None

    def dragEnterEvent(self,event):
        if self.incoming(event) or self.incoming_group(event):event.setDropAction(Qt.DropAction.CopyAction);event.accept()
        else:event.ignore()

    def dragMoveEvent(self,event):self.dragEnterEvent(event)

    def dropEvent(self,event):
        values=self.incoming(event)
        if values and self.on_add(values):event.setDropAction(Qt.DropAction.CopyAction);event.accept()
        else:
            group=self.incoming_group(event)
            if group and self.group_panel.apply(group[0],group[1]):event.setDropAction(Qt.DropAction.CopyAction);event.accept()
            else:event.ignore()


class RewardSplitEditor(QSplitter):
    def __init__(self,items,right,table,on_add,on_remove,parent=None):
        super().__init__(Qt.Orientation.Horizontal,parent);self.setChildrenCollapsible(False)
        left=QWidget();layout=QVBoxLayout(left);layout.setContentsMargins(0,0,10,0)
        title=QLabel('Tárgyak');title.setObjectName('SectionTitle');layout.addWidget(title)
        self.search=QLineEdit();self.search.setPlaceholderText('Tárgy neve… pl. fulbevalo');self.search.setAccessibleName('Keresés a bal oldali tárgylistában');layout.addWidget(self.search)
        self.category=QComboBox();self.category.addItem('Minden tárgytípus',None)
        categories={items.index(row,0).data(ITEM_CATEGORY_ROLE):items.index(row,1).data() for row in range(items.rowCount())}
        for key,label in sorted(categories.items(),key=lambda pair:str(pair[1])):
            if key is not None:self.category.addItem(label,key)
        layout.addWidget(self.category)
        self.proxy=LibraryFilter(self);self.proxy.setSourceModel(items)
        self.library=ItemLibraryView(left);self.library.setModel(self.proxy);self.library.session=secrets.token_hex(16)
        self.library.allowed_keys={str(items.index(row,0).data(Qt.ItemDataRole.UserRole)) for row in range(items.rowCount()) if not items.index(row,0).data(ITEM_RETIRED_ROLE)}
        self.library.partner=table;table.bind_library(self.library,on_add,on_remove)
        self.library.header().setStretchLastSection(False);self.library.header().setResizeContentsPrecision(24)
        self.library.header().setSectionResizeMode(0,QHeaderView.ResizeMode.Stretch)
        self.library.setColumnHidden(1,True);self.library.header().setSectionResizeMode(2,QHeaderView.ResizeMode.ResizeToContents)
        layout.addWidget(self.library,1);self.count=QLabel();self.count.setObjectName('Subtle');layout.addWidget(self.count)
        button=QPushButton('Kijelöltek hozzáadása →');button.clicked.connect(lambda:on_add(self.library.keys()));layout.addWidget(button)
        self.library.doubleClicked.connect(lambda _:on_add(self.library.keys()))
        hint=QLabel('Húzd a tárgyakat jobbra a tartalomba. Kivételhez a jobb oldali sorszámot húzd vissza ide. Több sor is kijelölhető.');hint.setWordWrap(True);hint.setObjectName('FieldHint');layout.addWidget(hint)
        self.search.textChanged.connect(self.filter);self.category.currentIndexChanged.connect(self.filter)
        self.addWidget(left);self.addWidget(right);self.setStretchFactor(0,0);self.setStretchFactor(1,1);self.setSizes([330,1120]);self.filter()

    def filter(self,*_):
        self.proxy.apply(self.search.text(),self.category.currentData());self.count.setText('%d tárgy'%self.proxy.rowCount())

    def attach_groups(self,panel):
        items=self.widget(0);items.setParent(None);tabs=QTabWidget();tabs.addTab(items,'Tárgyak');tabs.addTab(panel,'Tárgycsoportok')
        self.insertWidget(0,tabs);self.tabs=tabs;self.group_panel=panel;self.library.partner.group_panel=panel
        tabs.currentChanged.connect(lambda index:panel.refresh() if index==1 else None);self.setSizes([350,1120])
