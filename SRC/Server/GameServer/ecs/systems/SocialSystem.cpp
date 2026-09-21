#include "../../stdafx.h"
#include "PointSystem.hpp"
#include "PlayerRuntimeSystem.hpp"
#include "CombatSystem.hpp"

#include "SocialSystem.hpp"
#include "InventorySystem.hpp"

#include "../../affect.h"
#include "../../char.h"
#include "../../char_manager.h"
#include "../../config.h"
#include "../../desc.h"
#include "../../desc_client.h"
#include "../../db.h"
#include "../../guild.h"
#include "../../guild_manager.h"
#include "../../item.h"
#include "../../log.h"
#include "../../marriage.h"
#include "../../wedding.h"
#include "../../packet.h"
#include "../../party.h"
#include "../../utils.h"
#include "../../questmanager.h"
#include "../../banword.h"
#include "../../shop_manager.h"
#include "../../desc_client.h"
#include "../../db.h"
#include "../../log.h"
#include "../CharacterAccessors.hpp"
#include "../EntityFactory.hpp"
#include "../Registry.hpp"
#include "../components/dirty_components.hpp"
#include "../components/character_runtime_components.hpp"
#include "../components/social_components.hpp"
#include "../components/status_components.hpp"
#include "ItemSystem.hpp"
#include "ViewSystem.hpp"
#include "MountSystem.hpp"
#include "AffectSystem.hpp"
#include "InventorySystem.hpp"
#include "SessionSystem.hpp"
#include "ChatSystem.hpp"
#include "NetworkSyncSystem.hpp"
#include <Core/Logging.hpp>

