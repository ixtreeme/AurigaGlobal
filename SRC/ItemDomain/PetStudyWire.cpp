#include <item/PetStudyWire.hpp>
#include <algorithm>
namespace auriga::item {
std::expected<std::vector<std::uint8_t>,PetLifecycleError> EncodePetStudyRuntime(const ItemCatalog& c,const PetStudyRuntimeView& v){
    const auto* entry=v.target ? c.Find(*v.target) : nullptr;
    const auto* pet=v.seal && v.seal->growthPet ? &*v.seal->growthPet : nullptr;
    if(v.options.size()>4096 || v.chanceBp>10000 || v.slot>3 || v.status>29 ||
       (v.target && !entry) || (v.seal && (!pet || !ValidatePersistentItem(c,*v.seal))))return std::unexpected(PetLifecycleError::InvalidCodec);
    const auto* companion=pet ? std::get_if<ItemCompanionRow>(&c.Find(v.seal->key)->category) : nullptr;
    const auto* training=companion && companion->trainingProfile ? c.FindPetTrainingProfile(*companion->trainingProfile) : nullptr;
    const auto skill=[&](std::uint32_t id)->const PetSkillDefinition*{if(!training)return nullptr;const auto found=std::ranges::find(training->skills,id,&PetSkillDefinition::id);return found==training->skills.end() ? nullptr : &*found;};
    std::vector<std::uint8_t> out;const auto put=[&](std::uint64_t n,unsigned size){for(unsigned b=0;b<size;++b)out.push_back(static_cast<std::uint8_t>(n>>(b*8)));};
    const auto text=[&](std::string_view s){put(s.size(),2);out.insert(out.end(),s.begin(),s.end());};
    const auto code=[&](const PetSkillDefinition* s){text(s && s->code.size()<=128 ? std::string_view(s->code) : std::string_view{});};
    put(PetStudyRuntimeHeader,1);put(0,4);put(v.withStudy ? 7 : 1,1);put(v.status,1);put(v.item,8);put(v.quote,8);put(v.cost,8);put(v.gold,8);
    put(v.target ? static_cast<unsigned>(v.target->Category()) : 0,2);put(v.target ? v.target->LocalId() : 0,4);text(entry && entry->base.nameKey.size()<=256 ? std::string_view(entry->base.nameKey) : std::string_view{});
    put(pet ? v.seal->id.Value() : 0,8);
    if(pet){put(pet->name.size(),1);out.insert(out.end(),pet->name.begin(),pet->name.end());put(pet->bornAtUtcMs,8);put(pet->progress.level,2);put(pet->progress.stage,1);put(static_cast<unsigned>(pet->life.clock),1);put(pet->life.capacityMs,8);put(pet->life.remainingMs-std::min(pet->life.remainingMs,v.elapsedMs),8);put(pet->traits.maximumHpPercentTenths,2);put(pet->traits.monsterDamagePercentTenths,2);put(pet->traits.pveNormalDamagePercentTenths,2);}
    if(v.withStudy){
    put(v.book,8);put(v.skill,4);put(v.slot,1);put(v.nextRank,2);put(v.chanceBp,2);put(v.readyAtMs,8);code(skill(v.skill));put(v.options.size(),2);
    for(const auto& [book,mask]:v.options){if(!book || !mask || mask>63)return std::unexpected(PetLifecycleError::InvalidCodec);put(book,8);put(mask,1);}
    if(pet)for(const auto& slot:v.slots){const auto* s=skill(slot.skill);if(slot.skill && !s)return std::unexpected(PetLifecycleError::InvalidCodec);put(slot.unlocked,1);put(slot.skill,4);put(slot.rank,2);put(s ? s->ranks.size() : 0,2);code(s);}
    put(v.bypassTokens,4);put(v.usesBypass,1);}
    if(out.size()>65535)return std::unexpected(PetLifecycleError::InvalidCodec);
    for(unsigned b=0;b<4;++b)out[1+b]=static_cast<std::uint8_t>(out.size()>>(b*8));return out;
}
}
