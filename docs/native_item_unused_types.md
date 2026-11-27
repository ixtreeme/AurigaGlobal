# Retired, unused item data declarations

Removed 12 unreferenced structures from three server headers:
- Mount/pet SQL inventory records: TMountInventoryItemTable and TPetInventoryItemTable.
- Archived shop response/log records: TIShopData and TIShopLogData.
- Old award notification record: TPacketItemAwardInfromer.
- Unused Switchbot attribute and item-update records: TSwitchbottAttributeTable and TSwitchbotUpdateItem.
- Equipment display record: TEquipmentItemSet.
- Mount/pet client inventory headers and records: TPacketGCMountInventory, TMountInventoryItemData, TPacketGCPetInventory and TPetInventoryItemData.

Their tags and aliases had no uses outside their declarations in active server or test sources. Historical migration notes and archived client-source snapshots retain their original text.

The reserved packet identifiers, old-packet rejection, packet-size handling, packing directives and minimum payload guards remain intact. The retained Switchbot P2P table/frame definitions still describe the byte boundary of a discarded old message. They are required for that boundary and do not perform item operations. The one-way loot measurement importer and archived conversion tools remain available.

The client/server wire format has no change. Unity stays on cs236 with native ground v3 and canonical metadata; no Unity source change is needed for deleting unreferenced C++ declarations.

Validation passed: 9 native suites, 2 protocol-policy tests and 221 Unity validator assertions; LoginServer, Database and GameServer compilation; native lifecycle/item suites; server/Unity protocol-policy tests; Unity player compilation and Editor validators; isolated two-account native login, chat/whisper, linked tooltip, actual ground model, split drop/pickup, exact measured quantities and reconnect. Source hashes verify that rejection/framing code and unrelated pending work are preserved. All catalogue and quest revisions are checked again during local publication.

The isolated, reduced Unity fixture emitted the already-known skill-3 recipe-prefab exception. It did not affect the asserted native item flows. No Unity gameplay source was changed or checked in for this server-only declaration cleanup.
