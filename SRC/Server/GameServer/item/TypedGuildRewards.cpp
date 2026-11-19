#include "stdafx.h"
#include "TypedGuildRewards.hpp"
#include "ItemStore.hpp"
#include "ItemCatalogRuntime.hpp"
#include <NativeAwardsSql.hpp>
#include <Core/Logging.hpp>
#include <openssl/rand.h>

bool QueueTypedGuildReward(std::uint32_t guild,auriga::item::ItemKey key,std::uint16_t level,std::uint32_t count)
{
    namespace item=auriga::item;
    const auto catalog=item::runtime::CurrentCatalog();
    if (!guild || !count || !catalog || !catalog->Find(key)) return false;
    std::array<std::uint8_t,16> request{};
    if (RAND_bytes(request.data(),static_cast<int>(request.size()))!=1) return false;
    return item::runtime::ItemStore::Instance().Submit([=](CAsyncSQL& sql,const item::ItemCatalog& current) -> item::runtime::ItemStore::Completion {
        const auto queued=item::persistence::EnqueueGuildNativeAwards(sql,current,guild,request,key,level,count);
        return [=] {
            if (queued) LOG_INFO("Native guild reward {}: {} account awards queued",guild,*queued);
            else LOG_ERROR("Native guild reward {} could not be queued (error {})",guild,static_cast<int>(queued.error()));
        };
    });
}
