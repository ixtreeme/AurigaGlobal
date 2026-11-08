#include <item/ForgeAids.hpp>
namespace auriga::item {
bool ValidForgeAid(const ItemForgeAidRow& r){
    if(r.key.Category()!=ItemCategory::ForgeAid || !r.units || !r.targetMask || (r.targetMask&~15) || r.minimumLevel>r.maximumLevel || r.maximumLevel>255 || r.chanceBp>10000 || r.failure>ForgeFailure::Downgrade)return false;
    if(r.kind==ForgeAidKind::Booster)return r.boosterGroup>=1 && r.boosterGroup<=3 && r.chanceMode==ForgeChance::Add && r.chanceBp && r.failure==ForgeFailure::Recipe && r.levels.empty();
    if(r.kind!=ForgeAidKind::Scroll || r.boosterGroup || (r.chanceMode!=ForgeChance::Recipe && r.chanceMode!=ForgeChance::Replace && r.chanceMode!=ForgeChance::Levels))return false;
    if(r.chanceMode==ForgeChance::Recipe)return !r.chanceBp && r.levels.empty();
    if(r.chanceMode==ForgeChance::Replace)return r.levels.empty();
    if(r.chanceBp || r.levels.size()!=std::size_t(r.maximumLevel-r.minimumLevel)+1)return false;
    for(unsigned j=0;j<r.levels.size();++j)if(r.levels[j].level!=r.minimumLevel+j || r.levels[j].chanceBp>10000)return false;
    return true;
}
}
