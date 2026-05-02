#include "../../stdafx.h"
#include "PointSystem.hpp"
#include "PlayerRuntimeSystem.hpp"
#include "../AIHelpers.hpp"

#include "SocialSystem.hpp"

#include "../../affect.h"
#include "../../char.h"
#include "../../char_manager.h"
#include "../../config.h"
#include "../../desc.h"
#include "../../guild.h"
#include "../../guild_manager.h"
#include "../../item.h"
#include "../../marriage.h"
#include "../../packet.h"
#include "../../party.h"
#include "../../utils.h"
#include "../CharacterAccessors.hpp"
#include "../EntityFactory.hpp"
#include "../Registry.hpp"
#include "../components/social_components.hpp"
#include "ItemSystem.hpp"
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

} // namespace ecs::SocialSystem

void CHARACTER::SetParty(LPPARTY pkParty)
{
    const auto entity = AIHelpers::EcsOf(this);
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
                ItemSystem::UnlockItem(EntityFactory::CreateItemEntity(g_registry, pkItem));
                ItemSystem::SetItemSocket(EntityFactory::CreateItemEntity(g_registry, pkItem), 1, 0);
            }

            RemoveAffect(AFFECT_NEW_POTION31);
        }
    }
#endif

    m_pkParty = pkParty;

    if (IsPC())
    {
        if (m_pkParty)
            SET_BIT(m_bAddChrState, ADD_CHARACTER_STATE_PARTY);
        else
            REMOVE_BIT(m_bAddChrState, ADD_CHARACTER_STATE_PARTY);

        UpdatePacket();
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
        LOG_INFO("PartyRequestEvent {}", ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(ch)).data());
        ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_COMMAND, "PartyRequestDenied");
        ch->SetPartyRequestEvent(nullptr);
    }

    return 0;
}

bool CHARACTER::RequestToParty(LPCHARACTER leader)
{
    if (leader->GetParty())
        leader = leader->GetParty()->GetLeaderCharacter();

    if (!leader)
    {
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(AIHelpers::EcsOf(this), CHAT_TYPE_INFO, 488, "");
#endif
        return false;
    }

    if (m_pkPartyRequestEvent)
        return false;

    if (!IsPC() || !ecs::PlayerRuntime::IsPC(AIHelpers::EcsOf(leader)))
        return false;

    if (leader->IsBlockMode(BLOCK_PARTY_REQUEST))
        return false;

    PartyJoinErrCode errcode = IsPartyJoinableCondition(leader, this);

    switch (errcode)
    {
    case PERR_NONE:
        break;

    case PERR_SERVER:
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(AIHelpers::EcsOf(this), CHAT_TYPE_INFO, 208, "");
#endif
        return false;

    case PERR_DUNGEON:
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(AIHelpers::EcsOf(this), CHAT_TYPE_INFO, 200, "");
#endif
        return false;
    case PERR_OBSERVER:
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(AIHelpers::EcsOf(this), CHAT_TYPE_INFO, 195, "");
#endif
        return false;

    case PERR_LVBOUNDARY:
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(AIHelpers::EcsOf(this), CHAT_TYPE_INFO, 194, "");
#endif
        return false;

    case PERR_LOWLEVEL:
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(AIHelpers::EcsOf(this), CHAT_TYPE_INFO, 214, "");
#endif
        return false;

    case PERR_HILEVEL:
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(AIHelpers::EcsOf(this), CHAT_TYPE_INFO, 214, "");
#endif
        return false;

    case PERR_ALREADYJOIN:
        return false;

    case PERR_PARTYISFULL:
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(AIHelpers::EcsOf(this), CHAT_TYPE_INFO, 199, "");
#endif
        return false;

    default:
        LOG_ERROR("Do not process party join error({})", errcode);
        return false;
    }

    TPartyJoinEventInfo* info = AllocEventInfo<TPartyJoinEventInfo>();

    info->dwGuestPID = GetPlayerID();
    info->dwLeaderPID = ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(leader));

    SetPartyRequestEvent(event_create(party_request_event, info, PASSES_PER_SEC(10)));

    ecs::ChatSystem::Send(AIHelpers::EcsOf(leader), CHAT_TYPE_COMMAND, "PartyRequest %u", GetPacketVID());
