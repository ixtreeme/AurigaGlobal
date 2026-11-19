void SourceMagicBirth()
{
    for(bool addon:{false,true}) {
        auto source=f::Catalog();const auto key=item_catalog_fixture::Sword();ItemDefinitionRules rule{key};rule.birthAddon=addon;rule.birthMagicChanceBp=10000;rule.birthAttributeProfile=1;source.definitionRules={rule};
        source.attributeBirthProfiles={{1,"birth.fixture",10000,10000,{{0,1,10000},{1,2,10000},{2,1,10000}}}};
        const auto c=BuildItemCatalog(source).value();const ItemOwnerFence owner{f::Character(),7,1};const std::array bags{f::Inventory()};const std::array ids{ItemInstanceId::Create(650001).value()};const std::array<std::uint8_t,16> op{0xA5,static_cast<std::uint8_t>(addon)};
        const auto count=[](const auto& attrs){return std::ranges::count_if(attrs.slots,[](const auto& b){return b.has_value();});};
        ItemGrant plain{key};const auto baseline=MakeGrantCommit(*c,op,owner,bags,{},plain,ids).value();const auto& original=baseline.items[0].after;
        Check(addon || original.attributes==InstanceAttributes{},"source default never enables optional magic even at 100 percent");
        ItemGrant magic{key};magic.tryMagic=true;const auto created=MakeGrantCommit(*c,op,owner,bags,{},magic,ids);Check(created.has_value(),"explicit magic issuance succeeds");const auto& item=created->items[0].after;
        Check(count(item.attributes)==count(original.attributes)+3 && created->result.size()==5 && created->result[1]==2,"three source magic trials follow optional paired addon and carry a v2 grant proof");
        for(unsigned slot=0;slot<original.attributes.slots.size();++slot)if(original.attributes.slots[slot])Check(item.attributes.slots[slot]==original.attributes.slots[slot],"magic never overwrites original paired birth");
        const auto retry=MakeGrantCommit(*c,op,owner,bags,{},magic,ids).value();Check(retry.items[0].after==item && retry.result==created->result,"same magic issuance identity replays exactly");
        auto forged=*created;forged.result={1};Check(!ValidateItemCommit(*c,forged),"a magic award cannot disguise itself as an ordinary grant");
        forged=*created;for(auto& b:forged.items[0].after.attributes.slots)if(b){b->valueScaled++;break;}Check(!ValidateItemCommit(*c,forged),"magic birth values cannot be forged inside a grant");
        magic.magicChanceBp=0;const auto disabled=MakeGrantCommit(*c,op,owner,bags,{},magic,ids).value();Check(disabled.items[0].after.attributes==original.attributes,"explicit zero override suppresses magic while preserving source addon");
        magic.magicChanceBp=10001;Check(!MakeGrantCommit(*c,op,owner,bags,{},magic,ids),"magic override outside probability range rejected");magic.magicChanceBp=10000;magic.tryMagic=false;Check(!MakeGrantCommit(*c,op,owner,bags,{},magic,ids),"chance override requires an explicit magic award policy");
        MobDrop drop{1,1,key,0,1,10000};
        auto mobGrant=MakeMobDropGrant(drop);const auto fromMob=MakeGrantCommit(*c,op,owner,bags,{},mobGrant,ids).value();
        Check(fromMob.items[0].after.attributes==item.attributes,"mob table override reaches the actual item factory and normal bonus slots");
        auto suppressedSource=source;suppressedSource.definitionRules[0].birthMagicChanceBp=0;
        const auto suppressed=BuildItemCatalog(suppressedSource).value();
        Check(MakeGrantCommit(*suppressed,op,owner,bags,{},mobGrant,ids).value().items[0].after.attributes==item.attributes,"100 percent mob override works even when the item's default is zero");
        drop.rareAttributeChanceBp=1;mobGrant=MakeMobDropGrant(drop);
        Check(MakeGrantCommit(*c,op,owner,bags,{},mobGrant,ids).value().items[0].after.attributes==original.attributes,"small mob override replaces the item's 100 percent default");
        drop.rareAttributeChanceBp=0;mobGrant=MakeMobDropGrant(drop);
        Check(!mobGrant.magicChanceBp && MakeGrantCommit(*c,op,owner,bags,{},mobGrant,ids).value().items[0].after.attributes==item.attributes,"zero mob setting retains the existing per-item birth policy");
        Check(MakeGrantCommit(*suppressed,op,owner,bags,{},mobGrant,ids).value().items[0].after.attributes==original.attributes,"zero mob setting also retains a zero per-item policy");
        const auto noDraw=RollBirthMagicAttributes(*c,key,0,original.attributes,source.attributeBirthProfiles[0],0,[](auto)->std::optional<std::uint32_t>{throw std::runtime_error("zero chance drew entropy");});Check(noDraw && *noDraw==original.attributes,"zero source chance never draws or alters state");
        Check(!RollBirthMagicAttributes(*c,key,0,original.attributes,source.attributeBirthProfiles[0],10000,[](auto){return 0;}),"failed entropy creates no partial birth");
        auto profile=source.attributeBirthProfiles[0];profile.secondChanceBp=0;
        const auto independent=RollBirthMagicAttributes(*c,key,0,original.attributes,profile,10000,[](auto){return 1;});Check(independent && count(*independent)==count(original.attributes)+2,"third source trial is independent of failed second trial");
        profile=source.attributeBirthProfiles[0];profile.tiers.pop_back();Check(!ValidAttributeBirthProfile(profile),"birth profile requires three complete explicit tier tables");
        auto invalid=source;invalid.definitionRules[0].birthAttributeProfile=99;Check(!BuildItemCatalog(invalid),"birth policy reference must resolve at publication");
        invalid=source;invalid.attributeBirthProfiles[0].tiers.push_back({0,1,1});Check(!BuildItemCatalog(invalid),"duplicate or overflowing source tier probabilities reject publication");
    }
    std::deque<ItemGrant> queue;ItemGrant first{item_catalog_fixture::Leather(),0,2},second=first;second.tryMagic=true;
    Check(QueueGrant(queue,first,20) && QueueGrant(queue,second,20) && queue.size()==2,"grant queue cannot merge ordinary and magic award policies");
    second.magicChanceBp=5000;Check(QueueGrant(queue,second,20) && queue.size()==3,"grant queue retains distinct explicit rare overrides");
}

