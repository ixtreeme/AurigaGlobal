#include <item/PolymorphRuntime.hpp>
#include <item/TypedItemDetails.hpp>
#include <algorithm>
namespace auriga::item
{
std::expected<std::vector<std::byte>,TypedInventoryWireError> EncodePolymorphRuntime(const ItemCatalog& c,const PolymorphRuntimeView& v)
{
    using E=TypedInventoryWireError;
    if(!v.control || v.status>8 || (v.error!=255 && v.error>16) || v.purpose>3 || v.outcome>4 || v.roll>10000 || !v.schedule || !ValidateRecoverySchedule(c,*v.schedule) || v.now>INT64_MAX || v.formName.size()>1024 || v.activeName.size()>1024)return std::unexpected(E::BadValue);
    const auto* row=v.source ? PolymorphItem(c,v.source->key) : nullptr;const auto* p=row ? c.FindPolymorphProfile(row->profile) : nullptr;
    if(v.source && (!row || !v.source->polymorph || !v.preview || !v.actor || v.actor->skillLevel>40 || v.actor->mastery>3))return std::unexpected(E::BadValue);
    const auto formId=row ? row->fixedForm.value_or(v.source->polymorph->boundForm.value_or(0)) : 0;const auto* form=c.FindPolymorphForm(formId);
    const auto* skill=p && v.actor->skillLevel<=40 ? &p->skills[v.actor->skillLevel] : nullptr;
    const auto* mastery=p && p->kind==PolymorphKind::PracticeBook && v.actor->mastery<4 ? &p->masteries[v.actor->mastery] : nullptr;
    const auto chance=p && v.purpose==2 ? 10000 : p ? p->kind==PolymorphKind::Orb ? 10000 : static_cast<unsigned>(std::clamp<std::int64_t>(std::int64_t{v.actor->characterLevel}-v.actor->mobLevel+p->bookChanceBasePercent+v.actor->skillLevel+v.source->polymorph->bookGrade,0,100)*100) : 0;
    const auto bonus=p ? p->kind==PolymorphKind::Orb ? (skill && skill->attackBonusBp ? PolymorphBonus::Attack : PolymorphBonus::None) : form ? form->bookBonus : PolymorphBonus::None : PolymorphBonus::None;
    const auto amount=bonus==PolymorphBonus::None ? 0 : p->kind==PolymorphKind::Orb ? skill->attackBonusBp : (v.actor->skillLevel+v.source->polymorph->bookGrade)*(bonus==PolymorphBonus::Movement ? 1 : 100);
    const auto details=EncodeTypedItemDetails(c,v.source ? std::span(v.source,1) : std::span<const PersistentItem>{});if(!details)return std::unexpected(details.error());
    std::vector<std::byte> out;const auto put=[&](std::uint64_t n,unsigned count){for(unsigned i=0;i<count;++i)out.push_back(std::byte((n>>(8*i))&255));};const auto text=[&](const std::string& s){put(s.size(),2);for(unsigned char b:s)put(b,1);};
    put(247,1);put(0,4);put(1,1);put(v.status,1);put(v.error,1);put(v.control,8);put(v.preview,8);put(v.source ? v.source->id.Value() : 0,8);put(v.purpose,1);put(p ? static_cast<unsigned>(p->kind) : 0,1);put(v.source ? 13 : 0,2);put(v.source ? v.source->key.LocalId() : 0,4);
    put(v.source ? v.source->count : 0,4);put(v.source ? v.source->polymorph->bookGrade : 0,2);put(v.source ? v.source->polymorph->practiceRemaining : 0,4);put(v.source ? v.source->polymorph->boundForm.value_or(0) : 0,4);put(formId,4);put(form ? form->mobTemplate : 0,4);put(v.source ? v.actor->mobLevel : 0,2);text(v.formName);
    put(chance,2);put(p && p->kind==PolymorphKind::Orb ? 1 : 0,4);put(v.source ? v.actor->skillLevel : 0,1);put(v.source ? v.actor->mastery : 0,1);put(skill ? skill->powerBp : 0,2);put(mastery ? mastery->durationMs : skill ? skill->durationMs : 0,8);put(static_cast<unsigned>(bonus),1);put(bonus==PolymorphBonus::None ? 1 : bonus==PolymorphBonus::Movement ? 3 : 2,1);put(amount,4);put(mastery ? mastery->bonusDurationMs : skill ? skill->durationMs : 0,8);
    put(v.schedule->generation,8);put(v.now,8);put(v.schedule->polymorph.has_value(),1);
    if(v.schedule->polymorph){const auto& a=*v.schedule->polymorph;const auto& e=a.effect;put(13,2);put(a.source.LocalId(),4);put(e.formId,4);put(e.mobTemplate,4);put(e.powerBp,2);put(e.expiresAtUtcMs,8);put(e.bonusExpiresAtUtcMs,8);put(static_cast<unsigned>(e.bonus),1);put(static_cast<unsigned>(e.bonusUnit),1);put(e.bonusMagnitude,4);text(v.activeName);}
    put(v.outcome,1);put(v.roll,2);put(details->size(),4);out.insert(out.end(),details->begin(),details->end());if(out.size()>1u<<20)return std::unexpected(E::TooLarge);for(unsigned i=0;i<4;++i)out[1+i]=std::byte((out.size()>>(8*i))&255);return out;
}
}
