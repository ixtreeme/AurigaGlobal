#include <item/BookRuntime.hpp>
#include <item/AutomaticRuntime.hpp>
#include <item/RecoveryRuntime.hpp>

void ServiceCatalogWorker(const GameSqlConfig& player,const std::shared_ptr<const auriga::item::ItemCatalog>& catalog,
    const std::string& password,const std::string& path,const std::vector<auriga::item::ItemKey>& keys)
{
    using namespace auriga::item;
    CAsyncSQL sql;sql.SetTimeouts(5);sql.SetRetryConnectionErrors(false);
    Check(sql.Setup("127.0.0.1","root",password.c_str(),"player","utf8mb4",true,player.port),"source service importer connects");
    auto& store=runtime::ItemStore::Instance();Check(store.Start(player,catalog,"source-service-worker").has_value(),"actual service item worker starts");
    unsigned books=0,elixirs=0;
    for(unsigned n=0;n<keys.size();++n) {
        const auto key=keys[n];std::cout<<"SOURCE_SERVICE_CASE="<<n<<" key="<<static_cast<unsigned>(key.Category())<<":"<<key.LocalId()<<std::endl;
        const ItemOwner owner{ItemOwnerKind::Character,7120u+n};const ItemStorageLayout bag{{owner,ItemStorageKind::Inventory,1},5,9,4};
        if(key.Category()==ItemCategory::SkillBook)sql.DirectQuery(("INSERT INTO player.player(id) VALUES("+std::to_string(owner.id)+")").c_str());
        Check(persistence::ProvisionOwner(sql,*catalog,owner,std::span(&bag,1),{}).has_value(),"service owner provisioned");
        auto state=persistence::LoadCurrentItemOwner(sql,*catalog,owner).value();
        const auto op=[&](unsigned serial){return std::array<std::uint8_t,16>{0xF7,static_cast<std::uint8_t>(n),static_cast<std::uint8_t>(serial)};};
        bool replay=false;
        const auto apply=[&](const ItemCommit& commit){std::optional<runtime::CommitOutcome> got;Check(runtime::RequestCommit(owner,state.fence.sessionGeneration,commit,[&](auto out){got=std::move(out);}),"service commit queued");for(unsigned spin=0;!got && spin<1000;++spin){store.Drain();std::this_thread::sleep_for(std::chrono::milliseconds(10));}Check(got && got->write && got->state,"service commit persisted and loaded");replay=got->write->replay;state=*got->state;};
        const unsigned units=key.Category()==ItemCategory::SkillBook ? 4u : 1u;
        const auto ids=runtime::PeekInstanceIds(units);runtime::SpendInstanceIds(units);auto id=ids.front();
        apply(MakeGrantCommit(*catalog,op(1),state.fence,state.storages,state.items,{key,0,key.Category()==ItemCategory::SkillBook ? 4u : 1u},ids).value());
        std::optional<PersistentItem> lastSource;
        const auto item=[&]()->const PersistentItem&{const auto found=std::ranges::find(state.items,id,&PersistentItem::id);return found==state.items.end() ? lastSource.value() : *found;};
        const auto total=[&](){unsigned count=0;for(const auto& i:state.items)if(i.key==key)count+=i.count;return count;};
        const auto write=[&](const std::string& phase,const std::vector<std::byte>& wire) {
            const auto output=[&](const char* suffix,const std::vector<std::byte>& bytes){std::ofstream file(path+"."+std::to_string(n)+"."+phase+suffix);for(auto b:bytes){const auto v=std::to_integer<unsigned>(b);file<<"0123456789abcdef"[v>>4]<<"0123456789abcdef"[v&15];}file<<'\n';Check(file.good(),"actual service packet exported");};
            const auto inventory=EncodeTypedInventory(*catalog,state.storages,state.wallets,state.items).value();Check(DecodeTypedInventory(inventory,owner).has_value(),"service inventory decodes natively");output(".inventory.hex",inventory);output(".details.hex",EncodeTypedItemDetails(*catalog,state.items).value());output(".runtime.hex",wire);
        };
        if(const auto* profile=BookItemProfile(*catalog,key)) {
            ++books;const auto skill=profile->targets.front().skillId;auto live=state.learning.character;
            SkillBookAdmission facts;facts.characterLevel=100;facts.classIndex=0;facts.sexIndex=0;facts.empire=profile->targets.front().forbiddenEmpire%3+1;facts.alive=facts.skillSystemEnabled=facts.targetAllowedByRules=true;
            std::uint64_t now=100000;
            const auto admission=[&](){return BookRuntimeAdmission(*catalog,item(),state.learning,live,facts,now).value();};
            const auto packet=[&](unsigned status,const BookRuntimePlan* result=nullptr,SkillBookDecisions decisions={}){return EncodeBookRuntime(*catalog,{static_cast<std::uint8_t>(status),255,41,42,now,&item(),&state.learning,live,facts,"Source skill",result ? &result->result : nullptr,decisions}).value();};
            auto actor=admission();const auto quote=PrepareSkillBookUse(*catalog,key,actor);Check(quote.has_value(),"source secondary book admits");
            if(profile->targets.front().forbiddenEmpire){actor.empire=profile->targets.front().forbiddenEmpire;auto denied=PrepareSkillBookUse(*catalog,key,actor);Check(!denied && denied.error()==SkillBookError::NativeLanguage,"native language denied without book debit");}
            actor=admission();if(profile->steps.front().minimumCharacterLevel){actor.characterLevel=profile->steps.front().minimumCharacterLevel-1;Check(!PrepareSkillBookUse(*catalog,key,actor),"combo character-level gate enforced");}
            actor=admission();actor.skill->level=profile->steps.back().nextLevel;actor.skill->successfulReads=0;Check(!PrepareSkillBookUse(*catalog,key,actor),"maximum source skill level denied");
            write("bag",packet(1));
            if(quote->SuccessChanceBp()<10000) {
                lastSource=item();const SkillBookDecisions failedDecision{10000,quote->MinimumDelayMs()};const auto failed=MakeBookRuntimeCommit(*catalog,op(2),state.fence,state.storages,state.items,state.learning,live,id,facts,now,failedDecision).value();
                Check(failed.result.outcome==SkillBookOutcome::Failed,"source failed read is explicit");apply(failed.commit);live=state.learning.character;
                Check(total()==3 && live.skills[skill].level==0,"failed source read spends one book and no level");
                write("missed",packet(3,&failed,failedDecision));id=std::ranges::find(state.items,key,&PersistentItem::key)->id;
                Check(!MakeBookRuntimeCommit(*catalog,op(3),state.fence,state.storages,state.items,state.learning,live,id,facts,now,{1,quote->MinimumDelayMs()}),"source book cooldown blocks immediate reuse");now+=quote->MinimumDelayMs();
            }
            auto prepared=PrepareSkillBookUse(*catalog,key,admission()).value();
            const SkillBookDecisions decisions{static_cast<std::uint16_t>(prepared.NeedsSuccessRoll() ? 1 : 0),prepared.MinimumDelayMs()};
            lastSource=item();auto learned=MakeBookRuntimeCommit(*catalog,op(4),state.fence,state.storages,state.items,state.learning,live,id,facts,now,decisions).value();
            Check(learned.result.outcome==SkillBookOutcome::Learned,"source secondary book learns one rank");apply(learned.commit);live=state.learning.character;
            Check(live.skills[skill].level==1 && total()==(quote->SuccessChanceBp()==10000 ? 3u : 2u),"exact source book debit and rank saved");write("learned",packet(3,&learned,decisions));
            const auto before=state.learning;apply(learned.commit);Check(replay && state.learning==before,"duplicate source reading reuses receipt");
            const auto session=persistence::ClaimOwnerSession(sql,*catalog,owner).value();state=persistence::LoadItemOwner(sql,*catalog,owner,session).value();live=state.learning.character;Check(state.learning==before,"source skill rank and deadline survive relog");write("relogged",packet(3,&learned,decisions));
        } else {
            ++elixirs;const auto* automatic=AutomaticProfile(*catalog,key);Check(automatic!=nullptr,"source elixir has an automatic profile");const auto& definition=std::get<AutomaticRecovery>(automatic->effect);
            InstantRecoveryAdmission facts;facts.pools={1000,10000000,1000,10000000};facts.level=100;facts.classIndex=0;facts.sexIndex=0;facts.context=RecoveryContext::World;
            const auto packet=[&](unsigned status){return EncodeRecoveryRuntime(*catalog,{static_cast<std::uint8_t>(status),255,51,52,&item(),facts.pools,state.recovery,0,state.items}).value();};
            const auto base=[&](unsigned serial){ItemCommit c;c.operationId=op(serial);c.catalogRevision=catalog->Revision();c.catalogHash=catalog->ContentHash();c.kind=ItemOperationKind::Service;c.owners={state.fence};c.outcomeCode=0x2001;c.result={'A','U','T',1};return c;};
            const auto toggle=[&](unsigned serial){auto actor=AutomaticActor(*catalog,state.items,item(),facts,false,true);const auto plan=PlanAutomaticRecoveryToggle(*catalog,key,id,actor).value();auto commit=base(serial);Check(ChangeAutomatic(commit,state.storages,item(),plan.next).has_value(),"elixir activation write built");apply(commit);};
            Check(item().automatic->remainingRaw==definition.capacityRaw && !item().automatic->active,"source charge initializes once");write("bag",packet(1));toggle(2);Check(item().automatic->active,"source elixir activates");write("active",packet(3));
            auto commit=base(3);RecoverySchedulePlan healing{state.recovery};Check(TickAutomaticOwner(*catalog,commit,state.storages,state.items,facts,healing,1000,false,true).has_value(),"real automatic tick prepared");
            const auto spent=healing.hpRestoredRaw+healing.spRestoredRaw;Check(spent>0 && spent==700000,"source elixir restores exactly seven percent of the affected pool");apply(commit);
            Check(item().automatic->remainingRaw==definition.capacityRaw-spent,"tick charge durably debited");write("tick",packet(3));const auto after=*item().automatic;apply(commit);Check(replay && item().automatic==after,"elixir retry cannot spend charge twice");
            toggle(4);Check(!item().automatic->active,"source elixir deactivates");write("off",packet(3));
            const auto session=persistence::ClaimOwnerSession(sql,*catalog,owner).value();state=persistence::LoadItemOwner(sql,*catalog,owner,session).value();Check(item().automatic->remainingRaw==after.remainingRaw && !item().automatic->active,"elixir charge and inactive state survive relog");write("relogged",packet(1));
            auto actor=AutomaticActor(*catalog,state.items,item(),facts,false,true);actor.context=RecoveryContext::Arena;Check(!PlanAutomaticRecoveryToggle(*catalog,key,id,actor),"source elixir arena use denied");
        }
    }
    store.Stop();std::cout<<"SOURCE_SERVICE_WORKER=SUCCESS books="<<books<<" elixirs="<<elixirs<<" replay=reused relog=retained checks="<<checks<<std::endl;
}
