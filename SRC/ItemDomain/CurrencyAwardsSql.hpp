#pragma once
#include "ItemPersistenceSql.hpp"
#include <string_view>
namespace auriga::item::persistence {
// Store-thread only, after the authenticated bank-open path has provisioned the
// account owner. The transaction checks account ID/login again. No old item
// award, source prototype, socket or character-bag placement is involved.
std::expected<unsigned,ItemPersistenceError> DeliverCurrencyAwards(CAsyncSQL&,
    const ItemCatalog&,ItemOwner account,std::string_view authenticatedLogin);
}
