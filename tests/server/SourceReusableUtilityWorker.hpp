#include <item/BuiltinUtilityRuntime.hpp>
#include <item/PolymorphRuntime.hpp>
void ReusableUtilityCatalogWorker(const GameSqlConfig& player,const std::shared_ptr<const auriga::item::ItemCatalog>& catalog,
    const std::string& password,const std::string& path,const std::vector<BulkSource>& sources)
{
    using namespace auriga::item;
    Check(sources.size()==2,"two original reusable utilities");CAsyncSQL sql;sql.SetTimeouts(5);sql.SetRetryConnectionErrors(false);
    Check(sql.Setup("127.0.0.1","root",password.c_str(),"player","utf8mb4",true,player.port),"owned reusable utility SQL connection");
    auto& store=runtime::ItemStore::Instance();Check(store.Start(player,catalog,"source-reusable-utility-worker").has_value(),"actual reusable utility item worker starts");
    unsigned serial=0;bool replay=false;
    const auto op=[&](){++serial;return std::array<std::uint8_t,16>{0xF3,static_cast<std::uint8_t>(serial),static_cast<std::uint8_t>(serial>>8)};};
    const auto provision=[&](std::uint64_t id){const ItemOwner owner{ItemOwnerKind::Character,id};const std::array layouts{ItemStorageLayout{{owner,ItemStorageKind::Inventory,1},5,9,4}};Check(persistence::ProvisionOwner(sql,*catalog,owner,layouts,{}).has_value(),"reusable utility owner provisioned");return persistence::LoadCurrentItemOwner(sql,*catalog,owner).value();};
    const auto apply=[&](persistence::LoadedItemOwner& state,const ItemCommit& commit){
        std::optional<runtime::CommitOutcome> got;Check(runtime::RequestCommit(state.fence.owner,state.fence.sessionGeneration,commit,[&](auto out){got=std::move(out);}),"reusable utility/native clear queued");
        for(unsigned spin=0;!got && spin<1000;++spin){store.Drain();std::this_thread::sleep_for(std::chrono::milliseconds(10));}Check(got && got->write && got->state,"reusable utility/native clear persisted and reloaded");replay=got->write->replay;state=*got->state;
    };
    const auto grant=[&](persistence::LoadedItemOwner& state,const ItemGrant& spec){const auto ids=runtime::PeekInstanceIds(1);runtime::SpendInstanceIds(1);apply(state,MakeGrantCommit(*catalog,op(),state.fence,state.storages,state.items,spec,ids).value());return ids[0];};
    const auto relog=[&](persistence::LoadedItemOwner& state){const auto saved=state;const auto generation=persistence::ClaimOwnerSession(sql,*catalog,state.fence.owner).value();state=persistence::LoadItemOwner(sql,*catalog,state.fence.owner,generation).value();Check(state.items==saved.items && state.recovery==saved.recovery,"reusable utility and actual native form survive fresh SQL login");};
    const std::uint64_t now=1794697200000ULL;
    unsigned rings=0,cancels=0;
    for(unsigned n=0;n<sources.size();++n){
        auto state=provision(16000u+n);const auto id=grant(state,sources[n].grant);const auto original=state.items[0];
        const auto source=[&]()->const PersistentItem&{return *std::ranges::find(state.items,id,&PersistentItem::id);};
        BuiltinUtilityFacts facts{{source().key,1,255,QuestItemContext::World,false,false,true,true},0,0,21,true,false};
        const auto* use=catalog->FindUsePolicy(catalog->Find(source().key)->base.usePolicy);while(!(use->allowedClasses&(1u<<facts.characterClass)))++facts.characterClass;while(!(use->allowedSexes&(1u<<facts.sex)))++facts.sex;
        facts.summonZoneAllowed=facts.warpAllowed=facts.marriageAvailable=true;facts.maximumSp=1003;facts.currentSp=300;
        const auto plan=PrepareBuiltinUtility(*catalog,source(),state.fence.owner,facts).value();Check(plan.consume==0,"original reusable utility is not debited");
        const auto write=[&](const std::string& phase,const RecoverySchedule* target=nullptr){
            const auto file=[&](const char* kind,const auto& bytes){std::ofstream f(path+"."+std::to_string(n)+"."+phase+"."+kind+".hex");for(auto b:bytes){const auto v=static_cast<unsigned char>(b);f<<"0123456789abcdef"[v>>4]<<"0123456789abcdef"[v&15];}f<<'\n';Check(f.good(),"actual reusable utility wire state exported");};
            file("inventory",EncodeTypedInventory(*catalog,state.storages,state.wallets,state.items).value());file("details",EncodeTypedItemDetails(*catalog,state.items).value());
            if(target)file("polymorph",EncodePolymorphRuntime(*catalog,{1,255,0,0,0,123,0,now,nullptr,target,nullptr,"",target->polymorph ? "Original wolf" : ""}).value());
        };
        write("ready");
        for(unsigned repeat=0;repeat<10;++repeat){const auto p=PrepareBuiltinUtility(*catalog,source(),state.fence.owner,facts).value();Check(p.consume==0 && source()==original,"repeated source admission never edits reusable count, revision or payload");Check(!MakeBuiltinUtilityCommit(*catalog,op(),state.fence,state.storages,state.items,id,facts),"consuming utility API refuses the reusable source");}
        if(plan.handler==BuiltinQuestItem::SpouseWarp){
            ++rings;Check(plan.spCost==300,"actual original spouse ring costs floor of30 percent maximum SP");
            for(unsigned gate=0;gate<5;++gate){auto denied=facts;if(gate==0)denied.summonZoneAllowed=false;if(gate==1)denied.warpAllowed=false;if(gate==2)denied.marriageAvailable=false;if(gate==3)denied.marriageArenaBlocked=true;if(gate==4)denied.currentSp=299;Check(!PrepareBuiltinUtility(*catalog,source(),state.fence.owner,denied),"actual ring actor and SP restrictions do not consume item");}
        }else{
            ++cancels;Check(plan.handler==BuiltinQuestItem::PolymorphCancel,"actual converted cancel item has its independent compiled handler");
            for(unsigned other=0;other<2;++other){
                auto remote=other ? provision(16100u+n) : state;auto& target=other ? remote : state;
                const auto book=OriginalPolymorphBookGrant(*catalog,101,1,1).value();const auto bookId=grant(target,book);const auto& bookItem=*std::ranges::find(target.items,bookId,&PersistentItem::id);
                PolymorphRequest actor{book.key};actor.actorId=target.fence.owner.id;actor.rulesetRevision=actor.mobCatalogRevision=1;actor.characterLevel=255;actor.mobLevel=5;actor.mobTemplate=101;actor.characterClass=actor.sex=1;actor.skillLevel=20;actor.mastery=1;actor.action=PolymorphAction::Use;actor.admittedByServer=actor.formAllowedByServer=true;
                const auto* p=catalog->FindUsePolicy(catalog->Find(book.key)->base.usePolicy);while(!(p->allowedClasses&(1u<<(actor.characterClass-1))))++actor.characterClass;while(!(p->allowedSexes&(1u<<(actor.sex-1))))++actor.sex;
                const auto morph=MakePolymorphRuntimeCommit(*catalog,op(),target.fence,target.storages,target.items,target.recovery,bookId,actor,0,now,0).value();apply(target,morph.commit);Check(target.recovery.polymorph.has_value(),"native original book creates actual durable target transformation");
                write(other ? "other_active" : "self_active",&target.recovery);
                const auto before=source();const auto clear=MakePolymorphCancel(*catalog,op(),target.fence,target.recovery,0,now,target.recovery.generation).value();apply(target,clear);Check(!target.recovery.polymorph && source()==before,"native target cancellation clears durable schedule without spending actor's reusable item");const auto cleared=target.recovery;apply(target,clear);Check(replay && target.recovery==cleared,"repeated clear receipt cannot recreate native transformation");relog(target);write(other ? "other_cleared" : "self_cleared",&target.recovery);
                apply(target,morph.commit);Check(replay && !target.recovery.polymorph,"old target transformation receipt cannot resurrect cancelled form");
            }
        }
        Check(source()==original,"all source state survives world effect admission and target changes");write("repeated");relog(state);write("relogged");
    }
    Check(rings==1 && cancels==1,"both original reusable world utility functions covered");store.Stop();
    std::cout<<"SOURCE_REUSABLE_UTILITY_WORKER=SUCCESS sources=2 checks="<<checks<<std::endl;
}
