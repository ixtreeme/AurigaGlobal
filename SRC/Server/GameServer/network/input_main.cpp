#include "stdafx.h"
#include "../ecs/systems/InventorySystem.hpp"
#include "../ecs/systems/SessionSystem.hpp"
#include "../ecs/systems/MountSystem.hpp"
#include "../ecs/systems/ViewSystem.hpp"
#include "../ecs/systems/AffectSystem.hpp"
#include <Core/Logging.hpp>
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "../ecs/systems/AcceSystem.hpp"
#include "../ecs/systems/CombatSystem.hpp"
#include "../ecs/systems/SocialSystem.hpp"
#include "../ecs/systems/QuestSystem.hpp"
#include "../ecs/systems/SkillSystem.hpp"
#include "../ecs/services/SpatialService.hpp"
#include "../ecs/systems/MovementSystem.hpp"
#include "../ecs/systems/NetworkSyncSystem.hpp"
#include "../ecs/CharacterAccessors.hpp"


#include "constants.h"
#include "config.h"
#include "utils.h"
#include "desc_client.h"
#include "desc_manager.h"
#include "buffer_manager.h"
#include "packet.h"
#include "protocol.h"
#include "char_interface.hpp"
#include "char_manager.h"
#include "item.h"
#include "item_manager.h"
#include "cmd.h"
#include "shop.h"
#include "shop_manager.h"
#include "safebox.h"
#include "regen.h"
#include "battle.h"
#include "exchange.h"
#include "questmanager.h"
#include "profiler.h"
#include "messenger_manager.h"
#include "party.h"
#include "p2p.h"
#include "affect.h"
#include "guild.h"
#include "guild_manager.h"
#include "log.h"
#include "banword.h"
#include "empire_text_convert.h"
#include "unique_item.h"
#include "building.h"
#include "locale_service.h"
#include "gm.h"
#include "spam.h"
#include "ani.h"
#include "motion.h"
#include "OXEvent.h"
#include "locale_service.h"
#include "DragonSoul.h"
#include "../ecs/AIHelpers.hpp"
#include "../ecs/EntityFactory.hpp"
#ifdef __NEWPET_SYSTEM__
#include "New_PetSystem.h"
#endif
#include "belt_inventory_helper.h" // @fixme119
#include "mount_inventory_helper.h"
#include "MountInventory.h"
#include "input.h"
#include "input_item_helpers.hpp"
#include "../ecs/Registry.hpp"
#include "../ecs/VIDRegistry.hpp"
#include "../ecs/components/combat_components.hpp"
#include "../ecs/components/identity_components.hpp"
#include "../ecs/components/dirty_components.hpp"
#include "../ecs/components/movement_components.hpp"
#include "../ecs/systems/ItemSystem.hpp"
#include "../ecs/systems/PointSystem.hpp"

#ifdef ENABLE_SWITCHBOT
#include "new_switchbot.h"
#endif
#ifdef __ENABLE_NEW_OFFLINESHOP__
#include "new_offlineshop.h"
#include "new_offlineshop_manager.h"
#endif
#ifdef ENABLE_BATTLE_PASS
#include "battle_pass.h"
#include "../ecs/systems/MountSystem.hpp"
#include "../ecs/systems/ActivitySystem.hpp"
#endif

#ifdef ENABLE_FEATURES_REFINE_SYSTEM
	#include "refine.h"
#endif

#ifdef ENABLE_WHISPER_ADMIN_SYSTEM
	#include "whisper_admin.h"
#endif
#ifdef __INGAME_WIKI__
#include "mob_manager.h"
#endif
#include <common/CommonDefines.h>
#include "../ecs/systems/DragonSoulSystem.hpp"


#define ENABLE_CHECK_GHOSTMODE


static const int ComboSequenceBySkillLevel[3][8] =
{
	// 0   1   2   3   4   5   6   7
	{ 14, 15, 16, 17,  0,  0,  0,  0 },
	{ 14, 15, 16, 18, 20,  0,  0,  0 },
	{ 14, 15, 16, 18, 19, 17,  0,  0 },
};


#ifdef __SKILL_COLOR_SYSTEM__
void CInputMain::SetSkillColor(entt::entity character, const char* pcData)
{
    if (!pcData)
        return;
    TPacketCGSkillColor packet {};
    std::memcpy(&packet, pcData, sizeof(packet));
    SkillSystem::ChangeSkillColor(character, packet.skill,
        {packet.col1, packet.col2, packet.col3, packet.col4, packet.col5});
}
#endif


// SCRIPT_SELECT_ITEM
// END_OF_SCRIPT_SELECT_ITEM


bool IsInputInventoryPosition(TItemPos position)
{
    return position.window_type == INVENTORY || position.window_type == DRAGON_SOUL_INVENTORY
#ifdef ENABLE_EXTRA_INVENTORY
        || position.window_type == EXTRA_INVENTORY
#endif
        ;
}

bool IsInputItemAt(entt::entity owner, entt::entity item, TItemPos position)
{
    if (!ecs::PlayerRuntime::IsPC(owner) || !ItemSystem::IsValidItem(item))
        return false;
    const auto* ownership = g_registry.try_get<ecs::ItemOwner>(item);
    const auto* location = g_registry.try_get<ecs::ItemLocation>(item);
    return ownership && ownership->owner == owner && location &&
        location->window == position.window_type && location->cell == position.cell;
}

bool IsDetachedInputItem(entt::entity item)
{
    if (!ItemSystem::IsValidItem(item))
        return false;
    const auto* owner = g_registry.try_get<ecs::ItemOwner>(item);
    const auto* location = g_registry.try_get<ecs::ItemLocation>(item);
    return owner && owner->owner == entt::null && location && location->window == RESERVED_WINDOW;
}

bool RestoreInputItem(entt::entity owner, entt::entity item, TItemPos position)
{
    return ecs::PlayerRuntime::IsPC(owner) && IsDetachedInputItem(item) &&
        InventorySystem::IsEmptyItemGrid(owner, position, ItemSystem::GetItemSize(item)) &&
        ItemSystem::PlaceItemEcs(owner, item, position.window_type, position.cell);
}


