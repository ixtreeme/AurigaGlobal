#include <item/BookRuntime.hpp>
#include <algorithm>
namespace auriga::item
{
const BookAidProfile* BookAidItemProfile(const ItemCatalog& c,ItemKey key)
{const auto* e=c.Find(key);const auto* row=e ? std::get_if<ItemConsumableRow>(&e->category) : nullptr;return row && row->bookAidProfile ? c.FindBookAid(*row->bookAidProfile) : nullptr;}
std::expected<BookAidPlan,SkillBookError> PrepareBookAidUse(const ItemCatalog& c,const PersistentItem& source,const BookLearning& before,const BookCharacter& live,const SkillBookAdmission& a)
{
    using E=SkillBookError;const auto* entry=c.Find(source.key);if(!entry)return std::unexpected(E::MissingItem);
    const auto* p=BookAidItemProfile(c,source.key);if(!p)return std::unexpected(E::WrongCategory);
    if(entry->base.retired)return std::unexpected(E::RetiredItem);
    if(!ValidBookLearning(before) || !ValidBookCharacter(live) || a.classIndex>3 || a.sexIndex>1 || source.count>entry->base.maxStack || before.revision==UINT64_MAX)return std::unexpected(E::InvalidState);
    if(!a.skillSystemEnabled)return std::unexpected(E::Disabled);if(!a.targetAllowedByRules)return std::unexpected(E::RuleDenied);
    if(!a.alive)return std::unexpected(E::DeadCharacter);if(a.polymorphed)return std::unexpected(E::Polymorphed);
    const auto& use=*c.FindUsePolicy(entry->base.usePolicy);
    if(a.inCombat && !use.useDuringCombat)return std::unexpected(E::CombatDenied);
    if(a.characterLevel<entry->base.minimumLevel)return std::unexpected(E::LevelDenied);
    if(!(use.allowedClasses & (1u<<a.classIndex)))return std::unexpected(E::ClassDenied);
    if(!(use.allowedSexes & (1u<<a.sexIndex)))return std::unexpected(E::SexDenied);
    if(source.count<p->unitsPerUse)return std::unexpected(E::InsufficientUnits);
    const auto balance=p->kind==BookAidKind::Chance ? before.chanceTokens : before.bypassTokens;
    if(balance>p->maximumCharges || p->chargesPerUse>p->maximumCharges-balance)return std::unexpected(E::ChargeLimit);
    return BookAidPlan{p->kind,p->unitsPerUse,p->chargesPerUse,p->maximumCharges};
}
std::expected<BookAidRuntimePlan,SkillBookError> MakeBookAidRuntimeCommit(const ItemCatalog& c,std::array<std::uint8_t,16> operation,ItemOwnerFence fence,
    std::span<const ItemStorageLayout> layouts,std::span<const PersistentItem> items,const BookLearning& before,const BookCharacter& live,ItemInstanceId id,const SkillBookAdmission& facts)
{
    const auto source=std::ranges::find(items,id,&PersistentItem::id);
    if(source==items.end() || fence.owner.kind!=ItemOwnerKind::Character || !PlainInventoryItem(*source,fence.owner) || !ValidatePersistentItem(c,*source))return std::unexpected(SkillBookError::MissingItem);
    auto result=PrepareBookAidUse(c,*source,before,live,facts);if(!result)return std::unexpected(result.error());
    auto consume=MakeConsumeCommit(c,operation,fence,layouts,items,id,result->units);if(!consume)return std::unexpected(SkillBookError::InvalidState);
    BookLearning after=before;++after.revision;after.character=live;
    auto& balance=result->kind==BookAidKind::Chance ? after.chanceTokens : after.bypassTokens;balance+=result->charges;
    consume->learning=ItemLearningChange{fence.owner,before,after};consume->outcomeCode=0x2300+static_cast<unsigned>(result->kind);
    consume->result={'A','I','D',1,static_cast<std::uint8_t>(result->kind)};
    for(auto value:{result->charges,balance})for(unsigned i=0;i<4;++i)consume->result.push_back(static_cast<std::uint8_t>(value>>(8*i)));
    if(!ValidateItemCommit(c,*consume))return std::unexpected(SkillBookError::InvalidState);return BookAidRuntimePlan{std::move(*consume),*result};
}
const SkillBookProfile* BookItemProfile(const ItemCatalog& c,ItemKey key)
{const auto* e=c.Find(key);const auto* b=e ? std::get_if<ItemSkillBookRow>(&e->category) : nullptr;return b ? c.FindSkillBook(b->profile) : nullptr;}
std::expected<SkillBookAdmission,SkillBookError> BookRuntimeAdmission(const ItemCatalog& c,const PersistentItem& item,const BookLearning& before,const BookCharacter& live,SkillBookAdmission facts,std::uint64_t now)
{
    const auto* profile=BookItemProfile(c,item.key);if(!profile)return std::unexpected(SkillBookError::WrongCategory);
    if(!ValidBookLearning(before) || !ValidBookCharacter(live) || now>INT64_MAX)return std::unexpected(SkillBookError::InvalidState);
    facts.availableUnits=item.count;facts.experience=live.experience;facts.boundSkillId=item.bookSkill;facts.skill.reset();facts.horse.reset();facts.refundPointBalance=0;facts.cooldownRemainingMs=0;facts.chanceBonusAvailable=before.chanceTokens>0;facts.delayBypassAvailable=before.bypassTokens>0;
    const auto skill=profile->targetMode==SkillBookTargetMode::Fixed ? profile->targets.front().skillId : item.bookSkill.value_or(0);
    if(profile->operation==SkillBookOperation::HorsePoint)
    {
        // The skill domain has alternate melee and ranged allocations.
        const auto sum=[&](std::initializer_list<unsigned> ids){unsigned n=0;for(auto id:ids)n+=live.skills[id].level;return static_cast<std::uint8_t>(std::min(255u,n));};
        if(live.points[2]>20)return std::unexpected(SkillBookError::PointLimit);
        facts.horse=BookHorseState{static_cast<std::uint8_t>(live.points[2]),sum({137,138,139}),sum({140,138,139})};
    }
    else
    {
        if(!skill || skill>254)return std::unexpected(SkillBookError::TargetDenied);facts.skill=BookSkillState{skill,live.skills[skill].level,0};
        if(profile->operation==SkillBookOperation::Forget){const auto target=std::ranges::find(profile->targets,skill,&SkillBookTarget::skillId);if(target==profile->targets.end())return std::unexpected(SkillBookError::TargetDenied);facts.refundPointBalance=live.points[static_cast<unsigned>(target->refundPool)-1];}
    }
    const auto progress=std::ranges::find(before.progress,skill,&BookProgress::skill);
    if(progress!=before.progress.end()){facts.cooldownRemainingMs=progress->readyAtMs-std::min(now,progress->readyAtMs);if(facts.skill && progress->level==facts.skill->level)facts.skill->successfulReads=progress->reads;}
    if(skill && profile->operation!=SkillBookOperation::Forget && (progress==before.progress.end() || live.skills[skill].nextReadSeconds>(progress->readyAtMs+999)/1000)){const auto old=live.skills[skill].nextReadSeconds*1000;facts.cooldownRemainingMs=std::max(facts.cooldownRemainingMs,old-std::min(now,old));}
    return facts;
}
std::expected<BookRuntimePlan,SkillBookError> MakeBookRuntimeCommit(const ItemCatalog& c,std::array<std::uint8_t,16> operation,ItemOwnerFence fence,
    std::span<const ItemStorageLayout> layouts,std::span<const PersistentItem> items,const BookLearning& before,const BookCharacter& live,ItemInstanceId id,SkillBookAdmission facts,std::uint64_t now,SkillBookDecisions decisions)
{
    const auto source=std::ranges::find(items,id,&PersistentItem::id);if(source==items.end() || !PlainInventoryItem(*source,fence.owner) || !ValidatePersistentItem(c,*source) || fence.owner.kind!=ItemOwnerKind::Character)return std::unexpected(SkillBookError::MissingItem);
    auto actor=BookRuntimeAdmission(c,*source,before,live,facts,now);if(!actor)return std::unexpected(actor.error());auto prepared=PrepareSkillBookUse(c,source->key,*actor);if(!prepared)return std::unexpected(prepared.error());auto result=ResolveSkillBookUse(*prepared,decisions);if(!result)return std::unexpected(result.error());
    if(before.revision==UINT64_MAX || (result->nextReadInMs && *result->nextReadInMs>INT64_MAX-now))return std::unexpected(SkillBookError::InvalidState);
    auto consume=MakeConsumeCommit(c,operation,fence,layouts,items,id,result->unitsToConsume);if(!consume)return std::unexpected(SkillBookError::InvalidState);
    BookLearning after=before;++after.revision;after.character=live;after.character.experience-=static_cast<std::uint32_t>(result->experienceToConsume);
    const auto skill=result->skillId.value_or(0);auto progress=std::ranges::find(after.progress,skill,&BookProgress::skill);if(progress==after.progress.end()){after.progress.push_back({skill});progress=std::prev(after.progress.end());}
    progress->level=result->nextLevel;progress->reads=result->nextReadCount;if(result->nextReadInMs)progress->readyAtMs=now+*result->nextReadInMs;
    if(skill){auto& level=after.character.skills[skill];level.level=result->nextLevel;level.mastery=level.level>=40 ? 3 : level.level>=30 ? 2 : level.level>=20 ? 1 : 0;if(result->nextReadInMs)level.nextReadSeconds=(progress->readyAtMs+999)/1000;}
    if(result->pointsGranted){auto& points=after.character.points[static_cast<unsigned>(result->pointPool)-1];if(points>=INT16_MAX)return std::unexpected(SkillBookError::PointLimit);points+=result->pointsGranted;}
    if(result->consumeChanceBonus)--after.chanceTokens;if(result->consumeDelayBypass)--after.bypassTokens;
    std::ranges::sort(after.progress,{},&BookProgress::skill);consume->learning=ItemLearningChange{fence.owner,before,after};consume->outcomeCode=0x2200+static_cast<unsigned>(result->outcome);consume->result={'B','O','K',1,static_cast<std::uint8_t>(result->outcome),static_cast<std::uint8_t>(decisions.successRollBp),static_cast<std::uint8_t>(decisions.successRollBp>>8)};
    for(unsigned i=0;i<8;++i)consume->result.push_back(static_cast<std::uint8_t>(decisions.readDelayMs>>(8*i)));
    if(!ValidateItemCommit(c,*consume))return std::unexpected(SkillBookError::InvalidState);return BookRuntimePlan{std::move(*consume),*result};
}
}
