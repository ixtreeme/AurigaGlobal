#include "CatalogSnapshotFixture.hpp"
#include <CatalogSqlReader.hpp>
#include <SQL/AsyncSQL.h>
#include <Core/Logging.hpp>
#include <chrono>
#include <future>
#include <iostream>
#include <source_location>
#include <stdexcept>
#include <thread>

namespace db = auriga::item::sql;
using namespace auriga::item;
using E = db::ReadErrorCode;
using Message = std::unique_ptr<SQLMsg>;
namespace
{
std::size_t checks = 0;
void Check(bool value, const char* why, std::source_location at = std::source_location::current()) {
    ++checks;
    if (!value) throw std::runtime_error(std::string(why) + " at " + std::to_string(at.line()));
}
Message CompletedFixture() {
    auto message = std::make_unique<SQLMsg>();
    message->bTransaction = true;
    message->transactionOutcome = SqlTransactionOutcome::Committed;
    const auto add = [&](const db::Rows& rows) {
        auto result = std::make_unique<SQLResult>(); result->bPrepared = true; result->uiNumRows = rows.size();
        for (const auto& row : rows) {
            std::vector<char*> cells; std::vector<unsigned long> lengths;
            for (const auto& cell : row) {
                if (cell) {
                    result->preparedCells.push_back(*cell);
                    cells.push_back(result->preparedCells.back().data());
                    lengths.push_back(static_cast<unsigned long>(cell->size()));
                } else { cells.push_back(nullptr); lengths.push_back(0); }
            }
            result->preparedRows.push_back(std::move(cells)); result->preparedLengths.push_back(std::move(lengths));
        }
        message->transactionResultOffsets.push_back(message->vec_pkResult.size());
        message->vec_pkResult.push_back(result.release());
    };
    for (std::size_t i=0;i<db::ReadGuardSteps;++i) add({{std::string("1")}});
    for (const auto& rows : catalog_snapshot_fixture::Snapshot().tables) add(rows);
    return message;
}
void CompletionBoundary() {
    const auto expected = BuildItemCatalog(item_catalog_fixture::Catalog()).value()->ContentHash();
    auto good = CompletedFixture();
    auto catalog = db::ReadCompleted(*good,{4,expected});
    Check(catalog && (*catalog)->Size()==item_catalog_fixture::FixtureItems,"complete prepared rows differ from catalogue");
    good.reset();
    Check((*catalog)->ContentHash()==expected,"catalogue borrowed a destroyed SQL result");
    for (const auto outcome : {SqlTransactionOutcome::NotStarted,SqlTransactionOutcome::Failed,
             SqlTransactionOutcome::RolledBack,SqlTransactionOutcome::Conflict,SqlTransactionOutcome::CommitUnknown}) {
        auto message=CompletedFixture(); message->transactionOutcome=outcome;
        auto result=db::ReadCompleted(*message,{1,{}});
        Check(!result,"unconfirmed SQL snapshot was exposed");
        if (outcome==SqlTransactionOutcome::CommitUnknown)
            Check(result.error().code==E::SnapshotUnconfirmed,"unknown COMMIT lost its diagnostic");
    }
    const auto malformed = [](auto change) {
        auto message=CompletedFixture(); change(*message);
        auto result=db::ReadCompleted(*message,{1,{}});
        Check(!result && result.error().code==E::ResultShape,"malformed SQL completion was accepted");
    };
    malformed([](auto& m){m.transactionResultOffsets.pop_back();});
    malformed([](auto& m){m.transactionResultOffsets[5]=4;});
    malformed([](auto& m){m.vec_pkResult[5]->bPrepared=false;});
    malformed([](auto& m){m.vec_pkResult[0]->uiNumRows=0;});
    malformed([](auto& m){m.vec_pkResult[1]->preparedLengths[0][0]=2;});
    malformed([](auto& m){m.vec_pkResult[2]->preparedLengths.clear();});
    malformed([](auto& m){m.vec_pkResult[3]->preparedRows[0][0]=nullptr;});
    malformed([](auto& m){m.vec_pkResult[5]->preparedLengths.clear();});
    malformed([](auto& m){m.vec_pkResult[5]->preparedRows[0].pop_back();});
    auto failed=CompletedFixture(); failed->uiSQLErrno=2013;
    Check(!db::ReadCompleted(*failed,{1,{}}),"SQL error was hidden by committed outcome");
    auto unknown=CompletedFixture(); unknown->bTransaction=false;
    Check(!db::ReadCompleted(*unknown,{1,{}}),"unowned SQL results were exposed");
}
Message Query(CAsyncSQL& connection, const std::string& query, const SqlParams& params={}) {
    Message message(connection.DirectQueryPrepared(query.c_str(),params));
    Check(message && !message->uiSQLErrno && message->Get(),"isolated SQL query failed");
    return message;
}
std::string Scalar(CAsyncSQL& connection, const std::string& query, const SqlParams& params={}) {
    auto result=Query(connection,query,params); auto row=result->Get()->FetchRow();
    auto lengths=result->Get()->FetchLengths();
    Check(row && row[0] && lengths,"expected scalar result missing");
    return {row[0],lengths[0]};
}
void Execute(CAsyncSQL& connection,const std::string& query) {
    Message message(connection.DirectQuery(query.c_str()));
    if (message && message->uiSQLErrno) std::cerr<<"fixture SQL code="<<message->uiSQLErrno<<" query="<<query<<'\n';
    Check(message && !message->uiSQLErrno,"isolated SQL setup failed");
}
std::shared_ptr<const ItemCatalog> Load(CAsyncSQL& reader, std::optional<ItemCatalogDigest> hash={}) {
    auto result=db::ReadCatalog(reader,{8,hash});
    if (!result) std::cerr<<"loader code="<<static_cast<int>(result.error().code)
        <<" sql="<<result.error().sqlError<<" step="<<result.error().failedStep<<'\n';
    Check(result.has_value(),"real SQL catalogue load failed"); return *result;
}
void Reject(CAsyncSQL& reader,E code) {
    const auto result=db::ReadCatalog(reader,{9,{}});
    if (result || result.error().code!=code) {
        if (!result) std::cerr<<"unexpected loader code="<<static_cast<int>(result.error().code)
            <<" sql="<<result.error().sqlError<<" step="<<result.error().failedStep<<'\n';
        Check(false,"bad SQL catalogue accepted or diagnostic changed");
    }
    Check(Scalar(reader,"SELECT @@autocommit")=="1","loader left a transaction open");
}
void ActualSql(CAsyncSQL& admin,CAsyncSQL& reader) {
    const auto reference=BuildItemCatalog(item_catalog_fixture::Catalog()).value();
    const auto good=Load(reader,reference->ContentHash());
    Check(good->Size()==item_catalog_fixture::FixtureItems && good->Revision()==8,"real SQL catalogue has wrong identity");
    auto wrong=good->ContentHash();wrong[0]^=1;
    auto rejected=db::ReadCatalog(reader,{9,wrong});
    Check(!rejected && rejected.error().code==E::HashMismatch,"expected hash was ignored");
    Check(good->ContentHash()==reference->ContentHash(),"failed replacement mutated last good snapshot");
    Message forbidden(reader.DirectQueryPrepared("UPDATE protos.item_base SET sell_amount=999",{}));
    Check(forbidden->uiSQLErrno!=0,"test loader account can write catalogue");

    Execute(reader,"SET SESSION TRANSACTION ISOLATION LEVEL READ COMMITTED");
    Reject(reader,E::IsolationRequired);
    Execute(reader,"SET SESSION TRANSACTION ISOLATION LEVEL REPEATABLE READ");
    Execute(admin,"UPDATE protos.catalog_format SET schema_version=5");Reject(reader,E::FormatMismatch);
    Execute(admin,"UPDATE protos.catalog_format SET schema_version=6");
    Execute(admin,"UPDATE protos.item_category SET prototype_table='wrong' WHERE category_id=1");Reject(reader,E::RegistryMismatch);
    Execute(admin,"UPDATE protos.item_category SET prototype_table='item_weapon' WHERE category_id=1");
    Execute(admin,"UPDATE protos.bonus_definition SET display_key='tampered' WHERE bonus_id=1");Reject(reader,E::RegistryMismatch);
    Execute(admin,"UPDATE protos.bonus_definition SET display_key='"+std::string(FindBonusDefinition(1)->displayKey)+"' WHERE bonus_id=1");
    Execute(admin,"DELETE FROM protos.instant_recovery_profile WHERE profile_id=1");Reject(reader,E::InvalidChild);
    Execute(admin,"INSERT INTO protos.instant_recovery_profile VALUES(1,1,300000,100000,500,1000,10000)");
    Execute(admin,"UPDATE protos.item_weapon SET ammo_policy_id=2 WHERE category_id=1 AND local_id=1");Reject(reader,E::InvalidCatalog);
    Execute(admin,"UPDATE protos.item_weapon SET ammo_policy_id=1 WHERE category_id=1 AND local_id=1");
    Execute(admin,"ALTER TABLE protos.catalog_format ENGINE=MyISAM");Reject(reader,E::FormatMismatch);
    Execute(admin,"ALTER TABLE protos.catalog_format ENGINE=InnoDB");
    Execute(admin,"INSERT INTO protos.item_use_policy SELECT 10000+seq,CONCAT('use.test_',seq),15,3,0 FROM seq_1_to_4097");
    Reject(reader,E::ByteBudget);
    Execute(admin,"DELETE FROM protos.item_use_policy WHERE policy_id>=10000");
    Load(reader,reference->ContentHash());

    Execute(admin,"START TRANSACTION");
    Execute(admin,"UPDATE protos.refinement_profile SET max_level=30 WHERE profile_id=2");
    for (unsigned level=10;level<=30;++level)
        Query(admin,"INSERT INTO protos.refinement_level(profile_id,level,physical_attack_gain_raw,magical_attack_gain_raw,extra_bonus_profile_id) VALUES(2,?,?,?,3)",
            {SqlParam::UInt(level),SqlParam::Int(10000LL*level),SqlParam::Int(4000LL*level)});
    Execute(admin,"COMMIT");
    const auto extended=Load(reader);
    for (std::uint16_t level : {9,10,15,30})
        Check(extended->RefinedBaseStats(item_catalog_fixture::Sword(),level)->physicalAttackRaw==120000+10000LL*level,"real SQL refinement kept +9 ceiling");
    Check(extended->Size()==item_catalog_fixture::FixtureItems && !extended->RefinedBaseStats(item_catalog_fixture::Sword(),31),"refinement duplicated a prototype or ignored its maximum");
    Execute(admin,"DELETE FROM protos.refinement_level WHERE profile_id=2 AND level=10");Reject(reader,E::InvalidCatalog);
    Execute(admin,"START TRANSACTION");
    Execute(admin,"DELETE FROM protos.refinement_level WHERE profile_id=2 AND level>9");
    Execute(admin,"UPDATE protos.refinement_profile SET max_level=9 WHERE profile_id=2");Execute(admin,"COMMIT");
    Load(reader,reference->ContentHash());
}
void ConcurrentSnapshot(CAsyncSQL& admin,CAsyncSQL& reader,const std::string& token) {
    const auto oldHash=Load(reader)->ContentHash();
    const auto begun="ic_begin_"+token, proceed="ic_proceed_"+token;
    Check(Scalar(admin,"SELECT GET_LOCK(?,0)",{SqlParam::Text(proceed)})=="1","barrier lock failed");
    auto plan=db::PrepareRead();
    // Test-only barrier after the first InnoDB read. Production has no callback
    // or mutable query input: ReadCatalog always uses the fixed PrepareRead plan.
    plan[2].query+=" AND GET_LOCK(?,0)=1 AND GET_LOCK(?,10)=1";
    plan[2].params.push_back(SqlParam::Text(begun));plan[2].params.push_back(SqlParam::Text(proceed));
    auto pending=std::async(std::launch::async,[&reader,plan=std::move(plan)]() mutable {
        Message result(reader.DirectTransactionPrepared(std::move(plan))); return result;
    });
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(8);
    bool arrived=false;
    while (std::chrono::steady_clock::now()<deadline) {
        auto result=Query(admin,"SELECT IS_USED_LOCK(?)",{SqlParam::Text(begun)});
        if (result->Get()->FetchRow()[0]) {arrived=true;break;}
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    Execute(admin,"START TRANSACTION");
    Execute(admin,"UPDATE protos.item_base SET sell_amount=51 WHERE category_id=1 AND local_id=1");
    Execute(admin,"UPDATE protos.item_weapon SET physical_attack_base_raw=120001 WHERE category_id=1 AND local_id=1");
    Execute(admin,"COMMIT");
    Scalar(admin,"SELECT RELEASE_LOCK(?)",{SqlParam::Text(proceed)});
    auto message=pending.get();
    Check(arrived,"snapshot test never reached deterministic read barrier");
    auto snapshot=db::ReadCompleted(*message,{10,oldHash});
    Check(snapshot.has_value(),"concurrent update tore the catalogue snapshot");
    Scalar(reader,"SELECT RELEASE_ALL_LOCKS()");
    const auto fresh=Load(reader);
    Check(fresh->ContentHash()!=oldHash && fresh->Find(item_catalog_fixture::Sword())->base.sellAmount==51 &&
        fresh->RefinedBaseStats(item_catalog_fixture::Sword(),0)->physicalAttackRaw==120001,"next read missed committed catalogue update");
    Check((*snapshot)->Find(item_catalog_fixture::Sword())->base.sellAmount==50,"old snapshot changed after publication");
    Execute(admin,"START TRANSACTION");
    Execute(admin,"UPDATE protos.item_base SET sell_amount=50 WHERE category_id=1 AND local_id=1");
    Execute(admin,"UPDATE protos.item_weapon SET physical_attack_base_raw=120000 WHERE category_id=1 AND local_id=1");Execute(admin,"COMMIT");
    Load(reader,oldHash);
}
void DisconnectedSnapshot(CAsyncSQL& admin,CAsyncSQL& reader,const std::string& token) {
    const auto good=Load(reader);
    const auto id=Scalar(reader,"SELECT CONNECTION_ID()");
    const auto begun="ic_disconnect_"+token, proceed="ic_disconnect_wait_"+token;
    Scalar(admin,"SELECT GET_LOCK(?,0)",{SqlParam::Text(proceed)});
    auto plan=db::PrepareRead();
    plan[2].query+=" AND GET_LOCK(?,0)=1 AND GET_LOCK(?,10)=1";
    plan[2].params.push_back(SqlParam::Text(begun));plan[2].params.push_back(SqlParam::Text(proceed));
    auto pending=std::async(std::launch::async,[&reader,plan=std::move(plan)]() mutable {
        return Message(reader.DirectTransactionPrepared(std::move(plan)));
    });
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(8);
    bool arrived=false;
    while (std::chrono::steady_clock::now()<deadline) {
        auto result=Query(admin,"SELECT IS_USED_LOCK(?)",{SqlParam::Text(begun)});
        if (result->Get()->FetchRow()[0]) {arrived=true;break;}
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    Execute(admin,"KILL CONNECTION "+id);
    Scalar(admin,"SELECT RELEASE_LOCK(?)",{SqlParam::Text(proceed)});
    auto message=pending.get();
    Check(arrived,"disconnect test never reached read barrier");
    Check(message->transactionOutcome!=SqlTransactionOutcome::Committed,"disconnected catalogue was reported committed");
    Check(!db::ReadCompleted(*message,{11,{}}),"disconnected partial snapshot was published");
    Check(good->ContentHash()==BuildItemCatalog(item_catalog_fixture::Catalog()).value()->ContentHash(),"disconnect mutated previous snapshot");
}
}
std::shared_ptr<spdlog::logger> logging::GetLogger() {
    static auto logger=std::make_shared<spdlog::logger>("catalog-sql-test");return logger;
}
std::shared_ptr<spdlog::logger> logging::GetErrorLogger() {return logging::GetLogger();}
int main(int argc,char** argv) {
    try {
        std::cout<<std::unitbuf;
        if (argc!=2) throw std::runtime_error("expected --completion-only or --owned-sandbox");
        CompletionBoundary();
        if (std::string_view(argv[1])=="--owned-sandbox") {
            // Only the Python harness supplies these via a private stdin pipe.
            // No host, existing schema or arbitrary connection file is accepted.
            std::string portText,password,token;
            std::getline(std::cin,portText);std::getline(std::cin,password);std::getline(std::cin,token);
            const auto port=std::stoul(portText);
            Check(port>0 && port<=65535 && password.size()>=32 && token.size()==32 &&
                token.find_first_not_of("0123456789abcdef")==token.npos,"invalid owned sandbox input");
            CAsyncSQL admin,reader;
            for (auto* connection : {&admin,&reader}) {connection->SetTimeouts(5);connection->SetRetryConnectionErrors(false);}
            Check(admin.Setup("127.0.0.1","root",password.c_str(),"protos","utf8mb4",true,static_cast<unsigned>(port)),"owned sandbox connection failed");
            Check(Scalar(admin,"SELECT token FROM protos.ic_loader_owner")==token,"owned sandbox marker differs");
            Check(reader.Setup("127.0.0.1","catalog_reader",password.c_str(),"protos","utf8mb4",true,static_cast<unsigned>(port)),"read-only catalogue connection failed");
            ActualSql(admin,reader);ConcurrentSnapshot(admin,reader,token);DisconnectedSnapshot(admin,reader,token);
        } else Check(std::string_view(argv[1])=="--completion-only","unknown test mode");
        std::cout<<"Catalogue SQL reader: "<<checks<<" checks passed\n";return 0;
    } catch (const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
