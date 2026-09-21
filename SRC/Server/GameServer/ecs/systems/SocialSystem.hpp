#pragma once

#include <common/tables.h>
#include <entt/entt.hpp>
#include <functional>

#include "../../typedef.h"

class CGuild;
class CWarMap;

namespace ecs::SocialSystem {

int GetMarriageBonus(entt::entity e, uint32_t itemVnum, bool share = true);

enum PartyJoinErrCode {
    PERR_NONE = 0,
    PERR_SERVER,
    PERR_DUNGEON,
    PERR_OBSERVER,
    PERR_LVBOUNDARY,
    PERR_LOWLEVEL,
    PERR_HILEVEL,
    PERR_ALREADYJOIN,
    PERR_PARTYISFULL,
    PERR_SEPARATOR,
    PERR_DIFFEMPIRE,
    PERR_MAX
};

// Whether a guest may join a leader's party, and why not. The mutable
// form is the one that can change between the invitation and the answer.
PartyJoinErrCode IsPartyJoinableCondition(entt::entity leader, entt::entity guest);
PartyJoinErrCode IsPartyJoinableMutableCondition(entt::entity leader, entt::entity guest);
void PartyJoin(entt::entity guest, entt::entity leader);

// Asking a leader to join, and the leader's answer.
bool RequestToParty(entt::entity e, entt::entity leader);
void DenyToParty(entt::entity e, entt::entity member);
void AcceptToParty(entt::entity e, entt::entity member);
// Inviting someone into e's party, and the invitee's answer.
void PartyInvite(entt::entity e, entt::entity invitee);
void PartyInviteAccept(entt::entity e, entt::entity invitee);
void PartyInviteDeny(entt::entity e, uint32_t dwPID);
// Drops e's own pending join request (character teardown).
void CancelPartyRequest(entt::entity e);

entt::entity GetParty(entt::entity e);
void SetParty(entt::entity e, entt::entity party);
void SetGuild(entt::entity e, CGuild* guild);
bool HasReviverInParty(entt::entity e);
entt::entity GetPartyLeader(entt::entity e);
void ForEachNearPartyMember(entt::entity e, const std::function<void(entt::entity)>& visitor);
void ForEachOnlinePartyMember(entt::entity e, const std::function<void(entt::entity)>& visitor);
void ForEachPartyMemberOnMap(entt::entity e, int32_t mapIndex,
    const std::function<void(entt::entity)>& visitor);
CGuild* GetGuild(entt::entity e);

// Tells one viewer a guild's name, once. Legacy EncodeInsertPacket opens
// with this; the native character-insert path has to as well, or the
// viewer renders the character with no guild name.
void SendGuildName(entt::entity viewer, CGuild* pGuild);
void SetDungeon(entt::entity e, entt::entity dungeon);
entt::entity GetDungeon(entt::entity e);
// Drops the dungeon membership when the character is no longer headed to the
// dungeon's map, so an ex-member stops being counted.
void ClearDungeonIfOtherMap(entt::entity e, int32_t mapIndex);
void SetWarMap(entt::entity e, CWarMap* warMap);
void SetMarryPartner(entt::entity e, entt::entity partner);
entt::entity GetMarryPartner(entt::entity e);
void SetWeddingMap(entt::entity e, entt::entity map);
entt::entity GetWeddingMap(entt::entity e);
CWarMap* GetWarMap(entt::entity e);
bool HasExchange(entt::entity e);
entt::entity GetShop(entt::entity e);
int GetMyShopTime(entt::entity e);
void SetMyShopTime(entt::entity e);
int GetSafeboxLoadTime(entt::entity e);
void SetSafeboxLoadTime(entt::entity e);
int GetRefineTime(entt::entity e);
void SetRefineTime(entt::entity e);
CGuild* GetRefineGuild(entt::entity e);
bool IsRefineThroughGuild(entt::entity e);
int GetLastBuyTime(entt::entity e);
void SetLastBuyTime(entt::entity e);
uint32_t GetLastBuySellTime(entt::entity e);
void SetLastBuySellTime(entt::entity e, uint32_t when);
void OpenMyShop(entt::entity e, const char* sign, TShopItemTable* table, uint8_t itemCount
#ifdef KASMIR_PAKET_SYSTEM
    , uint32_t kasmirNpc, uint8_t kasmirTitle
#endif
    );
void CloseMyShop(entt::entity e);
entt::entity GetMyShop(entt::entity e);
bool GetNoOpenedShop(entt::entity e);
void SetNoOpenedShop(entt::entity e, bool value);
bool GetKasmirPaket(entt::entity e);
void SetKasmirPaket(entt::entity e, bool value);
void OpenPrivateShop(entt::entity e, bool bKasmir);
// The silk bundle: sends the saved price list, then opens the shop window.
void UseSilkBotaryReal(entt::entity e, const TPacketMyshopPricelistHeader* p);
void SendMyShopPriceListCmd(entt::entity e, uint32_t dwItemVnum, int64_t dwItemPrice);
void UseSilkBotary(entt::entity e);
entt::entity GetShopOwner(entt::entity e);
void SetShop(entt::entity e, entt::entity shop);
void SetShopOwner(entt::entity e, entt::entity owner);
bool CanDeposit(entt::entity e);
void UpdateDepositPulse(entt::entity e);
bool DepositGuildMoney(entt::entity character, CGuild& guild, int gold);

} // namespace ecs::SocialSystem