#ifdef TEXTS_IMPROVEMENT
    ecs::ChatSystem::SendNew(AIHelpers::EcsOf(this), CHAT_TYPE_INFO, 106, "%s", ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(leader)).data());
#endif
    return true;
}

void CHARACTER::DenyToParty(LPCHARACTER member)
{
    LOG_INFO("DenyToParty {} member {} {}", GetName(), ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(member)).data(), static_cast<const void*>(get_pointer(member->m_pkPartyRequestEvent)));

    if (!member->m_pkPartyRequestEvent)
        return;

    TPartyJoinEventInfo* info = dynamic_cast<TPartyJoinEventInfo*>(member->m_pkPartyRequestEvent->info);

    if (!info)
    {
        LOG_ERROR("CHARACTER::DenyToParty> <Factor> Null pointer");
        return;
    }

    if (info->dwGuestPID != ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(member)))
        return;

    if (info->dwLeaderPID != GetPlayerID())
        return;

    event_cancel(&member->m_pkPartyRequestEvent);

    ecs::ChatSystem::Send(AIHelpers::EcsOf(member), CHAT_TYPE_COMMAND, "PartyRequestDenied");
}

void CHARACTER::AcceptToParty(LPCHARACTER member)
{
    LOG_INFO("AcceptToParty {} member {} {}", GetName(), ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(member)).data(), static_cast<const void*>(get_pointer(member->m_pkPartyRequestEvent)));

    if (!member->m_pkPartyRequestEvent)
        return;

    TPartyJoinEventInfo* info = dynamic_cast<TPartyJoinEventInfo*>(member->m_pkPartyRequestEvent->info);

    if (!info)
    {
        LOG_ERROR("CHARACTER::AcceptToParty> <Factor> Null pointer");
        return;
    }

    if (info->dwGuestPID != ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(member)))
        return;

    if (info->dwLeaderPID != GetPlayerID())
        return;

    event_cancel(&member->m_pkPartyRequestEvent);

    if (GetParty())
    {
        if (GetPlayerID() != GetParty()->GetLeaderPID())
            return;

        PartyJoinErrCode errcode = IsPartyJoinableCondition(this, member);
        switch (errcode)
        {
        case PERR_NONE: member->PartyJoin(this); return;
        case PERR_SERVER:
#ifdef TEXTS_IMPROVEMENT
            ecs::ChatSystem::SendNew(AIHelpers::EcsOf(member), CHAT_TYPE_INFO, 208, "");
#endif
            break;
        case PERR_DUNGEON:
#ifdef TEXTS_IMPROVEMENT
            ecs::ChatSystem::SendNew(AIHelpers::EcsOf(member), CHAT_TYPE_INFO, 200, "");
#endif
            break;
        case PERR_OBSERVER:
#ifdef TEXTS_IMPROVEMENT
            ecs::ChatSystem::SendNew(AIHelpers::EcsOf(member), CHAT_TYPE_INFO, 195, "");
#endif
            break;
        case PERR_LOWLEVEL:
        case PERR_LVBOUNDARY:
#ifdef TEXTS_IMPROVEMENT
            ecs::ChatSystem::SendNew(AIHelpers::EcsOf(member), CHAT_TYPE_INFO, 194, "");
#endif
            break;
        case PERR_HILEVEL:
#ifdef TEXTS_IMPROVEMENT
            ecs::ChatSystem::SendNew(AIHelpers::EcsOf(member), CHAT_TYPE_INFO, 214, "");
#endif
            break;
        case PERR_ALREADYJOIN:
            break;
        case PERR_PARTYISFULL:
        {
#ifdef TEXTS_IMPROVEMENT
            ecs::ChatSystem::SendNew(AIHelpers::EcsOf(this), CHAT_TYPE_INFO, 199, "");
            ecs::ChatSystem::SendNew(AIHelpers::EcsOf(member), CHAT_TYPE_INFO, 220, "");
#endif
            break;
        }
        default:
            LOG_ERROR("Do not process party join error({})", errcode);
        }
    }

    ecs::ChatSystem::Send(AIHelpers::EcsOf(member), CHAT_TYPE_COMMAND, "PartyRequestDenied");
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
        LOG_INFO("PartyInviteEvent {}", ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(pchInviter)).data());
        pchInviter->PartyInviteDeny(pInfo->dwGuestPID);
    }

    return 0;
}