namespace ecs::SocialSystem {

entt::entity GetParty(entt::entity e)
{
    return PartySystem::GetCharacterParty(e);
}

CGuild* GetGuild(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return nullptr;

    const auto* refs = g_registry.try_get<ecs::SocialRefs>(e);
    if (!refs || !refs->guild)
        return nullptr;

    // A disbanded guild is deleted; the manager map is the liveness check, so
    // a member that still holds the pointer cannot dereference freed memory.
    if (CGuildManager::instance().FindGuild(refs->guild->GetID()) != refs->guild)
        return nullptr;

    return refs->guild;
}

// The dungeon this character is counted against. CHARACTER::m_pkDungeon held
// it and DungeonMembership::dungeon was written by one quest binding alone, so
// every reader of the component but that one saw nothing. One home now, as a
// validated entity handle; the bookkeeping lives in DungeonSystem.
void SetDungeon(entt::entity e, entt::entity pkDungeon)
{
    DungeonSystem::SetMemberDungeon(e, pkDungeon);
}

entt::entity GetDungeon(entt::entity e)
{
    return DungeonSystem::GetMemberDungeon(e);
}

// The membership is only valid while the character is on the dungeon's map.
// Legacy cleared it at entity destruction alone, so an ex-member stayed counted
// and a one-member set could destroy an instance under the rest of the party.
void ClearDungeonIfOtherMap(entt::entity e, int32_t mapIndex)
{
    DungeonSystem::ClearMemberDungeonIfOtherMap(e, mapIndex);
}

// The guild war map this character is counted against. CHARACTER::m_pWarMap
// held it and DungeonMembership::warMap was written by nothing, so GetWarMap
// and the two quest bindings over it answered "no war map" for everyone.
void SetWarMap(entt::entity e, CWarMap* pWarMap)
{
    if (e == entt::null || !g_registry.valid(e))
        return;

    auto& membership = g_registry.get_or_emplace<ecs::DungeonMembership>(e);

    // The previous relation may already point at a destroyed war map; only
    // re-enter the live one.
    if (membership.warMap && GetWarMap(e))
        membership.warMap->DecMember(e);

    membership.warMap = pWarMap;

    if (membership.warMap)
        membership.warMap->IncMember(e);

    g_registry.emplace_or_replace<ecs::DirtyTag>(e);
}

// Who this character is married to, while both are online. CHARACTER held it
// in m_pkChrMarried and MarriageState::partner was written by nothing, which
// is what left pc_is_engaged answering no for everybody.
void SetMarryPartner(entt::entity e, entt::entity partner)
{
    if (e == entt::null || !g_registry.valid(e))
        return;

    auto& marriageState = g_registry.get_or_emplace<ecs::MarriageState>(e);
    marriageState.partner =
        partner != entt::null && g_registry.valid(partner) ? partner : entt::null;

    g_registry.emplace_or_replace<ecs::DirtyTag>(e);
}

entt::entity GetMarryPartner(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return entt::null;

    const auto* marriageState = g_registry.try_get<ecs::MarriageState>(e);
    if (!marriageState || marriageState->partner == entt::null ||
        !g_registry.valid(marriageState->partner))
        return entt::null;

    return marriageState->partner;
}

// The wedding map relation is owned by the wedding system; this is the
// existing session-facing entry point over it.
void SetWeddingMap(entt::entity e, entt::entity pMap)
{
    marriage::WeddingSystem::SetMemberMap(e, pMap);
}

entt::entity GetWeddingMap(entt::entity e)
{
    return marriage::WeddingSystem::GetMemberMap(e);
}

CWarMap* GetWarMap(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return nullptr;

    const auto* membership = g_registry.try_get<ecs::DungeonMembership>(e);
    if (!membership || !membership->warMap)
        return nullptr;

    // A destroyed war map is deleted; the manager map is the liveness check,
    // so a stale relation reads as no war map.
    if (CWarMapManager::instance().Find(membership->warMap->GetMapIndex()) != membership->warMap)
        return nullptr;

    return membership->warMap;
}

bool HasExchange(entt::entity e)
{
    if (!g_registry.valid(e)) return false;
    const auto* ref = g_registry.try_get<ecs::ExchangeRef>(e);
    const auto* session = ref && g_registry.valid(ref->session)
        ? g_registry.try_get<ecs::ExchangeSession>(ref->session) : nullptr;
    return session && (session->offers[0].owner == e || session->offers[1].owner == e);
}

// When this character last opened a personal shop. The warp block reads it
// to stop a player from shopping and then jumping away.
int GetMyShopTime(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return 0;
    const auto* warp = g_registry.try_get<ecs::WarpBlockState>(e);
    return warp ? warp->myShopTime : 0;
}

void SetMyShopTime(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return;
    g_registry.get_or_emplace<ecs::WarpBlockState>(e).myShopTime = thecore_pulse();
    g_registry.emplace_or_replace<ecs::DirtyTag>(e);
}

// When this character last refined. Like the shop time above, it blocks a
// warp for a moment so the trade cannot be escaped mid-way.
// When the safebox was last loaded; the warp block reads it like the shop
// and refine times beside it.
int GetSafeboxLoadTime(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return 0;
    const auto* warp = g_registry.try_get<ecs::WarpBlockState>(e);
    return warp ? warp->safeboxLoadTime : 0;
}

void SetSafeboxLoadTime(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return;
    g_registry.get_or_emplace<ecs::WarpBlockState>(e).safeboxLoadTime = thecore_pulse();
    g_registry.emplace_or_replace<ecs::DirtyTag>(e);
}

int GetRefineTime(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return 0;
    const auto* warp = g_registry.try_get<ecs::WarpBlockState>(e);
    return warp ? warp->refineTime : 0;
}

void SetRefineTime(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return;
    g_registry.get_or_emplace<ecs::WarpBlockState>(e).refineTime = thecore_pulse();
    g_registry.emplace_or_replace<ecs::DirtyTag>(e);
}

// The guild behind the refine NPC this character is standing at, if any. A
// guild smith charges differently from the town one.
CGuild* GetRefineGuild(entt::entity e)
{
    return GetGuild(InventorySystem::GetRefineNPC(e));
}

bool IsRefineThroughGuild(entt::entity e)
{
    return GetRefineGuild(e) != nullptr;
}

int GetLastBuyTime(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return 0;
    const auto* timers = g_registry.try_get<ecs::ShopTimers>(e);
    return timers ? timers->lastBuyPulse : 0;
}

void SetLastBuyTime(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return;
    g_registry.get_or_emplace<ecs::ShopTimers>(e).lastBuyPulse = thecore_pulse();
}

uint32_t GetLastBuySellTime(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return 0;
    const auto* timers = g_registry.try_get<ecs::ShopTimers>(e);
    return timers ? timers->lastBuySellTime : 0;
}

void SetLastBuySellTime(entt::entity e, uint32_t when)
{
    if (e == entt::null || !g_registry.valid(e))
        return;
    g_registry.get_or_emplace<ecs::ShopTimers>(e).lastBuySellTime = when;
}

entt::entity GetShop(entt::entity e)
{
	if (e == entt::null || !g_registry.valid(e))
		return entt::null;
	const auto* state = g_registry.try_get<ecs::ShopState>(e);
	if (!state || state->currentShop == entt::null)
		return entt::null;
	return ShopSystem::IsValid(state->currentShop) ? state->currentShop : entt::null;
}

bool GetNoOpenedShop(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return false;
    const auto* shop = g_registry.try_get<ecs::ShopState>(e);
    return shop && shop->noOpenedShop;
}

void SetNoOpenedShop(entt::entity e, bool value)
{
    if (e == entt::null || !g_registry.valid(e))
        return;
    g_registry.get_or_emplace<ecs::ShopState>(e).noOpenedShop = value;
}

bool GetKasmirPaket(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return false;
    const auto* shop = g_registry.try_get<ecs::ShopState>(e);
    return shop && shop->kasmirPaket;
}

void SetKasmirPaket(entt::entity e, bool value)
{
    if (e == entt::null || !g_registry.valid(e))
        return;
    g_registry.get_or_emplace<ecs::ShopState>(e).kasmirPaket = value;
}

// Opening and closing a personal shop. m_pkMyShop and m_bKasmirPaketBaslik
// were a mirror of ShopState::myShop and ::kasmirTitle, which the bodies
// also wrote; the shop sign had the same shape and was fixed with them.
void OpenMyShop(entt::entity e, const char* c_pszSign, TShopItemTable* pTable, uint8_t bItemCount
#ifdef KASMIR_PAKET_SYSTEM
    , uint32_t KasmirNpc, uint8_t KasmirBaslik
#endif
    )
{
	if (e == entt::null || !g_registry.valid(e))
		return;

	auto& shop = g_registry.get_or_emplace<ecs::ShopState>(e);
	if (e == entt::null || !g_registry.valid(e))
		return;
    if (!InventorySystem::CanHandleItems(e))
    {
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 291, "");
#endif
        return;
    }

#ifdef ENABLE_RESTRICT_GM_PERMISSIONS
    if (ecs::PlayerRuntime::GetGMLevel(e) > GM_PLAYER && ecs::PlayerRuntime::GetGMLevel(e) < GM_IMPLEMENTOR) {
        return;
    }
#endif

#ifndef ENABLE_OPEN_SHOP_WITH_ARMOR
    if (GetPart(PART_MAIN) > 2)
    {
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 503, "");
#endif
        return;
    }
#endif

    if (shop.myShop != entt::null)
    {
        CloseMyShop(e);
        return;
    }

    quest::PC* pPC = quest::CQuestManager::instance().GetPCForce(ecs::PlayerRuntime::GetPlayerID(e));
    if (pPC->IsRunning())
        return;

    if (bItemCount == 0)
        return;

    int64_t nTotalMoney = 0;

    for (int n = 0; n < bItemCount; ++n)
    {
        nTotalMoney += static_cast<int64_t>((pTable + n)->price);
    }

    nTotalMoney += static_cast<int64_t>(ecs::PointSystem::GetGold(e));

    if (GOLD_MAX <= nTotalMoney)
    {
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 226,
            "%lld"

            , GOLD_MAX);
