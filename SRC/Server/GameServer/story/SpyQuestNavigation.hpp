#pragma once
#include "SpyQuestRules.hpp"
#include <fstream>
#include <cstring>
#include <string>
namespace SpyQuest {
// Generated from Unity terrain and original MDATR bridge/blocking surfaces.
// Missing or mismatched data fails closed; never fall back to walking on water.
class Navigation {
 int width=0,height=0;std::vector<uint8_t> allowed;std::vector<int32_t> elevations;
 public:
 bool Load(const std::string& path){std::ifstream in(path,std::ios::binary);return Load(in);}
 bool Load(std::istream& in){
  width=height=0;allowed.clear();elevations.clear();char magic[8]{};int32_t w=0,h=0;
  in.read(magic,8);in.read(reinterpret_cast<char*>(&w),4);in.read(reinterpret_cast<char*>(&h),4);
  if(!in||std::memcmp(magic,"SPYNAV2\0",8)||w<=0||h<=0||w>4096||h>4096)return false;
  std::vector<uint8_t> flags(size_t(w)*h);std::vector<int32_t> levels(flags.size());
  for(size_t i=0;i<flags.size();i++){char f;in.read(&f,1);in.read(reinterpret_cast<char*>(&levels[i]),4);flags[i]=uint8_t(f);if(!in||flags[i]>1)return false;}
  if(in.peek()!=std::char_traits<char>::eof())return false;
  width=w;height=h;allowed=std::move(flags);elevations=std::move(levels);return true;
 }
 bool Walk(Cell c)const{return c.x>=0&&c.y>=0&&c.x<width&&c.y<height&&allowed[size_t(c.y)*width+c.x];}
 bool Step(Cell a,Cell b)const{return Walk(a)&&Walk(b)&&std::abs(int64_t(elevations[size_t(a.y)*width+a.x])-elevations[size_t(b.y)*width+b.x])<=100;}
 bool Ready()const{return width>0&&height>0;}
};
}
