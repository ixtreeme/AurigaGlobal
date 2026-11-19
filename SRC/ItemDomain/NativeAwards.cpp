#include <item/NativeAwards.hpp>
#include <item/PersistentItems.hpp>
#include <limits>

namespace auriga::item
{
bool ValidNativeAwardClaim(const ItemCatalog&, const ItemCommit& out)
{
    if (!out.nativeAward) return true;
    const auto& claim=*out.nativeAward;
    if (!claim.id || claim.id>INT64_MAX || !claim.account || claim.account>UINT32_MAX || !claim.count ||
        out.kind!=ItemOperationKind::Grant || out.outcomeCode!=1 || out.owners.size()!=1 ||
        out.owners.front().owner!=ItemOwner{ItemOwnerKind::Account,claim.account} || out.items.empty() ||
        !out.wallets.empty() || out.ground || out.recovery || out.learning || out.petStudyCooldown ||
        out.currencyAward || out.currencyDrop || out.privateShop || out.dungeonEntry)
        return false;
    std::uint64_t added=0;
    for (const auto& change:out.items)
    {
        const auto& item=change.after;
        if (item.owner!=out.owners.front().owner || item.key!=claim.key || item.refinementLevel!=claim.level ||
            item.destroyed || !item.placement || item.placement->kind!=ItemStorageKind::Mall || item.placement->storageId!=1)
            return false;
        if (change.before)
        {
            auto expected=*change.before;
            if (item.count<=expected.count) return false;
            added+=item.count-expected.count;
            expected.count=item.count;
            expected.revision=item.revision;
            if (expected!=item) return false;
        }
        else added+=item.count;
    }
    return added==claim.count;
}
}