#endif
        return;
    }

    char szSign[SHOP_SIGN_MAX_LEN + 1];
    strlcpy(szSign, c_pszSign, sizeof(szSign));

    // The sign the viewers are told about lives in ShopState; nothing wrote it
    // there, so EntityNetworkDispatch never had one to send.
    auto& shopState = g_registry.get_or_emplace<ecs::ShopState>(e);
    shopState.shopSign = szSign;

    if (shopState.shopSign.length() == 0)
        return;

    if (CBanwordManager::instance().CheckString(shopState.shopSign.c_str(), shopState.shopSign.length()))
    {
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 358, "");
#endif
        return;
    }

#ifdef KASMIR_PAKET_SYSTEM
    shop.kasmirTitle = KasmirBaslik;
    if (shop.kasmirTitle < 1 && shop.kasmirTitle > 6)
    {
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 46, "");
#endif
        return;
    }
#endif

    std::map<uint32_t, uint32_t> itemkind;

    std::set<TItemPos> cont;
    for (uint8_t i = 0; i < bItemCount; ++i)
    {
        if (cont.contains((pTable + i)->pos))
        {
            LOG_ERROR("MYSHOP: duplicate shop item detected! (name: {})", ecs::PlayerRuntime::GetName(e).data());
            return;
        }

        const entt::entity item = ItemSystem::GetItem(e, (pTable + i)->pos);

        if (ItemSystem::IsValidItem(item))
        {
            const TItemTable* item_table = ItemSystem::GetItemProto(item);

            if (item_table && (IS_SET(item_table->dwAntiFlags, ITEM_ANTIFLAG_GIVE | ITEM_ANTIFLAG_MYSHOP)))
            {
#ifdef TEXTS_IMPROVEMENT
                ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 416, "%s", ItemSystem::GetItemName(item));
#endif
                return;
            }

            if (ItemSystem::IsItemEquipped(item) == true)
            {
#ifdef TEXTS_IMPROVEMENT
                ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 541, "");
#endif
                return;
            }

            if (ItemSystem::IsItemLocked(item))
            {
#ifdef TEXTS_IMPROVEMENT
                ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 656, "");
#endif
                return;
            }

			const uint32_t itemCount = ItemSystem::GetItemCount(item);
			if (itemCount == 0)
			{
				LOG_ERROR("MYSHOP: zero-count item rejected (name: {} item_id: {})",
					ecs::PlayerRuntime::GetName(e).data(), ItemSystem::GetItemID(item));
				return;
			}
			itemkind[ItemSystem::GetItemVnum(item)] = (pTable + i)->price / itemCount;
        }

        cont.insert((pTable + i)->pos);
    }

    if (ItemSystem::CountItem(e, 71049)
#ifdef KASMIR_PAKET_SYSTEM
        || ItemSystem::CountItem(e, 88901)
#endif
        ) {
        TItemPriceListTable header;
        memset(&header, 0, sizeof(TItemPriceListTable));

        header.dwOwnerID = ecs::PlayerRuntime::GetPlayerID(e);
        header.byCount = itemkind.size();

        size_t idx = 0;
        for (auto it = itemkind.begin(); it != itemkind.end(); ++it)
        {
            header.aPriceInfo[idx].dwVnum = it->first;
            header.aPriceInfo[idx].dwPrice = it->second;
            idx++;
        }

        db_clientdesc->DBPacket(HEADER_GD_MYSHOP_PRICELIST_UPDATE, ecs::PlayerRuntime::GetDesc(e)->GetHandle(), &header, sizeof(TItemPriceListTable));
    }
    else if (ItemSystem::CountItem(e, 50200))
        ItemSystem::RemoveSpecifyItemEcs(e, 50200, 1);
    else
        return;

    ExchangeSystem::Cancel(e);

    TPacketGCShopSign p;

    p.bHeader = HEADER_GC_SHOP_SIGN;
    p.dwVID = ecs::PlayerRuntime::GetPacketVID(e);
    strlcpy(p.szSign, c_pszSign, sizeof(p.szSign));
#ifdef KASMIR_PAKET_SYSTEM
    p.bShopKasmirTitle = KasmirBaslik;
#endif
    ViewSystem::PacketView(e, &p, sizeof(TPacketGCShopSign));

    shop.myShop = CShopManager::instance().CreatePCShop(e, pTable, bItemCount);
    g_registry.emplace_or_replace<ecs::DirtyTag>(e);

    if (AffectSystem::IsPolymorphed(e) == true)
    {
        AffectSystem::RemoveAffect(e, AFFECT_POLYMORPH);
    }

    if (MountSystem::GetSummonedHorse(e) != entt::null)
    {
        MountSystem::SummonHorse(e, false, true);
    }
    else if (MountSystem::GetMountVnum(e))
    {
        AffectSystem::RemoveAffect(e, AFFECT_MOUNT);
        AffectSystem::RemoveAffect(e, AFFECT_MOUNT_BONUS);
    }

    uint32_t dwNpcShop = 30000;
#ifdef KASMIR_PAKET_SYSTEM
    dwNpcShop = KasmirNpc >= 30000 && KasmirNpc <= 30007 ? KasmirNpc : 30000;
#endif
    AffectSystem::SetPolymorph(e, dwNpcShop, true);
}

void CloseMyShop(entt::entity e)
{
	if (e == entt::null || !g_registry.valid(e))
		return;

	auto& shop = g_registry.get_or_emplace<ecs::ShopState>(e);
    if (shop.myShop != entt::null)
    {
        g_registry.get_or_emplace<ecs::ShopState>(e).shopSign.clear();
        CShopManager::instance().DestroyPCShop(e);
        shop.myShop = entt::null;
        g_registry.emplace_or_replace<ecs::DirtyTag>(e);
#ifdef KASMIR_PAKET_SYSTEM
        shop.kasmirTitle = 0;
        ecs::SocialSystem::SetKasmirPaket(e, false);
#endif

        TPacketGCShopSign p;

        p.bHeader = HEADER_GC_SHOP_SIGN;
        p.dwVID = ecs::PlayerRuntime::GetPacketVID(e);
#ifdef KASMIR_PAKET_SYSTEM
        p.bShopKasmirTitle = shop.kasmirTitle;
#endif
        p.szSign[0] = '\0';

        ViewSystem::PacketView(e, &p, sizeof(p));
        AffectSystem::SetPolymorph(e, ecs::PlayerRuntime::GetJob(e), true);
    }
}

