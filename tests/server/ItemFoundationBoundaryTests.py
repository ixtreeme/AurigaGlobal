from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[2]
SERVER = ROOT / "SRC/Server/GameServer"

class FoundationBoundary(unittest.TestCase):
    def test_retired_item_engine_cannot_be_linked(self):
        retired = (
            'ecs/systems/ItemSystem.cpp',
            'ecs/systems/ItemSystem.hpp',
            'ecs/systems/ItemSystem_LegacyBridge.cpp',
            'ecs/systems/InventorySystem.cpp',
            'ecs/systems/InventorySystem.hpp',
            'ecs/systems/ItemAttributeSystem.cpp',
            'ecs/systems/ItemBonusMetadataSystem.cpp',
            'ecs/ItemRegistry.cpp',
            'ecs/ItemRegistry.hpp',
            'ecs/ItemInvariants.hpp',
            'ecs/components/item_components.hpp',
            'ecs/components/item_proto_components.hpp',
            'item/item_manager.cpp',
            'item/item_manager.h',
            'item/item_manager_idrange.cpp',
            'item/item.h',
            'item/ItemUse.cpp',
            'item/ItemUse.h',
            'item/over9refine.cpp',
            'item/over9refine.h',
            'item/attr_transfer.cpp',
            'item/attr_transfer.h',
            'item/safebox.cpp',
            'item/safebox.h',
            'item/belt_inventory_helper.h',
            'pet/mount_inventory_helper.h',
            'social/new_switchbot.cpp',
            'social/new_switchbot.h',
            'social/exchange.cpp',
            'social/exchange.h',
            'social/shop.cpp',
            'social/shop_manager.cpp',
            'social/shop_manager.h',
            'combat/buff_on_attributes.cpp',
            'combat/buff_on_attributes.h',
            'benchmark/BenchmarkEquipment.cpp',
            'benchmark/BenchmarkEquipment.hpp',
            'network/input_item_helpers.hpp',
        )
        for name in retired:
            with self.subTest(name=name):
                self.assertFalse((SERVER / name).exists())
        symbols = re.compile(r"\b(?:ItemSystem|InventorySystem|ITEM_MANAGER|CItemRegistry|SafeboxSystem|CSwitchbotManager|CShopManager|ExchangeSystem)::|\becs::(?:ItemIdentity|ItemSockets|ItemOwner|MainInventoryRuntimeComponent|ExtraInventoryRuntimeComponent)\b")
        for file in SERVER.rglob("*"):
            if file.suffix not in (".cpp", ".hpp", ".h"):
                continue
            with self.subTest(file=str(file.relative_to(SERVER))):
                self.assertIsNone(symbols.search(file.read_text(encoding="utf-8-sig", errors="replace")))

if __name__ == "__main__":
    unittest.main()
