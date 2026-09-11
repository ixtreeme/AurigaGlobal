#include "../../stdafx.h"

#include <common/stole_length.h>

#include "AcceSystem.hpp"
#include "OfflineShopSystem.hpp"
#include "ItemSystem.hpp"
#include "InventorySystem.hpp"
#include "PlayerRuntimeSystem.hpp"
#include "PointSystem.hpp"
#include "SessionSystem.hpp"
#include "SocialSystem.hpp"
#include "ChatSystem.hpp"
#include "NetworkSyncSystem.hpp"
#include "../CharacterAccessors.hpp"
#include "../Registry.hpp"
#include "../components/dirty_components.hpp"
#include "../components/inventory_components.hpp"

#include "../../char.h"
#include "../../desc.h"
#include "../../item.h"
#include "../../item_manager.h"
#include "../../log.h"
#include "../../packet.h"
#include "../../db.h"
#include "../../questmanager.h"

#include <Core/Logging.hpp>

#ifdef ENABLE_ACCE_SYSTEM
namespace ecs::AcceSystem {

// The four material slots the window is working on.
std::span<entt::entity> GetMaterials(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return {};

    return g_registry.get_or_emplace<ecs::AcceWindowComponent>(e).materials;
}

// Whether the combination or the absorption window is open. These were two
// CHARACTER flags beside AcceWindowComponent, which the setters also wrote.
bool IsOpened(entt::entity e, bool combination)
{
    if (e == entt::null || !g_registry.valid(e))
        return false;

    const auto* acce = g_registry.try_get<ecs::AcceWindowComponent>(e);
    if (!acce)
        return false;

    return combination ? acce->combinationOpen : acce->absorptionOpen;
}

bool IsOpen(entt::entity e)
{
    return IsOpened(e, true) || IsOpened(e, false);
}

void Open(entt::entity e, bool bCombination)
{
	if (e == entt::null || !g_registry.valid(e))
		return;

	auto& acce = g_registry.get_or_emplace<ecs::AcceWindowComponent>(e);
    if (IsOpened(e, bCombination))
    {
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 659, "");
#endif
        return;
    }

    if (bCombination)
    {
        if (acce.absorptionOpen)
        {
#ifdef TEXTS_IMPROVEMENT
            ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 660, "");
#endif
            return;
        }

        acce.combinationOpen = true;
        g_registry.emplace_or_replace<ecs::DirtyTag>(e);
    }
    else
    {
        if (acce.combinationOpen)
        {
#ifdef TEXTS_IMPROVEMENT
            ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 661, "");
#endif
            return;
        }

        acce.absorptionOpen = true;
        g_registry.emplace_or_replace<ecs::DirtyTag>(e);
    }

    TItemPos tPos;
    tPos.window_type = INVENTORY;
    tPos.cell = 0;

    TPacketAcce sPacket;
    sPacket.header = HEADER_GC_ACCE;
    sPacket.subheader = ACCE_SUBHEADER_GC_OPEN;
    sPacket.bWindow = bCombination;
    sPacket.dwPrice = 0;
    sPacket.bPos = 0;
    sPacket.tPos = tPos;
    sPacket.dwItemVnum = 0;
    sPacket.dwMinAbs = 0;
    sPacket.dwMaxAbs = 0;
    ecs::PlayerRuntime::GetDesc(e)->Packet(&sPacket, sizeof(TPacketAcce));

    ClearMaterials(e);
}