// PARTY_JOIN_BUG_FIX

// END_OF_PARTY_JOIN_BUG_FIX


void CInputMain::Hack(entt::entity character, const char * c_pData)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate Hack handler ECS
// DUAL-PATH: legacy only during migration window
	TPacketCGHack * p = (TPacketCGHack *) c_pData;

	char buf[sizeof(p->szBuf)];
	strlcpy(buf, p->szBuf, sizeof(buf));

	LOG_ERROR("HACK_DETECT: {} {}", ecs::PlayerRuntime::GetName(character).data(), buf);

	ecs::PlayerRuntime::GetDesc(character)->SetPhase(PHASE_CLOSE);
}


#ifdef __ENABLE_NEW_OFFLINESHOP__
#include "new_offlineshop.h"
#include "new_offlineshop_manager.h"
template <class T>
bool CanDecode(T* p, int buffleft) {
	return buffleft >= (int)sizeof(T);
}

template <class T>
const char* Decode(T*& pObj, const char* data, int* pbufferLeng = nullptr, int* piBufferLeft=nullptr)
{
	pObj = (T*) data;

	if(pbufferLeng)
		*pbufferLeng += sizeof(T);

	if(piBufferLeft)
		*piBufferLeft -= sizeof(T);

	return data + sizeof(T);
}

int OfflineshopPacketCreateNewShop(entt::entity ch, const char* data, int iBufferLeft)
{
	TSubPacketCGShopCreate* pack = nullptr;
	if(!CanDecode(pack, iBufferLeft))
		return -1;

	int iExtra=0;
	data = Decode(pack, data, &iExtra, &iBufferLeft);

	offlineshop::TShopInfo& rShopInfo = pack->shop;

	//fix flooding
	if (rShopInfo.dwCount > 500 || rShopInfo.dwCount == 0) {
		LOG_ERROR("tried to open a shop with 500+ items.");
		return -1;
	}

	std::vector<offlineshop::TShopItemInfo> vec;
	vec.reserve(rShopInfo.dwCount);

	offlineshop::TShopItemInfo* pItem=nullptr;


	for (uint32_t i = 0; i < rShopInfo.dwCount; ++i)
	{
		if(!CanDecode(pItem, iBufferLeft))
			return -1;

		data = Decode(pItem, data, &iExtra, &iBufferLeft);
		vec.push_back(*pItem);
	}

	offlineshop::CShopManager& rManager = offlineshop::GetManager();
	if(!rManager.RecvShopCreateNewClientPacket(ch, rShopInfo, vec)) {
		if (ecs::PlayerRuntime::IsValid(ch)) {
			offlineshop::SendChatPacket(ch, offlineshop::CHAT_PACKET_CANNOT_CREATE_SHOP);
			ecs::ChatSystem::Send(ch, CHAT_TYPE_COMMAND, "RefreshOfflineShop");
		}
	}

	return iExtra;
}


int OfflineshopPacketChangeShopName(entt::entity ch, const char* data, int iBufferLeft)
{
	TSubPacketCGShopChangeName* pack = nullptr;
	if(!CanDecode(pack, iBufferLeft))
		return -1;

	int iExtra=0;
	data = Decode(pack,data, &iExtra, &iBufferLeft);

	offlineshop::CShopManager& rManager = offlineshop::GetManager();
	if(!rManager.RecvShopChangeNameClientPacket(ch, pack->szName))
		offlineshop::SendChatPacket(ch, offlineshop::CHAT_PACKET_CANNOT_CHANGE_NAME);

	return iExtra;
}


int OfflineshopPacketForceCloseShop(entt::entity ch, const char* data, int iBufferLeft)
{
	offlineshop::CShopManager& rManager = offlineshop::GetManager();
	if(!rManager.RecvShopForceCloseClientPacket(ch))
		offlineshop::SendChatPacket(ch, offlineshop::CHAT_PACKET_CANNOT_FORCE_CLOSE);

	return 0;
}


int OfflineshopPacketRequestShopList(entt::entity ch, const char* data, int iBufferLeft)
{
	offlineshop::CShopManager& rManager = offlineshop::GetManager();
	rManager.RecvShopRequestListClientPacket(ch);
	return 0;
}


int OfflineshopPacketOpenShop(entt::entity ch, const char* data, int iBufferLeft)
{
	TSubPacketCGShopOpen* pack = nullptr;
	if(!CanDecode(pack, iBufferLeft))
		return -1;

	int iExtra=0;
	data = Decode(pack,data, &iExtra, &iBufferLeft);

	offlineshop::CShopManager& rManager = offlineshop::GetManager();
	if(!rManager.RecvShopOpenClientPacket(ch,pack->dwOwnerID))
		offlineshop::SendChatPacket(ch, offlineshop::CHAT_PACKET_CANNOT_OPEN_SHOP);

	return iExtra;
}


int OfflineshopPacketOpenShowOwner(entt::entity ch, const char* data, int iBufferLeft)
{
	offlineshop::CShopManager& rManager = offlineshop::GetManager();
	if(!rManager.RecvShopOpenMyShopClientPacket(ch))
		offlineshop::SendChatPacket(ch, offlineshop::CHAT_PACKET_CANNOT_OPEN_SHOP_OWNER);

	return 0;
}


int OfflineshopPacketBuyItem(entt::entity ch, const char* data, int iBufferLeft)
{
	TSubPacketCGShopBuyItem* pack = nullptr;
	if(!CanDecode(pack, iBufferLeft))
		return -1;

	int iExtra=0;
	data = Decode(pack,data, &iExtra, &iBufferLeft);

	offlineshop::CShopManager& rManager = offlineshop::GetManager();
	if(!rManager.RecvShopBuyItemClientPacket(ch, pack->dwOwnerID, pack->dwItemID, pack->bIsSearch, pack->TotalPriceSeen)) //patch seen price check
		offlineshop::SendChatPacket(ch, offlineshop::CHAT_PACKET_CANNOT_BUY_ITEM);

	return iExtra;
}


