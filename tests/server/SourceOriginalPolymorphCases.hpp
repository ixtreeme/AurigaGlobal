#pragma once
void OriginalPolymorphCases()
{
    auto source=polymorph_runtime_fixture::Catalog();
    const auto key=polymorph_runtime_fixture::Key(2);
    std::ranges::find(source.items,key,&ItemBaseRow::key)->contentCode="polymorph.practice_book";
    auto& row=*std::ranges::find(source.polymorphItems,key,&ItemPolymorphRow::key);row.fixedForm.reset();
    auto& profile=*std::ranges::find(source.polymorphProfiles,row.profile,&PolymorphProfile::id);
    profile.maxBookGrade=255;profile.practicePerGrade=50;profile.bookChanceBasePercent=29;
    for(unsigned n=0;n<4;++n)profile.masteries[n]={static_cast<std::uint8_t>(n),10000+5000*n,9000+5000*n};
    source.polymorphForms.push_back({25001,"polymorph.original.mob_101",101,PolymorphBonus::Movement});
    source.polymorphForms.push_back({25002,"polymorph.original.mob_103",103,PolymorphBonus::None});
    source.polymorphForms.push_back({25003,"polymorph.original.mob_1901",1901,PolymorphBonus::Movement});
    const auto c=BuildItemCatalog(std::move(source)).value();
    const ItemOwnerFence fence{persistent_item_fixture::Character(),7,1};const std::array layouts{persistent_item_fixture::Inventory()};
    Check(!OriginalPolymorphBookGrant(*c,0,1,0) && !OriginalPolymorphBookGrant(*c,101,1,256),"original book issuance rejects absent form and excessive grade");
    for(unsigned mob:{101u,103u,1901u})for(unsigned mastery=0;mastery<4;++mastery)for(unsigned skill:{0u,20u,40u}) {
        const auto grant=OriginalPolymorphBookGrant(*c,mob,2,1).value();
        Check(grant.key==key && grant.count==1 && grant.polymorph->bookGrade==1 && grant.polymorph->practiceRemaining==2,"original server award carries exact independent form, grade and practice");
        auto item=polymorph_runtime_fixture::Item(2);item.polymorph=grant.polymorph;
        auto actor=polymorph_runtime_fixture::Facts(key);actor.characterLevel=35;actor.mobLevel=5;actor.mobTemplate=mob;actor.mastery=mastery;actor.skillLevel=skill;
        const auto quote=QuotePolymorphRuntime(*c,item,{},actor,1000).value();
        Check(quote.chanceBp==std::min(100u,60+skill)*100 && quote.consumeUnits==0 && quote.effect->expiresAtUtcMs==11000+5000*mastery,"original 29-based chance and seconds-based mastery duration");
        Check(quote.effect->bonus==(mob==103 ? PolymorphBonus::None : PolymorphBonus::Movement) && quote.effect->bonusMagnitude==(mob==103 ? 0 : skill+1),"original monster maps select additive speed points only for the two actual speed forms");
        auto result=MakePolymorphRuntimeCommit(*c,{25,1},fence,layouts,{&item,1},{},item.id,actor,0,1000,quote.chanceBp==10000 ? 0 : quote.chanceBp);
        Check(result && result->result.success && result->result.after.practiceRemaining==1 && result->commit.items[0].after.count==1,"successful practice keeps book and decreases only one practice");
        if(quote.chanceBp<10000){const auto fail=MakePolymorphRuntimeCommit(*c,{25,2},fence,layouts,{&item,1},{},item.id,actor,0,1000,quote.chanceBp+1);Check(fail && !fail->result.success && fail->result.after==*item.polymorph && !fail->commit.recovery->after.polymorph,"failed original book use keeps full payload and no transformation");}
        item.polymorph->practiceRemaining=0;actor.action=PolymorphAction::UpgradeBook;
        const auto upgrade=MakePolymorphRuntimeCommit(*c,{25,3},fence,layouts,{&item,1},{},item.id,actor,0,1000,0);
        Check(upgrade && upgrade->result.after.bookGrade==2 && upgrade->result.after.practiceRemaining==50,"original upgrade uses previous grade times50, then increases grade");
    }
}
