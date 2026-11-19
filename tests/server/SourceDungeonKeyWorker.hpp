#include <item/DungeonItems.hpp>
void DungeonKeyCatalogWorker(const GameSqlConfig& player,const std::shared_ptr<const auriga::item::ItemCatalog>& catalog,
    const std::string& password,const std::string& path,const std::vector<BulkSource>& sources)
{
    using namespace auriga::item;
    Check(sources.size()==3,"three actual original dungeon keys");CAsyncSQL sql;sql.SetTimeouts(5);sql.SetRetryConnectionErrors(false);
    Check(sql.Setup("127.0.0.1","root",password.c_str(),"player","utf8mb4",true,player.port),"owned dungeon-key SQL connection");
    auto& store=runtime::ItemStore::Instance();Check(store.Start(player,catalog,"source-dungeon-key-worker").has_value(),"actual dungeon-key worker starts");
    unsigned serial=0;bool replay=false;
    const auto op=[&](){++serial;return std::array<std::uint8_t,16>{0x52,static_cast<std::uint8_t>(serial),static_cast<std::uint8_t>(serial>>8)};};
    const auto apply=[&](persistence::LoadedItemOwner& state,const ItemCommit& commit){
        std::optional<runtime::CommitOutcome> got;Check(runtime::RequestCommit(state.fence.owner,state.fence.sessionGeneration,commit,[&](auto out){got=std::move(out);}),"dungeon-key operation queued");
        for(unsigned spin=0;!got && spin<1000;++spin){store.Drain();std::this_thread::sleep_for(std::chrono::milliseconds(10));}
        Check(got && got->write && got->state,"actual SQL dungeon-key operation committed");replay=got->write->replay;
        if(!replay)runtime::SpendInstanceIds(static_cast<std::size_t>(std::ranges::count_if(commit.items,[](const auto& change){return !change.before;})));
        state=std::move(*got->state);
    };
    const auto relog=[&](persistence::LoadedItemOwner& state){const auto saved=state;const auto generation=persistence::ClaimOwnerSession(sql,*catalog,state.fence.owner).value();state=persistence::LoadItemOwner(sql,*catalog,state.fence.owner,generation).value();Check(state.items==saved.items && state.wallets==saved.wallets,"fresh SQL session preserves every native dungeon-key state");};
    for(unsigned n=0;n<sources.size();++n){
        const ItemOwner owner{ItemOwnerKind::Character,19000u+n};const std::array layouts{ItemStorageLayout{{owner,ItemStorageKind::Inventory,1},5,9,4}};
        Check(persistence::ProvisionOwner(sql,*catalog,owner,layouts,{}).has_value(),"dungeon-key owner provisioned");auto state=persistence::LoadCurrentItemOwner(sql,*catalog,owner).value();
        auto grant=sources[n].grant;grant.count=n==0 ? 11 : n==1 ? 3 : 5;auto ids=runtime::PeekInstanceIds(8);Check(ids.size()==8,"native IDs reserved");apply(state,MakeGrantCommit(*catalog,op(),state.fence,state.storages,state.items,grant,ids).value());
        const auto sourceId=std::ranges::find(state.items,grant.key,&PersistentItem::key)->id;
        const auto count=[&](ItemKey key){return CountMissionItems(state.items,owner,key,0);};
        const auto write=[&](const std::string& phase){
            const auto file=[&](const char* kind,const auto& bytes){std::ofstream f(path+"."+std::to_string(n)+"."+phase+"."+kind+".hex");for(auto b:bytes){const auto v=static_cast<unsigned char>(b);f<<"0123456789abcdef"[v>>4]<<"0123456789abcdef"[v&15];}f<<'\n';Check(f.good(),"actual dungeon-key wire state exported");};
            file("inventory",EncodeTypedInventory(*catalog,state.storages,state.wallets,state.items).value());
            file("details",EncodeTypedItemDetails(*catalog,state.items,false,std::array{sources[n].grant}).value());
        };
        write("ready");RuneKeyFacts facts;facts.admitted=facts.dungeon=facts.leader=true;facts.floor=2;facts.type=1;facts.level=255;
        const auto* use=catalog->FindUsePolicy(catalog->Find(grant.key)->base.usePolicy);while(!(use->allowedClasses&(1u<<facts.characterClass)))++facts.characterClass;while(!(use->allowedSexes&(1u<<facts.sex)))++facts.sex;
        if(n==0){
            const auto source=std::ranges::find(state.items,sourceId,&PersistentItem::id);
            const auto quote=QuoteRuneKey(*catalog,*source,owner,facts);Check(quote.has_value(),"actual source rune fragment admission");
            ids=runtime::PeekInstanceIds(8);const auto planned=MakeRuneKeyCommit(*catalog,op(),state.fence,state.storages,state.items,sourceId,facts,ids);
            if(!planned)throw std::runtime_error("Actual fragment exchange planner error="+std::to_string(static_cast<unsigned>(planned.error())));
            const auto exchange=*planned;
            Check(exchange.items.size()>=2 && exchange.result.back()==1,"source cost and new native key share one SQL operation");apply(state,exchange);Check(!replay && count(grant.key)==1,"exactly ten fragments spent");write("exchanged");
            const auto saved=state.items;apply(state,exchange);Check(replay && state.items==saved,"receipt replay cannot duplicate a key or debit more fragments");write("replayed");relog(state);write("relogged");
            const auto key=DungeonItemKey(*catalog,RuneFloorKeyCode).value();const auto id=std::ranges::find(state.items,key,&PersistentItem::key)->id;
            apply(state,MakeRuneKeyCommit(*catalog,op(),state.fence,state.storages,state.items,id,facts,{}).value());Check(!count(key) && count(grant.key)==1,"exchanged key advances once and is consumed");write("advanced");
        }else if(n==1){
            auto commit=MakeRuneKeyCommit(*catalog,op(),state.fence,state.storages,state.items,sourceId,facts,{}).value();Check(commit.result.back()==2,"normal floor advancement receipt");apply(state,commit);Check(count(grant.key)==2,"advance spends one floor key");write("advanced");
            const auto nextId=std::ranges::find(state.items,grant.key,&PersistentItem::key)->id;
            facts.floor=5;facts.type=6;facts.leader=false;Check(!MakeRuneKeyCommit(*catalog,op(),state.fence,state.storages,state.items,nextId,facts,{}),"unselected/unreachable altar never consumes");facts.pedestal=true;
            commit=MakeRuneKeyCommit(*catalog,op(),state.fence,state.storages,state.items,nextId,facts,{}).value();Check(commit.result.back()==3,"real pedestal offering receipt");apply(state,commit);Check(count(grant.key)==1,"offering spends one floor key");write("offered");
            apply(state,MakeDungeonCleanupCommit(*catalog,op(),state.fence,state.storages,state.items,grant.key).value());Check(!count(grant.key),"stage cleanup removes remaining native keys");write("cleaned");relog(state);write("relogged");
        }else{
            Check(!RuneKeyItem(*catalog,grant.key),"original green-orc key is a passive dungeon token");
            apply(state,MakeDungeonCleanupCommit(*catalog,op(),state.fence,state.storages,state.items,grant.key,3).value());Check(count(grant.key)==2,"bounded original cleanup consumes exactly requested units");write("partial");
            apply(state,MakeDungeonCleanupCommit(*catalog,op(),state.fence,state.storages,state.items,grant.key).value());Check(!count(grant.key),"full cleanup leaves no native green-orc key");write("cleaned");relog(state);write("relogged");
        }
    }
    store.Stop();std::cout<<"SOURCE_DUNGEON_KEY_WORKER=SUCCESS sources=3 packets=14 checks="<<checks<<std::endl;
}
