"""Scrollable name matches while typing, shared by item and monster pickers."""
from functools import lru_cache
from math import ceil
from PySide6.QtCore import QIdentityProxyModel,QModelIndex,Qt,QSize
from PySide6.QtWidgets import QAbstractItemView,QComboBox,QCompleter,QHeaderView,QTreeView
from content_names import search_text

SEARCH_ROLE=int(Qt.ItemDataRole.UserRole)+111


@lru_cache(maxsize=20000)
def normalized_name(text):
    return search_text(text)


class NameSearchModel(QIdentityProxyModel):
    """Search normalized text while displaying the original names and columns."""
    def data(self,index,role=Qt.ItemDataRole.DisplayRole):
        if role==SEARCH_ROLE:
            return normalized_name(str(super().data(index,Qt.ItemDataRole.DisplayRole) or ''))
        return super().data(index,role)


class NameCompleter(QCompleter):
    def splitPath(self,path):
        return [normalized_name(path)]

    def pathFromIndex(self,index):
        return str(index.data(Qt.ItemDataRole.DisplayRole) or '')


class NameSearchPopup(QTreeView):
    def sizeHintForRow(self,row):
        # QCompleter sizes its popup from rows alone. Include our column header
        # so a one- or two-result search still leaves room for every result.
        count=min(12,self.model().rowCount()) if self.model() else 1
        margins=self.contentsMargins()
        return super().sizeHintForRow(row)+ceil((self.header().height()+margins.top()+margins.bottom())/max(1,count))


def enable_name_search(combo,placeholder='Kezdd el beírni a nevet…'):
    combo.setEditable(True);combo.setInsertPolicy(QComboBox.InsertPolicy.NoInsert)
    combo.lineEdit().setPlaceholderText(placeholder)
    names=NameSearchModel(combo);names.setSourceModel(combo.model())
    completer=NameCompleter(names,combo);completer.setCompletionRole(SEARCH_ROLE)
    combo.setCompleter(completer)
    completer.setCompletionMode(QCompleter.CompletionMode.PopupCompletion)
    completer.setFilterMode(Qt.MatchFlag.MatchContains)
    completer.setCaseSensitivity(Qt.CaseSensitivity.CaseInsensitive)
    # This limits the viewport, never the number of matching catalog entries.
    completer.setMaxVisibleItems(12)
    popup=NameSearchPopup();popup.setObjectName('NameSearchPopup');popup.setAccessibleName('Név szerinti találatok')
    popup.setRootIsDecorated(False);popup.setItemsExpandable(False);popup.setUniformRowHeights(True)
    popup.setAlternatingRowColors(True);popup.setMinimumWidth(540)
    popup.setIconSize(QSize(32,32))
    popup.setSelectionBehavior(QAbstractItemView.SelectionBehavior.SelectRows)
    popup.setHorizontalScrollBarPolicy(Qt.ScrollBarPolicy.ScrollBarAsNeeded)
    popup.setVerticalScrollBarPolicy(Qt.ScrollBarPolicy.ScrollBarAsNeeded)
    completer.setPopup(popup)
    popup.header().setStretchLastSection(False)
    popup.header().setResizeContentsPrecision(32)
    popup.header().setSectionResizeMode(0,QHeaderView.ResizeMode.Stretch)
    for column in range(1,combo.model().columnCount()):popup.header().setSectionResizeMode(column,QHeaderView.ResizeMode.ResizeToContents)
    def selected(index):
        if index.isValid():
            source=names.mapToSource(completer.completionModel().mapToSource(index))
            combo.setCurrentIndex(source.row())
    completer.activated[QModelIndex].connect(selected)
    return combo
