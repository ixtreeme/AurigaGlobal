#pragma once
#include "ItemPersistenceSql.hpp"

namespace auriga::item::persistence
{
// Checks that a typed reward can be delivered to an empty account Mall.
bool CanQueueNativeItemAward(const ItemCatalog&,ItemKey,std::uint16_t level,std::uint32_t count);
// Trusted server-authored guild rewards. The request ID is fixed before queueing
// SQL; repeated submission cannot produce a second award for the same member.
std::expected<unsigned,ItemPersistenceError> EnqueueGuildNativeAwards(CAsyncSQL&,const ItemCatalog&,
    std::uint32_t guild,std::array<std::uint8_t,16> request,ItemKey,std::uint16_t level,std::uint32_t count);
std::expected<unsigned,ItemPersistenceError> EnqueueCharacterNativeAwards(CAsyncSQL&,const ItemCatalog&,
    std::span<const std::uint32_t> recipients,std::array<std::uint8_t,16> request,ItemKey,std::uint16_t level,std::uint32_t count);
// Store thread, after the authenticated Mall-open path provisions the account.
// Full Mall storage leaves the award pending. Claim + instances + receipt are atomic.
std::expected<unsigned,ItemPersistenceError> DeliverNativeItemAwards(CAsyncSQL&,const ItemCatalog&,ItemOwner account);
}