entt::entity GetMyShop(entt::entity e)
{
	if (e == entt::null || !g_registry.valid(e))
		return entt::null;
	const auto* state = g_registry.try_get<ecs::ShopState>(e);
	if (!state || state->myShop == entt::null)
		return entt::null;
	return ShopSystem::IsValid(state->myShop) ? state->myShop : entt::null;
}

entt::entity GetShopOwner(entt::entity e)
{
	if (e == entt::null || !g_registry.valid(e))
		return entt::null;
	const auto* state = g_registry.try_get<ecs::ShopState>(e);
	return state && state->shopOwner != entt::null && g_registry.valid(state->shopOwner)
		? state->shopOwner : entt::null;
}

void SetShopOwner(entt::entity e, entt::entity owner)
{
	if (e == entt::null || !g_registry.valid(e))
		return;
	auto& state = g_registry.get_or_emplace<ecs::ShopState>(e);
	state.shopOwner = owner != entt::null && g_registry.valid(owner) ? owner : entt::null;
	g_registry.emplace_or_replace<ecs::DirtyTag>(e);
}

void SetShop(entt::entity e, entt::entity shop)
{
	if (e == entt::null || !g_registry.valid(e))
		return;
	if (shop != entt::null && !ShopSystem::IsValid(shop))
		shop = entt::null;
	auto& state = g_registry.get_or_emplace<ecs::ShopState>(e);
	state.currentShop = shop;
	auto& flags = g_registry.get_or_emplace<ecs::CharacterRuntimeFlagsComponent>(e);
	if (shop != entt::null)
		SET_BIT(flags.instantFlag, INSTANT_FLAG_SHOP);
	else
	{
		REMOVE_BIT(flags.instantFlag, INSTANT_FLAG_SHOP);
		state.shopOwner = entt::null;
	}
	g_registry.emplace_or_replace<ecs::DirtyTag>(e);
}

entt::entity GetPartyLeader(entt::entity e)
{
    const entt::entity party = GetParty(e);
    return party != entt::null ? PartySystem::GetLeader(party) : entt::null;
}

void ForEachNearPartyMember(entt::entity e, const std::function<void(entt::entity)>& visitor)
{
    const entt::entity party = GetParty(e);

    if (party != entt::null)
    {
        auto walk = [&visitor](entt::entity member) { visitor(member); };
        PartySystem::ForEachNearMember(party, walk);
    }
}

void ForEachOnlinePartyMember(entt::entity e, const std::function<void(entt::entity)>& visitor)
{
    const entt::entity party = GetParty(e);

    if (party != entt::null)
    {
        auto walk = [&visitor](entt::entity member) { visitor(member); };
        PartySystem::ForEachOnlineMember(party, walk);
    }
}

void ForEachPartyMemberOnMap(entt::entity e, int32_t mapIndex,
    const std::function<void(entt::entity)>& visitor)
{
    const entt::entity party = GetParty(e);

    if (party != entt::null)
    {
        auto walk = [&visitor](entt::entity member) { visitor(member); };
        PartySystem::ForEachOnMapMember(party, walk, mapIndex);
    }
}

// The five minute wait between guild deposits. CHARACTER::m_deposit_pulse and
// GuildDepositState::nextAllowedPulse were two counters for one rule: the mine
// wrote the component, the guild window wrote the field, and neither could see
// the other's, so alternating the two entry points skipped the wait entirely.
bool CanDeposit(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return false;

    const auto* state = g_registry.try_get<ecs::GuildDepositState>(e);
    return !state || state->nextAllowedPulse == 0 ||
        state->nextAllowedPulse < thecore_pulse();
}

void UpdateDepositPulse(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return;

    auto& state = g_registry.get_or_emplace<ecs::GuildDepositState>(e);
    state.nextAllowedPulse = thecore_pulse() + PASSES_PER_SEC(60 * 5);
    g_registry.emplace_or_replace<ecs::DirtyTag>(e);
}

bool DepositGuildMoney(entt::entity character, CGuild& guild, int gold)
{
    if (!CanDeposit(character))
    {
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 493, "");
#endif
        return false;
    }

    if (gold <= 0 || ecs::PointSystem::GetGold(character) < gold)
        return false;

    ecs::PointSystem::Change(character, POINT_GOLD, -gold);

    TPacketGDGuildMoney packet{};
    packet.dwGuild = guild.GetID();
    packet.iGold = gold;
    db_clientdesc->DBPacket(
        HEADER_GD_GUILD_DEPOSIT_MONEY, 0, &packet, sizeof(packet));

    char hint[65];
    snprintf(hint, sizeof(hint), "%u %s", guild.GetID(), guild.GetName());
    LogManager::instance().CharLog(
        character, gold, "GUILD_DEPOSIT", hint);
    UpdateDepositPulse(character);

    LOG_INFO("GUILD: DEPOSIT {}:{} player {}[{}] gold {}",
        guild.GetName(), guild.GetID(),
        ecs::PlayerRuntime::GetName(character).data(),
        ecs::PlayerRuntime::GetPlayerID(character), gold);
    return true;
}

} // namespace ecs::SocialSystem

