#include "../../stdafx.h"
#include "PointSystem.hpp"
#include "PlayerRuntimeSystem.hpp"

#include "SocialSystem.hpp"

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
#include "../../packet.h"
#include "../../party.h"
#include "../../utils.h"
#include "../CharacterAccessors.hpp"
#include "../EntityFactory.hpp"
#include "../Registry.hpp"
#include "../components/dirty_components.hpp"
#include "../components/character_runtime_components.hpp"
#include "../components/social_components.hpp"
#include "../components/status_components.hpp"
#include "ItemSystem.hpp"
#include "NetworkSyncSystem.hpp"
#include <Core/Logging.hpp>

namespace ecs::SocialSystem {

LPPARTY GetParty(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return nullptr;

    if (const auto* refs = g_registry.try_get<ecs::SocialRefs>(e))
        return refs->party;

    if (const auto* party = g_registry.try_get<ecs::PartyMembership>(e))
        return party->party;

    return nullptr;
}

CGuild* GetGuild(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return nullptr;

    if (const auto* refs = g_registry.try_get<ecs::SocialRefs>(e))
        return refs->guild;

    if (const auto* guild = g_registry.try_get<ecs::GuildMembership>(e))
        return guild->guild;

    return nullptr;
}

LPDUNGEON GetDungeon(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return nullptr;

    const auto* membership = g_registry.try_get<ecs::DungeonMembership>(e);
    return membership ? membership->dungeon : nullptr;
}

CWarMap* GetWarMap(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return nullptr;

    const auto* membership = g_registry.try_get<ecs::DungeonMembership>(e);
    return membership ? membership->warMap : nullptr;
}

CExchange* GetExchange(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return nullptr;

    const auto* exchange = g_registry.try_get<ecs::ExchangeRef>(e);
    return exchange ? exchange->exchange : nullptr;
}

CShop* GetShop(entt::entity e)
{
	if (e == entt::null || !g_registry.valid(e))
		return nullptr;
	const auto* state = g_registry.try_get<ecs::ShopState>(e);
	return state ? state->currentShop : nullptr;
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

void CHARACTER::SetParty(LPPARTY pkParty)
{
    const auto entity = GetEntityHandle();
    if (entity != entt::null && g_registry.valid(entity)) {
        auto& refs = g_registry.get_or_emplace<ecs::SocialRefs>(entity);
        refs.party = pkParty;
        auto& membership = g_registry.get_or_emplace<ecs::PartyMembership>(entity);
        membership.party = pkParty;
    }

    if (pkParty == m_pkParty)
        return;

    if (pkParty && m_pkParty)
        LOG_ERROR("{} is trying to reassigning party (current {}, new party {})", GetName(), static_cast<const void*>(get_pointer(m_pkParty)), static_cast<const void*>(get_pointer(pkParty)));

    LOG_TRACE("PARTY set to {}", static_cast<const void*>(get_pointer(pkParty)));

#ifdef ENABLE_BUG_FIXES
    if (m_pkDungeon && IsPC() && !pkParty) {
        SetDungeon(nullptr);
    }
#endif

#ifdef ENABLE_NEW_USE_POTION
    if (IsPC() && m_pkParty && pkParty == nullptr && m_pkParty->GetLeaderPID() == GetPlayerID()) {
        CAffect* pAffect = FindAffect(AFFECT_NEW_POTION31);
        if (pAffect) {
            LPITEM pkItem = FindItemByID(pAffect->dwFlag);
            if (pkItem) {
                ItemSystem::UnlockItem((pkItem ? pkItem->GetEntityHandle() : entt::null));
                ItemSystem::SetItemSocket((pkItem ? pkItem->GetEntityHandle() : entt::null), 1, 0);
            }

            RemoveAffect(AFFECT_NEW_POTION31);
        }
    }
#endif

    m_pkParty = pkParty;

    if (IsPC())
    {
        // Phase C.4: legacy SET_BIT/REMOVE_BIT(m_bAddChrState, PARTY) removed.
        // ECS StatusFlags.isPartyState is the sole source.
        if (auto* status = g_registry.try_get<ecs::StatusFlags>(GetEntityHandle())) {
            status->isPartyState = (m_pkParty != nullptr);
            g_registry.emplace_or_replace<ecs::DirtyTag>(GetEntityHandle());
        }

        NetworkSyncSystem::UpdatePacket(GetEntityHandle());
    }
}

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
    if (leader->GetParty())
        leader = leader->GetParty()->GetLeaderCharacter();

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

    if (leader->IsBlockMode(BLOCK_PARTY_REQUEST))
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

    if (GetParty())
    {
        if (GetPlayerID() != GetParty()->GetLeaderPID())
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
    if (GetParty() && GetParty()->GetLeaderPID() != GetPlayerID())
    {
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 218, "");
#endif
        return;
    }
    else if (pkInvitee->IsBlockMode(BLOCK_PARTY_INVITE))
    {
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 192, "%s", ecs::PlayerRuntime::GetName(invitee).data());
#endif
        return;
    }

#ifdef ENABLE_PVP_ADVANCED
    else if ((GetDuel("BlockParty")))
    {
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 516, "");
#endif
        return;
    }

    else if ((pkInvitee->GetDuel("BlockParty")))
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

    if (GetParty() && GetParty()->GetLeaderPID() != GetPlayerID())
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

    if (GetParty())
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
    LPCHARACTER pchInvitee = CHARACTER_MANAGER::instance().FindByPID(dwPID);
    if (pchInvitee) {
		const entt::entity invitee = pchInvitee->GetEntityHandle();
        ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 192, "%s", ecs::PlayerRuntime::GetName(invitee).data());
    }
