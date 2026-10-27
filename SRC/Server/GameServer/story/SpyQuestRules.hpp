#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <map>
#include <queue>
#include <vector>
namespace SpyQuest {
struct Cell {int x=0,y=0;auto operator<=>(const Cell&)const=default;};
inline int Distance(Cell a,Cell b){return static_cast<int>(std::hypot(double(a.x-b.x),double(a.y-b.y)));}
// Incremental A*: a server tick has a fixed work budget. Never cuts diagonally
// through blocked corners, including bridge entrances and narrow passages.
class RouteSearch {
 struct Node {int g=0;Cell previous;bool closed=false;};
 struct Entry {int score;Cell cell;bool operator<(const Entry& b)const{return score>b.score;}};
 std::map<Cell,Node> nodes;std::priority_queue<Entry> open;
 Cell start,goal;std::function<bool(Cell)> walk;std::function<bool(Cell,Cell)> step;
 public:
 enum class Result {Searching,Found,Failed};Result result=Result::Searching;std::vector<Cell> route;
 RouteSearch(Cell a,Cell b,std::function<bool(Cell)> canWalk,std::function<bool(Cell,Cell)> canStep={}):start(a),goal(b),walk(std::move(canWalk)),step(std::move(canStep)){
  if(!walk(a)||!walk(b)){result=Result::Failed;return;}nodes[a]={0,a,false};open.push({Distance(a,b)*10,a});
 }
 Result Step(size_t budget=512){
  while(result==Result::Searching&&budget--){
   if(open.empty()||nodes.size()>150000){result=Result::Failed;break;}
   Cell c=open.top().cell;open.pop();auto& n=nodes.at(c);if(n.closed)continue;n.closed=true;
   if(c==goal){for(Cell p=c;;p=nodes.at(p).previous){route.push_back(p);if(p==start)break;}std::reverse(route.begin(),route.end());result=Result::Found;break;}
   for(int x=-1;x<=1;x++)for(int y=-1;y<=1;y++)if(x||y){Cell p{c.x+x,c.y+y};
    if(!walk(p)||(step&&!step(c,p))||(x&&y&&(!walk({c.x+x,c.y})||!walk({c.x,c.y+y})||(step&&(!step(c,{c.x+x,c.y})||!step(c,{c.x,c.y+y})||!step({c.x+x,c.y},p)||!step({c.x,c.y+y},p))))))continue;
    int cost=n.g+(x&&y?14:10);auto it=nodes.find(p);
    if(it!=nodes.end()&&(it->second.closed||it->second.g<=cost))continue;
    nodes[p]={cost,c,false};open.push({cost+Distance(p,goal)*10,p});
   }
  }return result;
 }
};
inline int Suspicion(int current,int distance,bool inFront,bool visible){
 if(visible){
  if(distance<=2000)return std::min(100,current+(inFront?40:35));
  if(distance<=4000)return std::min(100,current+(inFront?24:18));
  if(distance<5000)return std::min(100,current+(inFront?8:5));
 }
 return std::max(0,current-5);
}
}
