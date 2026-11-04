#include <item/BookRuntime.hpp>
#include <item/TypedItemDetails.hpp>
#include <algorithm>
namespace auriga::item
{
std::expected<std::vector<std::byte>,TypedInventoryWireError> EncodeBookRuntime(const ItemCatalog& c,const BookRuntimeView& v)
{
    using E=TypedInventoryWireError;if(!v.learning || !ValidBookLearning(*v.learning) || !ValidBookCharacter(v.character) || !v.control || v.status>8 || (v.error!=255 && v.error>21) || v.now>INT64_MAX || v.skillName.size()>1024 || (v.source ? !v.preview : v.preview!=0))return std::unexpected(E::BadValue);
    const auto* p=v.source ? BookItemProfile(c,v.source->key) : nullptr;const auto* aid=v.source ? BookAidItemProfile(c,v.source->key) : nullptr;if(v.source && !p && !aid)return std::unexpected(E::BadValue);
    std::uint16_t target=p ? (p->targetMode==SkillBookTargetMode::Fixed ? p->targets.front().skillId : v.source->bookSkill.value_or(0)) : 0;
    const auto level=p && p->operation==SkillBookOperation::HorsePoint ? v.character.points[2] : v.character.skills[target].level;
    const auto step=p ? std::ranges::find(p->steps,level,&SkillBookStep::currentLevel) : std::vector<SkillBookStep>::const_iterator{};const auto* s=p && step!=p->steps.end() ? &*step : nullptr;
    const auto actor=p ? BookRuntimeAdmission(c,*v.source,*v.learning,v.character,v.actor,v.now) : std::expected<SkillBookAdmission,SkillBookError>{v.actor};
    const auto details=EncodeTypedItemDetails(c,v.source ? std::span(v.source,1) : std::span<const PersistentItem>{},false);if(!details)return std::unexpected(details.error());
    std::vector<std::byte> out;const auto put=[&](std::uint64_t n,unsigned size){for(unsigned i=0;i<size;++i)out.push_back(std::byte((n>>(8*i))&255));};
    put(244,1);put(0,4);put(aid ? 2 : 1,1);put(v.status,1);put(v.error,1);put(v.control,8);put(v.preview,8);put(v.source ? v.source->id.Value() : 0,8);put(v.source ? static_cast<unsigned>(v.source->key.Category()) : 0,2);put(v.source ? v.source->key.LocalId() : 0,4);put(v.learning->revision,8);put(v.now,8);
    put(p ? p->id : aid ? aid->id : 0,4);put(p ? static_cast<unsigned>(p->operation) : aid ? 3+static_cast<unsigned>(aid->kind) : 0,1);put(target,2);put(v.source ? v.source->bookSkill.value_or(0) : 0,2);put(p ? level : 0,1);put(s ? s->nextLevel : 0,1);put(actor && actor->skill ? actor->skill->successfulReads : 0,2);put(s ? s->requiredSuccessfulReads : 0,2);put(s ? s->successChanceBp : 0,2);put(s ? s->boostedChanceBp : 0,2);put(p ? p->unitsPerUse : aid ? aid->unitsPerUse : 0,2);put(s ? s->experienceCost : 0,8);put(actor ? actor->cooldownRemainingMs : 0,8);put(s ? s->delayMinimumMs : 0,8);put(s ? s->delayMaximumMs : 0,8);
    put(v.character.experience,4);for(auto points:v.character.points)put(points,2);put(v.learning->chanceTokens,4);put(v.learning->bypassTokens,4);put(p ? (p->allowDelayBypass ? 1 : 0)|(p->bonus==SkillBookBonus::ApplyAndConsume ? 2 : 0) : 0,1);
    put(v.result ? static_cast<unsigned>(v.result->outcome) : v.aidResult ? 5+static_cast<unsigned>(v.aidResult->kind) : 0,1);put(v.result ? v.result->nextLevel : 0,1);put(v.result ? v.result->nextReadCount : 0,2);put(v.result ? v.decisions.successRollBp : 0,2);put(v.result ? v.decisions.readDelayMs : 0,8);
    if(aid){put(aid->chargesPerUse,4);put(aid->maximumCharges,4);}
    put(v.skillName.size(),2);for(unsigned char ch:v.skillName)put(ch,1);put(details->size(),4);out.insert(out.end(),details->begin(),details->end());if(out.size()>1u<<20)return std::unexpected(E::TooLarge);for(unsigned i=0;i<4;++i)out[1+i]=std::byte((out.size()>>(8*i))&255);return out;
}
}
