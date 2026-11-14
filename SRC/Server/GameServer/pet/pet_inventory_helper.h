#pragma once
#include <list>
#include <entt/entt.hpp>
#include "../ecs/systems/ItemSystem.hpp"
#include <set>
class CPetInventoryHelper {
public:
 static const std::set<uint32_t>& GetAllowedItems() {
  static const std::set<uint32_t> items={49010,53025,53031,53250,53253,53263,55701,55706,55707,55711,631000,631001,631002,631003,631004,631005,631006,631007,631008,631009,631010,631011,631012,631013,631014,631015,631016,631017,631018,631019,631020,631021,631022,631023,631024,631025,631026,631027,631028,631029,631030,631031,631032,631033,631034,631035,631036,631037,631038,631039,631040,631041,631042,631043,631044,631045,631046,631047,631048,631049,631050,631051,631052,631053,631054,631055,631056,631057,631058,631059,631060,631061,631062,631063,631064,631065,631066,631067,631068};return items;
 }
 static bool CanMoveIntoPetInventory(entt::entity item) {
  if(!ItemSystem::IsValidItem(item)||!GetAllowedItems().contains(ItemSystem::GetItemVnum(item)))return false;
  const auto* proto=ItemSystem::GetItemProto(item);if(!proto||proto->bType!=ITEM_QUEST)return false;
  for(const auto& limit:proto->aLimits)if(limit.bType!=0)return false;
  return true;
 }
};
