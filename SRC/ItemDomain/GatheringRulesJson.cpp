#include <item/ResourceGathering.hpp>
#include <nlohmann/json.hpp>
#include <set>
namespace auriga::item
{
std::expected<std::vector<GatheringSite>,std::string> ParseGatheringSites(std::string_view text,const ItemCatalog& c)
{
 try
 {
  if(text.size()>1u<<20)return std::unexpected("A gyűjtőhelyfájl legfeljebb 1 MiB lehet.");
  using J=nlohmann::json;std::vector<std::set<std::string>> keys;bool duplicate=false;
  auto json=J::parse(text,[&](int,J::parse_event_t event,J& value){if(event==J::parse_event_t::object_start)keys.emplace_back();else if(event==J::parse_event_t::object_end)keys.pop_back();else if(event==J::parse_event_t::key && !keys.back().insert(value.get<std::string>()).second)duplicate=true;return true;});
  if(duplicate)return std::unexpected("Ismétlődő mezőnév a gyűjtőhelyfájlban.");
  const auto number=[](const J& j,const char* key,std::uint64_t max)->std::uint32_t{const auto& n=j.at(key);if(!n.is_number_unsigned() || n.get<std::uint64_t>()>max)throw std::runtime_error("integer");return static_cast<std::uint32_t>(n.get<std::uint64_t>());};
  if(!json.is_object() || json.size()!=2 || !json.at("sites").is_array() || json.at("sites").size()>256)return std::unexpected("Hibás gyűjtőhely-dokumentum.");auto version=number(json,"version",2);if(version!=1 && version!=2)return std::unexpected("Csak az 1-es és 2-es gyűjtőhelyséma támogatott.");
  std::vector<GatheringSite> sites;
  for(const auto& j:json.at("sites"))
  {
   if(!j.is_object() || j.size()!=14 || !j.at("rewards").is_array() || j.at("rewards").size()>32)return std::unexpected("Hibás gyűjtőhely vagy jutalomlista.");GatheringSite s;
   s.id=number(j,"id",UINT32_MAX);s.map=number(j,"map",INT32_MAX);s.race=number(j,"race",UINT32_MAX);s.name=j.at("name").get<std::string>();s.kind=static_cast<ToolKind>(number(j,"kind",4));s.pool=static_cast<FishingPool>(number(j,"pool",3));s.range=number(j,"range",1000);s.durationMs=number(j,"duration_ms",120000);s.minimumPower=number(j,"minimum_power",INT32_MAX);s.powerStep=number(j,"power_step",INT32_MAX);s.baseChance=static_cast<std::uint16_t>(number(j,"base_chance_bp",10000));s.powerChance=static_cast<std::uint16_t>(number(j,"power_chance_bp",10000));s.baitChance=static_cast<std::uint16_t>(number(j,"bait_chance_bp",10000));
   if(!s.race && version!=2)return std::unexpected("A szabadvízi helyhez 2-es sémaverzió kell.");
   for(const auto& r:j.at("rewards")){if(!r.is_object() || r.size()!=4)return std::unexpected("Hibás jutalomsor.");auto key=ItemKey::Create(static_cast<std::uint16_t>(number(r,"category",UINT16_MAX)),number(r,"local",UINT32_MAX));if(!key)return std::unexpected("Érvénytelen jutalom-tárgyazonosító.");s.rewards.push_back({*key,number(r,"count",10000),number(r,"weight",UINT32_MAX)});}sites.push_back(std::move(s));
  }
  if(!ValidateGatheringSites(c,sites))return std::unexpected("Hibás gyűjtőhelyszabály vagy tárgyhivatkozás. Ellenőrizd az azonosítókat, eszközt, időt és jutalmakat.");return sites;
 }catch(const std::exception&){return std::unexpected("Hibás gyűjtőhely-JSON, hiányzó mező vagy nem megfelelő mezőtípus.");}
}
}
