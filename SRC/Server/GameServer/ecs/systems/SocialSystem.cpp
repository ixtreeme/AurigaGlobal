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

LPPARTY GetParty(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return nullptr;

    if (const auto* refs = g_registry.try_get<ecs::SocialRefs>(e))
        return refs->party;

    return nullptr;
}

CGuild* GetGuild(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return nullptr;

    if (const auto* refs = g_registry.try_get<ecs::SocialRefs>(e))
        return refs->guild;

    return nullptr;
}

// The dungeon this character is counted against. CHARACTER::m_pkDungeon held
// it and DungeonMembership::dungeon was written by one quest binding alone, so
// every reader of the component but that one saw nothing. One home now.
void SetDungeon(entt::entity e, LPDUNGEON pkDungeon)
{
	if (e == entt::null || !g_registry.valid(e))
		return;

	// CDungeon counts members by pointer, so the character is still needed for
	// the four Inc/Dec calls; each is its own migration.
	LPCHARACTER self = ecs::LegacyCharOf(e);
	if (!self)
		return;

	auto& membership = g_registry.get_or_emplace<ecs::DungeonMembership>(e);

	if (pkDungeon && membership.dungeon)
    {
        LOG_ERROR("{} is trying to reassigning dungeon (current {}, new party {})", ecs::PlayerRuntime::GetName(e).data(), static_cast<const void*>(get_pointer(membership.dungeon)), static_cast<const void*>(get_pointer(pkDungeon)));
    }

    if (membership.dungeon)
    {
        if (ecs::PlayerRuntime::IsPC(e))
        {
            if (ecs::SocialSystem::GetParty(e))
                membership.dungeon->DecPartyMember(ecs::SocialSystem::GetParty(e), self);
            else
                membership.dungeon->DecMember(self);
        }
    }

    membership.dungeon = pkDungeon;

    if (pkDungeon)
    {
        if (ecs::PlayerRuntime::IsPC(e))
        {
            if (ecs::SocialSystem::GetParty(e))
                membership.dungeon->IncPartyMember(ecs::SocialSystem::GetParty(e), self);
            else
                membership.dungeon->IncMember(self);
        }
        else if (ecs::PlayerRuntime::IsMonster(e) || ecs::PlayerRuntime::IsStone(e))
        {
            membership.dungeon->IncMonster();
        }
    }
	g_registry.emplace_or_replace<ecs::DirtyTag>(e);
}

LPDUNGEON GetDungeon(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return nullptr;

    const auto* membership = g_registry.try_get<ecs::DungeonMembership>(e);
    return membership ? membership->dungeon : nullptr;
}

