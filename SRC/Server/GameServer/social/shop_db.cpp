#include "stdafx.h"
#include <Core/Logging.hpp>
#include "input.h"
#include "../ecs/Registry.hpp"
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "char.h"
#include "packet.h"
#include "protocol.h"
#include "utils.h"
#include "new_offlineshop.h"
#include "new_offlineshop_manager.h"
#include "shop.h"
#include "../ecs/CharacterAccessors.hpp"
#include "../ecs/systems/SocialSystem.hpp"
#include "char_manager.h"
#include "desc.h"

void CInputDB::MyshopPricelistRes(LPDESC d, const TPacketMyshopPricelistHeader* p )
{
	const entt::entity chEntity = d ? d->GetEntity() : entt::null;

	if (!ecs::IsCharacter(chEntity))
		return;

	LOG_INFO("RecvMyshopPricelistRes name[{}]", ecs::PlayerRuntime::GetName(chEntity).data());
	ecs::SocialSystem::UseSilkBotaryReal(chEntity, p);

}

#ifdef ENABLE_ITEMSHOP
void CInputDB::ItemShop(LPDESC d, const char* c_pData)
{
	const uint8_t subIndex = *(uint8_t*)c_pData;
	c_pData += sizeof(uint8_t);

	if (subIndex == ITEMSHOP_LOAD)
		CHARACTER_MANAGER::Instance().LoadItemShopData(c_pData);
	else if (subIndex == ITEMSHOP_LOG)
	{
		if (!d)
			return;
		CHARACTER_MANAGER::Instance().LoadItemShopLogReal(d->GetEntity(), c_pData);
	}
	else if (subIndex == ITEMSHOP_BUY)
	{
		if (!d)
			return;
		CHARACTER_MANAGER::Instance().LoadItemShopBuyReal(d->GetEntity(), c_pData);
	}
}
#endif
