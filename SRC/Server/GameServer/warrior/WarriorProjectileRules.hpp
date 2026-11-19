#pragma once
#include <algorithm>
#include <cmath>
namespace WarriorExpansion {
inline bool WithinHop(double fromX,double fromY,double targetX,double targetY,int radius)
{ return std::hypot(targetX-fromX,targetY-fromY)<=std::clamp(radius,1000,2000); }
inline int ProjectileTravel(double distance,int speed)
{ return std::clamp(int(distance/std::max(1,speed)*1000),400,5000); }
}