void Close(entt::entity e)
{
	if (e == entt::null || !g_registry.valid(e))
		return;

	auto& acce = g_registry.get_or_emplace<ecs::AcceWindowComponent>(e);
    if ((!acce.combinationOpen) && (!acce.absorptionOpen))
        return;

    bool bWindow = (acce.combinationOpen == true ? true : false);

    TItemPos tPos;
    tPos.window_type = INVENTORY;
    tPos.cell = 0;

    TPacketAcce sPacket;
    sPacket.header = HEADER_GC_ACCE;
    sPacket.subheader = ACCE_SUBHEADER_GC_CLOSE;
    sPacket.bWindow = bWindow;
    sPacket.dwPrice = 0;
    sPacket.bPos = 0;
    sPacket.tPos = tPos;
    sPacket.dwItemVnum = 0;
    sPacket.dwMinAbs = 0;
    sPacket.dwMaxAbs = 0;
    ecs::PlayerRuntime::GetDesc(e)->Packet(&sPacket, sizeof(TPacketAcce));

    if (bWindow)
        acce.combinationOpen = false;
    else
        acce.absorptionOpen = false;

    g_registry.emplace_or_replace<ecs::DirtyTag>(e);

    ClearMaterials(e);
}

void ClearMaterials(entt::entity e)
{
	if (e == entt::null || !g_registry.valid(e))
		return;

	auto& acce = g_registry.get_or_emplace<ecs::AcceWindowComponent>(e);
	auto pkItemMaterial = acce.materials;
	for (int i = 0; i < ACCE_WINDOW_MAX_MATERIALS; ++i)
	{
		if (pkItemMaterial[i] == entt::null)
			continue;

		ItemSystem::UnlockItem(pkItemMaterial[i]);
		pkItemMaterial[i] = entt::null;
	}
}

bool IsSameGrade(entt::entity e, int32_t lGrade)
{
	if (e == entt::null || !g_registry.valid(e))
		return false;

	auto& acce = g_registry.get_or_emplace<ecs::AcceWindowComponent>(e);
	auto pkItemMaterial = acce.materials;
	if (pkItemMaterial[0] == entt::null)
		return false;

	bool bReturn = ItemSystem::GetItemValue(pkItemMaterial[0], ACCE_GRADE_VALUE_FIELD) == lGrade;
    return bReturn;
}

uint32_t GetCombinePrice(entt::entity e, int32_t lGrade
#ifdef ENABLE_STOLE_COSTUME
    , bool isCostume
#endif
    )
{
	if (e == entt::null || !g_registry.valid(e))
		return 0;

	auto& acce = g_registry.get_or_emplace<ecs::AcceWindowComponent>(e);
    uint32_t dwPrice;
    switch (lGrade)
    {
    case 2:
    {
#ifdef ENABLE_STOLE_COSTUME
        dwPrice = isCostume ? COSTUME_STOLE_GRADE_2_PRICE : ACCE_GRADE_2_PRICE;
#else
        dwPrice = ACCE_GRADE_2_PRICE;
#endif
    }
    break;
    case 3:
    {
#ifdef ENABLE_STOLE_COSTUME
        dwPrice = isCostume ? COSTUME_STOLE_GRADE_3_PRICE : ACCE_GRADE_3_PRICE;
#else
        dwPrice = ACCE_GRADE_2_PRICE;
#endif
    }
    break;
    case 4:
    {
#ifdef ENABLE_STOLE_COSTUME
        dwPrice = isCostume ? 0 : ACCE_GRADE_4_PRICE;
#else
        dwPrice = ACCE_GRADE_2_PRICE;
#endif
    }
    break;
    default:
    {
#ifdef ENABLE_STOLE_COSTUME
        dwPrice = isCostume ? COSTUME_STOLE_GRADE_1_PRICE : ACCE_GRADE_1_PRICE;
#else
        dwPrice = ACCE_GRADE_1_PRICE;
#endif
    }
    break;
    }

    return dwPrice;
}

uint8_t CheckEmptyMaterialSlot(entt::entity e)
{
	if (e == entt::null || !g_registry.valid(e))
		return 0;

	auto& acce = g_registry.get_or_emplace<ecs::AcceWindowComponent>(e);
	const auto pkItemMaterial = acce.materials;
    for (int i = 0; i < ACCE_WINDOW_MAX_MATERIALS; ++i)
    {
		if (pkItemMaterial[i] == entt::null)
            return i;
    }

    return 255;
}

