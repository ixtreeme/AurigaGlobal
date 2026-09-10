#pragma once

#include <entt/entt.hpp>
#include <functional>

#include "../../typedef.h"

class CGuild;
class CWarMap;
class CShop;

namespace ecs::SocialSystem {

int GetMarriageBonus(entt::entity e, uint32_t itemVnum, bool share = true);

LPPARTY GetParty(entt::entity e);
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
LPDUNGEON GetDungeon(entt::entity e);
CWarMap* GetWarMap(entt::entity e);
bool HasExchange(entt::entity e);
CShop* GetShop(entt::entity e);
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
CShop* GetMyShop(entt::entity e);
bool GetNoOpenedShop(entt::entity e);
void SetNoOpenedShop(entt::entity e, bool value);
bool GetKasmirPaket(entt::entity e);
void SetKasmirPaket(entt::entity e, bool value);
void OpenPrivateShop(entt::entity e, bool bKasmir);
void UseSilkBotary(entt::entity e);
entt::entity GetShopOwner(entt::entity e);
void SetShop(entt::entity e, CShop* shop);
void SetShopOwner(entt::entity e, entt::entity owner);
bool CanDeposit(entt::entity e);
void UpdateDepositPulse(entt::entity e);
bool DepositGuildMoney(entt::entity character, CGuild& guild, int gold);

} // namespace ecs::SocialSystem