void CHARACTER::PartyInvite(LPCHARACTER pchInvitee)
{
    if (GetParty() && GetParty()->GetLeaderPID() != GetPlayerID())
    {
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(AIHelpers::EcsOf(this), CHAT_TYPE_INFO, 218, "");
#endif
        return;
    }
    else if (pchInvitee->IsBlockMode(BLOCK_PARTY_INVITE))
    {
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(AIHelpers::EcsOf(this), CHAT_TYPE_INFO, 192, "%s", ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(pchInvitee)).data());
#endif
        return;
    }

#ifdef ENABLE_PVP_ADVANCED
    else if ((GetDuel("BlockParty")))
    {
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(AIHelpers::EcsOf(this), CHAT_TYPE_INFO, 516, "");
#endif
        return;
    }

    else if ((pchInvitee->GetDuel("BlockParty")))
    {
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(AIHelpers::EcsOf(this), CHAT_TYPE_INFO, 517, "%s", ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(pchInvitee)).data());
#endif
        return;
    }
#endif

    PartyJoinErrCode errcode = IsPartyJoinableCondition(this, pchInvitee);

    switch (errcode)
    {
    case PERR_NONE:
        break;

    case PERR_SERVER:
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(AIHelpers::EcsOf(this), CHAT_TYPE_INFO, 208, "");
#endif
        return;
    case PERR_DUNGEON:
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(AIHelpers::EcsOf(this), CHAT_TYPE_INFO, 200, "");
#endif
        return;
    case PERR_OBSERVER:
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(AIHelpers::EcsOf(this), CHAT_TYPE_INFO, 195, "");
#endif
        return;
    case PERR_LVBOUNDARY:
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(AIHelpers::EcsOf(this), CHAT_TYPE_INFO, 194, "");
#endif
        return;
    case PERR_LOWLEVEL:
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(AIHelpers::EcsOf(this), CHAT_TYPE_INFO, 214, "");
#endif
        return;
    case PERR_HILEVEL:
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(AIHelpers::EcsOf(this), CHAT_TYPE_INFO, 214, "");
#endif
        return;
    case PERR_ALREADYJOIN:
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(AIHelpers::EcsOf(this), CHAT_TYPE_INFO, 210, "%s", ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(pchInvitee)).data());
#endif
        return;
    case PERR_PARTYISFULL:
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(AIHelpers::EcsOf(this), CHAT_TYPE_INFO, 199, "");
#endif
        return;
    default:
        LOG_ERROR("Do not process party join error({})", errcode);
        return;
    }

    if (m_PartyInviteEventMap.contains(ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(pchInvitee))))
        return;

    TPartyJoinEventInfo* info = AllocEventInfo<TPartyJoinEventInfo>();

    info->dwGuestPID = ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(pchInvitee));
    info->dwLeaderPID = GetPlayerID();

    m_PartyInviteEventMap.insert(EventMap::value_type(ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(pchInvitee)), event_create(party_invite_event, info, PASSES_PER_SEC(10))));

    TPacketGCPartyInvite p;
    p.header = HEADER_GC_PARTY_INVITE;
    p.leader_vid = GetPacketVID();
    ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(pchInvitee))->Packet(&p, sizeof(p));
}