// The guild war map this character is counted against. CHARACTER::m_pWarMap
// held it and DungeonMembership::warMap was written by nothing, so GetWarMap
// and the two quest bindings over it answered "no war map" for everyone.
void SetWarMap(entt::entity e, CWarMap* pWarMap)
{
    if (e == entt::null || !g_registry.valid(e))
        return;

    // CWarMap counts its members by pointer; that is its own migration.
    LPCHARACTER self = ecs::LegacyCharOf(e);
    if (!self)
        return;

    auto& membership = g_registry.get_or_emplace<ecs::DungeonMembership>(e);

    if (membership.warMap)
        membership.warMap->DecMember(self);

    membership.warMap = pWarMap;

    if (membership.warMap)
        membership.warMap->IncMember(self);

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

// The wedding map, in the same shape: the field was written and the component
// beside it was only ever read, by two quest bindings that always saw nothing.
void SetWeddingMap(entt::entity e, marriage::WeddingMap* pMap)
{
    if (e == entt::null || !g_registry.valid(e))
        return;

    // WeddingMap counts its members by pointer; that is its own migration.
    LPCHARACTER self = ecs::LegacyCharOf(e);
    if (!self)
        return;

    auto& marriageState = g_registry.get_or_emplace<ecs::MarriageState>(e);

    if (marriageState.weddingMap)
        marriageState.weddingMap->DecMember(self);

    marriageState.weddingMap = pMap;

    if (marriageState.weddingMap)
        marriageState.weddingMap->IncMember(self);

    g_registry.emplace_or_replace<ecs::DirtyTag>(e);
}

marriage::WeddingMap* GetWeddingMap(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return nullptr;

    const auto* marriageState = g_registry.try_get<ecs::MarriageState>(e);
    return marriageState ? marriageState->weddingMap : nullptr;
}

CWarMap* GetWarMap(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return nullptr;

    const auto* membership = g_registry.try_get<ecs::DungeonMembership>(e);
    return membership ? membership->warMap : nullptr;
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

CShop* GetShop(entt::entity e)
{
	if (e == entt::null || !g_registry.valid(e))
		return nullptr;
	const auto* state = g_registry.try_get<ecs::ShopState>(e);
	return state ? state->currentShop : nullptr;
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
	// CountSpecifyItem, RemoveSpecifyItem, GetHorse and HorseSummon have no
	// entity form yet; each is its own migration and they share this resolve.
	LPCHARACTER self = ecs::LegacyCharOf(e);
	if (!self)
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

    if (shop.myShop)
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

    if (self->CountSpecifyItem(71049)
#ifdef KASMIR_PAKET_SYSTEM
        || self->CountSpecifyItem(88901)
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
    else if (self->CountSpecifyItem(50200))
        self->RemoveSpecifyItem(50200, 1);
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

    if (self->GetHorse())
    {
        self->HorseSummon(false, true);
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
    if (shop.myShop)
    {
        g_registry.get_or_emplace<ecs::ShopState>(e).shopSign.clear();
        CShopManager::instance().DestroyPCShop(e);
        shop.myShop = nullptr;
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

CShop* GetMyShop(entt::entity e)
{
	if (e == entt::null || !g_registry.valid(e))
		return nullptr;
	const auto* state = g_registry.try_get<ecs::ShopState>(e);
	return state ? state->myShop : nullptr;
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

void SetShop(entt::entity e, CShop* shop)
{
	if (e == entt::null || !g_registry.valid(e))
		return;
	auto& state = g_registry.get_or_emplace<ecs::ShopState>(e);
	state.currentShop = shop;
	auto& flags = g_registry.get_or_emplace<ecs::CharacterRuntimeFlagsComponent>(e);
	if (shop)
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
    LPPARTY party = GetParty(e);
    LPCHARACTER leader = party ? party->GetLeader() : nullptr;
    return leader ? leader->GetEntityHandle() : entt::null;
}

namespace {
struct EntityPartyVisitor
{
    const std::function<void(entt::entity)>& visitor;

    void operator()(LPCHARACTER character) const
    {
        visitor(character ? character->GetEntityHandle() : entt::null);
    }
};
} // namespace

void ForEachNearPartyMember(entt::entity e, const std::function<void(entt::entity)>& visitor)
{
    if (LPPARTY party = GetParty(e))
    {
        EntityPartyVisitor adapter { visitor };
        party->ForEachNearMember(adapter);
    }
}

void ForEachOnlinePartyMember(entt::entity e, const std::function<void(entt::entity)>& visitor)
{
    if (LPPARTY party = GetParty(e))
    {
        EntityPartyVisitor adapter { visitor };
        party->ForEachOnlineMember(adapter);
    }
}

void ForEachPartyMemberOnMap(entt::entity e, int32_t mapIndex,
    const std::function<void(entt::entity)>& visitor)
{
    if (LPPARTY party = GetParty(e))
    {
        EntityPartyVisitor adapter { visitor };
        party->ForEachOnMapMember(adapter, mapIndex);
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
// this one setter wrote all three.
void SetParty(entt::entity e, LPPARTY pkParty)
{
    if (e == entt::null || !g_registry.valid(e))
        return;

    auto& refs = g_registry.get_or_emplace<ecs::SocialRefs>(e);
    LPPARTY previous = refs.party;

    if (pkParty == previous)
        return;

    if (pkParty && previous)
        LOG_ERROR("{} is trying to reassigning party (current {}, new party {})",
            ecs::PlayerRuntime::GetName(e).data(),
            static_cast<const void*>(get_pointer(previous)),
            static_cast<const void*>(get_pointer(pkParty)));

    LOG_TRACE("PARTY set to {}", static_cast<const void*>(get_pointer(pkParty)));

    const bool isPC = ecs::PlayerRuntime::IsPC(e);

#ifdef ENABLE_BUG_FIXES
    if (GetDungeon(e) && isPC && !pkParty)
        SetDungeon(e, nullptr);
#endif

#ifdef ENABLE_NEW_USE_POTION
    if (isPC && previous && pkParty == nullptr &&
        previous->GetLeaderPID() == ecs::PlayerRuntime::GetPlayerID(e))
    {
        if (CAffect* pAffect = AffectSystem::FindAffect(e, AFFECT_NEW_POTION31))
        {
            // FindItemByID has no entity form yet; it is its own migration.
            if (LPCHARACTER self = ecs::LegacyCharOf(e))
            {
                if (LPITEM pkItem = self->FindItemByID(pAffect->dwFlag))
                {
                    ItemSystem::UnlockItem(pkItem->GetEntityHandle());
                    ItemSystem::SetItemSocket(pkItem->GetEntityHandle(), 1, 0);
                }
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
            status->isPartyState = (pkParty != nullptr);
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

    LPCHARACTER ch = CHARACTER_MANAGER::instance().FindByPID(info->dwGuestPID);

    if (ch)
    {
		const entt::entity guest = ch->GetEntityHandle();
        LOG_INFO("PartyRequestEvent {}", ecs::PlayerRuntime::GetName(guest).data());
        ecs::ChatSystem::Send(guest, CHAT_TYPE_COMMAND, "PartyRequestDenied");
        ch->SetPartyRequestEvent(nullptr);
    }

    return 0;
}

bool CHARACTER::RequestToParty(entt::entity leaderEntity)
{
    LPCHARACTER leader = ecs::LegacyCharOf(leaderEntity);
    if (ecs::SocialSystem::GetParty(leaderEntity))
        leader = ecs::SocialSystem::GetParty(leader->GetEntityHandle())->GetLeaderCharacter();

    if (!leader)
    {
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 488, "");
#endif
        return false;
    }

    if (m_pkPartyRequestEvent)
        return false;

    if (!IsPC() || !ecs::PlayerRuntime::IsPC(leaderEntity))
        return false;

    if (ecs::PlayerRuntime::IsBlockMode(leader->GetEntityHandle(), BLOCK_PARTY_REQUEST))
        return false;

    PartyJoinErrCode errcode = IsPartyJoinableCondition(leaderEntity, GetEntityHandle());

    switch (errcode)
    {
    case PERR_NONE:
        break;

    case PERR_SERVER:
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 208, "");
#endif
        return false;

    case PERR_DUNGEON:
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 200, "");
#endif
        return false;
    case PERR_OBSERVER:
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 195, "");
#endif
        return false;

    case PERR_LVBOUNDARY:
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 194, "");
#endif
        return false;

    case PERR_LOWLEVEL:
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 214, "");
#endif
        return false;

    case PERR_HILEVEL:
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 214, "");
#endif
        return false;

    case PERR_ALREADYJOIN:
        return false;

    case PERR_PARTYISFULL:
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 199, "");
#endif
        return false;

    default:
        LOG_ERROR("Do not process party join error({})", errcode);
        return false;
    }

    TPartyJoinEventInfo* info = AllocEventInfo<TPartyJoinEventInfo>();

    info->dwGuestPID = GetPlayerID();
    info->dwLeaderPID = ecs::PlayerRuntime::GetPlayerID(leaderEntity);

    SetPartyRequestEvent(event_create(party_request_event, info, PASSES_PER_SEC(10)));

    ecs::ChatSystem::Send(leaderEntity, CHAT_TYPE_COMMAND, "PartyRequest %u", GetPacketVID());
#ifdef TEXTS_IMPROVEMENT
    ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 106, "%s", ecs::PlayerRuntime::GetName(leaderEntity).data());
#endif
    return true;
}

void CHARACTER::DenyToParty(entt::entity memberEntity)
{
	LPCHARACTER member = ecs::LegacyCharOf(memberEntity);
    LOG_INFO("DenyToParty {} member {} {}", GetName(), ecs::PlayerRuntime::GetName(memberEntity).data(), static_cast<const void*>(get_pointer(member->m_pkPartyRequestEvent)));

    if (!member->m_pkPartyRequestEvent)
        return;

    TPartyJoinEventInfo* info = dynamic_cast<TPartyJoinEventInfo*>(member->m_pkPartyRequestEvent->info);

    if (!info)
    {
        LOG_ERROR("CHARACTER::DenyToParty> <Factor> Null pointer");
        return;
    }

    if (info->dwGuestPID != ecs::PlayerRuntime::GetPlayerID(memberEntity))
        return;

    if (info->dwLeaderPID != GetPlayerID())
        return;

    event_cancel(&member->m_pkPartyRequestEvent);

    ecs::ChatSystem::Send(memberEntity, CHAT_TYPE_COMMAND, "PartyRequestDenied");
}

void CHARACTER::AcceptToParty(entt::entity memberEntity)
{
	LPCHARACTER member = ecs::LegacyCharOf(memberEntity);
    LOG_INFO("AcceptToParty {} member {} {}", GetName(), ecs::PlayerRuntime::GetName(memberEntity).data(), static_cast<const void*>(get_pointer(member->m_pkPartyRequestEvent)));

    if (!member->m_pkPartyRequestEvent)
        return;

    TPartyJoinEventInfo* info = dynamic_cast<TPartyJoinEventInfo*>(member->m_pkPartyRequestEvent->info);

    if (!info)
    {
        LOG_ERROR("CHARACTER::AcceptToParty> <Factor> Null pointer");
        return;
    }

    if (info->dwGuestPID != ecs::PlayerRuntime::GetPlayerID(memberEntity))
        return;

    if (info->dwLeaderPID != GetPlayerID())
        return;

    event_cancel(&member->m_pkPartyRequestEvent);

    if (ecs::SocialSystem::GetParty(GetEntityHandle()))
    {
        if (GetPlayerID() != ecs::SocialSystem::GetParty(GetEntityHandle())->GetLeaderPID())
            return;

        PartyJoinErrCode errcode = IsPartyJoinableCondition(GetEntityHandle(), memberEntity);
        switch (errcode)
        {
        case PERR_NONE: member->PartyJoin(GetEntityHandle()); return;
        case PERR_SERVER:
#ifdef TEXTS_IMPROVEMENT
            ecs::ChatSystem::SendNew(memberEntity, CHAT_TYPE_INFO, 208, "");
#endif
            break;
        case PERR_DUNGEON:
#ifdef TEXTS_IMPROVEMENT
            ecs::ChatSystem::SendNew(memberEntity, CHAT_TYPE_INFO, 200, "");
#endif
            break;
        case PERR_OBSERVER:
#ifdef TEXTS_IMPROVEMENT
            ecs::ChatSystem::SendNew(memberEntity, CHAT_TYPE_INFO, 195, "");
#endif
            break;
        case PERR_LOWLEVEL:
        case PERR_LVBOUNDARY:
#ifdef TEXTS_IMPROVEMENT
            ecs::ChatSystem::SendNew(memberEntity, CHAT_TYPE_INFO, 194, "");
#endif
            break;
        case PERR_HILEVEL:
#ifdef TEXTS_IMPROVEMENT
            ecs::ChatSystem::SendNew(memberEntity, CHAT_TYPE_INFO, 214, "");
#endif
            break;
        case PERR_ALREADYJOIN:
            break;
        case PERR_PARTYISFULL:
        {
#ifdef TEXTS_IMPROVEMENT
            ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 199, "");
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

    LPCHARACTER pchInviter = CHARACTER_MANAGER::instance().FindByPID(pInfo->dwLeaderPID);

    if (pchInviter)
    {
		const entt::entity inviter = pchInviter->GetEntityHandle();
        LOG_INFO("PartyInviteEvent {}", ecs::PlayerRuntime::GetName(inviter).data());
        pchInviter->PartyInviteDeny(pInfo->dwGuestPID);
    }

    return 0;
}

void CHARACTER::PartyInvite(entt::entity invitee)
{
	LPCHARACTER pkInvitee = ecs::LegacyCharOf(invitee);
    if (ecs::SocialSystem::GetParty(GetEntityHandle()) && ecs::SocialSystem::GetParty(GetEntityHandle())->GetLeaderPID() != GetPlayerID())
    {
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 218, "");
#endif
        return;
    }
    else if (ecs::PlayerRuntime::IsBlockMode(invitee, BLOCK_PARTY_INVITE))
    {
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 192, "%s", ecs::PlayerRuntime::GetName(invitee).data());
#endif
        return;
    }

#ifdef ENABLE_PVP_ADVANCED
    else if ((ecs::PlayerRuntime::GetDuelOption(GetEntityHandle(), "BlockParty")))
    {
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 516, "");
#endif
        return;
    }

    else if ((ecs::PlayerRuntime::GetDuelOption(invitee, "BlockParty")))
    {
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 517, "%s", ecs::PlayerRuntime::GetName(invitee).data());
#endif
        return;
    }
#endif

    PartyJoinErrCode errcode = IsPartyJoinableCondition(GetEntityHandle(), invitee);

    switch (errcode)
    {
    case PERR_NONE:
        break;

    case PERR_SERVER:
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 208, "");
#endif
        return;
    case PERR_DUNGEON:
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 200, "");
#endif
        return;
    case PERR_OBSERVER:
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 195, "");
#endif
        return;
    case PERR_LVBOUNDARY:
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 194, "");
#endif
        return;
    case PERR_LOWLEVEL:
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 214, "");
#endif
        return;
    case PERR_HILEVEL:
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 214, "");
#endif
        return;
    case PERR_ALREADYJOIN:
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 210, "%s", ecs::PlayerRuntime::GetName(invitee).data());
#endif
        return;
    case PERR_PARTYISFULL:
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 199, "");
#endif
        return;
    default:
        LOG_ERROR("Do not process party join error({})", errcode);
        return;
    }

    if (m_PartyInviteEventMap.contains(ecs::PlayerRuntime::GetPlayerID(invitee)))
        return;

    TPartyJoinEventInfo* info = AllocEventInfo<TPartyJoinEventInfo>();

    info->dwGuestPID = ecs::PlayerRuntime::GetPlayerID(invitee);
    info->dwLeaderPID = GetPlayerID();

    m_PartyInviteEventMap.insert(EventMap::value_type(ecs::PlayerRuntime::GetPlayerID(invitee), event_create(party_invite_event, info, PASSES_PER_SEC(10))));

    TPacketGCPartyInvite p;
    p.header = HEADER_GC_PARTY_INVITE;
    p.leader_vid = GetPacketVID();
    ecs::PlayerRuntime::GetDesc(invitee)->Packet(&p, sizeof(p));
}