void GetCombineResult(entt::entity e, uint32_t& dwItemVnum, uint32_t& dwMinAbs, uint32_t& dwMaxAbs)
{
	if (e == entt::null || !g_registry.valid(e))
		return;

	auto& acce = g_registry.get_or_emplace<ecs::AcceWindowComponent>(e);
	const auto pkItemMaterial = acce.materials;

    if (acce.combinationOpen)
    {
		if (pkItemMaterial[0] != entt::null && pkItemMaterial[1] != entt::null)
		{
			int32_t lVal = ItemSystem::GetItemValue(pkItemMaterial[0], ACCE_GRADE_VALUE_FIELD);
            if (lVal == 4)
            {
				dwItemVnum = ItemSystem::GetItemOriginalVnum(pkItemMaterial[0]);
				dwMinAbs = ItemSystem::GetItemSocket(pkItemMaterial[0], ACCE_ABSORPTION_SOCKET);
                uint32_t dwMaxAbsCalc = (dwMinAbs + ACCE_GRADE_4_ABS_RANGE > ACCE_GRADE_4_ABS_MAX ? ACCE_GRADE_4_ABS_MAX : (dwMinAbs + ACCE_GRADE_4_ABS_RANGE));
                dwMaxAbs = dwMaxAbsCalc;
            }
            else
            {
				uint32_t dwMaskVnum = ItemSystem::GetItemOriginalVnum(pkItemMaterial[0]);
                TItemTable* pTable = ITEM_MANAGER::instance().GetTable(dwMaskVnum + 1);
                if (pTable)
                    dwMaskVnum += 1;

                dwItemVnum = dwMaskVnum;
                switch (lVal)
                {
                case 2:
                {
                    dwMinAbs = ACCE_GRADE_3_ABS;
                    dwMaxAbs = ACCE_GRADE_3_ABS;
                }
                break;
                case 3:
                {
                    dwMinAbs = ACCE_GRADE_4_ABS_MIN;
                    dwMaxAbs = ACCE_GRADE_4_ABS_MAX_COMB;
                }
                break;
                default:
                {
                    dwMinAbs = ACCE_GRADE_2_ABS;
                    dwMaxAbs = ACCE_GRADE_2_ABS;
                }
                break;
                }
            }
        }
        else
        {
            dwItemVnum = 0;
            dwMinAbs = 0;
            dwMaxAbs = 0;
        }
    }
    else
    {
		if (pkItemMaterial[0] != entt::null && pkItemMaterial[1] != entt::null)
		{
			dwItemVnum = ItemSystem::GetItemOriginalVnum(pkItemMaterial[0]);
			dwMinAbs = ItemSystem::GetItemSocket(pkItemMaterial[0], ACCE_ABSORPTION_SOCKET);
            dwMaxAbs = dwMinAbs;
        }
        else
        {
            dwItemVnum = 0;
            dwMinAbs = 0;
            dwMaxAbs = 0;
        }
    }
}

void AddMaterial(entt::entity e, TItemPos tPos, uint8_t bPos)
{
	if (e == entt::null || !g_registry.valid(e))
		return;

	auto& acce = g_registry.get_or_emplace<ecs::AcceWindowComponent>(e);
    if (bPos >= ACCE_WINDOW_MAX_MATERIALS)
    {
        if (bPos == 255)
        {
            bPos = CheckEmptyMaterialSlot(e);
            if (bPos >= ACCE_WINDOW_MAX_MATERIALS)
                return;
        }
        else
            return;
    }

	const entt::entity item = ItemSystem::GetItem(e, tPos);
	if (item == entt::null)
		return;
	else if ((ItemSystem::GetItemCell(item) >= INVENTORY_MAX_NUM) || ItemSystem::IsItemEquipped(item) || tPos.IsBeltInventoryPosition() || ItemSystem::GetItemType(item) == ITEM_DS)
		return;
	else if ((ItemSystem::GetItemType(item) != ITEM_COSTUME) && (acce.combinationOpen))
		return;
	else if ((ItemSystem::GetItemType(item) != ITEM_COSTUME) && (bPos == 0) && (acce.absorptionOpen))
		return;
	else if (ItemSystem::IsItemLocked(item))
    {
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 519, "");
#endif
        return;
    }
	else if ((ItemSystem::GetItemType(item) == ITEM_ARMOR) && (ItemSystem::GetItemSubType(item) == ARMOR_BODY))
    {
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 519, "");
#endif
        return;
    }