int OfflineshopPacketAddItem(entt::entity ch, const char* data, int iBufferLeft)
{
	TSubPacketCGAddItem* pack = nullptr;
	if(!CanDecode(pack, iBufferLeft))
		return -1;

	int iExtra=0;
	data = Decode(pack,data, &iExtra, &iBufferLeft);

	offlineshop::CShopManager& rManager = offlineshop::GetManager();
	if(!rManager.RecvShopAddItemClientPacket(ch, pack->pos, pack->price))
		offlineshop::SendChatPacket(ch, offlineshop::CHAT_PACKET_CANNOT_ADD_ITEM);

	return iExtra;
}


int OfflineshopPacketRemoveItem(entt::entity ch, const char* data, int iBufferLeft)
{
	TSubPacketCGRemoveItem* pack = nullptr;
	if(!CanDecode(pack, iBufferLeft))
		return -1;

	int iExtra=0;
	data = Decode(pack,data, &iExtra, &iBufferLeft);

	offlineshop::CShopManager& rManager = offlineshop::GetManager();
	if(!rManager.RecvShopRemoveItemClientPacket(ch, pack->dwItemID))
		offlineshop::SendChatPacket(ch, offlineshop::CHAT_PACKET_CANNOT_REMOVE_ITEM);

	return iExtra;
}


int OfflineshopPacketEditItem(entt::entity ch, const char* data, int iBufferLeft)
{
	TSubPacketCGEditItem* pack = nullptr;
	if(!CanDecode(pack, iBufferLeft))
		return -1;

	int iExtra=0;
	data = Decode(pack,data, &iExtra, &iBufferLeft);

	offlineshop::CShopManager& rManager = offlineshop::GetManager();
	if(!rManager.RecvShopEditItemClientPacket(ch, pack->dwItemID, pack->price))
		offlineshop::SendChatPacket(ch, offlineshop::CHAT_PACKET_CANNOT_EDIT_ITEM);

	return iExtra;
}


int OfflineshopPacketFilterRequest(entt::entity ch, const char* data, int iBufferLeft)
{
	TSubPacketCGFilterRequest* pack = nullptr;
	if(!CanDecode(pack, iBufferLeft))
		return -1;

	int iExtra=0;
	data = Decode(pack,data, &iExtra, &iBufferLeft);

	offlineshop::CShopManager& rManager = offlineshop::GetManager();
	if(!rManager.RecvShopFilterRequestClientPacket(ch, pack->filter))
		offlineshop::SendChatPacket(ch, offlineshop::CHAT_PACKET_CANNOT_FILTER);

	return iExtra;
}


int OfflineshopPacketCreateOffer(entt::entity ch, const char* data, int iBufferLeft)
{
	TSubPacketCGOfferCreate* pack = nullptr;
	if(!CanDecode(pack, iBufferLeft))
		return -1;

	int iExtra=0;
	data = Decode(pack,data, &iExtra, &iBufferLeft);

	offlineshop::CShopManager& rManager = offlineshop::GetManager();
	if(!rManager.RecvShopCreateOfferClientPacket(ch, pack->offer))
		offlineshop::SendChatPacket(ch, offlineshop::CHAT_PACKET_CANNOT_CREATE_OFFER);

	return iExtra;
}


int OfflineshopPacketAcceptOffer(entt::entity ch, const char* data, int iBufferLeft)
{
	TSubPacketCGOfferAccept* pack = nullptr;
	if(!CanDecode(pack, iBufferLeft))
		return -1;

	int iExtra=0;
	data = Decode(pack,data, &iExtra, &iBufferLeft);

	offlineshop::CShopManager& rManager = offlineshop::GetManager();
	if(!rManager.RecvShopAcceptOfferClientPacket(ch, pack->dwOfferID))
		offlineshop::SendChatPacket(ch, offlineshop::CHAT_PACKET_CANNOT_ACCEPT_OFFER);

	return iExtra;
}


int OfflineshopPacketOfferCancel(entt::entity ch, const char* data, int iBufferLeft)
{
	TSubPacketCGOfferCancel* pack = nullptr;
	if(!CanDecode(pack, iBufferLeft))
		return -1;

	int iExtra=0;
	data = Decode(pack,data, &iExtra, &iBufferLeft);

	offlineshop::CShopManager& rManager = offlineshop::GetManager();
	if(!rManager.RecvShopCancelOfferClientPacket(ch, pack->dwOfferID, pack->dwOwnerID))
		offlineshop::SendChatPacket(ch, offlineshop::CHAT_PACKET_CANNOT_ACCEPT_OFFER);

	return iExtra;
}


int OfflineshopPacketOfferListRequest(entt::entity ch)
{
	offlineshop::CShopManager& rManager = offlineshop::GetManager();
	rManager.RecvOfferListRequestPacket(ch);
	return 0;
}


int OfflineshopPacketOpenSafebox(entt::entity ch, const char* data, int iBufferLeft)
{
	offlineshop::CShopManager& rManager = offlineshop::GetManager();
	if(!rManager.RecvShopSafeboxOpenClientPacket(ch))
		offlineshop::SendChatPacket(ch, offlineshop::CHAT_PACKET_CANNOT_OPEN_SAFEBOX);

	return 0;
}


int OfflineshopPacketCloseBoard(entt::entity ch)
{
	offlineshop::CShopManager& rManager = offlineshop::GetManager();
	rManager.RecvCloseBoardClientPacket(ch);
	return 0;
}

int OfflineshopPacketCloseMyAuction(entt::entity ch)
{
	offlineshop::CShopManager& rManager = offlineshop::GetManager();
	rManager.RecvCloseMyAuction(ch);
	return 0;
}

int OfflineshopPacketGetItemSafebox(entt::entity ch, const char* data, int iBufferLeft)
{
	TSubPacketCGShopSafeboxGetItem* pack = nullptr;
	if(!CanDecode(pack, iBufferLeft))
		return -1;

	int iExtra=0;
	data = Decode(pack,data, &iExtra, &iBufferLeft);

	offlineshop::CShopManager& rManager = offlineshop::GetManager();
	if(!rManager.RecvShopSafeboxGetItemClientPacket(ch, pack->dwItemID))
		offlineshop::SendChatPacket(ch, offlineshop::CHAT_PACKET_CANNOT_SAFEBOX_GET_ITEM);

	return iExtra;

}


