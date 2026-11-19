#include "../../SRC/Server/GameServer/combat/DeathRecapModel.hpp"
#include "../../SRC/Server/GameServer/combat/DeathRecapEvents.hpp"
#include <iostream>
#include <stdexcept>
int main(){
    using namespace DeathRecap;unsigned checks=0;auto check=[&](bool value){++checks;if(!value)throw std::runtime_error("Death recap history invariant");};
    History history;Hit h;h.at=100;h.before=5000000000ULL;h.after=100;h.maxHp=h.before;h.source.name="Enemy";
    check(history.Record(h));check(history.hits.front().Amount()==4999999900ULL);
    h.after=h.before;check(!history.Record(h));h.after=h.before+1;check(!history.Record(h));h.after=0;h.maxHp=h.before-1;check(!history.Record(h));
    check(history.Capture(10100).size()==1);check(history.Capture(10101).empty());
    h.maxHp=h.before;for(unsigned i=0;i<MaxHits+4;++i){h.at=20000+i;check(history.Record(h));}
    check(history.hits.size()==MaxHits&&history.omitted==4);check(history.Capture(40000).empty()&&history.omitted==0);
    h.at=50000;check(history.Record(h));h.at=49999;check(!history.Record(h));check(history.hits.size()==1);
    check(Add(UINT64_MAX-2,10)==UINT64_MAX);check(Add(3,5)==8);
    const auto attacker=static_cast<entt::entity>(12);bool critical=true;
    {
        Events::SkillScope skill(73);Events::DamageScope hit(attacker,3);hit.Critical(critical);
        check(Events::current.skill==73&&Events::current.attacker==attacker&&*Events::current.critical&&!Events::current.dot);
        {Events::PeriodicScope periodic;Events::DamageScope fire(entt::null,5);check(Events::current.dot&&Events::current.critical==nullptr);}
        check(Events::current.attacker==attacker&&Events::current.skill==73&&*Events::current.critical&&!Events::current.dot);
    }
    check(Events::current.attacker==entt::null&&Events::current.critical==nullptr&&Events::skill==0&&!Events::periodic);
    {Events::DamageScope poison(attacker,9);check(Events::current.dot);}
    std::cout<<"DEATH_RECAP=PASS checks="<<checks<<'\n';
}