void ExportMobDropBirth(const std::string& directory)
{
    namespace fs=std::filesystem;using Json=nlohmann::json;
    const auto root=fs::absolute(directory);fs::create_directories(root);Json cases=Json::array();
    const auto key=item_catalog_fixture::Sword();
    for(unsigned n=0;n<4;++n) {
        auto source=f::Catalog();ItemDefinitionRules rule{key};rule.birthMagicChanceBp=n==0 || n==3 ? 0 : 10000;rule.birthAttributeProfile=1;source.definitionRules={rule};
        source.attributeBirthProfiles={{1,"birth.fixture",10000,10000,{{0,1,10000},{1,2,10000},{2,1,10000}}}};
        const std::uint16_t override=n==0 ? 10000 : n==1 ? 1 : 0;
        source.mobDropGroups={{7001,"drop.bonus_fixture",MobDropTarget::Mob,999601,MobDropRoll::EachEntry,0,0,{{1,key,0,1,MobDropChanceScale,0,override,1,65535}}}};
        const auto c=BuildItemCatalog(source).value();MobDropContext kill;kill.mobVnum=999601;kill.killerLevel=30;
        const auto rolled=PlanMobDrops(*c,kill,[](auto){return 1u;}).value();Check(rolled.size()==1,"mob fixture rolls its actual table");
        std::deque<ItemGrant> queue;Check(QueueGrant(queue,MakeMobDropGrant(rolled.front()),1) && queue.size()==1,"mob fixture reaches owner award queue");
        const std::array<std::uint8_t,16> operation{0xB9};const std::array ids{ItemInstanceId::Create(650002).value()};const std::array bags{f::Inventory()};
        const auto commit=MakeGrantCommit(*c,operation,{f::Character(),7,1},bags,{},queue.front(),ids).value();Check(ValidateItemCommit(*c,commit).has_value(),"mob bonus commit validates");
        const std::array items{commit.items[0].after};const auto bonuses=InstanceAttributeContributions(*c,key,items[0].attributes).value();
        Check(bonuses.size()==(n==0 || n==2 ? 3 : 0),"mob fixture override and default cases create exact expected bonus counts");
        const auto write=[&](const char* kind,const std::vector<std::byte>& bytes){const auto path=root/(std::to_string(n)+"."+kind+".hex");std::ofstream file(path);for(auto b:bytes){auto v=std::to_integer<unsigned>(b);file<<"0123456789abcdef"[v>>4]<<"0123456789abcdef"[v&15];}file<<'\n';Check(file.good(),"mob client packet fixture exported");return path.string();};
        Json expected=Json::array();for(const auto& bonus:bonuses)expected.push_back({{"id",static_cast<unsigned>(bonus.bonus)},{"value",bonus.valueScaled}});
        cases.push_back({{"tableChance",override},{"defaultChance",rule.birthMagicChanceBp},{"expected",expected},
            {"inventory",write("inventory",EncodeTypedInventory(*c,bags,{},items).value())},{"details",write("details",EncodeTypedItemDetails(*c,items).value())}});
    }
    std::ofstream report(root/"mob-drop-birth.json");report<<Json{{"cases",cases}}.dump(2)<<'\n';Check(report.good(),"mob client report exported");
}
