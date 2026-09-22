#pragma once

#include <array>
#include <cstdint>
#include <entt/entt.hpp>
#include <common/tables.h>

enum EExchangeValues
{
#ifdef __NEW_EXCHANGE_WINDOW__
    EXCHANGE_ITEM_MAX_NUM = 24,
#else
    EXCHANGE_ITEM_MAX_NUM = 12,
#endif
    EXCHANGE_MAX_DISTANCE = 1000
};

namespace ecs
{
struct ExchangeRef
{
    entt::entity session { entt::null };
};

struct ExchangeItem
{
    entt::entity item { entt::null };
    TItemPos source { NPOS };
    uint32_t id { 0 }, vnum { 0 }, count { 0 };
    uint8_t display { 0 }, size { 0 };
    std::array<int32_t, ITEM_SOCKET_MAX_NUM> sockets {};
    std::array<TPlayerItemAttribute, ITEM_ATTRIBUTE_MAX_NUM> attributes {};
    short lockedAttribute { -1 };
};

struct ExchangeOffer
{
    entt::entity owner { entt::null };
    std::array<ExchangeItem, EXCHANGE_ITEM_MAX_NUM> items {};
    int64_t gold { 0 };
    bool accepted { false };
};

enum class ExchangePhase { Open, Committed, Closing };

struct ExchangeSession
{
    std::array<ExchangeOffer, 2> offers {};
    uint64_t revision { 0 };
    ExchangePhase phase { ExchangePhase::Open };
};
}

namespace ExchangeSystem
{
entt::entity GetSession(entt::entity participant);
bool IsActive(entt::entity participant);
int GetLastExchangePulse(entt::entity participant);
bool Start(entt::entity initiator, entt::entity target);
bool AddItem(entt::entity participant, TItemPos position, uint32_t display);
bool RemoveItem(entt::entity participant, uint32_t slot);
bool AddGold(entt::entity participant, int64_t amount);
bool Accept(entt::entity participant, bool accepted = true);
void Cancel(entt::entity participant);
}
