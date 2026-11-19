#include <item/EffectRuntime.hpp>
struct BulkSource { auriga::item::ItemGrant grant;std::vector<auriga::item::BonusEntry> bonuses; };
void BulkCatalogWorker(const GameSqlConfig& player,const std::shared_ptr<const auriga::item::ItemCatalog>& catalog,
    const std::string& password,const std::string& path,const std::vector<BulkSource>& sources)
{
    using namespace auriga::item;
    CAsyncSQL sql;sql.SetTimeouts(5);sql.SetRetryConnectionErrors(false);
    Check(sql.Setup("127.0.0.1","root",password.c_str(),"player","utf8mb4",true,player.port),"bulk importer connects to private SQL");
    auto& store=runtime::ItemStore::Instance();Check(store.Start(player,catalog,"source-bulk-worker").has_value(),"actual bulk game item worker starts");
    unsigned gear=0,passive=0,permanent=0;
    for(unsigned n=0;n<sources.size();++n) {
        const auto& source=sources[n];const auto key=source.grant.key;std::cout<<"BULK_SOURCE_CASE="<<n<<" key="<<static_cast<unsigned>(key.Category())<<":"<<key.LocalId()<<std::endl;
        const ItemOwner owner{ItemOwnerKind::Character,7300u+n};
        const std::array layouts{ItemStorageLayout{{owner,ItemStorageKind::Inventory,1},5,9,4},ItemStorageLayout{{owner,ItemStorageKind::Equipment,1},28,1,1}};
        Check(persistence::ProvisionOwner(sql,*catalog,owner,layouts,{}).has_value(),"bulk owner provisioned");auto state=persistence::LoadCurrentItemOwner(sql,*catalog,owner).value();
        const auto op=[&](unsigned serial){return std::array<std::uint8_t,16>{0xF8,static_cast<std::uint8_t>(n),static_cast<std::uint8_t>(n>>8),static_cast<std::uint8_t>(serial)};};
        bool replay=false;
        const auto apply=[&](const ItemCommit& c){std::optional<runtime::CommitOutcome> got;Check(runtime::RequestCommit(owner,state.fence.sessionGeneration,c,[&](auto out){got=std::move(out);}),"bulk operation queued");for(unsigned spin=0;!got && spin<1000;++spin){store.Drain();std::this_thread::sleep_for(std::chrono::milliseconds(10));}Check(got && got->write && got->state,"bulk operation saved and reloaded");replay=got->write->replay;state=*got->state;};
        const auto ids=runtime::PeekInstanceIds(1);runtime::SpendInstanceIds(1);const auto id=ids.front();
        auto grant=MakeGrantCommit(*catalog,op(1),state.fence,state.storages,state.items,source.grant,ids);Check(grant.has_value(),"real source prototype can be granted");apply(*grant);
        const auto item=[&]()->const PersistentItem&{return *std::ranges::find(state.items,id,&PersistentItem::id);};
        const auto output=[&](const std::string& phase,const std::vector<std::byte>& runtime={}) {
            const auto write=[&](const char* suffix,const std::vector<std::byte>& bytes){std::ofstream file(path+"."+std::to_string(n)+"."+phase+suffix);for(auto b:bytes){const auto v=std::to_integer<unsigned>(b);file<<"0123456789abcdef"[v>>4]<<"0123456789abcdef"[v&15];}file<<'\n';Check(file.good(),"bulk source packet exported");};
            auto inventory=EncodeTypedInventory(*catalog,state.storages,state.wallets,state.items);Check(inventory && DecodeTypedInventory(*inventory,owner).has_value(),"bulk source inventory decodes natively");write(".inventory.hex",*inventory);write(".details.hex",EncodeTypedItemDetails(*catalog,state.items).value());if(!runtime.empty())write(".runtime.hex",runtime);
        };
        if(key.Category()==ItemCategory::Accessory) {
            ++gear;const auto* use=catalog->FindUsePolicy(catalog->Find(key)->base.usePolicy);unsigned job=0,sex=0;while(!(use->allowedClasses&(1u<<job)))++job;while(!(use->allowedSexes&(1u<<sex)))++sex;const EquipContext actor{255,static_cast<std::uint8_t>(job),static_cast<std::uint8_t>(sex),false};
            output("bag");auto equip=MakeEquipCommit(*catalog,op(2),state.fence,state.storages,state.items,id,actor);Check(equip.has_value(),"source refinement state can be equipped");apply(*equip);
            auto stats=BuildEquipmentStats(*catalog,owner,state.items).value();std::array<std::int64_t,MaximumBonusId+1> expected{};for(auto bonus:source.bonuses)expected[static_cast<unsigned>(bonus.bonus)]+=bonus.valueScaled;
            for(unsigned i=0;i<expected.size();++i)Check(stats.bonuses[i]==expected[i],"equipped stats match independently decoded source applies exactly");
            Check(item().placement->kind==ItemStorageKind::Equipment && item().refinementLevel==source.grant.refinementLevel,"exact source refinement and equipment location retained");output("equipped");
            const auto worn=state.items;apply(*equip);Check(replay && state.items==worn,"source equip retry cannot duplicate bonuses");
            const auto generation=persistence::ClaimOwnerSession(sql,*catalog,owner).value();state=persistence::LoadItemOwner(sql,*catalog,owner,generation).value();Check(state.items==worn,"source refinement and equipment survive relog");output("relogged");
            apply(MakeUnequipCommit(*catalog,op(3),state.fence,state.storages,state.items,id,{ItemStorageKind::Inventory,1,0},actor).value());
            stats=BuildEquipmentStats(*catalog,owner,state.items).value();Check(std::ranges::all_of(stats.bonuses,[](auto value){return value==0;}),"unequip removes all source bonuses");output("unequipped");
        } else if(const auto* profile=ToggleItemProfile(*catalog,key)) {
            ++permanent;Check(profile->durationMs==0 && item().toggle==ToggleEffectState{0,false},"permanent source stores explicit unlimited lifetime");
            ItemEffectFacts facts;facts.alive=facts.allowed=true;facts.actor.context=RecoveryContext::World;facts.actor.level=255;const auto* use=catalog->FindUsePolicy(catalog->Find(key)->base.usePolicy);while(!(use->allowedClasses&(1u<<facts.actor.classIndex)))++facts.actor.classIndex;while(!(use->allowedSexes&(1u<<facts.actor.sexIndex)))++facts.actor.sexIndex;
            const auto wire=[&](unsigned status){return EncodeItemEffectsRuntime(*catalog,{static_cast<std::uint8_t>(status),255,61,62,&item(),&state.recovery,state.items,100000}).value();};
            output("bag",wire(1));auto on=MakeItemEffectUse(*catalog,op(2),state.fence,state.storages,state.items,state.recovery,id,facts,0,100000,99);Check(on.has_value(),"permanent source activates");apply(*on);
            Check(item().toggle==ToggleEffectState{0,true},"permanent active state saves without finite time");
            const auto contributions=CollectItemEffectBonuses(*catalog,state.recovery,state.items).value();Check(contributions==catalog->FindBonus(profile->bonusProfile)->Data().entries,"permanent bonus appears once");output("active",wire(3));
            const auto active=state.items;const auto recovery=state.recovery;apply(*on);Check(replay && state.items==active && state.recovery==recovery,"permanent activation retry reuses receipt");
            const auto generation=persistence::ClaimOwnerSession(sql,*catalog,owner).value();state=persistence::LoadItemOwner(sql,*catalog,owner,generation).value();Check(state.items==active,"permanent state survives new session");output("relogged",wire(3));
            const auto projected=ProjectToggleItems(*catalog,state.items,1000000000000ULL,facts).value();Check(projected.front().toggle==ToggleEffectState{0,true} && !projected.front().destroyed,"long active interval never expires a permanent item");
            auto off=MakeItemEffectUse(*catalog,op(3),state.fence,state.storages,state.items,state.recovery,id,facts,1000000000000ULL,100000,100);Check(off.has_value(),"permanent can turn off after arbitrary active time");apply(*off);Check(!item().toggle->active && CollectItemEffectBonuses(*catalog,state.recovery,state.items)->empty(),"disable removes bonus and preserves item");output("off",wire(3));
            apply(MakeItemEffectUse(*catalog,op(4),state.fence,state.storages,state.items,state.recovery,id,facts,0,100000,101).value());output("reactivated",wire(3));
            facts.allowed=false;ItemCommit gate;gate.operationId=op(5);gate.catalogRevision=catalog->Revision();gate.catalogHash=catalog->ContentHash();gate.kind=ItemOperationKind::Service;gate.owners={state.fence};gate.outcomeCode=0x2003;auto settled=AdvanceRecoverySchedule(*catalog,state.recovery,1).value();++settled.revision;gate.recovery=ItemRecoveryChange{owner,state.recovery,settled,1};
            Check(SettleToggleTime(*catalog,gate,state.storages,state.items,1,facts).has_value(),"world rule boundary settles permanent toggle");apply(gate);Check(!item().toggle->active && !item().destroyed,"rule loss disables without destroying permanent source");output("rules_off",wire(3));
        } else {
            ++passive;Check(key.Category()==ItemCategory::Quest && !catalog->Find(key)->base.baseBonusProfile,"inert source is preserved without granting unused applies");output("bag");
            apply(*grant);Check(replay && state.items.size()==1,"passive grant receipt cannot mint a duplicate");
            apply(MakeMoveCommit(*catalog,op(2),state.fence,state.storages,state.items,id,{ItemStorageKind::Inventory,1,4}).value());
            const auto saved=state.items;const auto generation=persistence::ClaimOwnerSession(sql,*catalog,owner).value();state=persistence::LoadItemOwner(sql,*catalog,owner,generation).value();Check(state.items==saved,"inert source position survives relog");output("relogged");
        }
    }
    store.Stop();std::cout<<"SOURCE_BULK_WORKER=SUCCESS gear="<<gear<<" passive="<<passive<<" permanent="<<permanent<<" replay=reused relog=retained checks="<<checks<<std::endl;
}
