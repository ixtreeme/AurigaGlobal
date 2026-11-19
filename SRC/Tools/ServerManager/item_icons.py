"""Portable original item icons, decoded only when a visible item needs them."""
from functools import lru_cache
import json
from pathlib import Path
import sys
from zipfile import ZipFile
from PySide6.QtCore import Qt,QSize
from PySide6.QtGui import QColor,QIcon,QImage,QPainter,QPixmap
from PySide6.QtWidgets import QStyledItemDelegate,QStyleOptionViewItem

ICON_ASSET_ROLE=int(Qt.ItemDataRole.UserRole)+212
ITEM_CATEGORY_ROLE=ICON_ASSET_ROLE+1
ITEM_RETIRED_ROLE=ICON_ASSET_ROLE+2


@lru_cache(maxsize=1)
def archive():
    for path in (Path(getattr(sys,'_MEIPASS',Path(__file__).resolve().parent))/'item_icons.zip',
                 Path(__file__).resolve().parents[3]/'config/server_manager/item_icons.zip'):
        if path.is_file():
            bundle=ZipFile(path);return bundle,json.loads(bundle.read('icons.json'))['icons']
    return None,{}


@lru_cache(maxsize=1)
def missing_icon():
    pixmap=QPixmap(40,40);pixmap.fill(Qt.GlobalColor.transparent)
    painter=QPainter(pixmap);painter.setRenderHint(QPainter.RenderHint.Antialiasing)
    painter.setPen(QColor('#73849a'));painter.setBrush(QColor('#26384e'));painter.drawRoundedRect(3,3,34,34,5,5)
    painter.setPen(QColor('#c3cede'));painter.drawText(pixmap.rect(),Qt.AlignmentFlag.AlignCenter,'?');painter.end()
    return QIcon(pixmap)


@lru_cache(maxsize=4096)
def item_icon(asset):
    bundle,icons=archive();member=icons.get(str(asset).replace('\\','/').casefold())
    if bundle and member:
        image=QImage.fromData(bundle.read(member))
        if not image.isNull():return QIcon(QPixmap.fromImage(image))
    return missing_icon()


def icon_available(asset):
    return str(asset).replace('\\','/').casefold() in archive()[1]


class ItemIconDelegate(QStyledItemDelegate):
    def initStyleOption(self,option,index):
        super().initStyleOption(option,index)
        asset=index.data(ICON_ASSET_ROLE)
        if asset:
            option.icon=item_icon(asset);option.decorationSize=QSize(32,32)
            option.features|=QStyleOptionViewItem.ViewItemFeature.HasDecoration

    def sizeHint(self,option,index):
        size=super().sizeHint(option,index);size.setHeight(max(size.height(),42));return size