#endif
}

void CHARACTER::PartyJoin(entt::entity leader)
{
    LPCHARACTER pkLeader = ecs::LegacyCharOf(leader);
    if (pkLeader && pkLeader->GetParty()) {
		const entt::entity leader = pkLeader->GetEntityHandle();
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(leader, CHAT_TYPE_INFO, 1249, "%s", GetName());
        ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 193, "%s", ecs::PlayerRuntime::GetName(leader).data());
#endif
        pkLeader->GetParty()->Join(GetPlayerID());
        pkLeader->GetParty()->Link(GetEntityHandle());
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
    else if (pkLeader->GetDungeon())
        return PERR_DUNGEON;
    else if (pkGuest->IsObserverMode())
        return PERR_OBSERVER;
    else if (false == __party_can_join_by_level(
		pkLeader->GetEntityHandle(), pkGuest->GetEntityHandle()))
        return PERR_LVBOUNDARY;
    else if (pkGuest->GetParty())
        return PERR_ALREADYJOIN;
    else if (pkLeader->GetParty())
    {
        if (pkLeader->GetParty()->GetMemberCount() == PARTY_MAX_MEMBER)
            return PERR_PARTYISFULL;
    }

    return PERR_NONE;
}

void CHARACTER::SetGuild(CGuild* pGuild)
{
    const auto entity = GetEntityHandle();
    if (entity != entt::null && g_registry.valid(entity)) {
        auto& refs = g_registry.get_or_emplace<ecs::SocialRefs>(entity);
        refs.guild = pGuild;
        auto& membership = g_registry.get_or_emplace<ecs::GuildMembership>(entity);
        membership.guild = pGuild;
    }

    if (m_pGuild != pGuild)
    {
        m_pGuild = pGuild;
        NetworkSyncSystem::UpdatePacket(GetEntityHandle());
    }
}

int CHARACTER::GetMarriageBonus(uint32_t dwItemVnum, bool bSum)
{
    if (IsNPC())
        return 0;

    marriage::TMarriage* pMarriage = marriage::CManager::instance().Get(GetPlayerID());

    if (!pMarriage)
        return 0;

    return pMarriage->GetBonus(dwItemVnum, bSum, this);
}

CGuild* CHARACTER::GetRefineGuild() const
{
    LPCHARACTER chRefineNPC = CHARACTER_MANAGER::instance().Find(m_dwRefineNPCVID);

    return (chRefineNPC ? chRefineNPC->GetGuild() : nullptr);
}

bool CHARACTER::IsRefineThroughGuild() const
{
    return GetRefineGuild() != nullptr;
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

        if (ch->IsReviver() == true && pChar != ch && ch->IsDead() != true)
        {
            if (number(1, 100) <= ch->GetMobTable().bRevivePoint)
            {
                HasReviver = true;
                pChar = ch;
            }
        }
    }

    LPCHARACTER pChar;
    bool HasReviver;
};

bool CHARACTER::HasReviverInParty() const
{
    LPPARTY party = GetParty();

    if (party != nullptr)
    {
        if (party->GetMemberCount() == 1)
            return false;

        FFindReviver f;
        party->ForEachMemberPtr(f);
        return f.HasReviver;
    }

    return false;
}

void CHARACTER::SendGuildName(CGuild* pGuild)
{
    if (nullptr == pGuild)
        return;

    DESC* desc = GetDesc();

    if (nullptr == desc)
        return;
    if (m_known_guild.contains(pGuild->GetID()))
        return;

    m_known_guild.insert(pGuild->GetID());

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

void CHARACTER::SendGuildName(uint32_t dwGuildID)
{
    SendGuildName(CGuildManager::instance().FindGuild(dwGuildID));
}

