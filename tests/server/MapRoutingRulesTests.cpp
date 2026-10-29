#include "../../SRC/Server/common/CommonDefines.h"
#include "../../SRC/Server/GameServer/core/MapRoutingRules.hpp"
#include <array>
#include <iostream>
#include <stdexcept>
#include <vector>
void Require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(){
 static_assert(MAP_ALLOW_LIMIT>=64,"Dungeon core requires more than 32 maps");
 for(int count:{0,1,31,32,33,63,64,65}){
  std::vector<int> maps;for(int n=0;n<count;n++)maps.push_back(100+n);
  std::array<int32_t,MAP_ALLOW_LIMIT+2> buffer;buffer.fill(-999);
  auto copied=auriga::CopyMapAllow(buffer.data()+1,MAP_ALLOW_LIMIT,maps);
  Require(copied==size_t(std::min(count,int(MAP_ALLOW_LIMIT))),"bounded number of copied maps");
  Require(buffer.front()==-999&&buffer.back()==-999,"packet neighbours are not overwritten");
  for(int n=0;n<MAP_ALLOW_LIMIT;n++)Require(buffer[n+1]==(n<count?100+n:0),"routing array and unused tail are deterministic");
 }
 std::array<int32_t,MAP_ALLOW_LIMIT+1> packet{};packet.back()=777;
 std::vector<int> dungeonMaps;for(int n=1;n<=32;n++)dungeonMaps.push_back(n);dungeonMaps.push_back(430);
 Require(auriga::CopyMapAllow(packet.data(),MAP_ALLOW_LIMIT,dungeonMaps)==33&&packet[32]==430,"33rd map is announced");
 Require(packet.back()==777,"authentication flag after maps is preserved");
 Require(auriga::CopyMapAllow(packet.data(),0,dungeonMaps)==0&&packet[0]==1,"zero-capacity copy does not write");
 Require(auriga::CopyMapAllow(nullptr,64,dungeonMaps)==0,"null output is safe");
 std::cout<<"MAP_ROUTING_RULES_PASS capacity64 map33 map430 packet_canaries zero_tail\n";
}
