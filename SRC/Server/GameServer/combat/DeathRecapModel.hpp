#pragma once
#include <algorithm>
#include <cstdint>
#include <deque>
#include <limits>
#include <string>
#include <vector>

namespace DeathRecap {
constexpr std::uint64_t WindowMs=10000;
constexpr std::size_t MaxHits=256;
struct Source {std::string name;std::uint32_t vid=0,race=0,level=0;bool player=false;};
struct Hit {
    std::uint64_t at=0,before=0,after=0,maxHp=0;
    Source source;
    std::uint32_t skill=0;
    std::uint8_t type=0;
    bool critical=false,dot=false;
    std::uint64_t Amount()const{return before-after;}
};
inline std::uint64_t Add(std::uint64_t a,std::uint64_t b){return a+std::min(b,std::numeric_limits<std::uint64_t>::max()-a);}
struct History {
    std::deque<Hit> hits;
    unsigned omitted=0;
    std::uint64_t overflowAt=0;
    void Prune(std::uint64_t now){
        while(!hits.empty()&&now>=hits.front().at&&now-hits.front().at>WindowMs)hits.pop_front();
        if(now>=overflowAt&&now-overflowAt>WindowMs)omitted=0;
    }
    bool Record(Hit hit){
        if(!hit.maxHp||hit.after>=hit.before||hit.before>hit.maxHp)return false;
        Prune(hit.at);
        if(!hits.empty()&&hit.at<hits.back().at)return false;
        hits.push_back(std::move(hit));
        if(hits.size()>MaxHits){overflowAt=hits.front().at;hits.pop_front();if(omitted<std::numeric_limits<unsigned>::max())++omitted;}
        return true;
    }
    std::vector<Hit> Capture(std::uint64_t now){Prune(now);return {hits.begin(),hits.end()};}
};
}
