#include <item/NpcShopConfig.hpp>
#include <nlohmann/json.hpp>
#include <set>
#include <stdexcept>
namespace auriga::item {
std::expected<NpcShopManifest,std::string> ParseNpcShopManifest(std::string_view text,const ItemCatalog& catalog)
{
    using Json=nlohmann::json;
    try {
        if(text.size()>(1u<<20))return std::unexpected("Native shop document exceeds 1 MiB");
        const auto root=Json::parse(text);
        if(!root.is_object() || root.size()!=(root.contains("routes")?3:2) || (root.at("version")!=1 && root.at("version")!=2) ||
           !root.at("shops").is_array() || root.at("shops").size()>MaxNpcShops)return std::unexpected("Invalid native shop document");
        const bool extended=root.at("version")==2;
        const auto number=[](const Json& row,const char* field,std::uint64_t max){const auto& v=row.at(field);if(!v.is_number_unsigned() || v.get<std::uint64_t>()>max)throw std::runtime_error("Invalid native shop integer");return v.get<std::uint64_t>();};
        const auto key=[&](const Json& row){auto value=ItemKey::Create(static_cast<std::uint16_t>(number(row,"category",UINT16_MAX)),static_cast<std::uint32_t>(number(row,"local",UINT32_MAX)));if(!value)throw std::runtime_error("Invalid native shop key");return *value;};
        NpcShopManifest out;std::set<std::pair<std::uint32_t,std::uint32_t>> routes;
        const auto parse=[&](const Json& row,bool routed) {
            if(!row.is_object() || row.size()!=(routed?4:3) || !row.at("offers").is_array() || row.at("offers").size()>MaxNpcShopOffers)throw std::runtime_error("Invalid native shop listing");
            NpcShop shop{static_cast<std::uint32_t>(number(row,"npc_race",UINT32_MAX)),row.at("name").get<std::string>(),{}};
            for(const auto& offer:row.at("offers")) {
                if(!offer.is_object() || offer.size()!=(extended?(offer.contains("try_magic")?8:7):5))throw std::runtime_error("Invalid native shop offer");
                NpcShopOffer value{static_cast<std::uint32_t>(number(offer,"id",UINT32_MAX)),{key(offer),static_cast<std::uint16_t>(number(offer,"level",UINT16_MAX)),static_cast<std::uint32_t>(number(offer,"count",UINT32_MAX))}};
                if(extended) {
                    if(offer.contains("try_magic"))value.grant.tryMagic=offer.at("try_magic").get<bool>();
                    value.bundleGold=number(offer,"gold",INT64_MAX);
                    if(!offer.at("costs").is_array() || offer.at("costs").size()>5)throw std::runtime_error("Invalid native shop costs");
                    for(const auto& cost:offer.at("costs")) {
                        if(!cost.is_object() || cost.size()!=4)throw std::runtime_error("Invalid native shop cost");
                        value.costs.push_back({key(cost),static_cast<std::uint16_t>(number(cost,"level",UINT16_MAX)),static_cast<std::uint32_t>(number(cost,"count",UINT32_MAX))});
                    }
                }
                shop.offers.push_back(std::move(value));
            }
            const auto source=routed ? static_cast<std::uint32_t>(number(row,"source_id",UINT32_MAX)) : 0;
            const bool standalone=shop.npcRace==0 && source==RuneExchangeShopRoute;
            if(!ValidateNpcShops(catalog,std::span(&shop,1),standalone) || !EncodeNpcShop(catalog,&shop,1,1,0,NpcShopStatus::Ready,{}))throw std::runtime_error("Shop stock does not match the native catalog: NPC "+std::to_string(shop.npcRace));
            if(routed) {
                if(!source || !routes.emplace(shop.npcRace,source).second)throw std::runtime_error("Invalid or duplicate authored shop route");out.routes.push_back({source,std::move(shop)});
            }else out.shops.push_back(std::move(shop));
        };
        for(const auto& row:root.at("shops"))parse(row,false);
        if(root.contains("routes")) {
            if(!extended || !root.at("routes").is_array() || root.at("routes").size()>MaxNpcShops)return std::unexpected("Invalid authored shop routes");
            for(const auto& row:root.at("routes"))parse(row,true);
        }
        if(!ValidateNpcShops(catalog,out.shops))return std::unexpected("Invalid or duplicate NPC stock");return out;
    }catch(const std::exception& error){return std::unexpected(error.what());}
}
}