void CHARACTER::PartyInviteAccept(LPCHARACTER pchInvitee)
{
    const auto itFind = m_PartyInviteEventMap.find(ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(pchInvitee)));

    if (itFind == m_PartyInviteEventMap.end())
    {
        LOG_INFO("PartyInviteAccept from not invited character({})", ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(pchInvitee)).data());
        return;
    }

    event_cancel(&itFind->second);
    m_PartyInviteEventMap.erase(itFind);

    if (GetParty() && GetParty()->GetLeaderPID() != GetPlayerID())
    {
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(AIHelpers::EcsOf(this), CHAT_TYPE_INFO, 218, "");
#endif
        return;
    }

    PartyJoinErrCode errcode = IsPartyJoinableMutableCondition(this, pchInvitee);

    switch (errcode)
    {
    case PERR_NONE:
        break;
    case PERR_SERVER:
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(AIHelpers::EcsOf(pchInvitee), CHAT_TYPE_INFO, 208, "");
#endif
        return;
    case PERR_DUNGEON:
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(AIHelpers::EcsOf(pchInvitee), CHAT_TYPE_INFO, 201, "");
#endif
        return;
    case PERR_OBSERVER:
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(AIHelpers::EcsOf(pchInvitee), CHAT_TYPE_INFO, 195, "");
#endif
        return;
    case PERR_LVBOUNDARY:
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(AIHelpers::EcsOf(pchInvitee), CHAT_TYPE_INFO, 194, "");
#endif
        return;
    case PERR_LOWLEVEL:
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(AIHelpers::EcsOf(pchInvitee), CHAT_TYPE_INFO, 214, "");
#endif
        return;
    case PERR_HILEVEL:
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(AIHelpers::EcsOf(pchInvitee), CHAT_TYPE_INFO, 214, "");
#endif
        return;
    case PERR_ALREADYJOIN:
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(AIHelpers::EcsOf(pchInvitee), CHAT_TYPE_INFO, 212, "");
#endif
        return;
    case PERR_PARTYISFULL:
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(AIHelpers::EcsOf(this), CHAT_TYPE_INFO, 199, "");
        ecs::ChatSystem::SendNew(AIHelpers::EcsOf(pchInvitee), CHAT_TYPE_INFO, 220, "");
#endif
        return;
    default:
        LOG_ERROR("ignore party join error({})", errcode);
        return;
    }

    if (GetParty())
        pchInvitee->PartyJoin(this);
    else
    {
        LPPARTY pParty = CPartyManager::instance().CreateParty(this);

        pParty->Join(ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(pchInvitee)));
        pParty->Link(pchInvitee);
        pParty->SendPartyInfoAllToOne(this);
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
        ecs::ChatSystem::SendNew(AIHelpers::EcsOf(this), CHAT_TYPE_INFO, 192, "%s", ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(pchInvitee)).data());
    }
#endif
}

void CHARACTER::PartyJoin(LPCHARACTER pLeader)
{
    if (pLeader && pLeader->GetParty()) {
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(AIHelpers::EcsOf(pLeader), CHAT_TYPE_INFO, 1249, "%s", GetName());
        ecs::ChatSystem::SendNew(AIHelpers::EcsOf(this), CHAT_TYPE_INFO, 193, "%s", ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(pLeader)).data());
#endif
        pLeader->GetParty()->Join(GetPlayerID());
        pLeader->GetParty()->Link(this);
    }
}

CHARACTER::PartyJoinErrCode CHARACTER::IsPartyJoinableCondition(const LPCHARACTER pchLeader, const LPCHARACTER pchGuest)
{
    return IsPartyJoinableMutableCondition(pchLeader, pchGuest);
}

static bool __party_can_join_by_level(LPCHARACTER leader, LPCHARACTER quest)
{
    int level_limit = 50;
    return (abs(ecs::PointSystem::GetLevel(AIHelpers::EcsOf(leader)) - ecs::PointSystem::GetLevel(AIHelpers::EcsOf(quest))) <= level_limit);
}

CHARACTER::PartyJoinErrCode CHARACTER::IsPartyJoinableMutableCondition(const LPCHARACTER pchLeader, const LPCHARACTER pchGuest)
{
    if (!CPartyManager::instance().IsEnablePCParty())
        return PERR_SERVER;
    else if (pchLeader->GetDungeon())
        return PERR_DUNGEON;
    else if (pchGuest->IsObserverMode())
        return PERR_OBSERVER;
    else if (false == __party_can_join_by_level(pchLeader, pchGuest))
        return PERR_LVBOUNDARY;
    else if (pchGuest->GetParty())
        return PERR_ALREADYJOIN;
    else if (pchLeader->GetParty())
    {
        if (pchLeader->GetParty()->GetMemberCount() == PARTY_MAX_MEMBER)
            return PERR_PARTYISFULL;
    }

    return PERR_NONE;
}

void CHARACTER::SetGuild(CGuild* pGuild)
{
    const auto entity = AIHelpers::EcsOf(this);
    if (entity != entt::null && g_registry.valid(entity)) {
        auto& refs = g_registry.get_or_emplace<ecs::SocialRefs>(entity);
        refs.guild = pGuild;
        auto& membership = g_registry.get_or_emplace<ecs::GuildMembership>(entity);
        membership.guild = pGuild;
    }

    if (m_pGuild != pGuild)
    {
        m_pGuild = pGuild;
        UpdatePacket();
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

