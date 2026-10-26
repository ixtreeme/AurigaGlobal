#include "stdafx.h"
#include "ClientManager.h"
#include <Core/peer_auth.h>

void CClientManager::HandlePlayerCacheRequest(CPeer *peer, uint32_t descriptor, std::span<const uint8_t> bytes)
{
    namespace wire = auriga::player_cache_wire;
    const auto request = wire::DecodeRequest(bytes);
    if (!PeerAuth::HasKey() || !peer->IsAuthenticated() || !request ||
        request->key.link != peer->PlayerSessionLink()) return;
    wire::Receipt result{request->key, request->challenge, wire::Status::Stale};
    const auto lease = m_playerSessions.Find(request->key.account);
    if (lease && lease->key == request->key && lease->peer == peer->GetHandle() && lease->descriptor == descriptor)
    {
        if (request->action == wire::Action::Begin && !BeginPlayerCacheDrain(*lease, request->challenge))
            result.status = wire::Status::Rejected;
        else if (const auto receipt = PollPlayerCacheDrain(*lease, request->challenge))
            result = auriga::player_cache::ToWire(*receipt);
        else result.status = wire::Status::Rejected;
    }
    const auto frame = wire::Encode(result);
    peer->EncodeHeader(HEADER_DG_PLAYER_CACHE_RECEIPT, descriptor, frame.size());
    peer->Encode(frame.data(), frame.size());
}

bool CClientManager::PlayerCacheDrainCurrent(const auriga::player_session::Lease &lease)
{
    auto *login = GetLoginDataByAID(lease.key.account);
    auto *peer = GetPeer(lease.peer);
    if (!login || !peer || !m_playerSessions.Loaded(lease))
        return false;
    for (uint8_t slot = 0; slot < PLAYER_PER_ACCOUNT; ++slot)
        if (login->GetAccountRef().players[slot].dwID == lease.key.character)
        {
            ClientHandleInfo info(lease.descriptor, lease.key.character, lease.key.account);
            info.account_index = slot;
            info.playerSession = lease;
            return PlayerSessionCurrent(peer, info) && login->IsPlay();
        }
    return false;
}
bool CClientManager::PlayerCacheDrainView(uint32_t player, CPlayerTableCache *&cache,
                                          std::vector<CItemCache *> &items)
{
    cache = GetPlayerCache(player);
    if (!cache)
        return false; // The source must have submitted its final player snapshot first.
    // Include every owned cache, even if a legacy owner-set index is missing.
    for (const auto &[id, item] : m_map_itemCache)
        if (item->Get(false)->owner == player)
        {
            if (item->Get(false)->id != id || items.size() >= auriga::player_cache::MaximumItems)
                return false;
            items.push_back(item);
        }
    return true;
}
bool CClientManager::BeginPlayerCacheDrain(const auriga::player_session::Lease &lease,
                                           const auriga::player_session::Token &challenge)
{
    if (!PlayerCacheDrainCurrent(lease) || !auriga::player_session::Nonzero(challenge))
        return false;
    auto existing = m_playerCacheDrains.find(lease.key.account);
    if (existing != m_playerCacheDrains.end())
    {
        if (existing->second.Session() == lease)
            return existing->second.Challenge() == challenge;
        m_playerCacheDrains.erase(existing);
    }
    // One preparation per native login; no unbounded client-controlled job registry.
    if (m_playerCacheDrains.size() >= 256)
        return false;
    CPlayerTableCache *player = nullptr;
    std::vector<CItemCache *> items;
    if (!PlayerCacheDrainView(lease.key.character, player, items))
        return false;
    auto drain = auriga::player_cache::Drain::Begin(m_playerSessions, lease, challenge, {player, items},
                                                    CDBManager::instance().PlayerQueueOrigins(), []
                                                    { return CDBManager::instance().DrainPlayerWrites(); });
    if (!drain)
        return false;
    m_playerCacheDrains.emplace(lease.key.account, std::move(*drain));
    return true;
}
std::optional<auriga::player_cache::Receipt>
CClientManager::PollPlayerCacheDrain(const auriga::player_session::Lease &lease,
                                     const auriga::player_session::Token &challenge)
{
    const auto it = m_playerCacheDrains.find(lease.key.account);
    if (it == m_playerCacheDrains.end() || it->second.Session() != lease ||
        it->second.Challenge() != challenge)
        return {};
    if (!PlayerCacheDrainCurrent(lease))
    {
        m_playerCacheDrains.erase(it);
        return auriga::player_cache::Receipt{lease, challenge, auriga::player_cache::Status::Stale};
    }
    CPlayerTableCache *player = nullptr;
    std::vector<CItemCache *> items;
    if (!PlayerCacheDrainView(lease.key.character, player, items))
        player = nullptr;
    return it->second.Poll(m_playerSessions, {player, items});
}