namespace ecs::SocialSystem {

// The party this character belongs to. CHARACTER::m_pkParty, SocialRefs::party
// and PartyMembership::party were three copies of it, kept level only because
// this one setter wrote all three. The party itself is now an entity; a stale
// handle reads as null.
void SetParty(entt::entity e, entt::entity pkParty)
{
    if (e == entt::null || !g_registry.valid(e))
        return;

    if (pkParty != entt::null && !PartySystem::IsValid(pkParty))
        pkParty = entt::null;

    auto& refs = g_registry.get_or_emplace<ecs::SocialRefs>(e);
    const entt::entity previous = refs.party;

    if (pkParty == previous)
        return;

    if (pkParty != entt::null && previous != entt::null)
        LOG_ERROR("{} is trying to reassigning party (current {}, new party {})",
            ecs::PlayerRuntime::GetName(e).data(),
            static_cast<uint32_t>(previous),
            static_cast<uint32_t>(pkParty));

    LOG_TRACE("PARTY set to {}", static_cast<uint32_t>(pkParty));

    const bool isPC = ecs::PlayerRuntime::IsPC(e);

#ifdef ENABLE_BUG_FIXES
    if (GetDungeon(e) != entt::null && isPC && pkParty == entt::null)
        SetDungeon(e, entt::null);
#endif

#ifdef ENABLE_NEW_USE_POTION
    if (isPC && previous != entt::null && pkParty == entt::null &&
        PartySystem::GetLeaderPID(previous) == ecs::PlayerRuntime::GetPlayerID(e))
    {
        if (CAffect* pAffect = AffectSystem::FindAffect(e, AFFECT_NEW_POTION31))
        {
            const auto item = ItemSystem::FindItemByID(e, pAffect->dwFlag);
            if (ItemSystem::IsValidItem(item))
            {
                ItemSystem::UnlockItem(item);
                ItemSystem::SetItemSocket(item, 1, 0);
            }

            AffectSystem::RemoveAffect(e, AFFECT_NEW_POTION31);
        }
    }
#endif

    refs.party = pkParty;

    if (isPC)
    {
        if (auto* status = g_registry.try_get<ecs::StatusFlags>(e))
        {
            status->isPartyState = (pkParty != entt::null);
            g_registry.emplace_or_replace<ecs::DirtyTag>(e);
        }

        NetworkSyncSystem::UpdatePacket(e);
    }
}

// The guild, in the same shape: three copies, one setter.
void SetGuild(entt::entity e, CGuild* pGuild)
{
    if (e == entt::null || !g_registry.valid(e))
        return;

    auto& refs = g_registry.get_or_emplace<ecs::SocialRefs>(e);
    if (refs.guild == pGuild)
        return;

    refs.guild = pGuild;
    NetworkSyncSystem::UpdatePacket(e);
}

} // namespace ecs::SocialSystem

EVENTINFO(TPartyJoinEventInfo)
{
    uint32_t dwGuestPID;
    uint32_t dwLeaderPID;

    TPartyJoinEventInfo()
        : dwGuestPID(0)
        , dwLeaderPID(0)
    {
    }
};

EVENTFUNC(party_request_event)
{
    TPartyJoinEventInfo* info = dynamic_cast<TPartyJoinEventInfo*>(event->info);

    if (info == nullptr)
    {
        LOG_ERROR("party_request_event> <Factor> Null pointer");
        return 0;
    }

    const entt::entity guest = CHARACTER_MANAGER::instance().FindEntityByPID(info->dwGuestPID);

    if (ecs::IsCharacter(guest))
    {
        LOG_INFO("PartyRequestEvent {}", ecs::PlayerRuntime::GetName(guest).data());
        ecs::ChatSystem::Send(guest, CHAT_TYPE_COMMAND, "PartyRequestDenied");
        if (auto* invitations = g_registry.try_get<ecs::PartyInvitations>(guest))
            invitations->requestEvent = nullptr;
    }

    return 0;
}

bool ecs::SocialSystem::RequestToParty(entt::entity e, entt::entity leaderEntity)
{
    if (!ecs::IsCharacter(e))
        return false;

    entt::entity leader = ecs::PlayerRuntime::IsValid(leaderEntity) ? leaderEntity : entt::null;
    const entt::entity leaderParty = ecs::SocialSystem::GetParty(leaderEntity);
    if (leaderParty != entt::null)
        leader = PartySystem::GetLeader(leaderParty);

    if (leader == entt::null)
    {
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 488, "");
#endif
        return false;
    }

    if (const auto* invitations = g_registry.try_get<ecs::PartyInvitations>(e); invitations && invitations->requestEvent)
        return false;

    if (!ecs::PlayerRuntime::GetDesc(e) || !ecs::PlayerRuntime::IsPC(leaderEntity))
        return false;

    if (ecs::PlayerRuntime::IsBlockMode(leader, BLOCK_PARTY_REQUEST))
        return false;

    ecs::SocialSystem::PartyJoinErrCode errcode = ecs::SocialSystem::IsPartyJoinableCondition(leaderEntity, e);

    switch (errcode)
    {
    case ecs::SocialSystem::PERR_NONE:
        break;

    case ecs::SocialSystem::PERR_SERVER:
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 208, "");
#endif
        return false;

    case ecs::SocialSystem::PERR_DUNGEON:
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 200, "");
#endif
        return false;
    case ecs::SocialSystem::PERR_OBSERVER:
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 195, "");
#endif
        return false;

    case ecs::SocialSystem::PERR_LVBOUNDARY:
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 194, "");
#endif
        return false;

    case ecs::SocialSystem::PERR_LOWLEVEL:
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 214, "");
#endif
        return false;

    case ecs::SocialSystem::PERR_HILEVEL:
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 214, "");
#endif
        return false;

    case ecs::SocialSystem::PERR_ALREADYJOIN:
        return false;

    case ecs::SocialSystem::PERR_PARTYISFULL:
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 199, "");
#endif
        return false;

    default:
        LOG_ERROR("Do not process party join error({})", errcode);
        return false;
    }

    TPartyJoinEventInfo* info = AllocEventInfo<TPartyJoinEventInfo>();

    info->dwGuestPID = ecs::PlayerRuntime::GetPlayerID(e);
    info->dwLeaderPID = ecs::PlayerRuntime::GetPlayerID(leaderEntity);

    g_registry.get_or_emplace<ecs::PartyInvitations>(e).requestEvent = event_create(party_request_event, info, PASSES_PER_SEC(10));

    ecs::ChatSystem::Send(leaderEntity, CHAT_TYPE_COMMAND, "PartyRequest %u", ecs::PlayerRuntime::GetPacketVID(e));
