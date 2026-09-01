#pragma once

#include <entt/entt.hpp>
#include <functional>

#include "../../typedef.h"

class CGuild;
class CExchange;
class CWarMap;
class CShop;

namespace ecs::SocialSystem {

LPPARTY GetParty(entt::entity e);
entt::entity GetPartyLeader(entt::entity e);
void ForEachNearPartyMember(entt::entity e, const std::function<void(entt::entity)>& visitor);
void ForEachOnlinePartyMember(entt::entity e, const std::function<void(entt::entity)>& visitor);
void ForEachPartyMemberOnMap(entt::entity e, int32_t mapIndex,
    const std::function<void(entt::entity)>& visitor);
CGuild* GetGuild(entt::entity e);
LPDUNGEON GetDungeon(entt::entity e);
CWarMap* GetWarMap(entt::entity e);
CExchange* GetExchange(entt::entity e);
CShop* GetShop(entt::entity e);
CShop* GetMyShop(entt::entity e);
entt::entity GetShopOwner(entt::entity e);
void SetShop(entt::entity e, CShop* shop);
void SetShopOwner(entt::entity e, entt::entity owner);
bool CanDeposit(entt::entity e);
void UpdateDepositPulse(entt::entity e);
bool DepositGuildMoney(entt::entity character, CGuild& guild, int gold);

} // namespace ecs::SocialSystem
