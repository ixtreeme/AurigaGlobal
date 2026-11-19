#pragma once
#include "TypedItemOps.hpp"
bool OpenTypedPrivateShopComposer(entt::entity,auriga::item::ItemInstanceId);
bool TryOpenTypedPrivateShop(entt::entity viewer,entt::entity seller);
bool CloseTypedPrivateShop(entt::entity);
bool HasTypedPrivateShop(entt::entity);
// Returns the dynamic payload beyond the registered minimum, or -1 when more
// bytes are needed. Malformed declared sizes close the connection.
int HandleTypedPrivateShopPacket(entt::entity,const char*,std::size_t available);
void TickTypedPrivateShops();