#ifdef __SOULBINDING_SYSTEM__
	else if (ItemSystem::IsItemBound(item))
    {
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 519, "");
#endif
        return;
    }
#endif
#ifdef ENABLE_STOLE_COSTUME
	else if (acce.absorptionOpen && bPos == 0 && ItemSystem::GetItemSubType(item) != COSTUME_ACCE)
    {
        return;
    }
#endif
	else if ((acce.combinationOpen) && (bPos == 1) && (!IsSameGrade(e, ItemSystem::GetItemValue(item, ACCE_GRADE_VALUE_FIELD))))
    {
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 662, "");
#endif
        return;
    }
#ifdef ENABLE_STOLE_COSTUME
	else if ((acce.combinationOpen) && (ItemSystem::GetItemSubType(item) == COSTUME_STOLE) && (ItemSystem::GetItemValue(item, 0) == 4))
    {
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 20, "%s", ItemSystem::GetItemName(item));
#endif
        return;
    }
#endif
	else if ((acce.combinationOpen) && (ItemSystem::GetItemSocket(item, ACCE_ABSORPTION_SOCKET) >= ACCE_GRADE_4_ABS_MAX))
    {
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 663, "%d", ACCE_GRADE_4_ABS_MAX);
#endif
        return;
    }
    else if ((bPos == 1) && (acce.absorptionOpen))
    {
		if ((ItemSystem::GetItemType(item) != ITEM_WEAPON) && (ItemSystem::GetItemType(item) != ITEM_ARMOR))
        {
#ifdef TEXTS_IMPROVEMENT
            ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 520, "");
#endif
            return;
        }
		else if ((ItemSystem::GetItemType(item) == ITEM_ARMOR) && (ItemSystem::GetItemSubType(item) != ARMOR_BODY))
        {
#ifdef TEXTS_IMPROVEMENT
            ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 520, "");
#endif
            return;
        }
    }
    else if
#ifdef ENABLE_STOLE_COSTUME
    (
#endif
		((ItemSystem::GetItemSubType(item) != COSTUME_ACCE)
#ifdef ENABLE_STOLE_COSTUME
			&& (ItemSystem::GetItemSubType(item) != COSTUME_STOLE))
#endif
        && (acce.combinationOpen))
        return;
    else if
#ifdef ENABLE_STOLE_COSTUME
    (
#endif
		((ItemSystem::GetItemSubType(item) != COSTUME_ACCE)
#ifdef ENABLE_STOLE_COSTUME
			&& (ItemSystem::GetItemSubType(item) != COSTUME_STOLE))
#endif
        && (bPos == 0) && (acce.absorptionOpen))
        return;
	else if ((ItemSystem::GetItemSocket(item, ACCE_ABSORBED_SOCKET) > 0) && (bPos == 0) && (acce.absorptionOpen))
		return;

	auto pkItemMaterial = acce.materials;
	if ((bPos == 1) && pkItemMaterial[0] == entt::null)
        return;

#ifdef ENABLE_STOLE_COSTUME
	if ((!acce.absorptionOpen) && (bPos == 1) && (ItemSystem::GetItemSubType(pkItemMaterial[0]) != ItemSystem::GetItemSubType(item))) {
#ifdef TEXTS_IMPROVEMENT
		if (ItemSystem::GetItemSubType(pkItemMaterial[0]) == COSTUME_STOLE) {
            ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 18, "");
        }
        else {
            ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 822, "");
        }
#endif
        return;
    }
	else if (!acce.absorptionOpen && bPos == 1 && ItemSystem::GetItemSubType(pkItemMaterial[0]) == COSTUME_STOLE && ItemSystem::GetItemVnum(pkItemMaterial[0]) != ItemSystem::GetItemVnum(item)) {
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 1293, "");
#endif
        return;
    }