#ifdef TEXTS_IMPROVEMENT
    ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 106, "%s", ecs::PlayerRuntime::GetName(leaderEntity).data());
#endif
    return true;
}

void ecs::SocialSystem::DenyToParty(entt::entity e, entt::entity memberEntity)
{
    if (!ecs::IsCharacter(e) || !ecs::IsCharacter(memberEntity))
        return;

    auto* memberInvitations = g_registry.try_get<ecs::PartyInvitations>(memberEntity);

    LOG_INFO("DenyToParty {} member {} {}", ecs::PlayerRuntime::GetName(e), ecs::PlayerRuntime::GetName(memberEntity), static_cast<const void*>(memberInvitations ? get_pointer(memberInvitations->requestEvent) : nullptr));

    if (!memberInvitations || !memberInvitations->requestEvent)
        return;

    TPartyJoinEventInfo* info = dynamic_cast<TPartyJoinEventInfo*>(memberInvitations->requestEvent->info);

    if (!info)
    {
        LOG_ERROR("SocialSystem::DenyToParty> <Factor> Null pointer");
        return;
    }

    if (info->dwGuestPID != ecs::PlayerRuntime::GetPlayerID(memberEntity))
        return;

    if (info->dwLeaderPID != ecs::PlayerRuntime::GetPlayerID(e))
        return;

    event_cancel(&memberInvitations->requestEvent);

    ecs::ChatSystem::Send(memberEntity, CHAT_TYPE_COMMAND, "PartyRequestDenied");
}

void ecs::SocialSystem::AcceptToParty(entt::entity e, entt::entity memberEntity)
{
    if (!ecs::IsCharacter(e) || !ecs::IsCharacter(memberEntity))
        return;

    auto* memberInvitations = g_registry.try_get<ecs::PartyInvitations>(memberEntity);

    LOG_INFO("AcceptToParty {} member {} {}", ecs::PlayerRuntime::GetName(e), ecs::PlayerRuntime::GetName(memberEntity), static_cast<const void*>(memberInvitations ? get_pointer(memberInvitations->requestEvent) : nullptr));

    if (!memberInvitations || !memberInvitations->requestEvent)
        return;

    TPartyJoinEventInfo* info = dynamic_cast<TPartyJoinEventInfo*>(memberInvitations->requestEvent->info);

    if (!info)
    {
        LOG_ERROR("SocialSystem::AcceptToParty> <Factor> Null pointer");
        return;
    }

    if (info->dwGuestPID != ecs::PlayerRuntime::GetPlayerID(memberEntity))
        return;

    if (info->dwLeaderPID != ecs::PlayerRuntime::GetPlayerID(e))
        return;

    event_cancel(&memberInvitations->requestEvent);

    const entt::entity party = ecs::SocialSystem::GetParty(e);

    if (party != entt::null)
    {
        if (ecs::PlayerRuntime::GetPlayerID(e) != PartySystem::GetLeaderPID(party))
            return;

        ecs::SocialSystem::PartyJoinErrCode errcode = ecs::SocialSystem::IsPartyJoinableCondition(e, memberEntity);
        switch (errcode)
        {
        case ecs::SocialSystem::PERR_NONE: ecs::SocialSystem::PartyJoin(memberEntity, e); return;
        case ecs::SocialSystem::PERR_SERVER:
#ifdef TEXTS_IMPROVEMENT
            ecs::ChatSystem::SendNew(memberEntity, CHAT_TYPE_INFO, 208, "");
#endif
            break;
        case ecs::SocialSystem::PERR_DUNGEON:
#ifdef TEXTS_IMPROVEMENT
            ecs::ChatSystem::SendNew(memberEntity, CHAT_TYPE_INFO, 200, "");
#endif
            break;
        case ecs::SocialSystem::PERR_OBSERVER:
#ifdef TEXTS_IMPROVEMENT
            ecs::ChatSystem::SendNew(memberEntity, CHAT_TYPE_INFO, 195, "");
#endif
            break;
        case ecs::SocialSystem::PERR_LOWLEVEL:
        case ecs::SocialSystem::PERR_LVBOUNDARY:
#ifdef TEXTS_IMPROVEMENT
            ecs::ChatSystem::SendNew(memberEntity, CHAT_TYPE_INFO, 194, "");
#endif
            break;
        case ecs::SocialSystem::PERR_HILEVEL:
#ifdef TEXTS_IMPROVEMENT
            ecs::ChatSystem::SendNew(memberEntity, CHAT_TYPE_INFO, 214, "");
#endif
            break;
        case ecs::SocialSystem::PERR_ALREADYJOIN:
            break;
        case ecs::SocialSystem::PERR_PARTYISFULL:
        {
#ifdef TEXTS_IMPROVEMENT
            ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 199, "");
            ecs::ChatSystem::SendNew(memberEntity, CHAT_TYPE_INFO, 220, "");
#endif
            break;
        }
        default:
            LOG_ERROR("Do not process party join error({})", errcode);
        }
    }

    ecs::ChatSystem::Send(memberEntity, CHAT_TYPE_COMMAND, "PartyRequestDenied");
}

EVENTFUNC(party_invite_event)
{
    TPartyJoinEventInfo* pInfo = dynamic_cast<TPartyJoinEventInfo*>(event->info);

    if (pInfo == nullptr)
    {
        LOG_ERROR("party_invite_event> <Factor> Null pointer");
        return 0;
    }

    const entt::entity inviter = CHARACTER_MANAGER::instance().FindEntityByPID(pInfo->dwLeaderPID);

    if (ecs::IsCharacter(inviter))
    {
        LOG_INFO("PartyInviteEvent {}", ecs::PlayerRuntime::GetName(inviter).data());
        ecs::SocialSystem::PartyInviteDeny(inviter, pInfo->dwGuestPID);
    }

    return 0;
}

