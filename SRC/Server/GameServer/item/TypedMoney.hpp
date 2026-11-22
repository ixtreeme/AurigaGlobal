#pragma once
#include "TypedItemOps.hpp"
#include <item/NativeMoney.hpp>
// Server-issued money uses the same serialized grant queue as item rewards.
void QueueTypedGold(entt::entity,std::uint64_t amount);
bool RequestTypedQuestGoldChange(entt::entity,std::int64_t delta,std::function<void(bool)>);
bool RequestTypedQuestGayaChange(entt::entity,std::int64_t delta,std::function<void(bool)>);
bool RequestTypedAdminGayaChange(entt::entity gm, entt::entity target, std::int64_t delta, std::function<void(bool)> done);
std::expected<void,TypedGrantRefusal> RequestTypedGoldCredit(entt::entity,std::uint64_t amount,
    std::optional<auriga::item::CurrencyDropIntent> claim={},std::function<void(bool)> done={});
bool HandleTypedMoneyPacket(entt::entity,const char*);
void TickTypedMoney();
