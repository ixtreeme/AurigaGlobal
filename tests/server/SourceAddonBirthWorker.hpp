#include <map>
void AddonBirthCatalogWorker(const GameSqlConfig& player,const std::shared_ptr<const auriga::item::ItemCatalog>& catalog,
    const std::string& password,const std::string& path,const std::vector<BulkSource>& sources)
{
    using namespace auriga::item;const ItemOwner probe{ItemOwnerKind::Character,12500};const std::array bags{ItemStorageLayout{{probe,ItemStorageKind::Inventory,1},5,9,4}};
    std::map<std::pair<unsigned,unsigned>,unsigned> samples;
    const auto output=[&](unsigned n,const char* phase,const auto& layouts,const auto& wallets,const auto& items){for(unsigned kind=0;kind<2;++kind){const auto bytes=kind ? EncodeTypedItemDetails(*catalog,items).value() : EncodeTypedInventory(*catalog,layouts,wallets,items).value();std::ofstream f(path+"."+std::to_string(n)+"."+phase+(kind ? ".details.hex" : ".inventory.hex"));for(auto b:bytes){auto v=std::to_integer<unsigned>(b);f<<"0123456789abcdef"[v>>4]<<"0123456789abcdef"[v&15];}f<<'\n';Check(f.good(),"source addon packet export");}};
    for(unsigned n=0;n<sources.size();++n) {
        const auto& source=sources[n];const auto* weapon=std::get_if<ItemWeaponRow>(&catalog->Find(source.grant.key)->category);const unsigned family=weapon ? static_cast<unsigned>(weapon->subtype) : 8u;
        const std::array<std::uint8_t,16> op{0xA3,static_cast<std::uint8_t>(n),static_cast<std::uint8_t>(n>>8)};const std::array ids{ItemInstanceId::Create(13000000u+n).value()};
        const auto grant=MakeGrantCommit(*catalog,op,{probe,1,1},bags,{},source.grant,ids);Check(grant.has_value(),"every original addon state issues successfully");const auto& item=grant->items[0].after;
        Check(InstanceAttributeContributions(*catalog,item.key,item.attributes).value()==source.bonuses,"independent Python source formula matches actual native birth exactly");
        const auto retry=MakeGrantCommit(*catalog,op,{probe,1,1},bags,{},source.grant,ids);Check(retry && retry->items[0].after==item,"source birth deterministic retry");
        unsigned mask=0;for(const auto b:source.bonuses)mask|=b.bonus==BonusId::NormalDamagePercent ? 1 : 2;
        samples.try_emplace({family,mask},n);const auto profiles=catalog->BonusProfilesAt(item.key,item.refinementLevel).value();bool collision=false;for(const auto* profile:{profiles.base,profiles.refinement})if(profile)for(const auto b:profile->Data().entries)collision|=b.bonus==BonusId::NormalDamagePercent || b.bonus==BonusId::SkillDamagePercent;
        if(collision)samples.try_emplace({100+n,mask},n);output(n,"probe",bags,std::vector<ItemWallet>{},std::array{item});
    }
    std::set<unsigned> sampleIndices;for(const auto& [family,n]:samples)sampleIndices.insert(n);
    CAsyncSQL sql;sql.SetTimeouts(5);sql.SetRetryConnectionErrors(false);Check(sql.Setup("127.0.0.1","root",password.c_str(),"player","utf8mb4",true,player.port),"source addon owned SQL");auto& store=runtime::ItemStore::Instance();Check(store.Start(player,catalog,"source-addon-birth-worker").has_value(),"source addon actual worker starts");
    for(const auto n:sampleIndices) {
        const auto& source=sources[n];const ItemOwner owner{ItemOwnerKind::Character,13000u+n};const std::array layouts{ItemStorageLayout{{owner,ItemStorageKind::Inventory,1},5,9,4},ItemStorageLayout{{owner,ItemStorageKind::Equipment,1},28,1,1}};
        Check(persistence::ProvisionOwner(sql,*catalog,owner,layouts,{}).has_value(),"source addon owner provisioned");auto state=persistence::LoadCurrentItemOwner(sql,*catalog,owner).value();unsigned serial=0;bool replay=false;
        const auto op=[&](){++serial;return std::array<std::uint8_t,16>{0xA4,static_cast<std::uint8_t>(n),static_cast<std::uint8_t>(n>>8),static_cast<std::uint8_t>(serial)};};
        const auto apply=[&](const ItemCommit& commit){std::optional<runtime::CommitOutcome> got;Check(runtime::RequestCommit(owner,state.fence.sessionGeneration,commit,[&](auto out){got=std::move(out);}),"source addon commit queued");for(unsigned spin=0;!got && spin<1000;++spin){store.Drain();std::this_thread::sleep_for(std::chrono::milliseconds(10));}Check(got && got->write && got->state,"source addon saved and reloaded");replay=got->write->replay;state=*got->state;};
        const auto relog=[&](){const auto saved=state.items;const auto generation=persistence::ClaimOwnerSession(sql,*catalog,owner).value();state=persistence::LoadItemOwner(sql,*catalog,owner,generation).value();Check(state.items==saved,"source birth pair survives a new SQL owner session");};
        const auto packet=[&](const char* phase){output(n,phase,state.storages,state.wallets,state.items);};
        const auto ids=runtime::PeekInstanceIds(1);runtime::SpendInstanceIds(1);const auto grant=MakeGrantCommit(*catalog,op(),state.fence,state.storages,state.items,source.grant,ids).value();apply(grant);const auto attributes=state.items[0].attributes;packet("born");const auto saved=state.items;apply(grant);Check(replay && state.items==saved,"grant receipt replay cannot redraw or duplicate the source pair");relog();packet("relogged");
        EquipContext actor{255,0,0,1794770000000ULL};const auto* use=catalog->FindUsePolicy(catalog->Find(source.grant.key)->base.usePolicy);while(!(use->allowedClasses&(1u<<actor.characterClass)))++actor.characterClass;while(!(use->allowedSexes&(1u<<actor.sex)))++actor.sex;
        apply(MakeEquipCommit(*catalog,op(),state.fence,state.storages,state.items,ids[0],actor).value());Check(state.items[0].attributes==attributes,"equipping cannot recreate birth attributes");const auto stats=BuildEquipmentStats(*catalog,owner,state.items);Check(stats.has_value(),"source addon participates in real equipment stats");packet("equipped");
        apply(MakeUnequipCommit(*catalog,op(),state.fence,state.storages,state.items,ids[0],{ItemStorageKind::Inventory,1,20},actor).value());Check(state.items[0].attributes==attributes && std::ranges::all_of(BuildEquipmentStats(*catalog,owner,state.items)->bonuses,[](auto v){return v==0;}),"unequip removes every contribution without altering issued pair");relog();packet("moved");
    }
    store.Stop();std::cout<<"SOURCE_ADDON_BIRTH_WORKER=SUCCESS states="<<sources.size()<<" sql_samples="<<sampleIndices.size()<<" checks="<<checks<<std::endl;
}
