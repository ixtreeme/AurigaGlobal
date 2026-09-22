#ifndef _attr_transfer_h_
#define _attr_transfer_h_

#include <entt/entity/entity.hpp>
#include <string_view>

#define MAX_ATTR_TRANSFER_SLOT	3
#define ATTR_TRANSFER_MAX_DISTANCE	1000

bool AttrTransfer_is_open(entt::entity character);
void AttrTransfer_command(entt::entity character, std::string_view argument);
void AttrTransfer_open(entt::entity character);
void AttrTransfer_close(entt::entity character);
void AttrTransfer_clean_item(entt::entity character);
bool AttrTransfer_make(entt::entity character);
void AttrTransfer_add_item(entt::entity character, int windowSlot, int inventoryCell);
void AttrTransfer_delete_item(entt::entity character, int windowSlot);
#endif