int OfflineshopPacketGetValutesSafebox(entt::entity ch, const char* data, int iBufferLeft)
{
	TSubPacketCGShopSafeboxGetValutes* pack = nullptr;
	if(!CanDecode(pack, iBufferLeft))
		return -1;

	int iExtra=0;
	data = Decode(pack,data, &iExtra, &iBufferLeft);

	offlineshop::CShopManager& rManager = offlineshop::GetManager();
	if(!rManager.RecvShopSafeboxGetValutesClientPacket(ch, pack->valutes))
		offlineshop::SendChatPacket(ch, offlineshop::CHAT_PACKET_CANNOT_SAFEBOX_GET_VALUTES);

	return iExtra;
}


int OfflineshopPacketCloseSafebox(entt::entity ch, const char* data, int iBufferLeft)
{
	offlineshop::CShopManager& rManager = offlineshop::GetManager();
	if(!rManager.RecvShopSafeboxCloseClientPacket(ch))
		offlineshop::SendChatPacket(ch, offlineshop::CHAT_PACKET_CANNOT_SAFEBOX_CLOSE);

	return 0;
}


int OfflineshopPacketListRequest(entt::entity ch, const char* data, int iBufferLeft)
{
	offlineshop::CShopManager& rManager = offlineshop::GetManager();
	if(!rManager.RecvAuctionListRequestClientPacket(ch))
		offlineshop::SendChatPacket(ch, offlineshop::CHAT_PACKET_AUCTION_CANNOT_SEND_LIST);

	return 0;
}


int OfflineshopPacketOpenAuctionRequest(entt::entity ch, const char* data, int iBufferLeft)
{
	TSubPacketCGAuctionOpenRequest* pack = nullptr;
	if(!CanDecode(pack, iBufferLeft))
		return -1;

	int iExtra=0;
	data = Decode(pack,data, &iExtra, &iBufferLeft);

	offlineshop::CShopManager& rManager = offlineshop::GetManager();
	if(!rManager.RecvAuctionOpenRequestClientPacket(ch, pack->dwOwnerID))
		offlineshop::SendChatPacket(ch, offlineshop::CHAT_PACKET_AUCTION_CANNOT_OPEN_AUCTION);

	return iExtra;
}


int OfflineshopPacketOpenMyAuctionRequest(entt::entity ch, const char* data, int iBufferLeft)
{
	offlineshop::CShopManager& rManager = offlineshop::GetManager();
	if(!rManager.RecvMyAuctionOpenRequestClientPacket(ch))
		offlineshop::SendChatPacket(ch, offlineshop::CHAT_PACKET_AUCTION_CANNOT_SEND_LIST);

	return 0;
}


int OfflineshopPacketCreateAuction(entt::entity ch, const char* data, int iBufferLeft)
{
	TSubPacketCGAuctionCreate* pack = nullptr;
	if(!CanDecode(pack, iBufferLeft))
		return -1;

	int iExtra=0;
	data = Decode(pack,data, &iExtra, &iBufferLeft);

	offlineshop::CShopManager& rManager = offlineshop::GetManager();
	if(!rManager.RecvAuctionCreateClientPacket(ch, pack->dwDuration, pack->init_price, pack->pos))
		offlineshop::SendChatPacket(ch, offlineshop::CHAT_PACKET_AUCTION_CANNOT_CREATE_AUCTION);

	return iExtra;
}


int OfflineshopPacketAddOffer(entt::entity ch, const char* data, int iBufferLeft)
{
	TSubPacketCGAuctionAddOffer* pack = nullptr;
	if(!CanDecode(pack, iBufferLeft))
		return -1;

	int iExtra=0;
	data = Decode(pack,data, &iExtra, &iBufferLeft);

	offlineshop::CShopManager& rManager = offlineshop::GetManager();
	if(!rManager.RecvAuctionAddOfferClientPacket(ch, pack->dwOwnerID, pack->price))
		offlineshop::SendChatPacket(ch, offlineshop::CHAT_PACKET_AUCTION_CANNOT_ADD_OFFER);

	return iExtra;
}


int OfflineshopPacketExitFromAuction(entt::entity ch, const char* data, int iBufferLeft)
{
	TSubPacketCGAuctionExitFrom* pack = nullptr;
	if(!CanDecode(pack, iBufferLeft))
		return -1;

	int iExtra=0;
	data = Decode(pack,data, &iExtra, &iBufferLeft);

	offlineshop::CShopManager& rManager = offlineshop::GetManager();
	rManager.RecvAuctionExitFromAuction(ch, pack->dwOwnerID);
	return iExtra;
}


#ifdef ENABLE_NEW_SHOP_IN_CITIES
int OfflineshopPacketClickEntity(entt::entity ch, const char* data, int iBufferLeft)
{
	TSubPacketCGShopClickEntity* pack = nullptr;
	if(!CanDecode(pack, iBufferLeft))
		return -1;

	int iExtra=0;
	data = Decode(pack, data, &iExtra, &iBufferLeft);


	offlineshop::CShopManager& rManager = offlineshop::GetManager();
	rManager.RecvShopClickEntity(ch, pack->dwShopVID);
	return iExtra;
}

#endif