#endif

	if (pkItemMaterial[bPos] != entt::null)
		return;

	pkItemMaterial[bPos] = item;
	ItemSystem::LockItem(pkItemMaterial[bPos]);

    uint32_t dwItemVnum, dwMinAbs, dwMaxAbs;
    GetCombineResult(e, dwItemVnum, dwMinAbs, dwMaxAbs);

    TPacketAcce sPacket;
    sPacket.header = HEADER_GC_ACCE;
    sPacket.subheader = ACCE_SUBHEADER_GC_ADDED;
    sPacket.bWindow = acce.combinationOpen == true ? true : false;
	sPacket.dwPrice = GetCombinePrice(e, ItemSystem::GetItemValue(item, ACCE_GRADE_VALUE_FIELD)
#ifdef ENABLE_STOLE_COSTUME
		, ItemSystem::GetItemSubType(item) == COSTUME_STOLE
#endif
    );
    sPacket.bPos = bPos;
    sPacket.tPos = tPos;
    sPacket.dwItemVnum = dwItemVnum;
    sPacket.dwMinAbs = dwMinAbs;
    sPacket.dwMaxAbs = dwMaxAbs;
    ecs::PlayerRuntime::GetDesc(e)->Packet(&sPacket, sizeof(TPacketAcce));
}

void RemoveMaterial(entt::entity e, uint8_t bPos)
{
	if (e == entt::null || !g_registry.valid(e))
		return;

	auto& acce = g_registry.get_or_emplace<ecs::AcceWindowComponent>(e);
    if (bPos >= ACCE_WINDOW_MAX_MATERIALS)
        return;

	auto pkItemMaterial = acce.materials;

    uint32_t dwPrice = 0;

    if (bPos == 1)
    {
		if (pkItemMaterial[bPos] != entt::null)
		{
			ItemSystem::UnlockItem(pkItemMaterial[bPos]);
			pkItemMaterial[bPos] = entt::null;
		}

		if (pkItemMaterial[0] != entt::null) {
			dwPrice = GetCombinePrice(e, ItemSystem::GetItemValue(pkItemMaterial[0], ACCE_GRADE_VALUE_FIELD)
#ifdef ENABLE_STOLE_COSTUME
				, ItemSystem::GetItemSubType(pkItemMaterial[0]) == COSTUME_STOLE
#endif
            );
        }
    }
    else
        ClearMaterials(e);

    TItemPos tPos;
    tPos.window_type = INVENTORY;
    tPos.cell = 0;

    TPacketAcce sPacket;
    sPacket.header = HEADER_GC_ACCE;
    sPacket.subheader = ACCE_SUBHEADER_GC_REMOVED;
    sPacket.bWindow = acce.combinationOpen == true ? true : false;
    sPacket.dwPrice = dwPrice;
    sPacket.bPos = bPos;
    sPacket.tPos = tPos;
    sPacket.dwItemVnum = 0;
    sPacket.dwMinAbs = 0;
    sPacket.dwMaxAbs = 0;
    ecs::PlayerRuntime::GetDesc(e)->Packet(&sPacket, sizeof(TPacketAcce));
}