void ecs::SocialSystem::PartyInvite(entt::entity e, entt::entity invitee)
{
    if (!ecs::IsCharacter(e))
        return;

    const entt::entity inviteParty = ecs::SocialSystem::GetParty(e);

    if (inviteParty != entt::null && PartySystem::GetLeaderPID(inviteParty) != ecs::PlayerRuntime::GetPlayerID(e))
    {
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 218, "");
#endif
        return;
    }
    else if (ecs::PlayerRuntime::IsBlockMode(invitee, BLOCK_PARTY_INVITE))
    {
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 192, "%s", ecs::PlayerRuntime::GetName(invitee).data());
#endif
        return;
    }

#ifdef ENABLE_PVP_ADVANCED
    else if ((ecs::PlayerRuntime::GetDuelOption(e, "BlockParty")))
    {
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 516, "");
#endif
        return;
    }

    else if ((ecs::PlayerRuntime::GetDuelOption(invitee, "BlockParty")))
    {
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 517, "%s", ecs::PlayerRuntime::GetName(invitee).data());
#endif
        return;
    }
#endif

    ecs::SocialSystem::PartyJoinErrCode errcode = ecs::SocialSystem::IsPartyJoinableCondition(e, invitee);

    switch (errcode)
    {
    case ecs::SocialSystem::PERR_NONE:
        break;

    case ecs::SocialSystem::PERR_SERVER:
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 208, "");
#endif
        return;
    case ecs::SocialSystem::PERR_DUNGEON:
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 200, "");
#endif
        return;
    case ecs::SocialSystem::PERR_OBSERVER:
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 195, "");
#endif
        return;
    case ecs::SocialSystem::PERR_LVBOUNDARY:
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 194, "");
#endif
        return;
    case ecs::SocialSystem::PERR_LOWLEVEL:
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 214, "");
#endif
        return;
    case ecs::SocialSystem::PERR_HILEVEL:
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 214, "");
#endif
        return;
    case ecs::SocialSystem::PERR_ALREADYJOIN:
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 210, "%s", ecs::PlayerRuntime::GetName(invitee).data());
#endif
        return;
    case ecs::SocialSystem::PERR_PARTYISFULL:
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 199, "");
#endif
        return;
    default:
        LOG_ERROR("Do not process party join error({})", errcode);
        return;
    }

    if (const auto* invitations = g_registry.try_get<ecs::PartyInvitations>(e);
        invitations && invitations->inviteEvents.contains(ecs::PlayerRuntime::GetPlayerID(invitee)))
        return;

    TPartyJoinEventInfo* info = AllocEventInfo<TPartyJoinEventInfo>();

    info->dwGuestPID = ecs::PlayerRuntime::GetPlayerID(invitee);
    info->dwLeaderPID = ecs::PlayerRuntime::GetPlayerID(e);

    g_registry.get_or_emplace<ecs::PartyInvitations>(e).inviteEvents.emplace(ecs::PlayerRuntime::GetPlayerID(invitee), event_create(party_invite_event, info, PASSES_PER_SEC(10)));

    TPacketGCPartyInvite p;
    p.header = HEADER_GC_PARTY_INVITE;
    p.leader_vid = ecs::PlayerRuntime::GetPacketVID(e);
    ecs::PlayerRuntime::GetDesc(invitee)->Packet(&p, sizeof(p));
}

void ecs::SocialSystem::PartyInviteAccept(entt::entity e, entt::entity invitee)
{
    if (!ecs::IsCharacter(e))
        return;

    auto* invitations = g_registry.try_get<ecs::PartyInvitations>(e);
    if (!invitations || !invitations->inviteEvents.contains(ecs::PlayerRuntime::GetPlayerID(invitee)))
    {
        LOG_INFO("PartyInviteAccept from not invited character({})", ecs::PlayerRuntime::GetName(invitee).data());
        return;
    }

    const auto itFind = invitations->inviteEvents.find(ecs::PlayerRuntime::GetPlayerID(invitee));
    event_cancel(&itFind->second);
    invitations->inviteEvents.erase(itFind);

    const entt::entity acceptParty = ecs::SocialSystem::GetParty(e);

    if (acceptParty != entt::null && PartySystem::GetLeaderPID(acceptParty) != ecs::PlayerRuntime::GetPlayerID(e))
    {
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 218, "");
#endif
        return;
    }

    ecs::SocialSystem::PartyJoinErrCode errcode = ecs::SocialSystem::IsPartyJoinableMutableCondition(e, invitee);

    switch (errcode)
    {
    case ecs::SocialSystem::PERR_NONE:
        break;
    case ecs::SocialSystem::PERR_SERVER:
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(invitee, CHAT_TYPE_INFO, 208, "");
#endif
        return;
    case ecs::SocialSystem::PERR_DUNGEON:
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(invitee, CHAT_TYPE_INFO, 201, "");
#endif
        return;
    case ecs::SocialSystem::PERR_OBSERVER:
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(invitee, CHAT_TYPE_INFO, 195, "");
#endif
        return;
    case ecs::SocialSystem::PERR_LVBOUNDARY:
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(invitee, CHAT_TYPE_INFO, 194, "");
#endif
        return;
    case ecs::SocialSystem::PERR_LOWLEVEL:
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(invitee, CHAT_TYPE_INFO, 214, "");
#endif
        return;
    case ecs::SocialSystem::PERR_HILEVEL:
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(invitee, CHAT_TYPE_INFO, 214, "");
#endif
        return;
    case ecs::SocialSystem::PERR_ALREADYJOIN:
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(invitee, CHAT_TYPE_INFO, 212, "");
#endif
        return;
    case ecs::SocialSystem::PERR_PARTYISFULL:
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 199, "");
        ecs::ChatSystem::SendNew(invitee, CHAT_TYPE_INFO, 220, "");
#endif
        return;
    default:
        LOG_ERROR("ignore party join error({})", errcode);
        return;
    }

    if (ecs::SocialSystem::GetParty(e) != entt::null)
        ecs::SocialSystem::PartyJoin(invitee, e);
    else
    {
        const entt::entity pParty = CPartyManager::instance().CreateParty(e);

        PartySystem::Join(pParty, ecs::PlayerRuntime::GetPlayerID(invitee));
        PartySystem::Link(pParty, invitee);
        PartySystem::SendPartyInfoAllToOne(pParty, e);
    }
}

