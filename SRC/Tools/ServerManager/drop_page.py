"""Dedicated reward workspace, sharing the loaded catalog and checked writer."""
from PySide6.QtCore import Qt
from PySide6.QtWidgets import QHBoxLayout,QLabel,QPushButton,QTabWidget,QVBoxLayout,QWidget
from manager_navigation import DROP_PAGE
from manager_ui import line_icon


class DropsPage(QWidget):
    def __init__(self,owner,controller):
        super().__init__();self.owner=owner;self.controller=controller;self.seen_packet=None;self.message=''
        layout=QVBoxLayout(self);layout.setContentsMargins(0,0,0,0);layout.setSpacing(12)
        bar=QHBoxLayout()
        hint=QLabel('A helyi tervezetet szerkeszted. A feltöltés és az élő frissítés a Függő módosítások oldalon, közös jóváhagyás után indul.');hint.setObjectName('Subtle');hint.setWordWrap(True);bar.addWidget(hint,1)
        self.start_db_button=QPushButton('Helyi adatbázis indítása');self.start_db_button.setProperty('role','success');self.start_db_button.clicked.connect(self.start_database)
        self.refresh_button=QPushButton('Dropok újraolvasása');self.refresh_button.setIcon(line_icon('refresh'));self.refresh_button.setProperty('role','reload');self.refresh_button.clicked.connect(controller.reload_rewards)
        self.reload_button=QPushButton('Élő verzió újratöltése');self.reload_button.setProperty('role','reload');self.reload_button.clicked.connect(controller.reload_live_drops)
        for button in (self.start_db_button,self.refresh_button,self.reload_button):bar.addWidget(button)
        layout.addLayout(bar)
        self.status=QLabel('Válassz helyi szervercsomagot.');self.status.setTextFormat(Qt.TextFormat.PlainText);self.status.setObjectName('Subtle');self.status.setWordWrap(True);layout.addWidget(self.status)
        self.tabs=QTabWidget();self.tabs.setAccessibleName('Dropok: mobdropok, ládatartalmak és tárgycsoportok');layout.addWidget(self.tabs,1)
        for page,title in ((controller.drops_page,'Mobdropok'),(controller.containers_page,'Ládák tartalma')):
            controller.workspace.removeWidget(page);self.tabs.addTab(page,title)
        self.groups_page = QWidget()
        self.groups_layout = QVBoxLayout(self.groups_page)
        self.groups_layout.setContentsMargins(0, 0, 0, 0)
        self.groups_placeholder = QLabel('A tárgycsoportok szerkesztéséhez töltsd be a helyi katalógust.')
        self.groups_layout.addWidget(self.groups_placeholder)
        self.groups_panel = None
        self.groups_packet = None
        self.tabs.addTab(self.groups_page, 'Tárgycsoportok')
        controller.drops_back_button.hide();controller.containers_back_button.hide();controller.reload_drop_button.hide()
        self.tabs.currentChanged.connect(self.tab_changed)

    def activate(self,page):
        self.owner.navigation.setCurrentRow(DROP_PAGE);self.tabs.setCurrentWidget(page);self.tab_changed()

    def tab_changed(self,*_):
        if self.controller.packet is not None:
            if self.tabs.currentWidget() is self.controller.drops_page:self.controller.fill_drops()
            elif self.tabs.currentWidget() is self.controller.containers_page:self.controller.fill_containers()
            elif self.groups_panel is not None and self.groups_packet is self.controller.packet:self.groups_panel.refresh()
        self.sync()

    def sync_groups(self, packet, busy):
        if self.groups_packet is not packet:
            self.groups_packet = packet
            if self.groups_panel is not None:
                self.groups_layout.removeWidget(self.groups_panel)
                self.groups_panel.hide()
                self.groups_panel.deleteLater()
                self.groups_panel = None
            if packet is not None:
                from reward_group_ui import GroupPanel
                self.groups_panel = GroupPanel(self, self.owner, self.controller.catalog_model(True),
                                               distribute=self.controller.open_group_distribution)
                self.groups_layout.addWidget(self.groups_panel, 1)
            self.groups_placeholder.setVisible(packet is None)
        if self.groups_panel is not None:
            self.groups_panel.setEnabled(not busy)

    def start_database(self):
        if not self.controller.local() or self.owner.busy():return
        self.owner.job(['start-db'],lambda ok:self.controller.reload_rewards() if ok else None)

    def show_status(self,message):
        self.message=message;self.sync()

    def sync(self):
        controller=self.controller;state=getattr(controller,'state',None) or {};busy=controller.active() or self.owner.busy()
        local=controller.local();db=bool(state.get('database_running'));ready=controller.packet is not None
        if self.tabs.currentWidget() is self.groups_page:
            self.sync_groups(controller.packet if local else None, busy)
        self.start_db_button.setVisible(local and not db and state.get('database_kind')!='external')
        self.start_db_button.setEnabled(local and not busy and bool(state.get('database_initialized')))
        self.refresh_button.setEnabled(local and db and not busy)
        self.reload_button.setEnabled(local and db and ready and not busy and bool(state.get('running')))
        if self.seen_packet is not controller.packet:self.seen_packet=controller.packet;self.message=''
        if not local:text='A dropok szerkesztéséhez válassz saját helyi szervercsomagot.'
        elif busy or not ready:text=controller.status.text()
        elif self.message:text=self.message
        else:
            rewards=controller.rewards_model()
            text='%d mobdroptábla · %d szerkeszthető láda'%(len(rewards.model.tables['mob_drop_group']),len(rewards.containers()))
        self.status.setText(text)

    def tick(self,state):self.sync()
