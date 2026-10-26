#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
namespace CaravanQuest::Encounter {
inline constexpr std::array<uint32_t,6> Pursuers{654,655,656,657,689,690};
inline constexpr int MaxBoarders=1;
inline constexpr uint32_t BoardingCooldownMs=18000;
inline bool BoardingReady(uint32_t now,uint32_t next){return next==0||int32_t(now-next)>=0;}
inline bool CanyonStage(unsigned stage){return stage==1||stage==3;}
inline constexpr float ClimbSeconds=1.8f,LeapSeconds=1.5f,WarningSeconds=3.2f,ObstacleSeconds=10.f;
inline float ImpactLeadMetres(float speed){return std::clamp(speed*(WarningSeconds+.9f),13.f,35.f);}
inline constexpr std::array<float,5> Milestones{.12f,.32f,.49f,.66f,.82f};
inline bool Due(unsigned stage,float progress){return stage<Milestones.size()&&progress>=Milestones[stage];}
inline bool Hit(float wagonX,float wagonY,float obstacleX,float obstacleY){return std::hypot(wagonX-obstacleX,wagonY-obstacleY)<1.65f;}
inline float ImpactLane(uint32_t serial){return std::array<float,3>{0.f,2.1f,-2.1f}[(serial-1)%3];}
inline float DeckX(int slot){return slot%2?-.55f:.55f;}
inline float DeckZ(int slot){return -.35f-(slot/2)*.9f;}
inline bool Boardable(bool alive,bool travelling,float distance,int aboard){return alive&&travelling&&distance<3.2f&&aboard<MaxBoarders;}
struct Hazard {
 uint32_t id=0,created=0;float x=0,y=0,fromX=0,fromY=0;bool struck=false;
 bool Landed(uint32_t now)const{return uint32_t(now-created)>=uint32_t(WarningSeconds*1000);}
 bool Expired(uint32_t now)const{return uint32_t(now-created)>=uint32_t(ObstacleSeconds*1000);}
 bool TryHit(uint32_t now,float cartX,float cartY){if(struck||!Landed(now)||Expired(now)||!Hit(cartX,cartY,x,y))return false;struck=true;return true;}
};
}
