#include <item/AbsorptionSashes.hpp>
#include <algorithm>
#include <iostream>
#include <set>
#include <stdexcept>
using namespace auriga::item;
namespace {
unsigned checks=0;
void Check(bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);}
ItemKey Key(unsigned category,unsigned local){return ItemKey::Create(category,local).value();}
AbsorptionSashProfile Profile(){const auto k=Key(19,90000);return {k,k,4,11,19,1,5,25,6000,8000000};}
void ProfilesAndBirth(){
    auto p=Profile();Check(ValidAbsorptionSashProfile(p),"source grade-four absorption profile valid");
    const auto initial=InitialAbsorptionSash(p,std::array<std::uint8_t,16>{1,2,3},100);Check(initial.percent==17 && !initial.donor,"birth agrees with independent Python SHA256 fixture");
    std::set<unsigned> values;for(unsigned i=1;i<=10000;++i){const auto s=InitialAbsorptionSash(p,std::array<std::uint8_t,16>{1,2,3},i);Check(ValidAbsorptionSashState(p,s) && s.percent>=11 && s.percent<=19,"every birth remains in original 11..19 range");values.insert(s.percent);}Check(values.size()==9,"every original grade-four birth outcome is reachable");
    for(unsigned grade=1;grade<=3;++grade){p={Key(19,grade),Key(19,grade+1),static_cast<std::uint8_t>(grade),0,0,0,0,25,10000,1000000};p.initialMinimum=p.initialMaximum=std::array<std::uint8_t,3>{1,5,10}[grade-1];p.combineMinimum=std::array<std::uint8_t,3>{5,10,12}[grade-1];p.combineMaximum=std::array<std::uint8_t,3>{5,10,19}[grade-1];Check(ValidAbsorptionSashProfile(p) && InitialAbsorptionSash(p,std::array<std::uint8_t,16>{},1).percent==p.initialMinimum,"three fixed source birth grades stay fixed");}
    p=Profile();p.initialMinimum=10;Check(!ValidAbsorptionSashProfile(p),"grade-four birth cannot silently use another range");p=Profile();p.next=Key(19,90001);Check(!ValidAbsorptionSashProfile(p),"last-grade improvement retains the first sash family");
}
void RoundingAndContributions(){
    Check(AbsorbSashComponent({BonusId::Strength,1},1)==1,"positive integer component retains original minimum one");
    Check(AbsorbSashComponent({BonusId::CritChance,500},10)==100,"percentage components round in whole source percent units");
    Check(AbsorbSashComponent({BonusId::MaximumHpFlat,15000},10)==2000,"raw source component rounds nearest with half up");
    Check(AbsorbSashComponent({BonusId::Strength,-5},25)==0 && AbsorbSashComponent({BonusId::Strength,0},25)==0,"negative and zero donor components contribute nothing");
    const auto donor=Key(1,123);SashDonorProfile profile{donor,9,399000,20000,true,true,{{BonusId::Strength,1},{BonusId::Strength,1}}};AbsorptionSashState s{25,AbsorbedSashDonor{donor,9}};s.donor->attributes[0]=BonusEntry{BonusId::MaximumHpFlat,500000};
    Check(ValidSashDonorProfile(profile),"weapon donor retains maximum attacks and separate applies");const auto result=AbsorptionSashContributions(profile,s);
    Check(result.size()==5 && result[0]==BonusEntry{BonusId::PhysicalAttackGradeBonus,101000} && result[1]==BonusEntry{BonusId::MagicalAttackGradeBonus,6000},"active source weapon maxima use nearest integer plus one");
    Check(result[2]==BonusEntry{BonusId::Strength,1} && result[3]==result[2],"round donor APPLY components before merging equal bonuses");Check(result[4]==BonusEntry{BonusId::MaximumHpFlat,125000},"copied instance attributes scale independently");
    s.donor->refinement=8;Check(AbsorptionSashContributions(profile,s).empty(),"wrong donor refinement cannot borrow another profile");
    profile={Key(2,555),0,0,0,false,false,{{BonusId::CritChance,900}}};s={25,AbsorbedSashDonor{profile.key,0}};Check(AbsorptionSashContributions(profile,s)==std::vector<BonusEntry>{{BonusId::CritChance,200}},"body donor applies remain supported without restoring disabled armor-base absorption");profile.physicalMaximumRaw=1000;Check(!ValidSashDonorProfile(profile),"body donor cannot inject a weapon base");
}
void Codec(){
    auto p=Profile();AbsorptionSashState s{19,{}};const auto empty=EncodeAbsorptionSashState(s).value();Check(empty.size()==110 && DecodeAbsorptionSashState(empty)==s,"empty native sash roundtrips fixed owned payload");s.donor=AbsorbedSashDonor{Key(1,100),9};s.donor->attributes[7]=BonusEntry{BonusId::Strength,2};auto bytes=EncodeAbsorptionSashState(s).value();Check(DecodeAbsorptionSashState(bytes)==s,"typed donor and final extra attribute slot roundtrip");
    for(std::size_t cut=0;cut<bytes.size();++cut)Check(!DecodeAbsorptionSashState(std::span(bytes).first(cut)),"all truncated sash payloads reject");bytes.push_back(0);Check(!DecodeAbsorptionSashState(bytes),"trailing sash bytes reject");bytes=empty;bytes[14]=1;Check(!DecodeAbsorptionSashState(bytes),"empty donor cannot carry hidden attributes");bytes=empty;bytes[4]=26;Check(!DecodeAbsorptionSashState(bytes),"unbounded absorption percentage rejected");s.percent=25;Check(ValidAbsorptionSashState(p,s),"source cap is a valid last-grade sash");s.percent=26;Check(!ValidAbsorptionSashState(p,s),"source cap cannot be exceeded");
}
}
int main(){try{ProfilesAndBirth();RoundingAndContributions();Codec();std::cout<<"Absorption sash state checks passed: "<<checks<<'\n';return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
