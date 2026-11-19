#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace WarriorMobility {
// Check the complete grounded segment, including its starting footprint. The
// five-centimetre spacing also covers a delayed tick that crosses several cells.
template<class ClearPosition>
inline bool TracePath(int32_t sx,int32_t sy,int32_t ex,int32_t ey,int32_t& acceptedX,int32_t& acceptedY,ClearPosition clear) {
 acceptedX=sx;acceptedY=sy;
 if(!clear(sx,sy))return false;
 const double dx=double(ex)-sx,dy=double(ey)-sy;
 const int steps=std::max(1,int(std::ceil(std::hypot(dx,dy)/5.)));
 for(int i=1;i<=steps;i++){
  const int32_t x=int32_t(std::lround(sx+dx*i/steps)),y=int32_t(std::lround(sy+dy*i/steps));
  if(!clear(x,y))return false;
  acceptedX=x;acceptedY=y;
 }
 return true;
}

// A client collision stop may only shorten its current server-authored ray. It
// cannot redirect the action, exceed elapsed travel, or address another action.
inline bool StopOnPath(int32_t sx,int32_t sy,int32_t ex,int32_t ey,int32_t x,int32_t y,double progress) {
 const double dx=double(ex)-sx,dy=double(ey)-sy,px=double(x)-sx,py=double(y)-sy,length=dx*dx+dy*dy;
 if(length<1.)return std::hypot(px,py)<=25.;
 const double t=(px*dx+py*dy)/length;
 return t>=-.01&&t<=std::min(1.01,progress+.01)&&std::hypot(px-t*dx,py-t*dy)<=25.;
}
}
