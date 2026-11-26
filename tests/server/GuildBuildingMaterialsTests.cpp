#include <common/item/GuildBuildingMaterials.hpp>
#include <stdexcept>
#include <iostream>
int main() {
 using auriga::item::ReadGuildBuildingMaterials;
 const auto check=[](bool ok){if(!ok)throw std::runtime_error("Native construction recipe assertion");};
 auto empty=ReadGuildBuildingMaterials("[]");check(empty.has_value());
 auto native=ReadGuildBuildingMaterials(R"([{"category":6,"local":755,"count":20},{"category":6,"local":756,"count":30}])");
 check(native && (*native)[0].category==6 && (*native)[0].local==755 && (*native)[0].count==20 && (*native)[1].count==30);
 for(auto bad:{"90010,20/90011,30","null","{}",
 R"([{"vnum":90010,"count":20}])",
 R"([{"category":6,"local":755,"count":0}])",
 R"([{"category":-1,"local":755,"count":20}])",
 R"([{"category":6,"local":4294967296,"count":20}])",
 R"([{"category":6,"local":755,"count":20,"vnum":90010}])",
 R"([{"category":6,"local":755,"count":1},{"category":6,"local":755,"count":2}])"})
   check(!ReadGuildBuildingMaterials(bad));
 std::cout<<"Native construction material parsing passed\n";
}
