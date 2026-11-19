#include <set>
void SourceAddonBirth()
{
    for(unsigned scenario=0;scenario<3;++scenario) {
        auto source=f::Catalog();const auto key=scenario==2 ? item_catalog_fixture::Armor() : item_catalog_fixture::Sword();
        if(scenario==2)source.attributeBindings.push_back({key,1});
        ItemDefinitionRules rule{key};rule.birthAddon=1;source.definitionRules={rule};
        if(scenario){source.bonuses.push_back({61,"bonus.source_static_addon",BonusSource::ItemBase,{{scenario==1 ? BonusId::NormalDamagePercent : BonusId::SkillDamagePercent,1600}}});std::ranges::find(source.items,key,&ItemBaseRow::key)->baseBonusProfile=61;}
        const auto c=BuildItemCatalog(source).value();const ItemOwnerFence fence{f::Character(),7,1};const std::array bags{f::Inventory()};const std::array ids{ItemInstanceId::Create(550001).value()};std::set<std::pair<std::int64_t,std::int64_t>> outcomes;
        for(unsigned n=0;n<256;++n) {
            const std::array<std::uint8_t,16> op{0xA1,static_cast<std::uint8_t>(scenario),static_cast<std::uint8_t>(n)};
            const auto issued=MakeGrantCommit(*c,op,fence,bags,{},ItemGrant{key},ids);Check(issued.has_value(),"source paired addon is created by actual grant factory");
            const auto& item=issued->items[0].after;std::int64_t hit=0,skill=0;unsigned count=0;
            for(unsigned slot=0;slot<8;++slot)if(const auto& b=item.attributes.slots[slot]){Check(slot<2 && b->valueScaled && (b->bonus==BonusId::NormalDamagePercent || b->bonus==BonusId::SkillDamagePercent),"birth has only compact nonzero source pair");++count;(b->bonus==BonusId::NormalDamagePercent ? hit : skill)=b->valueScaled;}
            Check((scenario || count) && count<=2 && !item.attributes.lockedSlot && skill>=-3000 && skill<=3000 && hit>=-5900 && hit<=6500,"source addon bounds and no fabricated rare bonus or lock");
            if(scenario)Check(scenario==1 ? !hit : !skill,"source static addon is not duplicated during birth");outcomes.emplace(hit,skill);
            const auto retry=MakeGrantCommit(*c,op,fence,bags,{},ItemGrant{key},ids);Check(retry && retry->items[0].after==item,"same issue identity never redraws addon");
            auto forged=*issued;forged.items[0].after.attributes={};if(count)Check(!ValidateItemCommit(*c,forged),"grant receipt cannot strip its source birth pair");
            const auto moved=MakeMoveCommit(*c,{0xA2},fence,bags,std::array{item},item.id,{ItemStorageKind::Inventory,1,20});Check(moved && moved->items[0].after.attributes==item.attributes,"relocation never draws again");
        }
        Check(outcomes.size()>10,"separate server issuance operations produce diverse source pairs");
        source.attributeBindings.clear();Check(!BuildItemCatalog(source),"birth addon without a native normal-attribute profile is rejected at publication");
    }
}