void CHARACTER::PartyInviteAccept(entt::entity invitee)
{
	LPCHARACTER pkInvitee = ecs::LegacyCharOf(invitee);
    const auto itFind = m_PartyInviteEventMap.find(ecs::PlayerRuntime::GetPlayerID(invitee));

    if (itFind == m_PartyInviteEventMap.end())
    {
        LOG_INFO("PartyInviteAccept from not invited character({})", ecs::PlayerRuntime::GetName(invitee).data());
        return;
    }

    event_cancel(&itFind->second);
    m_PartyInviteEventMap.erase(itFind);

    if (ecs::SocialSystem::GetParty(GetEntityHandle()) && ecs::SocialSystem::GetParty(GetEntityHandle())->GetLeaderPID() != GetPlayerID())
    {
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 218, "");
#endif
        return;
    }

    PartyJoinErrCode errcode = IsPartyJoinableMutableCondition(GetEntityHandle(), invitee);

    switch (errcode)
    {
    case PERR_NONE:
        break;
    case PERR_SERVER:
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(invitee, CHAT_TYPE_INFO, 208, "");
#endif
        return;
    case PERR_DUNGEON:
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(invitee, CHAT_TYPE_INFO, 201, "");
#endif
        return;
    case PERR_OBSERVER:
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(invitee, CHAT_TYPE_INFO, 195, "");
#endif
        return;
    case PERR_LVBOUNDARY:
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(invitee, CHAT_TYPE_INFO, 194, "");
#endif
        return;
    case PERR_LOWLEVEL:
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(invitee, CHAT_TYPE_INFO, 214, "");
#endif
        return;
    case PERR_HILEVEL:
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(invitee, CHAT_TYPE_INFO, 214, "");
#endif
        return;
    case PERR_ALREADYJOIN:
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(invitee, CHAT_TYPE_INFO, 212, "");
#endif
        return;
    case PERR_PARTYISFULL:
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 199, "");
        ecs::ChatSystem::SendNew(invitee, CHAT_TYPE_INFO, 220, "");
