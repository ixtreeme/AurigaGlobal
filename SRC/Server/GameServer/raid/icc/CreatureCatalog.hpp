#pragma once
#include "Difficulty.hpp"
#include <array>
#include <algorithm>
#include <cstdint>
#include <cstddef>
namespace EclipseRaid::ICC {
struct CreatureRecord {uint32_t id=0;std::array<uint32_t,3> variants{};uint32_t health=1;float minimumDamage=0,maximumDamage=0;uint32_t attackPeriod=2000;float runSpeed=7;uint8_t sourceLevel=80;};
class CreatureCatalog {
 const CreatureRecord* rows;size_t count;
public:
 CreatureCatalog(const CreatureRecord* data,size_t size):rows(data),count(size){}
 const CreatureRecord* Find(uint32_t id)const{auto row=std::lower_bound(rows,rows+count,id,[](auto& r,uint32_t key){return r.id<key;});return row!=rows+count&&row->id==id?row:nullptr;}
 const CreatureRecord* Resolve(uint32_t id,Difficulty mode)const{auto row=Find(id);if(!row)return nullptr;auto difficulty=uint32_t(mode);if(!difficulty||difficulty>3)return row;auto variant=row->variants[difficulty-1];if(!variant&&difficulty>1){difficulty-=2;variant=difficulty?row->variants[difficulty-1]:0;}auto resolved=variant?Find(variant):nullptr;return resolved?resolved:row;}
};
inline constexpr std::array<uint32_t,12> BossCreatureIDs={36612,36855,37215,37813,36626,36627,36678,37970,37955,36789,36853,36597};
}
