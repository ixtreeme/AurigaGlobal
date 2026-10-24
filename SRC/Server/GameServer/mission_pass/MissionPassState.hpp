#pragma once
#include "MissionPassConfig.hpp"
namespace AurigaMissionPass {
inline nlohmann::json SaveState(const State& s) {
    nlohmann::json result={{"account",s.account},{"season",s.season},{"xp",s.xp},
        {"sequence",s.lastSequence},{"progress",nlohmann::json::array()}};
    for(const auto& [key,p]:s.progress)
        result["progress"].push_back({key.first,key.second,p.value,p.xpAwarded,p.subjects});
    return result;
}
inline State LoadState(const Season& season,uint32_t account,const std::string& json) {
    State s;s.account=account;s.season=season.id;
    if(json.empty() || json=="null")return s;
    auto doc=nlohmann::json::parse(json);
    if(doc.at("account")!=account || doc.at("season")!=season.id)
        throw std::runtime_error("Mission Pass state identity mismatch");
    s.xp=doc.at("xp").get<uint64_t>();s.lastSequence=doc.at("sequence").get<uint64_t>();
    if(s.xp>season.MaximumXP() || doc.at("progress").size()>20000)
        throw std::runtime_error("Mission Pass state bounds");
    for(const auto& row:doc.at("progress")) {
        auto id=row.at(0).get<uint32_t>();auto period=row.at(1).get<int64_t>();
        auto m=std::find_if(season.missions.begin(),season.missions.end(),[&](const Mission& x){return x.id==id;});
        if(m==season.missions.end())continue; // retired mission, never reset other progress
        Progress p;p.value=row.at(2).get<uint32_t>();p.xpAwarded=row.at(3).get<bool>();
        p.subjects=row.at(4).get<std::set<uint32_t>>();
        if(p.value>m->goal || p.subjects.size()>256 || (p.xpAwarded && p.value!=m->goal))
            throw std::runtime_error("Mission Pass progress corruption");
        s.progress.emplace(ProgressKey{id,period},std::move(p));
    }
    return s;
}
inline std::string SqlText(const std::string& text) {
    static constexpr char digits[]="0123456789abcdef";
    std::string hex;hex.reserve(text.size()*2);
    for(unsigned char c:text){hex+=digits[c>>4];hex+=digits[c&15];}
    return text.empty()?"''":"CONVERT(0x"+hex+" USING utf8mb4)";
}
}