#endif
        return;
    default:
        LOG_ERROR("ignore party join error({})", errcode);
        return;
    }

    if (ecs::SocialSystem::GetParty(GetEntityHandle()))
        pkInvitee->PartyJoin(GetEntityHandle());
    else
    {
        LPPARTY pParty = CPartyManager::instance().CreateParty(GetEntityHandle());

        pParty->Join(ecs::PlayerRuntime::GetPlayerID(invitee));
        pParty->Link(invitee);
        pParty->SendPartyInfoAllToOne(GetEntityHandle());
    }
}

void CHARACTER::PartyInviteDeny(uint32_t dwPID)
{
    const auto itFind = m_PartyInviteEventMap.find(dwPID);

    if (itFind == m_PartyInviteEventMap.end())
    {
        LOG_INFO("PartyInviteDeny to not exist event(inviter PID: {}, invitee PID: {})", GetPlayerID(), dwPID);
        return;
    }

    event_cancel(&itFind->second);
    m_PartyInviteEventMap.erase(itFind);
#ifdef TEXTS_IMPROVEMENT
    const entt::entity pchInvitee = CHARACTER_MANAGER::instance().FindEntityByPID(dwPID);
    if (pchInvitee != entt::null) {
        ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 192, "%s", ecs::PlayerRuntime::GetName(pchInvitee).data());
    }
