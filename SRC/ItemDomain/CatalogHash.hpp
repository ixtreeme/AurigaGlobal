#pragma once
#include <item/ItemCatalog.hpp>

namespace auriga::item::detail
{
std::expected<ItemCatalogDigest, ItemCatalogErrorCode> CanonicalCatalogHash(const ItemCatalogSource& source);
}
