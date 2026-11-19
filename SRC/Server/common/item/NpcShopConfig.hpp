#pragma once
#include "NpcShops.hpp"
#include <string_view>
namespace auriga::item {
struct NpcShopRoute { std::uint32_t source=0;NpcShop stock; };
struct NpcShopManifest { std::vector<NpcShop> shops;std::vector<NpcShopRoute> routes; };
std::expected<NpcShopManifest,std::string> ParseNpcShopManifest(std::string_view,const ItemCatalog&);
}
