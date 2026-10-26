#include "stdafx.h"
#include "desc_client.h"
#include "desc_manager.h"
#include "../ecs/systems/SessionSystem.hpp"
#include "../ecs/systems/InventorySystem.hpp"
#include "../social/exchange.h"
#include "../item/item_manager.h"
#include <Core/player_cache_snapshot.h>
#include "../ecs/CharacterAccessors.hpp"
#include "../ecs/systems/PlayerRuntimeSystem.hpp"

namespace
{
bool CurrentCacheTarget(LPDESC target, const auriga::player_session::Key &key)
{
    if (!target || (!target->IsPhase(PHASE_GAME) && !target->IsPhase(PHASE_LOADING)) ||
        target->GetAccountTable().id != key.account || !target->PlayerSession().Loaded())
        return false;
    const auto entity = target->GetEntity();
    return ecs::IsCharacter(entity) && ecs::PlayerRuntime::GetPlayerID(entity) == key.character &&
           target->PlayerSession().MatchesEntity(key, entt::to_integral(entity));
}
} // namespace
void CLIENT_DESC::NegotiatePlayerCache()
{
    if (this != db_clientdesc || !PlayerSessionsReady() || m_playerCacheHelloSent)
        return;
    m_playerCacheHelloSent = true;
    const auto frame = auriga::player_cache_wire::Hello(PlayerSessionLink(), false);
    DBPacket(HEADER_GD_PLAYER_CACHE_HELLO, 0, frame.data(), frame.size());
}
bool CLIENT_DESC::AcceptPlayerCacheHello(auriga::player_session::Token link)
{
    if (!PlayerSessionsReady() || !m_playerCacheHelloSent || link != PlayerSessionLink())
        return false;
    m_playerCacheReady = true;
    return true;
}
bool CLIENT_DESC::RequestPlayerCacheDrain(LPDESC target, auriga::player_session::Token challenge)
{
    if (this != db_clientdesc || !IsConnected() || !m_playerCacheReady || !target)
        return false;
    const auto key = target->PlayerSession().Current();
    if (!key || key->link != PlayerSessionLink() || !CurrentCacheTarget(target, *key) ||
        (!m_playerCacheWatched.contains(target->GetHandle()) && m_playerCacheWatched.size() >= 256))
        return false;
    const auto old = m_playerCacheWatched.find(target->GetHandle());
    if (old != m_playerCacheWatched.end() && old->second != *key)
    {
        m_playerCacheWatched.erase(old);
        m_playerCachePolls.erase(target->GetHandle());
    }
    auto &state = target->PlayerCacheDrain();
    const auto entity = target->GetEntity();
    const auto handle = target->GetHandle();
    const auto stillCurrent = [&]
    {
        return DESC_MANAGER::instance().FindByHandle(handle) == target && CurrentCacheTarget(target, *key) &&
               target->GetEntity() == entity;
    };
    if (state.Matches(*key, entt::to_integral(entity)))
    {
        // Same operation is idempotent. Never refresh a snapshot under an old
        // Begin challenge, even while waiting for its first acknowledgement.
        const auto request = state.Next();
        const auto receipt = state.Result(*key, entt::to_integral(entity));
        return (request && request->challenge == challenge) || (receipt && receipt->challenge == challenge);
    }
    if (state.Started() || !target->IsPhase(PHASE_GAME) || !InventorySystem::IsItemLoaded(entity) ||
        !InventorySystem::CanHandleItems(entity) || ExchangeSystem::IsActive(entity) ||
        !auriga::player_session::Nonzero(challenge) || ecs::SessionSystem::GetSkipSave(entity) ||
        ecs::SessionSystem::GetSafebox(entity) != entt::null ||
        ecs::SessionSystem::GetMall(entity) != entt::null || ecs::SessionSystem::IsSafeboxLoading(entity))
        return false;
    auriga::player_cache_snapshot::Snapshot snapshot;
    snapshot.account = key->account;
    ecs::SessionSystem::CreatePlayerProto(entity, snapshot.player);
    if (!stillCurrent() || !ITEM_MANAGER::instance().BuildPlayerCacheSnapshot(entity, snapshot.items))
        return false;
    const auto batch = auriga::player_cache_snapshot::Batch(*key, challenge, handle, snapshot);
    if (!batch || !IsConnected() || !m_playerCacheReady || key->link != PlayerSessionLink() ||
        !stillCurrent())
        return false;
    // Invalidate existing proofs before the save batch, then watch this new
    // operation. Packet appends the whole owned batch before starting output.
    ObservePlayerCacheWrite(HEADER_GD_PLAYER_SAVE);
    if (!state.Begin(*key, entt::to_integral(entity), challenge))
        return false;
    const auto now = std::chrono::steady_clock::now();
    m_playerCacheWatched.insert_or_assign(target->GetHandle(), *key);
    m_playerCachePolls.emplace(target->GetHandle(), CachePoll{now, now + std::chrono::milliseconds(200)});
    Packet(batch->data(), static_cast<int>(batch->size()));
    return true;
}
void CLIENT_DESC::PollPlayerCacheDrains()
{
    std::erase_if(m_playerCacheWatched,
                  [](const auto &entry)
                  {
                      auto *target = DESC_MANAGER::instance().FindByHandle(entry.first);
                      return !CurrentCacheTarget(target, entry.second) ||
                             !target->PlayerCacheDrain().Matches(entry.second,
                                                                 entt::to_integral(target->GetEntity()));
                  });
    const auto now = std::chrono::steady_clock::now();
    for (auto it = m_playerCachePolls.begin(); it != m_playerCachePolls.end();)
    {
        auto *target = DESC_MANAGER::instance().FindByHandle(it->first);
        auto request = target ? target->PlayerCacheDrain().Next() : std::nullopt;
        if (!request || !CurrentCacheTarget(target, request->key) || !IsConnected() || !m_playerCacheReady ||
            request->key.link != PlayerSessionLink() || now - it->second.started >= std::chrono::seconds(10))
        {
            if (target && target->PlayerCacheDrain().Waiting())
                target->PlayerCacheDrain().Invalidate();
            m_playerCacheWatched.erase(it->first);
            it = m_playerCachePolls.erase(it);
            continue;
        }
        if (now >= it->second.next)
        {
            const auto frame = auriga::player_cache_wire::Encode(*request);
            DBPacket(HEADER_GD_PLAYER_CACHE_REQUEST, it->first, frame.data(), frame.size());
            it->second.next = now + std::chrono::milliseconds(200);
        }
        ++it;
    }
}
void CLIENT_DESC::AcceptPlayerCacheReceipt(uint32_t descriptor,
                                           const auriga::player_cache_wire::Receipt &receipt)
{
    if (this != db_clientdesc || !m_playerCacheReady || receipt.key.link != PlayerSessionLink())
        return;
    auto *target = DESC_MANAGER::instance().FindByHandle(descriptor);
    const auto watched = m_playerCacheWatched.find(descriptor);
    if (watched == m_playerCacheWatched.end() || watched->second != receipt.key ||
        !CurrentCacheTarget(target, receipt.key))
        return;
    target->PlayerCacheDrain().Accept(receipt, receipt.key, entt::to_integral(target->GetEntity()));
    if (!target->PlayerCacheDrain().Waiting())
        m_playerCachePolls.erase(descriptor);
}
std::optional<auriga::player_cache_wire::Receipt> CLIENT_DESC::PlayerCacheReceipt(LPDESC target)
{
    if (this != db_clientdesc || !m_playerCacheReady || !IsConnected() || !target)
        return {};
    const auto key = target->PlayerSession().Current();
    const auto watched = m_playerCacheWatched.find(target->GetHandle());
    if (!key || key->link != PlayerSessionLink() || !CurrentCacheTarget(target, *key) ||
        watched == m_playerCacheWatched.end() || watched->second != *key)
        return {};
    return target->PlayerCacheDrain().Result(*key, entt::to_integral(target->GetEntity()));
}
void CLIENT_DESC::ObservePlayerCacheWrite(uint8_t header)
{
    if (this != db_clientdesc || m_playerCacheWatched.empty() ||
        !auriga::player_cache_wire::MayMutateCache(header))
        return;
    for (const auto &[handle, key] : m_playerCacheWatched)
        if (auto *target = DESC_MANAGER::instance().FindByHandle(handle); CurrentCacheTarget(target, key))
            target->PlayerCacheDrain().ObservePacket(header);
    m_playerCacheWatched.clear();
    m_playerCachePolls.clear();
}