int OfflineshopPacket(const char* data , entt::entity ch, int32_t iBufferLeft)
{
	unsigned int iBufferLeftCompare = iBufferLeft;
	if(iBufferLeftCompare < sizeof(TPacketCGNewOfflineShop))
		return -1;

	TPacketCGNewOfflineShop* pPack=nullptr;
	iBufferLeft -= sizeof(TPacketCGNewOfflineShop);
	data = Decode(pPack, data);


	switch (pPack->bSubHeader)
	{

	case offlineshop::SUBHEADER_CG_SHOP_CREATE_NEW:				return /*sizeof(TPacketCGNewOfflineShop) +*/ OfflineshopPacketCreateNewShop(ch,data,iBufferLeft);
	case offlineshop::SUBHEADER_CG_SHOP_CHANGE_NAME:			return /*sizeof(TPacketCGNewOfflineShop) +*/ OfflineshopPacketChangeShopName(ch,data,iBufferLeft);
	case offlineshop::SUBHEADER_CG_SHOP_FORCE_CLOSE:			return /*sizeof(TPacketCGNewOfflineShop) +*/ OfflineshopPacketForceCloseShop(ch,data,iBufferLeft);
	case offlineshop::SUBHEADER_CG_SHOP_REQUEST_SHOPLIST:		return /*sizeof(TPacketCGNewOfflineShop) +*/ OfflineshopPacketRequestShopList(ch,data,iBufferLeft);
	case offlineshop::SUBHEADER_CG_SHOP_OPEN:					return /*sizeof(TPacketCGNewOfflineShop) +*/ OfflineshopPacketOpenShop(ch,data,iBufferLeft);
	case offlineshop::SUBHEADER_CG_SHOP_OPEN_OWNER:				return /*sizeof(TPacketCGNewOfflineShop) +*/ OfflineshopPacketOpenShowOwner(ch,data,iBufferLeft);

	case offlineshop::SUBHEADER_CG_SHOP_BUY_ITEM:				return /*sizeof(TPacketCGNewOfflineShop) +*/ OfflineshopPacketBuyItem(ch, data , iBufferLeft);
	case offlineshop::SUBHEADER_CG_SHOP_ADD_ITEM:				return /*sizeof(TPacketCGNewOfflineShop) +*/ OfflineshopPacketAddItem(ch,data,iBufferLeft);
	case offlineshop::SUBHEADER_CG_SHOP_REMOVE_ITEM:			return /*sizeof(TPacketCGNewOfflineShop) +*/ OfflineshopPacketRemoveItem(ch,data,iBufferLeft);
	case offlineshop::SUBHEADER_CG_SHOP_EDIT_ITEM:				return /*sizeof(TPacketCGNewOfflineShop) +*/ OfflineshopPacketEditItem(ch,data,iBufferLeft);

	case offlineshop::SUBHEADER_CG_SHOP_FILTER_REQUEST:			return /*sizeof(TPacketCGNewOfflineShop) +*/ OfflineshopPacketFilterRequest(ch,data,iBufferLeft);

	case offlineshop::SUBHEADER_CG_SHOP_OFFER_CREATE:			return /*sizeof(TPacketCGNewOfflineShop) +*/ OfflineshopPacketCreateOffer(ch,data,iBufferLeft);
	case offlineshop::SUBHEADER_CG_SHOP_OFFER_ACCEPT:			return /*sizeof(TPacketCGNewOfflineShop) +*/ OfflineshopPacketAcceptOffer(ch,data,iBufferLeft);
	case offlineshop::SUBHEADER_CG_SHOP_REQUEST_OFFER_LIST:		return /*sizeof(TPacketCGNewOfflineShop) +*/ OfflineshopPacketOfferListRequest(ch);
	case offlineshop::SUBHEADER_CG_SHOP_OFFER_CANCEL:			return /*sizeof(TPacketCGNewOfflineShop) +*/ OfflineshopPacketOfferCancel(ch, data,iBufferLeft);

	case offlineshop::SUBHEADER_CG_SHOP_SAFEBOX_OPEN:			return /*sizeof(TPacketCGNewOfflineShop) +*/ OfflineshopPacketOpenSafebox(ch,data,iBufferLeft);
	case offlineshop::SUBHEADER_CG_SHOP_SAFEBOX_GET_ITEM:		return /*sizeof(TPacketCGNewOfflineShop) +*/ OfflineshopPacketGetItemSafebox(ch,data,iBufferLeft);
	case offlineshop::SUBHEADER_CG_SHOP_SAFEBOX_GET_VALUTES:	return /*sizeof(TPacketCGNewOfflineShop) +*/ OfflineshopPacketGetValutesSafebox(ch,data,iBufferLeft);
	case offlineshop::SUBHEADER_CG_SHOP_SAFEBOX_CLOSE:			return /*sizeof(TPacketCGNewOfflineShop) +*/ OfflineshopPacketCloseSafebox(ch,data,iBufferLeft);

	case offlineshop::SUBHEADER_CG_AUCTION_LIST_REQUEST:		return /*sizeof(TPacketCGNewOfflineShop) +*/ OfflineshopPacketListRequest(ch, data, iBufferLeft);
	case offlineshop::SUBHEADER_CG_AUCTION_OPEN_REQUEST:		return /*sizeof(TPacketCGNewOfflineShop) +*/ OfflineshopPacketOpenAuctionRequest(ch, data, iBufferLeft);
	case offlineshop::SUBHEADER_CG_MY_AUCTION_OPEN_REQUEST:		return /*sizeof(TPacketCGNewOfflineShop) +*/ OfflineshopPacketOpenMyAuctionRequest(ch, data, iBufferLeft);
	case offlineshop::SUBHEADER_CG_CREATE_AUCTION:				return /*sizeof(TPacketCGNewOfflineShop) +*/ OfflineshopPacketCreateAuction(ch, data, iBufferLeft);
	case offlineshop::SUBHEADER_CG_AUCTION_ADD_OFFER:			return /*sizeof(TPacketCGNewOfflineShop) +*/ OfflineshopPacketAddOffer(ch, data, iBufferLeft);
	case offlineshop::SUBHEADER_CG_EXIT_FROM_AUCTION:			return /*sizeof(TPacketCGNewOfflineShop) +*/ OfflineshopPacketExitFromAuction(ch, data, iBufferLeft);

	case offlineshop::SUBHEADER_CG_CLOSE_BOARD:					return /*sizeof(TPacketCGNewOfflineshop) +*/ OfflineshopPacketCloseBoard(ch);
#ifdef ENABLE_NEW_SHOP_IN_CITIES
	case offlineshop::SUBHEADER_CG_CLICK_ENTITY:				return /*sizeof(TPacketCGNewOfflineshop) +*/ OfflineshopPacketClickEntity(ch, data, iBufferLeft);
#endif
	case offlineshop::SUBHEADER_CG_AUCTION_CLOSE:
		return /*sizeof(TPacketCGNewOfflineshop) +*/ OfflineshopPacketCloseMyAuction(ch);

	default:
		LOG_ERROR("UNKNOWN SUBHEADER {} ", pPack->bSubHeader);
		return -1;
	}

}
#endif


