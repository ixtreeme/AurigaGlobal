#pragma once
#include "ItemPersistenceSql.hpp"
#include <item/PrivateShops.hpp>
namespace auriga::item::persistence {
struct PrivateShopPrice {ItemKey key;std::uint16_t level=0;std::uint64_t unit=0;};
void PrivateShopWriteBefore(SqlTransaction&,const ItemCommit&);
void PrivateShopWriteAfter(SqlTransaction&,const ItemCommit&);
std::expected<std::optional<PrivateShopHeader>,ItemPersistenceError> LoadPrivateShopHeader(CAsyncSQL&,std::uint64_t owner);
std::expected<std::vector<PrivateShopPrice>,ItemPersistenceError> LoadPrivateShopPrices(CAsyncSQL&,const ItemCatalog&,std::uint64_t owner);
// Store-thread cleanup, bound to the authenticated native owner session. A late
// logout cannot close a newly claimed login's market. The original inventory
// cells remain occupied until this transaction releases all listing locks.
std::expected<bool,ItemPersistenceError> RecoverPrivateShop(CAsyncSQL&,const ItemCatalog&,ItemOwner,std::uint64_t session);
}