uint8_t CanRefine(entt::entity e)
{
	if (e == entt::null || !g_registry.valid(e))
		return 0;

	auto& acce = g_registry.get_or_emplace<ecs::AcceWindowComponent>(e);
    if (ecs::OfflineShopSystem::GetOfflineShopGuest(e) || ecs::OfflineShopSystem::GetAuctionGuest(e))
        return 0;

    if (ecs::SocialSystem::HasExchange(e) || ecs::SocialSystem::GetMyShop(e) || (ecs::SocialSystem::GetShopOwner(e) != entt::null) || ecs::SessionSystem::IsSafeboxOpen(e) || ecs::SessionSystem::IsCubeOpen(e)
#ifdef __ATTR_TRANSFER_SYSTEM__
        || AttrTransfer_is_open(e)
#endif
        )
        return 0;

    uint8_t bReturn = 0;
	auto pkItemMaterial = acce.materials;
    if (acce.combinationOpen)
    {
        for (int i = 0; i < ACCE_WINDOW_MAX_MATERIALS; ++i)
        {
			if (pkItemMaterial[i] != entt::null)
			{
				if ((ItemSystem::GetItemType(pkItemMaterial[i]) == ITEM_COSTUME) && (ItemSystem::GetItemSubType(pkItemMaterial[i]) == COSTUME_ACCE))
                    bReturn = 1;
#ifdef ENABLE_STOLE_COSTUME
				else if ((ItemSystem::GetItemType(pkItemMaterial[i]) == ITEM_COSTUME) && (ItemSystem::GetItemSubType(pkItemMaterial[i]) == COSTUME_STOLE))
                    bReturn = 1;
#endif
                else
                {
                    bReturn = 0;
                    break;
                }
            }
            else
            {
                bReturn = 0;
                break;
            }
        }
    }
    else if (acce.absorptionOpen)
    {
		if (pkItemMaterial[0] != entt::null && pkItemMaterial[1] != entt::null)
		{
			if ((ItemSystem::GetItemType(pkItemMaterial[0]) == ITEM_COSTUME) && (ItemSystem::GetItemSubType(pkItemMaterial[0]) == COSTUME_ACCE))
                bReturn = 2;
            else
                bReturn = 0;

			if ((ItemSystem::GetItemType(pkItemMaterial[1]) == ITEM_WEAPON) || ((ItemSystem::GetItemType(pkItemMaterial[1]) == ITEM_ARMOR) && (ItemSystem::GetItemSubType(pkItemMaterial[1]) == ARMOR_BODY)))
                bReturn = 2;
#ifdef ATTR_LOCK
			if ((ItemSystem::GetItemType(pkItemMaterial[1]) == ITEM_WEAPON) || ((ItemSystem::GetItemType(pkItemMaterial[1]) == ITEM_ARMOR) && (ItemSystem::GetItemSubType(pkItemMaterial[1]) == ARMOR_BODY)))
			{
				if (ItemSystem::GetItemLockedAttributeIndex(pkItemMaterial[1]) != -1)
                {
                    bReturn = 0;
#ifdef TEXTS_IMPROVEMENT
                    ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 783, "");
#endif
                }
            }
#endif
            else
                bReturn = 0;

			if (ItemSystem::GetItemSocket(pkItemMaterial[0], ACCE_ABSORBED_SOCKET) > 0)
                bReturn = 0;
        }
        else
            bReturn = 0;
    }

    return bReturn;
}