int CInputMain::Analyze(LPDESC d, uint8_t bHeader, const char * c_pData)
{

	if (!d || !c_pData)
		return 0;
	const entt::entity character = d->GetEntity();
	if (!ecs::PlayerRuntime::IsPC(character) || ecs::PlayerRuntime::GetDesc(character) != d)
	{
		LOG_ERROR("no character on desc");
		d->SetPhase(PHASE_CLOSE);
		return (0);
	}


	int iExtraLen = 0;

	if (test_server && bHeader != HEADER_CG_MOVE)
		LOG_INFO("CInputMain::Analyze() ==> Header [{}] ", bHeader);

	switch (bHeader)
	{
		case HEADER_CG_PONG:
			Pong(d);
			break;

		case HEADER_CG_TIME_SYNC:
			Handshake(d, c_pData);
			break;

		case HEADER_CG_CHAT:
			if ((iExtraLen = Chat(character, c_pData, m_iBufferLeft)) < 0)
				return -1;
			break;

		case HEADER_CG_WHISPER:
			if ((iExtraLen = Whisper(character, c_pData, m_iBufferLeft)) < 0)
				return -1;
			break;

		case HEADER_CG_MOVE:
			Move(character, c_pData);
			// @fixme103 (removed CheckClientVersion since useless in here)
			break;

		case HEADER_CG_CHARACTER_POSITION:
			Position(character, c_pData);
			break;

		case HEADER_CG_ITEM_USE:
			if (!ecs::PlayerRuntime::IsObserverMode(character))
				ItemUse(character, c_pData);
			break;

		case HEADER_CG_ITEM_DROP:
			if (!ecs::PlayerRuntime::IsObserverMode(character))
			{
				ItemDrop(character, c_pData);
			}
			break;

#ifdef ENABLE_ACCE_SYSTEM
		case HEADER_CG_ACCE:
			Acce(character, c_pData);
			break;
#endif

		case HEADER_CG_ITEM_DROP2:
			if (!ecs::PlayerRuntime::IsObserverMode(character))
				ItemDrop2(character, c_pData);
			break;
		case HEADER_CG_ITEM_DESTROY:
			if (!ecs::PlayerRuntime::IsObserverMode(character))
				ItemDestroy(character, c_pData);
			break;
		case HEADER_CG_ITEM_DIVISION:
			{
				if (!ecs::PlayerRuntime::IsObserverMode(character))
					ItemDivision(character, c_pData);
			}
			break;
		case HEADER_CG_ITEM_MOVE:
			if (!ecs::PlayerRuntime::IsObserverMode(character))
				ItemMove(character, c_pData);
			break;


#ifdef __ENABLE_EXTEND_INVEN_SYSTEM__
		case ENVANTER_BLACK:
			if (!ecs::PlayerRuntime::IsObserverMode(character))
				InventoryExpansion(character, c_pData);
		break;
#endif

		case HEADER_CG_ITEM_PICKUP:
			if (!ecs::PlayerRuntime::IsObserverMode(character))
				ItemPickup(character, c_pData);
			break;

		case HEADER_CG_ITEM_USE_TO_ITEM:
			if (!ecs::PlayerRuntime::IsObserverMode(character))
				ItemToItem(character, c_pData);
			break;

		case HEADER_CG_ITEM_GIVE:
			if (!ecs::PlayerRuntime::IsObserverMode(character))
				ItemGive(character, c_pData);
			break;

		case HEADER_CG_EXCHANGE:
			if (!ecs::PlayerRuntime::IsObserverMode(character))
				Exchange(character, c_pData);
			break;

		case HEADER_CG_ATTACK:
		case HEADER_CG_SHOOT:
			if (!ecs::PlayerRuntime::IsObserverMode(character))
			{
				Attack(character, bHeader, c_pData);
			}
			break;

		case HEADER_CG_USE_SKILL:
			if (!ecs::PlayerRuntime::IsObserverMode(character))
				UseSkill(character, c_pData);
			break;

#ifdef __SKILL_COLOR_SYSTEM__
		case HEADER_CG_SKILL_COLOR:
			SetSkillColor(character, c_pData);
			break;
#endif
#ifdef ENABLE_OPENSHOP_PACKET
		case HEADER_CG_OPENSHOP: {
				TPacketOpenShop* p = reinterpret_cast<TPacketOpenShop*>((void*)c_pData);
				if (p->shopid > 0) {
					if (!(ecs::PlayerRuntime::IsObserverMode(character) || ecs::SessionSystem::IsSafeboxOpen(character) || ecs::SocialSystem::HasExchange(character) || ecs::SessionSystem::IsCubeOpen(character) || CombatSystem::IsStun(character) || CombatSystem::IsDead(character)
#ifdef __ATTR_TRANSFER_SYSTEM__
						 || AttrTransfer_is_open(character)
#endif
#ifdef __ENABLE_NEW_OFFLINESHOP__
						 || (g_registry.try_get<ecs::ShopState>(character) &&
						     (g_registry.get<ecs::ShopState>(character).offlineShopGuest || g_registry.get<ecs::ShopState>(character).auctionGuest))
#endif
					)) {
						const entt::entity shop = CShopManager::instance().Get(p->shopid);
						if (shop != entt::null) {
							ShopSystem::AddGuest(shop, character, 0, false);
							ecs::SocialSystem::SetShopOwner(character, entt::null);
						}
					}
				}
			} break;
#endif
		case HEADER_CG_QUICKSLOT_ADD:
			QuickslotAdd(character, c_pData);
			break;

		case HEADER_CG_QUICKSLOT_DEL:
			QuickslotDelete(character, c_pData);
			break;

		case HEADER_CG_QUICKSLOT_SWAP:
			QuickslotSwap(character, c_pData);
			break;

		case HEADER_CG_SHOP:
			if ((iExtraLen = Shop(character, c_pData, m_iBufferLeft)) < 0)
				return -1;
			break;

		case HEADER_CG_MESSENGER:
			if ((iExtraLen = Messenger(character, c_pData, m_iBufferLeft))<0)
				return -1;
			break;

#ifdef ENABLE_BATTLE_PASS
		case HEADER_CG_BATTLE_PASS:
			if ((iExtraLen = BattlePass(character, c_pData, m_iBufferLeft)) < 0)
				return -1;
			break;
#endif

		case HEADER_CG_ON_CLICK:
			OnClick(character, c_pData);
			break;

		case HEADER_CG_SYNC_POSITION:
			if ((iExtraLen = SyncPosition(character, c_pData, m_iBufferLeft)) < 0)
				return -1;
			break;

		case HEADER_CG_ADD_FLY_TARGETING:
		case HEADER_CG_FLY_TARGETING:
			FlyTarget(character, c_pData, bHeader);
			break;

		case HEADER_CG_SCRIPT_BUTTON:
			ScriptButton(character, c_pData);
			break;

			// SCRIPT_SELECT_ITEM
		case HEADER_CG_SCRIPT_SELECT_ITEM:
			ScriptSelectItem(character, c_pData);
			break;
			// END_OF_SCRIPT_SELECT_ITEM

		case HEADER_CG_SCRIPT_ANSWER:
			ScriptAnswer(character, c_pData);
			break;

		case HEADER_CG_QUEST_INPUT_STRING:
			QuestInputString(character, c_pData);
			break;

		case HEADER_CG_QUEST_CONFIRM:
			QuestConfirm(character, c_pData);
			break;

		case HEADER_CG_TARGET:
			Target(character, c_pData);
			break;

		case HEADER_CG_WARP:
			Warp(character, c_pData);
			break;

		case HEADER_CG_SAFEBOX_CHECKIN:
			SafeboxCheckin(character, c_pData);
			break;

		case HEADER_CG_SAFEBOX_CHECKOUT:
			SafeboxCheckout(character, c_pData, false);
			break;

		case HEADER_CG_SAFEBOX_ITEM_MOVE:
			SafeboxItemMove(character, c_pData);
			break;

		case HEADER_CG_MALL_CHECKOUT:
			SafeboxCheckout(character, c_pData, true);
			break;


		case HEADER_CG_MOUNT_INVENTORY_CHECKIN:
			MountInventoryCheckin(character, c_pData);
			break;

		case HEADER_CG_MOUNT_INVENTORY_CHECKOUT:
			MountInventoryCheckout(character, c_pData);
			break;

		case HEADER_CG_MOUNT_INVENTORY_ITEM_MOVE:
			MountInventoryItemMove(character, c_pData);
			break;

		case HEADER_CG_PARTY_INVITE:
			PartyInvite(character, c_pData);
			break;

		case HEADER_CG_PARTY_REMOVE:
			PartyRemove(character, c_pData);
			break;

		case HEADER_CG_PARTY_INVITE_ANSWER:
			PartyInviteAnswer(character, c_pData);
			break;

		case HEADER_CG_PARTY_SET_STATE:
			PartySetState(character, c_pData);
			break;

		case HEADER_CG_PARTY_USE_SKILL:
			PartyUseSkill(character, c_pData);
			break;

		case HEADER_CG_PARTY_PARAMETER:
			PartyParameter(character, c_pData);
			break;
#ifdef __INGAME_WIKI__
		case InGameWiki::HEADER_CG_WIKI:
			RecvWikiPacket(character, c_pData);
			break;
#endif
		case HEADER_CG_ANSWER_MAKE_GUILD:
#ifdef ENABLE_NEWGUILDMAKE
			ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "<%s> AnswerMakeGuild disabled", __FUNCTION__);
#else
			AnswerMakeGuild(character, c_pData);
#endif
			break;

		case HEADER_CG_GUILD:
			if ((iExtraLen = Guild(character, c_pData, m_iBufferLeft)) < 0)
				return -1;
			break;

		case HEADER_CG_FISHING:
			Fishing(character, c_pData);
			break;
		case HEADER_CG_HACK:
			Hack(character, c_pData);
			break;

#ifdef __NEWPET_SYSTEM__
		case HEADER_CG_PetSetName:
			BraveRequestPetName(character, c_pData);
			break;
#endif
		case HEADER_CG_MYSHOP:
			if ((iExtraLen = MyShop(character, c_pData, m_iBufferLeft)) < 0)
				return -1;
			break;

		case HEADER_CG_REFINE:
			Refine(character, c_pData);
			break;

#ifdef ENABLE_WHISPER_ADMIN_SYSTEM
		case HEADER_CG_WHISPER_ADMIN:
			CWhisperAdmin::instance().Manager(character, c_pData);
			break;
#endif

		case HEADER_CG_CLIENT_VERSION:
			Version(character, c_pData);
			break;


#ifdef ENABLE_MULTI_LANGUAGE
		case HEADER_CG_CHANGE_LANGUAGE:
			{
				TPacketChangeLanguage* p = reinterpret_cast <TPacketChangeLanguage*>((void*)c_pData);
				ChangeLanguage(character, p->bLanguage);
			}
			break;
		case HEADER_CG_REQUEST_LANGUAGE:
			{
				TPacketRequestLang* p = reinterpret_cast <TPacketRequestLang*>((void*)c_pData);
				RequestLanguage(character, p->targetName);
			}
			break;
#endif
#ifdef __SEND_TARGET_INFO__
		case HEADER_CG_TARGET_INFO_LOAD:
			{
				TargetInfoLoad(character, c_pData);
			}
			break;
#endif
#ifdef ENABLE_SWITCHBOT
		case HEADER_CG_SWITCHBOT:
			if ((iExtraLen = Switchbot(character, c_pData, m_iBufferLeft)) < 0)
			{
				return -1;
			}
			break;
#endif
#ifdef ENABLE_MAP_TELEPORTER
		case HEADER_CG_MAP_TELEPORTER:
			MapTeleporter(character, (TPacketCGMapTeleporter*) c_pData);
			break;
#endif
		case HEADER_CG_DRAGON_SOUL_REFINE:
			{
				TPacketCGDragonSoulRefine* p = reinterpret_cast <TPacketCGDragonSoulRefine*>((void*)c_pData);
				switch(p->bSubType)
				{
				case DS_SUB_HEADER_CLOSE:
					DragonSoulSystem::CloseRefineWindow(character);
					break;
				case DS_SUB_HEADER_DO_REFINE_GRADE:
					{
						DSManager::instance().DoRefineGradeEcs(character, p->ItemGrid);
					}
					break;
				case DS_SUB_HEADER_DO_REFINE_STEP:
					{
						DSManager::instance().DoRefineStepEcs(character, p->ItemGrid);
					}
					break;
				case DS_SUB_HEADER_DO_REFINE_STRENGTH:
					{
						DSManager::instance().DoRefineStrengthEcs(character, p->ItemGrid);
					}
					break;
				}
			}
			break;
#ifdef ENABLE_DS_REFINE_ALL
		case HEADER_CG_DRAGON_SOUL_REFINE_ALL: {
			TPacketDragonSoulRefineAll* p = reinterpret_cast <TPacketDragonSoulRefineAll*>((void*)c_pData);
			DSManager::instance().DoRefineAllEcs(character, p->subheader, p->type, p->grade);
		} break;
#endif
#ifdef __ENABLE_NEW_OFFLINESHOP__
		case HEADER_CG_NEW_OFFLINESHOP:
			if((iExtraLen = OfflineshopPacket(c_pData, character, m_iBufferLeft))< 0)
				return -1;
			break;
#endif
#ifdef ENABLE_CUBE_RENEWAL_WORLDARD
		case HEADER_CG_CUBE_RENEWAL:
			CubeRenewalSend(character, c_pData);
			break;
#endif
#ifdef ENABLE_NEW_FISHING_SYSTEM
		case HEADER_CG_FISHING_NEW:
			{
				FishingNew(character, c_pData);
			}
			break;
#endif
#if defined(ENABLE_CHRISTMAS_WHEEL_OF_DESTINY)
		case HEADER_CG_WHEEL_DESTINY:
		{
			WheelDestiny(character, c_pData);
		}
		break;
#endif
	}
	return (iExtraLen);
}

