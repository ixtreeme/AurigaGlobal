#include "stdafx.h"
#include <Core/Logging.hpp>
#include "input.h"
#include "../ecs/Registry.hpp"
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "char.h"
#include "packet.h"
#include "protocol.h"
#include "utils.h"
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
void CInputDB::ItemShop(const char* data)
{
    if (static_cast<std::uint8_t>(*data) == ITEMSHOP_RELOAD)
        CHARACTER_MANAGER::Instance().RefreshItemShop();
}
#endif