void ecs::SocialSystem::PartyInviteDeny(entt::entity e, uint32_t dwPID)
{
    if (!ecs::IsCharacter(e))
        return;

    auto* invitations = g_registry.try_get<ecs::PartyInvitations>(e);
    if (!invitations || !invitations->inviteEvents.contains(dwPID))
    {
        LOG_INFO("PartyInviteDeny to not exist event(inviter PID: {}, invitee PID: {})", ecs::PlayerRuntime::GetPlayerID(e), dwPID);
        return;
    }

    const auto itFind = invitations->inviteEvents.find(dwPID);
    event_cancel(&itFind->second);
    invitations->inviteEvents.erase(itFind);
#ifdef TEXTS_IMPROVEMENT
    const entt::entity pchInvitee = CHARACTER_MANAGER::instance().FindEntityByPID(dwPID);
    if (pchInvitee != entt::null) {
        ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 192, "%s", ecs::PlayerRuntime::GetName(pchInvitee).data());
    }
#endif
}

void ecs::SocialSystem::CancelPartyRequest(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return;

    if (auto* invitations = g_registry.try_get<ecs::PartyInvitations>(e))
        event_cancel(&invitations->requestEvent);
}

void ecs::SocialSystem::PartyJoin(entt::entity guest, entt::entity leader)
{
    const entt::entity party = ecs::SocialSystem::GetParty(leader);

    if (party != entt::null) {
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(leader, CHAT_TYPE_INFO, 1249, "%s", ecs::PlayerRuntime::GetName(guest).data());
        ecs::ChatSystem::SendNew(guest, CHAT_TYPE_INFO, 193, "%s", ecs::PlayerRuntime::GetName(leader).data());
#endif
        PartySystem::Join(party, ecs::PlayerRuntime::GetPlayerID(guest));
        PartySystem::Link(party, guest);
    }
}

ecs::SocialSystem::PartyJoinErrCode ecs::SocialSystem::IsPartyJoinableCondition(const entt::entity leader, const entt::entity guest)
{
    return IsPartyJoinableMutableCondition(leader, guest);
}

static bool __party_can_join_by_level(entt::entity leader, entt::entity guest)
{
    int level_limit = 50;
    return (abs(ecs::PointSystem::GetLevel(leader) - ecs::PointSystem::GetLevel(guest)) <= level_limit);
}

ecs::SocialSystem::PartyJoinErrCode ecs::SocialSystem::IsPartyJoinableMutableCondition(const entt::entity leader, const entt::entity guest)
{
    if (!CPartyManager::instance().IsEnablePCParty())
        return PERR_SERVER;
    else if (ecs::SocialSystem::GetDungeon(leader) != entt::null)
        return PERR_DUNGEON;
    else if (ecs::PlayerRuntime::IsObserverMode(guest))
        return PERR_OBSERVER;
    else if (false == __party_can_join_by_level(
		leader, guest))
        return PERR_LVBOUNDARY;
    else if (ecs::SocialSystem::GetParty(guest) != entt::null)
        return PERR_ALREADYJOIN;
    else if (ecs::SocialSystem::GetParty(leader) != entt::null)
    {
        if (PartySystem::GetMemberCount(ecs::SocialSystem::GetParty(leader)) == PARTY_MAX_MEMBER)
            return PERR_PARTYISFULL;
    }

    return PERR_NONE;
}

int ecs::SocialSystem::GetMarriageBonus(entt::entity e, uint32_t itemVnum, bool share)
{
    if (!ecs::PlayerRuntime::IsPC(e))
        return 0;
    auto* pair = marriage::CManager::instance().Get(ecs::PlayerRuntime::GetPlayerID(e));
    return pair ? pair->GetBonus(itemVnum, share, e) : 0;
}

// ForEachMemberPtr handed this a null pointer for every member not linked to a
// character, and it dereferenced it; the online walk skips those members.
struct FFindReviver
{
    void operator()(entt::entity member)
    {
        if (!ecs::PlayerRuntime::IsMonster(member))
            return;

        if (ecs::PlayerRuntime::IsReviver(member) && reviver != member && !CombatSystem::IsDead(member))
        {
            const TMobTable* mobTable = ecs::PlayerRuntime::GetMobTable(member);
            if (mobTable && number(1, 100) <= mobTable->bRevivePoint)
            {
                HasReviver = true;
                reviver = member;
            }
        }
    }

    entt::entity reviver { entt::null };
    bool HasReviver { false };
};

namespace ecs::SocialSystem {

// Whether a reviver mob stands in this one's party.
bool HasReviverInParty(entt::entity e)
{
    const entt::entity party = GetParty(e);
    if (party == entt::null)
        return false;

    if (PartySystem::GetMemberCount(party) == 1)
        return false;

    FFindReviver f;
    PartySystem::ForEachOnlineMember(party, f);
    return f.HasReviver;
}

void SendGuildName(entt::entity viewer, CGuild* pGuild)
{
    if (nullptr == pGuild || viewer == entt::null || !g_registry.valid(viewer))
        return;

    LPDESC desc = ecs::PlayerRuntime::GetDesc(viewer);
    if (nullptr == desc)
        return;

    auto& known = g_registry.get_or_emplace<ecs::KnownGuilds>(viewer);
    if (!known.ids.insert(pGuild->GetID()).second)
        return;  // already sent to this viewer

    TPacketGCGuildName pack = {};

    pack.header = HEADER_GC_GUILD;
    pack.subheader = GUILD_SUBHEADER_GC_GUILD_NAME;
    pack.size = sizeof(TPacketGCGuildName);
    pack.guildID = pGuild->GetID();
    memcpy(pack.guildName, pGuild->GetName(), GUILD_NAME_MAX_LEN);
#ifdef ENABLE_GUILD_RENEWAL_BY_RAZOR93
    pack.guildLevel = pGuild->GetLevel();
#endif

    desc->Packet(&pack, sizeof(pack));
}

} // namespace ecs::SocialSystem