int CInputDead::Analyze(LPDESC d, uint8_t bHeader, const char * c_pData)
{
	if (!d || !c_pData)
		return 0;
	const entt::entity character = d->GetEntity();
	if (!ecs::PlayerRuntime::IsPC(character) || ecs::PlayerRuntime::GetDesc(character) != d)
	{
		LOG_ERROR("no character on desc");
		return 0;
	}


	int iExtraLen = 0;

	switch (bHeader)
	{
		case HEADER_CG_PONG:
			Pong(d);
			break;

		case HEADER_CG_TIME_SYNC:
			Handshake(d, c_pData);
			break;

		case HEADER_CG_CHAT:
			if ((iExtraLen = Chat(character, c_pData, m_iBufferLeft)) < 0)
				return -1;

			break;

		case HEADER_CG_WHISPER:
			if ((iExtraLen = Whisper(character, c_pData, m_iBufferLeft)) < 0)
				return -1;

			break;

		case HEADER_CG_HACK:
			Hack(character, c_pData);
			break;

		default:
			return (0);
	}

	return (iExtraLen);
}


#ifdef ENABLE_MULTI_LANGUAGE
void CInputMain::ChangeLanguage(entt::entity character, uint8_t bLanguage)
{
	if (!ecs::PlayerRuntime::IsValid(character))
		return;

	if (!ecs::PlayerRuntime::GetDesc(character))
		return;

	uint8_t bCurrentLanguage = ecs::PlayerRuntime::GetDesc(character)->GetLanguage();

	if(bCurrentLanguage == bLanguage)
		return;

	if(bLanguage > LANGUAGE_DEFAULT && bLanguage < LANGUAGE_MAX_NUM)
	{
		std::unique_ptr<SQLMsg> msg(DBManager::instance().DirectQuery("UPDATE account.account SET language = %d WHERE id = %d;", bLanguage, ecs::PlayerRuntime::GetAccountID(character)));
		ecs::PlayerRuntime::GetDesc(character)->SetLanguage(bLanguage);
	}
}

