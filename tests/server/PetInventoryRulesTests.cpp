#include "../../SRC/Server/GameServer/pet/pet_inventory_helper.h"
#include "../../SRC/Server/common/Headers/Header_CG.h"
#include "../../SRC/Server/common/Headers/CG_Packet.h"
#include "../../SRC/Server/common/Headers/GC_Packet.h"
#include <stdexcept>
#include <iostream>
namespace { TItemTable proto{};uint32_t currentVnum=631000;bool valid=true; }
namespace ItemSystem {
bool IsValidItem(entt::entity){return valid;}
uint32_t GetItemVnum(entt::entity){return currentVnum;}
const TItemTable* GetItemProto(entt::entity){return &proto;}
}
int main()
{
 try {
  auto require=[](bool value){if(!value)throw std::runtime_error("pet inventory eligibility regression");};
  proto.bType=ITEM_QUEST;const auto item=static_cast<entt::entity>(1);
  require(CPetInventoryHelper::GetAllowedItems().size()==79);
  for(uint32_t id=631000;id<=631068;++id){currentVnum=id;require(CPetInventoryHelper::CanMoveIntoPetInventory(item));}
  for(uint32_t id=630000;id<=630202;++id){currentVnum=id;require(!CPetInventoryHelper::CanMoveIntoPetInventory(item));}
  for(uint32_t id:{53250u,55701u,55706u,55707u,55711u,53263u,53031u,53253u,49010u,53025u}){currentVnum=id;require(CPetInventoryHelper::CanMoveIntoPetInventory(item));}
  currentVnum=631000;
  for(auto& limit:proto.aLimits){limit.bType=1;require(!CPetInventoryHelper::CanMoveIntoPetInventory(item));limit.bType=0;}
  proto.bType=ITEM_COSTUME;require(!CPetInventoryHelper::CanMoveIntoPetInventory(item));proto.bType=ITEM_QUEST;
  valid=false;require(!CPetInventoryHelper::CanMoveIntoPetInventory(item));valid=true;
  currentVnum=53011;require(!CPetInventoryHelper::CanMoveIntoPetInventory(item));
  static_assert(sizeof(TPacketCGPetInventoryCheckin)==6);
  static_assert(sizeof(TPacketCGPetInventoryCheckout)==6);
  static_assert(sizeof(TPacketCGPetInventoryItemMove)==5);
  static_assert(sizeof(TPacketGCPetInventory)==7);
  static_assert(sizeof(TPetInventoryItemData)==94);
  static_assert(PET_INVENTORY==14);
  std::cout<<"PET_INVENTORY_RULES=SUCCESS allowed=79 teraPets=69 rejectedMounts=203\n";
  return 0;
 } catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
