#include "../../SRC/Server/GameServer/core/AtlasTeleportRules.hpp"
#include <iostream>
#include <stdexcept>
void Check(bool value){if(!value)throw std::runtime_error("Atlas coordinate boundary failure");}
int main(){
    using namespace auriga::atlas;int value=0,x=0,y=0;
    Check(ParseCoordinate("0",value)&&value==0);Check(ParseCoordinate("1234",value)&&value==1234);
    for(auto text:{"","-1","+1","1x","1.5"," 1","1 ","2147483648","999999999999999999999"})Check(!ParseCoordinate(text,value));
    Check(Destination(0,0,100000,200000,125600,225600,x,y)&&x==100000&&y==200000);
    Check(Destination(255,255,100000,200000,125600,225600,x,y)&&x==125500&&y==225500);
    Check(!Destination(256,255,100000,200000,125600,225600,x,y));Check(!Destination(-1,0,100000,200000,125600,225600,x,y));
    Check(!Destination(INT32_MAX,0,100000,200000,INT32_MAX,INT32_MAX,x,y));
    Check(Fee==1000000&&CooldownSeconds==10);std::cout<<"ATLAS_TELEPORT_RULES=PASS\n";
}
