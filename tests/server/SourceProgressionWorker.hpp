#include <item/SoulRuntime.hpp>
#include <item/SpecialBonusRuntime.hpp>
#include <set>

// The same planner, receipt validator and asynchronous SQL worker used by the game.
void ProgressionCatalogWorker(const GameSqlConfig& player,const std::shared_ptr<const auriga::item::ItemCatalog>& catalog,
    const std::string& password,const std::string& path,const std::vector<BulkSource>& sources,
    auriga::item::ItemKey scrollKey,auriga::item::ItemGrant costume,auriga::item::ItemGrant weapon)
{
    using namespace auriga::item;CAsyncSQL sql;sql.SetTimeouts(5);sql.SetRetryConnectionErrors(false);
    Check(sql.Setup("127.0.0.1","root",password.c_str(),"player","utf8mb4",true,player.port),"progression importer connects to owned SQL");
    auto& store=runtime::ItemStore::Instance();Check(store.Start(player,catalog,"source-progression-worker").has_value(),"actual progression item worker starts");
    unsigned souls=0,donors=0,addons=0,books=0,boxes=0,opened=0,unsupported=0;
    for(unsigned n=0;n<sources.size();++n) {
        const auto key=sources[n].grant.key;std::cout<<"PROGRESSION_SOURCE_CASE="<<n<<" key="<<static_cast<unsigned>(key.Category())<<":"<<key.LocalId()<<std::endl;
        const ItemOwner owner{ItemOwnerKind::Character,7900u+n};const std::array layouts{ItemStorageLayout{{owner,ItemStorageKind::Inventory,1},5,9,4}};
        if(key.Category()==ItemCategory::SkillBook)sql.DirectQuery(("INSERT INTO player.player(id) VALUES("+std::to_string(owner.id)+")").c_str());
        const std::array purse{ItemWallet{owner,WalletCurrency::Gold,1,0}};Check(persistence::ProvisionOwner(sql,*catalog,owner,layouts,purse).has_value(),"progression owner provisioned");auto state=persistence::LoadCurrentItemOwner(sql,*catalog,owner).value();
        unsigned serial=0;bool replay=false;
        const auto op=[&](){++serial;return std::array<std::uint8_t,16>{0xFA,static_cast<std::uint8_t>(n),static_cast<std::uint8_t>(serial),static_cast<std::uint8_t>(serial>>8)};};
        const auto apply=[&](const ItemCommit& commit){std::optional<runtime::CommitOutcome> got;Check(runtime::RequestCommit(owner,state.fence.sessionGeneration,commit,[&](auto out){got=std::move(out);}),"progression write queued");for(unsigned spin=0;!got && spin<1000;++spin){store.Drain();std::this_thread::sleep_for(std::chrono::milliseconds(10));}Check(got && got->write && got->state,"progression receipt saved and owner reloaded");replay=got->write->replay;state=*got->state;};
        const auto grant=[&](ItemGrant spec){const auto ids=runtime::PeekInstanceIds(spec.count);runtime::SpendInstanceIds(spec.count);spec.createdAtUtcMs=1794660000000ULL;const auto commit=MakeGrantCommit(*catalog,op(),state.fence,state.storages,state.items,spec,ids);Check(commit.has_value(),"real source grant");apply(*commit);return ids.front();};
        const auto find=[&](ItemInstanceId id)->const PersistentItem&{const auto it=std::ranges::find(state.items,id,&PersistentItem::id);Check(it!=state.items.end(),"expected item retained");return *it;};
        const auto relog=[&](){const auto old=state.items;const auto learning=state.learning;const auto generation=persistence::ClaimOwnerSession(sql,*catalog,owner).value();state=persistence::LoadItemOwner(sql,*catalog,owner,generation).value();Check(state.items==old && state.learning==learning,"exact progression state and learning survive new session");};
        const auto output=[&](const std::string& phase,const std::vector<std::byte>& service={}){
            const auto write=[&](const char* suffix,const std::vector<std::byte>& bytes){std::ofstream f(path+"."+std::to_string(n)+"."+phase+suffix);for(auto b:bytes){const auto v=std::to_integer<unsigned>(b);f<<"0123456789abcdef"[v>>4]<<"0123456789abcdef"[v&15];}f<<'\n';Check(f.good(),"actual progression packet exported");};
            const auto wire=EncodeTypedInventory(*catalog,state.storages,state.wallets,state.items);Check(wire && DecodeTypedInventory(*wire,owner).has_value(),"progression inventory roundtrip");write(".inventory.hex",*wire);write(".details.hex",EncodeTypedItemDetails(*catalog,state.items).value());if(!service.empty())write(".service.hex",service);
        };
        const auto reply=[&](unsigned header,unsigned status,ItemInstanceId source,std::optional<ItemInstanceId> target,unsigned action){std::vector<std::byte> wire;const auto put=[&](std::uint64_t v,unsigned bytes){for(unsigned j=0;j<bytes;++j)wire.push_back(std::byte(v>>(j*8)));};put(header,1);put(35,4);put(1,1);put(status,1);put(313,8);put(source.Value(),8);put(target ? target->Value() : 0,8);put(action,1);put(0,3);return wire;};
        auto spec=sources[n].grant;if(key.Category()==ItemCategory::SkillBook || key.Category()==ItemCategory::BonusTool)spec.count=key.Category()==ItemCategory::SkillBook ? 4 : (std::get<ItemBonusToolRow>(catalog->Find(key)->category).operation==SpecialBonusOperation::PairedAddon ? 2 : 1);
        auto id=grant(spec);
        if(key.Category()==ItemCategory::Soul) {
            ++souls;const auto& row=std::get<ItemSoulRow>(catalog->Find(key)->category);const auto& level=catalog->FindSoulProfile(*row.profile)->levels[0];SoulContext actor;actor.actor={255,0,0};
            Check(find(id).soul==SoulState{0,level.initialStrikes,0,false},"source core starts with exact charge and strike budget");output("bag");
            Check(!MakeSoulCommit(*catalog,op(),state.fence,state.storages,state.items,id,{},SoulAction::Activate,0,0,actor),"empty source cannot activate");
            apply(MakeSoulCommit(*catalog,op(),state.fence,state.storages,state.items,id,{},SoulAction::Recharge,level.chargeCapacity*level.rechargeIntervalMs,0,actor)->commit);output("charged",reply(219,0,id,{},1));
            auto on=MakeSoulCommit(*catalog,op(),state.fence,state.storages,state.items,id,{},SoulAction::Activate,0,0,actor).value();apply(on.commit);output("active",reply(219,1,id,{},1));
            SoulContext hit=actor;hit.channel=*row.channel;hit.targetIsNpc=true;hit.event=20;hit.target=100;hit.targetRevision=5;hit.damage=10000;
            auto struck=MakeSoulCommit(*catalog,op(),state.fence,state.storages,state.items,id,{},SoulAction::Strike,0,0,hit);Check(struck && struck->damage==(level.chargeCapacity/60==1 ? level.firstMultiplierBp : level.chargeCapacity/60==2 ? level.secondMultiplierBp : level.thirdMultiplierBp),"source charge tier applies exact source damage");apply(struck->commit);
            hit.targetIsNpc=false;Check(!MakeSoulCommit(*catalog,op(),state.fence,state.storages,state.items,id,{},SoulAction::Strike,0,0,hit),"source Soul has no PvP effect or debit");
            apply(MakeSoulCommit(*catalog,op(),state.fence,state.storages,state.items,id,{},SoulAction::Deactivate,0,0,actor)->commit);output("off",reply(219,1,id,{},2));
            const auto scroll=grant({scrollKey,0,2});const auto before=find(id);const auto* scrollRow=std::get_if<ItemSoulRow>(&catalog->Find(scrollKey)->category);Check(scrollRow && scrollRow->refineChanceBp>0 && scrollRow->refineChanceBp<10000,"source scroll has real probabilistic refinement");
            if(row.nextCore) {
                output("ready",reply(219,0,scroll,id,4));const auto miss=MakeSoulCommit(*catalog,op(),state.fence,state.storages,state.items,id,scroll,SoulAction::Refine,0,10000,actor).value();Check(!miss.success,"source refinement failure explicit");apply(miss.commit);Check(find(id).key==key && find(id).soul==before.soul && find(scroll).count==1,"failed source refinement spends exactly one scroll");output("missed",reply(219,8,scroll,id,4));
                const auto next=MakeSoulCommit(*catalog,op(),state.fence,state.storages,state.items,id,scroll,SoulAction::Refine,0,1,actor).value();auto forged=next.commit;forged.items[0].after.key=key;Check(!ValidateItemCommit(*catalog,forged),"receipt refuses a forged transition result");apply(next.commit);Check(find(id).key==*row.nextCore && find(id).refinementLevel==0 && find(id).soul==MakeSoulState(*catalog,*row.nextCore,0).value(),"source transition retains identity and selects next prototype state");output("refined",reply(219,1,scroll,id,4));const auto saved=state.items;apply(next.commit);Check(replay && state.items==saved,"transition retry cannot advance twice");
            } else Check(!MakeSoulCommit(*catalog,op(),state.fence,state.storages,state.items,id,scroll,SoulAction::Refine,0,1,actor) && find(scroll).count==2,"terminal source cannot spend scroll");
            relog();output("relogged");
        } else if(key.Category()==ItemCategory::BonusTool) {
            const auto& tool=std::get<ItemBonusToolRow>(catalog->Find(key)->category);const bool donor=tool.operation==SpecialBonusOperation::CostumeGrant;donor ? ++donors : ++addons;
            const auto target=grant(donor ? costume : weapon);const auto savedPayload=find(id).bonusTool;output("bag");
            if(donor) {
                std::set<BonusId> seen{savedPayload->payload->bonus};
                for(unsigned sample=0;sample<32 && seen.size()<tool.choices.size();++sample){const auto other=grant({key});const auto& saved=find(other);seen.insert(saved.bonusTool->payload->bonus);Check(saved.count==1 && saved.bonusTool->payload->valueScaled==tool.choices.front().payload.valueScaled,"each source donor gets one saved five/ten percent payload");}
                Check(seen.size()==5,"actual SQL issues every configured donor choice");relog();Check(find(id).bonusTool==savedPayload,"relog never rerolls the donor");
            }
            output("ready",reply(220,0,id,target,3));unsigned draws=0;const auto service=MakeSpecialBonusCommit(*catalog,op(),state.fence,state.storages,state.items,id,target,SpecialBonusAction::Service,0,0,{255,0,0},[&](std::uint32_t maximum){++draws;return std::optional<std::uint32_t>{maximum/2};});
            Check(service.has_value(),"actual source special service admits captured target");apply(*service);
            if(donor)Check(draws==0 && find(target).attributes.slots[5]==savedPayload->payload && std::ranges::none_of(state.items,[&](const auto& i){return i.id==id;}),"donor transfers issued payload and consumes exactly itself without new random draw");
            else Check(draws>=3 && find(id).count==1 && std::ranges::any_of(find(target).attributes.slots,[](const auto& b){return b && b->bonus==BonusId::NormalDamagePercent;}),"paired source grants original normal/skill damage bonus family and spends one unit");
            output("applied",reply(220,1,id,target,3));const auto saved=state.items;apply(*service);Check(replay && state.items==saved,"source bonus service receipt is idempotent");relog();output("relogged",reply(220,1,id,target,3));
        } else if(key.Category()==ItemCategory::SkillBook) {
            ++books;SkillBookAdmission facts;facts.characterLevel=100;facts.empire=1;facts.alive=facts.skillSystemEnabled=facts.targetAllowedByRules=true;const auto* profile=BookItemProfile(*catalog,key);Check(profile && profile->operation==SkillBookOperation::HorsePoint,"source selects horse pool without fake skill");const auto now=100000ULL;auto live=state.learning.character;
            const auto quote=BookRuntimeAdmission(*catalog,find(id),state.learning,live,facts,now).value();Check(PrepareSkillBookUse(*catalog,key,quote)->SuccessChanceBp()==5000,"source horse chance exactly fifty percent");auto low=quote;low.characterLevel=49;Check(!PrepareSkillBookUse(*catalog,key,low),"source horse minimum level enforced");auto capped=quote;capped.horse=BookHorseState{20,0,0};Check(!PrepareSkillBookUse(*catalog,key,capped),"horse unspent point cap enforced");
            std::optional<PersistentItem> usedSource;const auto total=[&](){unsigned count=0;for(const auto& item:state.items)if(item.key==key)count+=item.count;return count;};const auto packet=[&](unsigned status,const BookRuntimePlan* plan=nullptr,SkillBookDecisions decision={}){return EncodeBookRuntime(*catalog,{static_cast<std::uint8_t>(status),255,41,42,now,usedSource ? &*usedSource : &find(id),&state.learning,state.learning.character,facts,"Lovasképzés",plan ? &plan->result : nullptr,decision}).value();};
            output("bag",packet(1));usedSource=find(id);const SkillBookDecisions bad{10000,profile->steps[0].delayMinimumMs};const auto miss=MakeBookRuntimeCommit(*catalog,op(),state.fence,state.storages,state.items,state.learning,live,id,facts,now,bad).value();apply(miss.commit);Check(state.learning.character.points[2]==0 && total()==3,"horse failed read spends one and adds no point");output("missed",packet(3,&miss,bad));id=std::ranges::find(state.items,key,&PersistentItem::key)->id;usedSource=find(id);
            live=state.learning.character;const SkillBookDecisions good{1,profile->steps[0].delayMinimumMs};const auto learned=MakeBookRuntimeCommit(*catalog,op(),state.fence,state.storages,state.items,state.learning,live,id,facts,now,good).value();Check(learned.result.outcome==SkillBookOutcome::HorsePoint,"failed horse read has no cooldown and immediate retry succeeds");apply(learned.commit);Check(state.learning.character.points[2]==1 && total()==2,"source horse point saved with exact debit");output("learned",packet(3,&learned,good));
            id=std::ranges::find(state.items,key,&PersistentItem::key)->id;Check(!MakeBookRuntimeCommit(*catalog,op(),state.fence,state.storages,state.items,state.learning,state.learning.character,id,facts,now,good),"successful horse read sets source cooldown");const auto saved=state.learning;apply(learned.commit);Check(replay && state.learning==saved,"horse receipt cannot add a second point");relog();output("relogged",packet(3,&learned,good));
        } else {
            ++boxes;Check(key.Category()==ItemCategory::Container,"other new definition is a chest");output("bag");const auto valid=ValidateContainerContents(*catalog,key);if(valid)output("ready",EncodeContainerOpening(*catalog,313,&find(id),nullptr,state.fence.revision,ContainerOpeningStatus::Ready,{},{}).value());const auto ids=runtime::PeekInstanceIds(128);unsigned draws=0;
            const auto plan=MakeContainerOpeningCommit(*catalog,op(),state.fence,state.storages,state.items,state.wallets,{id,{}, {255,0,0,false},1794660000000ULL},ids,[&](auto maximum){++draws;return maximum-1;});
            if(!valid){++unsupported;Check(valid.error()==ContainerOpeningError::Unsupported && !plan && plan.error()==ContainerOpeningError::Unsupported && !draws && find(id).count==1,"unfinished random birth policy refuses before debit and RNG");output("unsupported",EncodeContainerOpening(*catalog,313,&find(id),nullptr,state.fence.revision,ContainerOpeningStatus::Unsupported,{},{}).value());}
            else {++opened;Check(plan.has_value(),"resolved chest opens through source loot table");runtime::SpendInstanceIds(ids.size());const auto before=find(id);apply(plan->commit);output("opened",EncodeContainerOpening(*catalog,313,&before,nullptr,state.fence.revision,ContainerOpeningStatus::Opened,{},plan->won).value());relog();output("relogged");}
        }
    }
    store.Stop();std::cout<<"SOURCE_PROGRESSION_WORKER=SUCCESS souls="<<souls<<" donors="<<donors<<" addons="<<addons<<" books="<<books<<" boxes="<<boxes<<" opened="<<opened<<" unsupported="<<unsupported<<" checks="<<checks<<std::endl;
}
