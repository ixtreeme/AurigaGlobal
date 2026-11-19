#pragma once
#include "CatalogSnapshot.hpp"
#include <SQL/PreparedTransaction.h>
class CAsyncSQL;
namespace auriga::item::sql
{
struct LiveMobDrops
{
    bool supported=false;
    std::string release,baseHash,hash;
    std::shared_ptr<const ItemCatalog> catalog;
    Rows groups,entries;
    Rows containerGroups,containerEntries;
    bool containersSupported=false;
};
struct LiveDropWrite { LiveMobDrops state; SqlTransaction steps; };
// A missing migration uses the static catalog; a partial/invalid release fails.
// Unchanged head returns nullopt, avoiding rebuilding a catalog on every poll.
std::expected<std::optional<LiveMobDrops>,std::string> ReadLiveMobDrops(CAsyncSQL&,
    std::shared_ptr<const ItemCatalog>,std::optional<std::string> knownRelease={});
std::expected<LiveDropWrite,std::string> PrepareLiveDropWrite(std::shared_ptr<const ItemCatalog>,
    Rows groups,Rows entries,std::string expectedRelease,std::string release,
    Rows containerGroups={},Rows containerEntries={},bool containersSupported=false);
std::expected<LiveMobDrops,std::string> PublishLiveMobDrops(CAsyncSQL&,
    std::shared_ptr<const ItemCatalog>,Rows groups,Rows entries,std::string expectedRelease,
    std::string expectedHash,std::string release,
    std::optional<std::pair<Rows,Rows>> containers={});
std::string CatalogHashHex(const ItemCatalogDigest&);
}