void Refine(entt::entity e)
{
	if (e == entt::null || !g_registry.valid(e))
		return;

	auto& acce = g_registry.get_or_emplace<ecs::AcceWindowComponent>(e);
    uint8_t bCan = CanRefine(e);
    if (bCan == 0)
        return;

	auto pkItemMaterial = acce.materials;
	if (!ItemSystem::IsValidItem(pkItemMaterial[0]) ||
		!ItemSystem::IsValidItem(pkItemMaterial[1]))
		return;

    uint32_t dwItemVnum, dwMinAbs, dwMaxAbs;
    GetCombineResult(e, dwItemVnum, dwMinAbs, dwMaxAbs);

	int64_t dwPrice = GetCombinePrice(e, ItemSystem::GetItemValue(pkItemMaterial[0], ACCE_GRADE_VALUE_FIELD)
#ifdef ENABLE_STOLE_COSTUME
		, ItemSystem::GetItemSubType(pkItemMaterial[0]) == COSTUME_STOLE
#endif
    );


    if (bCan == 1)
    {
#ifdef ENABLE_STOLE_COSTUME
		bool bStole = ItemSystem::GetItemSubType(pkItemMaterial[0]) == COSTUME_STOLE;
#endif
        int iSuccessChance = 0;
		int32_t lVal = ItemSystem::GetItemValue(pkItemMaterial[0], ACCE_GRADE_VALUE_FIELD);
        switch (lVal)
        {
        case 2:
        {
#ifdef ENABLE_STOLE_COSTUME
            if (bStole) {
                iSuccessChance = STOLA_COMBINE_GRADE_2;
                break;
            }
#endif
            iSuccessChance = ACCE_COMBINE_GRADE_2;
        }
        break;
        case 3:
        {
#ifdef ENABLE_STOLE_COSTUME
            if (bStole) {
                iSuccessChance = STOLA_COMBINE_GRADE_3;
                break;
            }
#endif
            iSuccessChance = ACCE_COMBINE_GRADE_3;
        }
        break;
        case 4:
        {
            iSuccessChance = ACCE_COMBINE_GRADE_4;
        }
        break;
        default:
        {
#ifdef ENABLE_STOLE_COSTUME
            if (bStole) {
                iSuccessChance = STOLA_COMBINE_GRADE_1;
                break;
            }
#endif
            iSuccessChance = ACCE_COMBINE_GRADE_1;
        }
        break;
        }

        if (ecs::PointSystem::GetGold(e) < dwPrice)
        {
#ifdef TEXTS_IMPROVEMENT
            ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 232, "");
#endif
            return;
        }

        int iChance = number(1, 100);
        bool bSucces = (iChance <= iSuccessChance ? true : false);
        if (bSucces)
        {
			const entt::entity resultItem =
				ITEM_MANAGER::instance().CreateItem(dwItemVnum, 1, 0, false);
			if (!ItemSystem::IsValidItem(resultItem))
            {
                LOG_ERROR("{} can't be created.", dwItemVnum);
				return;
			}

#ifdef ENABLE_STOLE_COSTUME
			if (ItemSystem::GetItemSubType(resultItem) != COSTUME_STOLE)
				ItemSystem::CopyAllAttrToEcs(pkItemMaterial[0], resultItem);
#else
			ItemSystem::CopyAllAttrToEcs(pkItemMaterial[0], resultItem);
#endif
            LogManager::instance().ItemLogEntity(
				e, resultItem, "COMBINE SUCCESS",
				ItemSystem::GetItemName(resultItem));
            uint32_t dwAbs = (dwMinAbs == dwMaxAbs ? dwMinAbs : number(dwMinAbs + 1, dwMaxAbs));
			ItemSystem::SetItemSocket(resultItem, ACCE_ABSORPTION_SOCKET, dwAbs);
			ItemSystem::SetItemSocket(resultItem, ACCE_ABSORBED_SOCKET, ItemSystem::GetItemSocket(pkItemMaterial[0], ACCE_ABSORBED_SOCKET));

            ecs::PointSystem::Change(e, POINT_GOLD, -dwPrice);
			DBManager::instance().SendMoneyLog(MONEY_LOG_REFINE, ItemSystem::GetItemVnum(pkItemMaterial[0]), -dwPrice);

			uint16_t wCell = ItemSystem::GetItemCell(pkItemMaterial[0]);
			const entt::entity material0 = pkItemMaterial[0];
			const entt::entity material1 = pkItemMaterial[1];
			pkItemMaterial[0] = entt::null;
			pkItemMaterial[1] = entt::null;
			ItemSystem::DestroyItemEntityEcs(
				material0, "COMBINE (REFINE SUCCESS)");
			ItemSystem::DestroyItemEntityEcs(
				material1, "COMBINE (REFINE SUCCESS)");

			if (!ItemSystem::PlaceItemEcs(
					e, resultItem, INVENTORY, wCell))
			{
				ItemSystem::DestroyItemEntityEcs(
					resultItem, "COMBINE RESULT PLACE FAILED");
				ClearMaterials(e);
				return;
			}
			ItemSystem::FlushDelayedSaveEcs(resultItem);
			ItemSystem::AttrLogEcs(resultItem);

#ifdef TEXTS_IMPROVEMENT
            if (lVal == 4) {
                ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 521, "%d", dwAbs);
            }
            else {
                ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 389, "");
            }