#endif
}

void CHARACTER::PartyJoin(entt::entity leader)
{
    LPCHARACTER pkLeader = ecs::LegacyCharOf(leader);
    if (pkLeader && ecs::SocialSystem::GetParty(leader)) {
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(leader, CHAT_TYPE_INFO, 1249, "%s", GetName());
        ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 193, "%s", ecs::PlayerRuntime::GetName(leader).data());
#endif
        ecs::SocialSystem::GetParty(leader)->Join(GetPlayerID());
        ecs::SocialSystem::GetParty(leader)->Link(GetEntityHandle());
    }
}

CHARACTER::PartyJoinErrCode CHARACTER::IsPartyJoinableCondition(const entt::entity leader, const entt::entity guest)
{
    return IsPartyJoinableMutableCondition(leader, guest);
}

static bool __party_can_join_by_level(entt::entity leader, entt::entity guest)
{
    int level_limit = 50;
    return (abs(ecs::PointSystem::GetLevel(leader) - ecs::PointSystem::GetLevel(guest)) <= level_limit);
}

CHARACTER::PartyJoinErrCode CHARACTER::IsPartyJoinableMutableCondition(const entt::entity leader, const entt::entity guest)
{
    LPCHARACTER pkLeader = ecs::LegacyCharOf(leader);
    LPCHARACTER pkGuest = ecs::LegacyCharOf(guest);
    if (!CPartyManager::instance().IsEnablePCParty())
        return PERR_SERVER;
    else if (ecs::SocialSystem::GetDungeon(leader))
        return PERR_DUNGEON;
    else if (pkGuest->IsObserverMode())
        return PERR_OBSERVER;
    else if (false == __party_can_join_by_level(
		leader, guest))
        return PERR_LVBOUNDARY;
    else if (ecs::SocialSystem::GetParty(guest))
        return PERR_ALREADYJOIN;
    else if (ecs::SocialSystem::GetParty(leader))
    {
        if (ecs::SocialSystem::GetParty(leader)->GetMemberCount() == PARTY_MAX_MEMBER)
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

int CHARACTER::GetMarriageBonus(uint32_t dwItemVnum, bool bSum)
{
    return ecs::SocialSystem::GetMarriageBonus(GetEntityHandle(), dwItemVnum, bSum);
}

struct FFindReviver
{
    FFindReviver()
        : pChar(nullptr)
        , HasReviver(false)
    {
    }

    void operator()(LPCHARACTER ch)
    {
        if (ch->IsMonster() != true)
        {
            return;
        }

        if (ecs::PlayerRuntime::IsReviver(ch->GetEntityHandle()) == true && pChar != ch && CombatSystem::IsDead(ch->GetEntityHandle()) != true)
        {
            const TMobTable* mobTable = ecs::PlayerRuntime::GetMobTable(ch->GetEntityHandle());
            if (mobTable && number(1, 100) <= mobTable->bRevivePoint)
            {
                HasReviver = true;
                pChar = ch;
            }
        }
    }

    LPCHARACTER pChar;
    bool HasReviver;
};

namespace ecs::SocialSystem {

// Whether a reviver mob stands in this one's party. The party still iterates
// CHARACTER pointers; CParty is its own migration.
bool HasReviverInParty(entt::entity e)
{
    LPPARTY party = GetParty(e);
    if (party == nullptr)
        return false;

    if (party->GetMemberCount() == 1)
        return false;

    FFindReviver f;
    party->ForEachMemberPtr(f);
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

void CHARACTER::SendGuildName(CGuild* pGuild)
{
    ecs::SocialSystem::SendGuildName(GetEntityHandle(), pGuild);
}

void CHARACTER::SendGuildName(uint32_t dwGuildID)
{
    SendGuildName(CGuildManager::instance().FindGuild(dwGuildID));
}

