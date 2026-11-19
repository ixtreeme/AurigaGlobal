#include <item/BookRuntime.hpp>

void BoundBookCatalogWorker(const GameSqlConfig& player,const std::shared_ptr<const auriga::item::ItemCatalog>& catalog,
    const std::string& password,const std::string& path,const std::vector<BulkSource>& sources)
{
    using namespace auriga::item;CAsyncSQL sql;sql.SetTimeouts(5);sql.SetRetryConnectionErrors(false);
    Check(sql.Setup("127.0.0.1","root",password.c_str(),"player","utf8mb4",true,player.port),"bound book owned SQL connection");
    auto& store=runtime::ItemStore::Instance();Check(store.Start(player,catalog,"source-bound-book-worker").has_value(),"actual book item worker starts");
    unsigned reads=0,refunds=0;
    for(unsigned n=0;n<sources.size();++n){const auto key=sources[n].grant.key;const auto* profile=BookItemProfile(*catalog,key);
        Check(profile && profile->targetMode==SkillBookTargetMode::InstanceBound && profile->targets.size()==48,"original 48 main skills form the issuance pool");
        const bool forget=profile->operation==SkillBookOperation::Forget;const ItemOwner owner{ItemOwnerKind::Character,8900u+n};const std::array layouts{ItemStorageLayout{{owner,ItemStorageKind::Inventory,1},5,9,4}};
        sql.DirectQuery(("INSERT INTO player.player(id) VALUES("+std::to_string(owner.id)+")").c_str());Check(persistence::ProvisionOwner(sql,*catalog,owner,layouts,{}).has_value(),"book owner provisioned");auto state=persistence::LoadCurrentItemOwner(sql,*catalog,owner).value();
        unsigned serial=0;bool replay=false;const auto op=[&](){++serial;return std::array<std::uint8_t,16>{0xED,static_cast<std::uint8_t>(n),static_cast<std::uint8_t>(serial),static_cast<std::uint8_t>(serial>>8)};};
        const auto apply=[&](const ItemCommit& commit){std::optional<runtime::CommitOutcome> got;Check(runtime::RequestCommit(owner,state.fence.sessionGeneration,commit,[&](auto out){got=std::move(out);}),"bound book operation queued");for(unsigned spin=0;!got && spin<1000;++spin){store.Drain();std::this_thread::sleep_for(std::chrono::milliseconds(10));}Check(got && got->write && got->state,"bound book operation persisted and reloaded");replay=got->write->replay;state=*got->state;};
        const auto find=[&](ItemInstanceId id)->const PersistentItem&{const auto it=std::ranges::find(state.items,id,&PersistentItem::id);Check(it!=state.items.end(),"issued book still live");return *it;};
        const auto relog=[&](){const auto saved=state;const auto generation=persistence::ClaimOwnerSession(sql,*catalog,owner).value();state=persistence::LoadItemOwner(sql,*catalog,owner,generation).value();Check(state.items==saved.items && state.learning==saved.learning,"issued bindings and learned character survive a fresh SQL session");};
        const auto grant=[&](std::optional<std::uint16_t> skill){const auto ids=runtime::PeekInstanceIds(1);runtime::SpendInstanceIds(1);const auto commit=MakeGrantCommit(*catalog,op(),state.fence,state.storages,state.items,{key,0,1,skill},ids).value();apply(commit);const auto saved=state.items;apply(commit);Check(replay && state.items==saved,"grant replay cannot redraw an issued book");return ids.front();};
        auto random=grant({});const auto drawn=find(random).bookSkill;Check(drawn && std::ranges::any_of(profile->targets,[&](const auto& t){return t.skillId==*drawn;}),"production grant chooses only an available main skill");
        apply(MakeMoveCommit(*catalog,op(),state.fence,state.storages,state.items,random,{ItemStorageKind::Inventory,1,10}).value());relog();Check(find(random).bookSkill==drawn,"moving and relogging never redraw a book");apply(MakeConsumeCommit(*catalog,op(),state.fence,state.storages,state.items,random,1).value());
        auto live=state.learning.character;live.experience=2000000000;live.points[0]=10;
        for(const auto& target:profile->targets)live.skills[target.skillId]={static_cast<std::uint8_t>(forget ? 0 : 1),static_cast<std::uint8_t>(forget ? 5 : 20),0};
        SkillBookAdmission facts;facts.characterLevel=255;facts.empire=1;facts.alive=facts.skillSystemEnabled=facts.targetAllowedByRules=true;std::uint64_t now=100000;
        for(const auto& target:profile->targets){const auto skill=target.skillId;const auto id=grant(skill);auto before=find(id);const auto output=[&](const std::string& phase,unsigned status,const BookRuntimePlan* result=nullptr,SkillBookDecisions decisions={}){
            const auto write=[&](const char* suffix,const std::vector<std::byte>& bytes){std::ofstream f(path+"."+std::to_string(n)+"."+std::to_string(skill)+"."+phase+suffix);for(auto b:bytes){const auto v=std::to_integer<unsigned>(b);f<<"0123456789abcdef"[v>>4]<<"0123456789abcdef"[v&15];}f<<'\n';Check(f.good(),"actual bound book packet exported");};
            write(".inventory.hex",EncodeTypedInventory(*catalog,state.storages,state.wallets,state.items).value());write(".details.hex",EncodeTypedItemDetails(*catalog,state.items).value());write(".service.hex",EncodeBookRuntime(*catalog,{static_cast<std::uint8_t>(status),255,41,42,now,&before,&state.learning,live,facts,"Main skill "+std::to_string(skill),result ? &result->result : nullptr,decisions}).value());};
            auto denied=facts;denied.targetAllowedByRules=false;Check(!MakeBookRuntimeCommit(*catalog,op(),state.fence,state.storages,state.items,state.learning,live,id,denied,now,{}),"foreign class or group is rejected without a debit");
            const auto admission=BookRuntimeAdmission(*catalog,find(id),state.learning,live,facts,now).value();const auto quote=PrepareSkillBookUse(*catalog,key,admission).value();output("ready",1);
            const SkillBookDecisions decisions{static_cast<std::uint16_t>(quote.NeedsSuccessRoll() ? 1 : 0),quote.MinimumDelayMs()};const auto oldXP=live.experience;const auto oldPoints=live.points[0];
            const auto read=MakeBookRuntimeCommit(*catalog,op(),state.fence,state.storages,state.items,state.learning,live,id,facts,now,decisions).value();apply(read.commit);live=state.learning.character;
            Check(std::ranges::none_of(state.items,[&](const auto& item){return item.id==id;}),"one successful read spends exactly the issued book");
            if(forget){++refunds;Check(read.result.outcome==SkillBookOutcome::Forgotten && live.skills[skill].level==4 && live.points[0]==oldPoints+1 && live.experience==oldXP,"forget book refunds exactly one main point");}
            else{++reads;Check(read.result.outcome==SkillBookOutcome::Learned && live.skills[skill].level==21 && live.points[0]==oldPoints && live.experience+read.result.experienceToConsume==oldXP,"generic book learns the bound skill and debits exact source XP");}
            output("applied",3,&read,decisions);const auto saved=state.learning;apply(read.commit);Check(replay && state.learning==saved,"read retry cannot grant another level or refund");relog();live=state.learning.character;output("relogged",3,&read,decisions);
        }
    }
    store.Stop();std::cout<<"SOURCE_BOUND_BOOK_WORKER=SUCCESS sources="<<sources.size()<<" reads="<<reads<<" refunds="<<refunds<<" checks="<<checks<<std::endl;
}