#endif
            NetworkSyncSystem::BroadcastEffect(g_registry, e, SE_EFFECT_ACCE_SUCCEDED);
			LogManager::instance().AcceLog(ecs::PlayerRuntime::GetPlayerID(e), ecs::PlayerRuntime::GetX(e), ecs::PlayerRuntime::GetY(e), dwItemVnum, ItemSystem::GetItemID(resultItem), 1, dwAbs, 1);

            ClearMaterials(e);
        }
        else
		{
            ecs::PointSystem::Change(e, POINT_GOLD, -dwPrice);
			DBManager::instance().SendMoneyLog(MONEY_LOG_REFINE, ItemSystem::GetItemVnum(pkItemMaterial[0]), -dwPrice);
			const entt::entity failedMaterial = pkItemMaterial[1];
			pkItemMaterial[1] = entt::null;
			ItemSystem::DestroyItemEntityEcs(
				failedMaterial, "COMBINE (REFINE FAIL)");
#ifdef TEXTS_IMPROVEMENT
            ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 390, "");
#endif
            LogManager::instance().AcceLog(ecs::PlayerRuntime::GetPlayerID(e), ecs::PlayerRuntime::GetX(e), ecs::PlayerRuntime::GetY(e), dwItemVnum, 0, 0, 0, 0);
		}

        TItemPos tPos;
        tPos.window_type = INVENTORY;
        tPos.cell = 0;

        TPacketAcce sPacket;
        sPacket.header = HEADER_GC_ACCE;
        sPacket.subheader = ACCE_SUBHEADER_CG_REFINED;
        sPacket.bWindow = acce.combinationOpen == true ? true : false;
        sPacket.dwPrice = dwPrice;
        sPacket.bPos = 0;
        sPacket.tPos = tPos;
        sPacket.dwItemVnum = 0;
        sPacket.dwMinAbs = 0;
        if (bSucces)
            sPacket.dwMaxAbs = 100;
        else
            sPacket.dwMaxAbs = 0;

        ecs::PlayerRuntime::GetDesc(e)->Packet(&sPacket, sizeof(TPacketAcce));
    }
    else
    {
		ItemSystem::CopyItemAttributesEcs(pkItemMaterial[1], pkItemMaterial[0]);
		LogManager::instance().ItemLogEntity(
			e, pkItemMaterial[0], "ABSORB (REFINE SUCCESS)",
			ItemSystem::GetItemName(pkItemMaterial[0]));
		ItemSystem::SetItemSocket(pkItemMaterial[0], ACCE_ABSORBED_SOCKET, ItemSystem::GetItemOriginalVnum(pkItemMaterial[1]));
		for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
		{
			if (ItemSystem::GetItemAttributeValue(pkItemMaterial[0], i) < 0)
				ItemSystem::SetItemForceAttributeEcs(pkItemMaterial[0], i, ItemSystem::GetItemAttributeType(pkItemMaterial[0], i), 0);
		}

		const entt::entity absorbedMaterial = pkItemMaterial[1];
		pkItemMaterial[1] = entt::null;
		ItemSystem::DestroyItemEntityEcs(
			absorbedMaterial, "ABSORBED (REFINE SUCCESS)");

		ItemSystem::FlushDelayedSaveEcs(pkItemMaterial[0]);
		ItemSystem::AttrLogEcs(pkItemMaterial[0]);

#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 629, "");
#endif
        ClearMaterials(e);

        TItemPos tPos;
        tPos.window_type = INVENTORY;
        tPos.cell = 0;

        TPacketAcce sPacket;
        sPacket.header = HEADER_GC_ACCE;
        sPacket.subheader = ACCE_SUBHEADER_CG_REFINED;
        sPacket.bWindow = acce.combinationOpen == true ? true : false;
        sPacket.dwPrice = dwPrice;
        sPacket.bPos = 255;
        sPacket.tPos = tPos;
        sPacket.dwItemVnum = 0;
        sPacket.dwMinAbs = 0;
        sPacket.dwMaxAbs = 1;
        ecs::PlayerRuntime::GetDesc(e)->Packet(&sPacket, sizeof(TPacketAcce));
    }
}

} // namespace ecs::AcceSystem
#endif