void CInputMain::RequestLanguage(entt::entity character, const char* targetName)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate RequestLanguage handler ECS
// DUAL-PATH: legacy only during migration window
	if (!ecs::PlayerRuntime::IsValid(character))
		return;

	LPDESC d = ecs::PlayerRuntime::GetDesc(character);
	if (!d)
		return;

	int id = 0;
	std::unique_ptr<SQLMsg> pMsg(DBManager::instance().DirectQuery("SELECT account_id FROM player.player WHERE name='%s'", targetName));
	if (pMsg->Get()->uiNumRows != 0) {
		MYSQL_ROW row = mysql_fetch_row(pMsg->Get()->pSQLResult);
		id = atoi(row[0]);
	}

	if (id == 0)
		return;

	std::unique_ptr<SQLMsg> pMsg2(DBManager::instance().DirectQuery("SELECT language FROM account.account WHERE id=%d", id));
	if (pMsg2->Get()->uiNumRows != 0) {
		MYSQL_ROW row = mysql_fetch_row(pMsg2->Get()->pSQLResult);

		TPacketRecvLang p;
		p.bHeader = HEADER_GC_RECV_LANGUAGE;
		strncpy(p.targetName, targetName, sizeof(p.targetName));
		strncpy(p.targetLanguage, row[0], sizeof(p.targetLanguage));
		d->Packet(&p, sizeof(p));
	}
}
#endif

