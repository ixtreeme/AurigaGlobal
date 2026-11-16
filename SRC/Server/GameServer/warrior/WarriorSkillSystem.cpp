#include "WarriorSkillSystem.hpp"
#include "WarriorProjectileRules.hpp"
#include "../command/cmd.h"
#include "../core/stdafx.h"
#include "../core/config.h"
#include "../ecs/systems/ChatSystem.hpp"
#ifndef ENABLE_WARRIOR_SKILL_EXPANSION
namespace WarriorExpansion
{
bool IsEnabled()
{
    return false;
}
bool IsCasting(entt::entity)
{
    return false;
}
bool AllowBuff(entt::entity, uint32_t)
{
    return false;
}
bool EnsureLoaded(entt::entity)
{
    return false;
}
bool IsChosen(entt::entity, uint32_t)
{
    return false;
}
bool CommitLevel(entt::entity, uint32_t, uint8_t &)
{
    return false;
}
void PersistLevel(entt::entity, uint32_t, uint8_t)
{
}
void LevelUp(entt::entity, uint32_t, uint8_t)
{
}
void PersistReadTime(entt::entity, uint32_t, time_t)
{
}
bool ReadBook(entt::entity, uint32_t, int64_t)
{
    return false;
}
bool ValidateCast(entt::entity, uint32_t, entt::entity)
{
    return false;
}
void AcceptedCast(entt::entity, uint32_t, entt::entity)
{
}
void Command(entt::entity e, const char *)
{
    ecs::ChatSystem::Send(e, CHAT_TYPE_COMMAND, "wex_disabled");
}
void SendState(entt::entity e)
{
    Command(e, "");
}
void OnGroupChanged(entt::entity) {}
bool IsServerHit(entt::entity, uint32_t) { return false; }
int ScaleHit(entt::entity, uint32_t, int damage) { return damage; }
int64_t ReduceIncoming(entt::entity, entt::entity, int64_t damage, uint8_t) { return damage; }
} // namespace WarriorExpansion
#else
#include "../combat/affect.h"
#include "../combat/battle.h"
#include "../combat/skill.h"
#include "../core/JsonFile.h"
#include "../core/locale_service.h"
#include "../core/utils.h"
#include "../ecs/Registry.hpp"
#include "../ecs/SpatialHelpers.hpp"
#include "../ecs/systems/AffectSystem.hpp"
#include "../ecs/systems/ItemSystem.hpp"
#include "../ecs/systems/MountSystem.hpp"
#include "../ecs/systems/MovementSystem.hpp"
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "../ecs/systems/PointSystem.hpp"
#include "../ecs/systems/QuestSystem.hpp"
#include "../ecs/systems/SessionSystem.hpp"
#include "../ecs/systems/SkillSystem.hpp"
#include "../ecs/systems/SocialSystem.hpp"
#include "../entity/char_manager.h"
#include "../network/db.h"
#include "../network/desc.h"
#include "../quest/questmanager.h"
#include "../world/event.h"
#include <cmath>
#include <limits>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_set>
#include <unordered_map>
#include <vector>
namespace WarriorExpansion
{
namespace
{
struct Row
{
    uint16_t skill = 0, reads = 0;
    uint8_t level = 1, spent = 0;
    time_t nextRead = 0;
};
struct Runtime
{
    bool loaded = false, loading = false;
    std::array<Row, 5> rows{};
    uint64_t serial = 0;
    uint32_t castingUntil = 0, lastCommand = 0;
    std::unordered_map<uint32_t,uint32_t> fields;
    bool ground = false;
    int32_t x = 0, y = 0;
};
struct Balance
{
    int range = 0, radius = 0, maxTargets = 1, windup = 450, speed = 1800;
    bool homing = false;
    int tickMs = 0;
    std::string behavior;
};
std::array<Balance, SkillCount> balance{};
bool checked = false, enabled = false;
Runtime *State(entt::entity e)
{
    return g_registry.valid(e) ? &g_registry.get_or_emplace<Runtime>(e) : nullptr;
}
Row *Find(entt::entity e, uint32_t id)
{
    auto *s = State(e);
    if (!s)
        return nullptr;
    for (auto &r : s->rows)
        if (r.skill == id)
            return &r;
    return nullptr;
}
Choices Selected(const Runtime &s)
{
    Choices a{};
    for (size_t i = 0; i < a.size(); ++i)
        a[i] = s.rows[i].skill;
    return a;
}
void Notice(entt::entity e, const char *key)
{
    ecs::ChatSystem::Send(e, CHAT_TYPE_COMMAND, "wex_notice %s", key);
}
bool SqlOk(const std::unique_ptr<SQLMsg> &msg)
{
    return msg && !msg->uiSQLErrno && msg->Get();
}
uint32_t Pid(entt::entity e)
{
    return ecs::PlayerRuntime::GetPlayerID(e);
}
uint32_t Weapon(entt::entity e)
{
    auto item = ItemSystem::GetWearItem(e, WEAR_WEAPON);
    return ItemSystem::IsValidItem(item) ? ItemSystem::GetItemID(item) : 0;
}
bool WeaponAllowed(entt::entity e)
{
    auto item = ItemSystem::GetWearItem(e, WEAR_WEAPON);
    if (!ItemSystem::IsValidItem(item) || ItemSystem::GetItemType(item) != ITEM_WEAPON) return false;
    const auto weapon = ItemSystem::GetItemSubType(item);
    switch (ecs::PlayerRuntime::GetJob(e)) {
        case 0: return weapon == WEAPON_SWORD || weapon == WEAPON_TWO_HANDED;
        case 1: return weapon == WEAPON_SWORD || weapon == WEAPON_DAGGER || weapon == WEAPON_BOW;
        case 2: return weapon == WEAPON_SWORD;
        case 3: return weapon == WEAPON_BELL || weapon == WEAPON_FAN;
        default: return false;
    }
}
bool Alive(entt::entity e)
{
    return ecs::PlayerRuntime::IsPC(e) && ecs::PlayerRuntime::GetDesc(e) && ecs::PlayerRuntime::GetHP(e) > 0 &&
           ecs::MovementSystem::CanMove(e) && !AffectSystem::IsPolymorphed(e) && !MountSystem::IsRiding(e) &&
           !ecs::PlayerRuntime::IsObserverMode(e);
}
double Distance(int32_t ax, int32_t ay, int32_t bx, int32_t by)
{
    return std::hypot(double(ax) - bx, double(ay) - by);
}
bool PointAllowed(entt::entity e, int32_t x, int32_t y, int range)
{
    auto map = ecs::PlayerRuntime::GetMapIndex(e);
    return Distance(ecs::PlayerRuntime::GetX(e), ecs::PlayerRuntime::GetY(e), x, y) <= range &&
           ecs::IsMovablePosition(map, x, y) && !ecs::IsAttrAt(map, x, y, ATTR_BANPK);
}
bool TargetAllowed(entt::entity e, entt::entity target, int range)
{
    if (!ecs::IsCharacter(target) || target == e)
        return false;
    ImpactGate gate;
    gate.alive = ecs::PlayerRuntime::GetHP(e) > 0 && ecs::PlayerRuntime::GetHP(target) > 0;
    gate.connected = ecs::PlayerRuntime::GetDesc(e) != nullptr;
    gate.sameMap = ecs::PlayerRuntime::GetMapIndex(e) == ecs::PlayerRuntime::GetMapIndex(target);
    gate.targetValid = battle_is_attackable(e, target) &&
                       ecs::IsMovablePosition(ecs::PlayerRuntime::GetMapIndex(e), ecs::PlayerRuntime::GetX(target),
                                              ecs::PlayerRuntime::GetY(target)) &&
                       !ecs::IsAttrAt(ecs::PlayerRuntime::GetMapIndex(e), ecs::PlayerRuntime::GetX(target),
                                      ecs::PlayerRuntime::GetY(target), ATTR_BANPK);
    gate.distance = Distance(ecs::PlayerRuntime::GetX(e), ecs::PlayerRuntime::GetY(e), ecs::PlayerRuntime::GetX(target),
                             ecs::PlayerRuntime::GetY(target));
    gate.range = range;
    return CanImpact(gate);
}
const Balance &Tuning(uint32_t id)
{
    return balance[id - FirstSkill];
}
void CastMessage(entt::entity e, const char *command, uint32_t id, uint32_t target, int32_t x, int32_t y, int delay,
                 int travel, int duration)
{
    auto vid = ecs::PlayerRuntime::GetPacketVID(e);
    ecs::ChatSystem::Broadcast(e, CHAT_TYPE_COMMAND, "%s %u %u %u %d %d %d %d %d", command, vid, id, target, x, y,
                               delay, travel, duration);
}
struct HitContext { entt::entity caster=entt::null; uint32_t skill=0; double scale=1; };
thread_local HitContext hit;
struct HitScope {
    HitContext previous;
    HitScope(entt::entity e,uint32_t id,double scale):previous(hit) { hit={e,id,scale}; }
    ~HitScope() { hit=previous; }
};
bool Friendly(entt::entity e,entt::entity target,int range)
{
    if(!ecs::PlayerRuntime::IsPC(target) || ecs::PlayerRuntime::GetHP(target)<=0 ||
       ecs::PlayerRuntime::IsObserverMode(target) || ecs::PlayerRuntime::GetMapIndex(e)!=ecs::PlayerRuntime::GetMapIndex(target)) return false;
    bool ally=e==target || (ecs::PlayerRuntime::GetEmpire(e)==ecs::PlayerRuntime::GetEmpire(target) && !battle_is_attackable(e,target));
    return ally && Distance(ecs::PlayerRuntime::GetX(e),ecs::PlayerRuntime::GetY(e),ecs::PlayerRuntime::GetX(target),ecs::PlayerRuntime::GetY(target))<=range;
}
std::vector<entt::entity> Targets(entt::entity e,int32_t x,int32_t y,int radius,int casterRange)
{
    std::vector<entt::entity> result;
    std::unordered_set<uint32_t> seen;
    ecs::ForEachAround(g_registry,e,[&](entt::entity target) {
        if(!TargetAllowed(e,target,casterRange) || Distance(x,y,ecs::PlayerRuntime::GetX(target),ecs::PlayerRuntime::GetY(target))>radius) return;
        if(seen.insert(ecs::PlayerRuntime::GetPacketVID(target)).second) result.push_back(target);
    });
    std::sort(result.begin(),result.end(),[&](entt::entity a,entt::entity b) {
        const double da=Distance(x,y,ecs::PlayerRuntime::GetX(a),ecs::PlayerRuntime::GetY(a));
        const double db=Distance(x,y,ecs::PlayerRuntime::GetX(b),ecs::PlayerRuntime::GetY(b));
        return da==db ? ecs::PlayerRuntime::GetPacketVID(a)<ecs::PlayerRuntime::GetPacketVID(b) : da<db;
    });
    return result;
}
bool TargetFrom(entt::entity e,entt::entity target,int32_t x,int32_t y,int radius)
{
    return TargetAllowed(e,target,std::numeric_limits<int>::max()) &&
        WithinHop(x,y,ecs::PlayerRuntime::GetX(target),ecs::PlayerRuntime::GetY(target),radius);
}
std::vector<entt::entity> HopTargets(entt::entity e,int32_t x,int32_t y,int radius)
{
    std::vector<entt::entity> result;
    auto collect=[&](entt::entity target){if(TargetFrom(e,target,x,y,radius))result.push_back(target);};
    if(auto *sector=ecs::SectorAt(ecs::PlayerRuntime::GetMapIndex(e),x,y))sector->ForEachAround(collect);
    std::sort(result.begin(),result.end(),[&](entt::entity a,entt::entity b){
        auto da=Distance(x,y,ecs::PlayerRuntime::GetX(a),ecs::PlayerRuntime::GetY(a));
        auto db=Distance(x,y,ecs::PlayerRuntime::GetX(b),ecs::PlayerRuntime::GetY(b));
        return da==db?ecs::PlayerRuntime::GetPacketVID(a)<ecs::PlayerRuntime::GetPacketVID(b):da<db;
    });
    result.erase(std::unique(result.begin(),result.end()),result.end());return result;
}
void ProjectileHit(entt::entity e,uint32_t id,entt::entity target,int32_t x,int32_t y,int radius)
{
    if(!TargetFrom(e,target,x,y,radius))return;
    HitScope scope(e,id,1);SkillSystem::ComputeSkill(e,id,target);
}
void HitTarget(entt::entity e,uint32_t id,entt::entity target,double scale)
{
    if(!TargetAllowed(e,target,Tuning(id).range)) return;
    HitScope scope(e,id,scale);
    SkillSystem::ComputeSkill(e,id,target);
}
void Bolt(entt::entity e,uint32_t id,entt::entity target,int32_t x,int32_t y,int travel)
{
    // A chain may leave the caster's view. Notify viewers around both ends, plus its owner.
    std::unordered_set<uint32_t> sent;
    auto emit=[&](entt::entity viewer) {
        if(!ecs::PlayerRuntime::IsPC(viewer)||!ecs::PlayerRuntime::GetDesc(viewer)||
           ecs::PlayerRuntime::GetMapIndex(viewer)!=ecs::PlayerRuntime::GetMapIndex(e)||
           !sent.insert(ecs::PlayerRuntime::GetPacketVID(viewer)).second)return;
        ecs::ChatSystem::Send(viewer,CHAT_TYPE_COMMAND,"wex_bolt %u %u %u %d %d %d %d %d",
            ecs::PlayerRuntime::GetPacketVID(e),id,ecs::PlayerRuntime::GetPacketVID(target),
            ecs::PlayerRuntime::GetX(target),ecs::PlayerRuntime::GetY(target),x,y,travel);
    };
    emit(e);
    if(auto *sector=ecs::SectorAt(ecs::PlayerRuntime::GetMapIndex(e),x,y))sector->ForEachAround(emit);
    ecs::ForEachAround(g_registry,target,emit);
}
EVENTINFO(CastEvent)
{
    entt::entity caster=entt::null,target=entt::null;
    uint32_t pid=0,skill=0,weapon=0;
    int32_t map=0,x=0,y=0,originX=0,originY=0;
    uint64_t serial=0;
    bool launched=false,started=false;
    int travel=0,range=0,pulse=0,pulses=0,seconds=0;
    std::vector<entt::entity> visited;
};
void FieldMessage(const CastEvent &i,int milliseconds)
{
    CastMessage(i.caster,"wex_field",i.skill,0,ecs::PlayerRuntime::GetX(i.caster),ecs::PlayerRuntime::GetY(i.caster),
        Tuning(i.skill).radius,Tuning(i.skill).tickMs,milliseconds);
}
EVENTFUNC(VolleyImpact)
{
    auto *i=dynamic_cast<CastEvent*>(event->info); if(!i)return 0;
    auto e=i->caster;
    if(!g_registry.valid(e)||Pid(e)!=i->pid||!Alive(e)||Weapon(e)!=i->weapon||!IsChosen(e,i->skill)||ecs::PlayerRuntime::GetMapIndex(e)!=i->map)return 0;
    if(TargetFrom(e,i->target,i->x,i->y,i->range)) {
        ProjectileHit(e,i->skill,i->target,i->x,i->y,i->range);
        CastMessage(e,"wex_hit",i->skill,ecs::PlayerRuntime::GetPacketVID(i->target),ecs::PlayerRuntime::GetX(i->target),ecs::PlayerRuntime::GetY(i->target),0,0,0);
    }
    return 0;
}
EVENTFUNC(MeteorImpact)
{
    auto *i=dynamic_cast<CastEvent*>(event->info);if(!i)return 0;
    auto e=i->caster;
    if(!g_registry.valid(e)||Pid(e)!=i->pid||!Alive(e)||Weapon(e)!=i->weapon||!IsChosen(e,i->skill)||ecs::PlayerRuntime::GetMapIndex(e)!=i->map)return 0;
    for(auto target:Targets(e,i->x,i->y,180,Tuning(i->skill).range))
        HitTarget(e,i->skill,target,1.5/(i->pulses*3));
    return 0;
}
EVENTFUNC(Impact)
{
    auto *info=dynamic_cast<CastEvent*>(event->info); if(!info)return 0;
    const auto e=info->caster; const auto id=info->skill;
    auto *state=g_registry.valid(e)?g_registry.try_get<Runtime>(e):nullptr;
    if(!state || !state->loaded || (state->serial!=info->serial&&!info->launched) || Pid(e)!=info->pid ||
       !Alive(e) || !IsChosen(e,id) || Weapon(e)!=info->weapon || !WeaponAllowed(e) || ecs::PlayerRuntime::GetMapIndex(e)!=info->map) {
        if(state&&Pid(e)==info->pid) {
            CastMessage(e,"wex_cancel",id,0,info->x,info->y,0,0,0);
            state->fields.erase(id);
        }
        return 0;
    }
    const auto &cfg=Tuning(id);
    if(!info->launched) {
        if(Distance(ecs::PlayerRuntime::GetX(e),ecs::PlayerRuntime::GetY(e),info->originX,info->originY)>60) {
            CastMessage(e,"wex_cancel",id,0,info->x,info->y,0,0,0);return 0;
        }
        info->launched=true;
        if(info->travel>0 && id!=195 && id!=196)return std::max<int32_t>(1,PASSES_PER_SEC(info->travel/1000.0));
    }
    if(IsBuff(id)) {
        const auto target=info->target;
        if(!Friendly(e,target,CanShare(id)?cfg.range:0))return 0;
        if(id<194) SkillSystem::ComputeSkill(e,id,target);
        else {
            const double p=std::clamp(SkillSystem::GetSkillPower(e,id)/125.0,0.0,1.0);
            const double value=(id==200?5:id==204?6:20)*p;
            const int seconds=int((id>=202?120:60)+(id>=202?480:240)*p);
            const uint8_t point=id==194?POINT_ATT_SPEED:id==201?POINT_MOV_SPEED:id==202?POINT_ATTBONUS_METIN:
                id==203?POINT_ATTBONUS_BOSS:id==204?POINT_ATTBONUS_HUMAN:POINT_NONE;
            if(!AffectSystem::AddAffect(target,BuffAffect(id),point,value,0,seconds,0,true))return 0;
        }
        if(!AffectSystem::FindAffect(target,BuffAffect(id)))return 0;
        CastMessage(e,"wex_impact",id,ecs::PlayerRuntime::GetPacketVID(target),ecs::PlayerRuntime::GetX(target),ecs::PlayerRuntime::GetY(target),0,0,0);
        return 0;
    }
    if(id==184) {
        double dx=info->x-info->originX,dy=info->y-info->originY,length=std::hypot(dx,dy);
        if(length<1)return 0;
        const double ux=dx/length,uy=dy/length;
        // Clamp the visible and damaging corridor at the first wall or safe-area boundary.
        int reach=0;
        for(int d=50;d<=info->range;d+=50) {
            const int32_t x=int32_t(info->originX+ux*d),y=int32_t(info->originY+uy*d);
            if(!ecs::IsMovablePosition(info->map,x,y)||ecs::IsAttrAt(info->map,x,y,ATTR_BANPK))break;
            reach=d;
        }
        for(auto target:Targets(e,info->originX,info->originY,info->range,info->range))
            if(InsideBeam(ecs::PlayerRuntime::GetX(target)-info->originX,ecs::PlayerRuntime::GetY(target)-info->originY,ux,uy,reach,cfg.radius))HitTarget(e,id,target,1);
        CastMessage(e,"wex_beam",id,0,int32_t(info->originX+ux*reach),int32_t(info->originY+uy*reach),cfg.radius,reach,2000);
        return 0;
    }
    if(id==195) {
        if(!info->started){info->x=info->originX;info->y=info->originY;}
        // Validate arrival against the launch point of this hop, not against the caster.
        const bool reached=info->started && (info->visited.size()==1 ?
            TargetAllowed(e,info->target,info->range) : TargetFrom(e,info->target,info->x,info->y,cfg.radius));
        if(info->started && !reached)return 0;
        if(info->started && ecs::IsCharacter(info->target)) {
            info->x=ecs::PlayerRuntime::GetX(info->target);info->y=ecs::PlayerRuntime::GetY(info->target);
        }
        if(reached) {
            const auto vid=ecs::PlayerRuntime::GetPacketVID(info->target);
            // First cast range is validated above; subsequent hops are capped at 20 m.
            HitScope scope(e,id,1);SkillSystem::ComputeSkill(e,id,info->target);
            CastMessage(e,"wex_hit",id,vid,info->x,info->y,0,0,0);
        }
        if(int(info->visited.size())>=TargetCount(id,SkillSystem::GetSkillLevel(e,id)))return 0;
        auto candidates=info->started?HopTargets(e,info->x,info->y,cfg.radius):std::vector<entt::entity>{};
        entt::entity next=entt::null;
        if(!info->started && TargetAllowed(e,info->target,info->range))next=info->target;
        else for(auto candidate:candidates)if(std::find(info->visited.begin(),info->visited.end(),candidate)==info->visited.end()){next=candidate;break;}
        if(next==entt::null)return 0;
        info->target=next;info->visited.push_back(next);info->started=true;
        int travel=ProjectileTravel(Distance(info->x,info->y,ecs::PlayerRuntime::GetX(next),ecs::PlayerRuntime::GetY(next)),cfg.speed);
        Bolt(e,id,next,info->x,info->y,travel);
        return std::max<int32_t>(1,PASSES_PER_SEC(travel/1000.0));
    }
    if(id==196) {
        if(!TargetAllowed(e,info->target,info->range))return 0;
        auto targets=HopTargets(e,info->x,info->y,cfg.radius);
        targets.erase(std::remove(targets.begin(),targets.end(),info->target),targets.end());targets.insert(targets.begin(),info->target);
        targets.resize(std::min<size_t>(targets.size(),TargetCount(id,SkillSystem::GetSkillLevel(e,id))));
        for(auto target:targets) {
            auto *bolt=AllocEventInfo<CastEvent>();
            bolt->caster=e;bolt->target=target;bolt->pid=info->pid;bolt->skill=id;bolt->weapon=info->weapon;bolt->map=info->map;
            bolt->x=info->x;bolt->y=info->y;bolt->range=cfg.radius;
            int travel=ProjectileTravel(Distance(info->originX,info->originY,ecs::PlayerRuntime::GetX(target),ecs::PlayerRuntime::GetY(target)),cfg.speed);
            Bolt(e,id,target,info->originX,info->originY,travel);
            event_create(VolleyImpact,bolt,std::max<int32_t>(1,PASSES_PER_SEC(travel/1000.0)));
        }
        return 0;
    }
    if(IsSelfArea(id)) {
        if(cfg.tickMs) {
            if(!info->started) {
                info->started=true;info->seconds=EffectDuration(id,SkillSystem::GetSkillLevel(e,id));
                info->pulses=info->seconds*1000/cfg.tickMs;
                state->fields[id]=get_dword_time()+info->seconds*1000;
                FieldMessage(*info,info->seconds*1000);
                return std::max<int32_t>(1,PASSES_PER_SEC((id==199?500:cfg.tickMs)/1000.0));
            }
            if(!state->fields.count(id))return 0;
        }
        info->x=ecs::PlayerRuntime::GetX(e);info->y=ecs::PlayerRuntime::GetY(e);
        // Total budget is 1.5 normal hits, divided over all pulses; defenses remain native.
        const double scale=cfg.tickMs?1.5/info->pulses:1.0;
        if(id==199) {
            for(int n=0;n<3;++n) {
                const double angle=(info->pulse*2.399963+n*2.094395+info->serial*.3);
                const double distance=n==0?0:cfg.radius*.64;
                const int32_t x=int32_t(info->x+std::cos(angle)*distance),y=int32_t(info->y+std::sin(angle)*distance);
                if(!ecs::IsMovablePosition(info->map,x,y)||ecs::IsAttrAt(info->map,x,y,ATTR_BANPK))continue;
                auto *meteor=AllocEventInfo<CastEvent>();
                meteor->caster=e;meteor->pid=info->pid;meteor->weapon=info->weapon;meteor->map=info->map;meteor->skill=id;
                meteor->x=x;meteor->y=y;meteor->pulses=info->pulses;
                CastMessage(e,"wex_meteor",id,0,x,y,180,500,0);
                event_create(MeteorImpact,meteor,std::max<int32_t>(1,PASSES_PER_SEC(.5)));
            }
        } else {
            for(auto target:Targets(e,info->x,info->y,cfg.radius,cfg.range))HitTarget(e,id,target,scale);
            CastMessage(e,"wex_pulse",id,0,info->x,info->y,cfg.radius,cfg.tickMs,0);
        }
        if(cfg.tickMs && ++info->pulse<info->pulses)return std::max<int32_t>(1,PASSES_PER_SEC(cfg.tickMs/1000.0));
        state->fields.erase(id);
        if(cfg.tickMs)FieldMessage(*info,0);
        return 0;
    }
    if(IsArea(id)) {
        if(!PointAllowed(e,info->x,info->y,cfg.range))return 0;
        auto targets=Targets(e,info->x,info->y,cfg.radius,cfg.range);
        targets.resize(std::min<size_t>(targets.size(),cfg.maxTargets));
        for(auto target:targets)HitTarget(e,id,target,1);
    } else {
        if(!TargetAllowed(e,info->target,info->range))return 0;
        if(!cfg.homing&&Distance(ecs::PlayerRuntime::GetX(info->target),ecs::PlayerRuntime::GetY(info->target),info->x,info->y)>180)return 0;
        HitTarget(e,id,info->target,1);
    }
    CastMessage(e,"wex_impact",id,ecs::IsCharacter(info->target)?ecs::PlayerRuntime::GetPacketVID(info->target):0,info->x,info->y,0,0,0);
    return 0;
}
void Visuals(entt::entity viewer)
{
    ecs::ChatSystem::Send(viewer, CHAT_TYPE_COMMAND, "wex_buffs_begin");
    std::unordered_set<uint32_t> seen;
    auto emit = [&](entt::entity other) {
        if (!ecs::PlayerRuntime::IsPC(other) ||
            ecs::PlayerRuntime::GetMapIndex(other) != ecs::PlayerRuntime::GetMapIndex(viewer) ||
            !seen.insert(ecs::PlayerRuntime::GetPacketVID(other)).second)
            return;
        for (uint32_t id = FirstSkill; id <= LastSkill; ++id)
            if (IsBuff(id)) if (const auto *affect = AffectSystem::FindAffect(other, BuffAffect(id)))
                ecs::ChatSystem::Send(viewer, CHAT_TYPE_COMMAND, "wex_buff %u %u %d",
                                      ecs::PlayerRuntime::GetPacketVID(other), id, std::max(0, affect->lDuration));
        if(auto *state=g_registry.try_get<Runtime>(other))
            for(const auto &[id,end]:state->fields) {
                const int remaining=int32_t(end-get_dword_time());
                if(remaining>0)
                    ecs::ChatSystem::Send(viewer,CHAT_TYPE_COMMAND,"wex_field %u %u 0 %d %d %d %d %d",
                        ecs::PlayerRuntime::GetPacketVID(other),id,ecs::PlayerRuntime::GetX(other),ecs::PlayerRuntime::GetY(other),
                        Tuning(id).radius,Tuning(id).tickMs,remaining);
            }
    };
    emit(viewer);
    ecs::ForEachAround(g_registry, viewer, emit);
    ecs::ChatSystem::Send(viewer, CHAT_TYPE_COMMAND, "wex_buffs_end");
}
} // namespace
bool IsServerHit(entt::entity e,uint32_t id) { return hit.caster==e && hit.skill==id && IsSkill(id); }
int ScaleHit(entt::entity e,uint32_t id,int damage)
{ return IsServerHit(e,id) ? std::max(0,int(damage*hit.scale)) : damage; }
int64_t ReduceIncoming(entt::entity victim,entt::entity attacker,int64_t damage,uint8_t type)
{
    if(!IsEnabled() || damage<=0 || !ecs::IsCharacter(attacker) || attacker==victim ||
       (type!=DAMAGE_TYPE_NORMAL && type!=DAMAGE_TYPE_NORMAL_RANGE && type!=DAMAGE_TYPE_MELEE && type!=DAMAGE_TYPE_RANGE && type!=DAMAGE_TYPE_MAGIC &&
        type!=DAMAGE_TYPE_FIRE && type!=DAMAGE_TYPE_ICE && type!=DAMAGE_TYPE_ELEC)) return damage;
    if(const auto *affect=AffectSystem::FindAffect(victim,BuffAffect(200)))
        return int64_t(damage*(1-std::clamp(double(affect->lApplyValue),0.0,5.0)/100.0));
    return damage;
}
bool IsEnabled()
{
    if (checked)
        return enabled;
    checked = true;
    nlohmann::json config;
    if (!JsonFile::Load(LocaleService_GetBasePath() + "/warrior_skills.json", config))
        return false;
    try
    {
        if (!config.at("enabled").get<bool>())
            return false;
        std::array<bool, SkillCount> seen{};
        for (const auto &row : config.at("skills"))
        {
            auto id = row.at("id").get<uint32_t>();
            if (!IsSkill(id) || seen[id - FirstSkill])
                throw std::runtime_error("Duplicate skill id");
            seen[id - FirstSkill] = true;
            Balance b;
            b.range = row.value("range", 0);
            b.radius = row.value("radius", 0);
            b.maxTargets = row.value("maxTargets", 1);
            b.windup = row.value("windupMs", 450);
            b.speed = row.value("speedCmPerSecond", 1800);
            b.homing = row.value("homing", false);
            b.tickMs = row.value("tickMs", 0);
            b.behavior = row.value("behavior", "");
            if (b.range < 0 || b.range > 4000 || b.radius < 0 || b.radius > ((id==195||id==196)?2000:1000) || b.maxTargets < 1 ||
                b.maxTargets > 127 || b.windup < 150 || b.windup > 2000 || b.speed < 0 || b.speed > 10000 ||
                (b.tickMs && (b.tickMs<250 || b.tickMs>2000)) || (!IsBuff(id) && b.range < 200))
                throw std::runtime_error("Unsafe balance value");
            balance[id - FirstSkill] = b;
            auto *proto = CSkillManager::instance().Get(id);
            if (!proto)
                throw std::runtime_error("Missing skill_proto");
            proto->dwTargetRange = b.range;
            proto->iSplashRange = b.radius;
            proto->lMaxHit = b.maxTargets;
            // Target enumeration belongs exclusively to the authoritative expansion event.
            proto->dwFlag &= ~SKILL_FLAG_SPLASH;
        }
        if (std::find(seen.begin(), seen.end(), false) != seen.end())
            throw std::runtime_error("Incomplete configuration");
        enabled = true;
        LOG_INFO("WARRIOR_EXPANSION enabled skills=25 vfx=2");
    }
    catch (const std::exception &error)
    {
        LOG_ERROR("WARRIOR_EXPANSION disabled: {}", error.what());
    }
    return enabled;
}
bool IsCasting(entt::entity e)
{
    auto *s = g_registry.valid(e) ? g_registry.try_get<Runtime>(e) : nullptr;
    return s && s->castingUntil && int32_t(s->castingUntil - get_dword_time()) > 0;
}
bool EnsureLoaded(entt::entity e)
{
    if (!IsEnabled() || !ecs::PlayerRuntime::IsPC(e) || ecs::PlayerRuntime::GetJob(e) > 3 || !Pid(e) ||
        !SkillSystem::HasSkillLevels(e))
        return false;
    auto *state = State(e);
    if (state->loaded)
        return true;
    if (state->loading)
        return false;
    state->loading = true;
    std::unique_ptr<SQLMsg> msg(
        DBManager::instance().DirectQuery("SELECT tier,skill_id,skill_level,successful_reads,next_read,spent_points "
                                          "FROM warrior_skill_choices WHERE player_id=%u ORDER BY tier",
                                          Pid(e)));
    if (!SqlOk(msg))
    {
        state->loading = false;
        Notice(e, "WEX_STORAGE_ERROR");
        return false;
    }
    std::array<Row, 5> rows{};
    while (auto row = msg->Get()->FetchRow())
    {
        unsigned tier = std::stoul(row[0]);
        uint32_t id = std::stoul(row[1]);
        int level = std::stoi(row[2]);
        if (tier >= 5 || !IsSkill(id) || level < 1 || level > 40 || (tier == 0 && !IsBuff(id)) ||
            (tier >= 1 && tier <= 3 && !IsRanged(id)) || (tier == 4 && !IsArea(id)))
        {
            state->loading = false;
            return false;
        }
        for (const auto &prior : rows)
            if (prior.skill == id)
            {
                state->loading = false;
                return false;
            }
        const unsigned reads = std::stoul(row[3]), spent = std::stoul(row[5]);
        if (spent > 16 || reads > 10 || (reads && (!BooksRequired(level) || reads >= BooksRequired(level))))
        {
            state->loading = false;
            return false;
        }
        rows[tier] = {static_cast<uint16_t>(id), static_cast<uint16_t>(reads), static_cast<uint8_t>(level),
                      static_cast<uint8_t>(spent), static_cast<time_t>(std::stoll(row[4]))};
    }
    state->rows = rows;
    state->loaded = true;
    state->loading = false;
    // Restore canonical new-skill levels before publication; legacy skill blobs remain unchanged in shape.
    for (auto row : rows)
        if (row.skill)
        {
            SkillSystem::SetSkillLevel(e, row.skill, row.level);
            SkillSystem::SetSkillNextReadTime(e, row.skill, row.nextRead);
        }
    return true;
}
bool IsChosen(entt::entity e, uint32_t id)
{
    if (!IsSkill(id) || !EnsureLoaded(e))
        return false;
    auto *state = State(e);
    for (size_t i = 0; i < 5; ++i)
        if (state->rows[i].skill == id)
            return ecs::PointSystem::GetLevel(e) >= UnlockLevels[i] &&
                   (!IsBuff(id) || BuffForGroup(id, SkillSystem::GetSkillGroup(e), ecs::PlayerRuntime::GetJob(e)));
    return false;
}
bool AllowBuff(entt::entity e, uint32_t id)
{
    return IsEnabled() && ((CanShare(id) && ecs::PlayerRuntime::IsPC(e)) || IsChosen(e, id));
}
bool CommitLevel(entt::entity e, uint32_t id, uint8_t &level)
{
    if (!IsSkill(id) || !EnsureLoaded(e))
        return false;
    auto *row = Find(e, id);
    if (!row)
        return level == 0;
    if (level == 0)
        return false;
    level = std::min<uint8_t>(40, level);
    if (row->level == level)
        return true;
    std::unique_ptr<SQLMsg> msg(
        DBManager::instance().DirectQuery("UPDATE warrior_skill_choices SET skill_level=%u,successful_reads=0 WHERE "
                                          "player_id=%u AND skill_id=%u AND skill_level=%u",
                                          level, Pid(e), id, row->level));
    if (!SqlOk(msg) || msg->Get()->uiAffectedRows != 1)
    {
        Notice(e, "WEX_STORAGE_ERROR");
        return false;
    }
    row->level = level;
    row->reads = 0;
    return true;
}
void LevelUp(entt::entity e, uint32_t id, uint8_t method)
{
    if (!IsChosen(e, id) || !ecs::PlayerRuntime::GetDesc(e) || AffectSystem::IsPolymorphed(e))
        return;
    auto *row = Find(e, id);
    if (!row)
        return;
    bool point = method == SkillSystem::SKILL_UP_BY_POINT;
    if (point)
    {
        if (row->level >= 20)
            return;
    }
    else if (method != SkillSystem::SKILL_UP_BY_QUEST || row->level < 30 || row->level >= 40)
        return;
    uint8_t next = row->level + 1;
    if (point && next >= 17)
        next = 20;
    int spent = row->spent + (point ? 1 : 0);
    std::unique_ptr<SQLMsg> begin(DBManager::instance().DirectQuery("START TRANSACTION"));
    if (!SqlOk(begin))
        return;
    auto rollback = [&]() { std::unique_ptr<SQLMsg> ignored(DBManager::instance().DirectQuery("ROLLBACK")); };
    std::unique_ptr<SQLMsg> lock(
        DBManager::instance().DirectQuery("SELECT id FROM player WHERE id=%u FOR UPDATE", Pid(e)));
    std::unique_ptr<SQLMsg> sum(DBManager::instance().DirectQuery(
        "SELECT COALESCE(SUM(spent_points),0) FROM warrior_skill_choices WHERE player_id=%u", Pid(e)));
    if (!SqlOk(lock) || !lock->Get()->uiNumRows || !SqlOk(sum) || !sum->Get()->uiNumRows)
    {
        rollback();
        Notice(e, "WEX_STORAGE_ERROR");
        return;
    }
    auto count = sum->Get()->FetchRow();
    int total = count && count[0] ? std::stoi(count[0]) : 0;
    if (point && AvailablePoints(ecs::PointSystem::GetLevel(e), total) < 1)
    {
        rollback();
        Notice(e, "WEX_NO_POINTS");
        return;
    }
    std::unique_ptr<SQLMsg> update(DBManager::instance().DirectQuery(
        "UPDATE warrior_skill_choices SET skill_level=%u,spent_points=%u,successful_reads=0 WHERE player_id=%u AND "
        "skill_id=%u AND skill_level=%u AND spent_points=%u",
        next, spent, Pid(e), id, row->level, row->spent));
    if (!SqlOk(update) || update->Get()->uiAffectedRows != 1)
    {
        rollback();
        Notice(e, "WEX_STORAGE_ERROR");
        return;
    }
    std::unique_ptr<SQLMsg> commit(DBManager::instance().DirectQuery("COMMIT"));
    if (!SqlOk(commit))
    {
        rollback();
        Notice(e, "WEX_STORAGE_ERROR");
        return;
    }
    row->level = next;
    row->spent = spent;
    row->reads = 0;
    SkillSystem::SetSkillLevel(e, id, next);
    ecs::SessionSystem::Save(e);
    ecs::PointSystem::Compute(e);
    SendState(e);
}
void PersistLevel(entt::entity e, uint32_t id, uint8_t level)
{
    CommitLevel(e, id, level);
}
void PersistReadTime(entt::entity e, uint32_t id, time_t next)
{
    auto *state = g_registry.valid(e) ? g_registry.try_get<Runtime>(e) : nullptr;
    if (!state || !state->loaded)
        return;
    auto *row = Find(e, id);
    if (!row || row->nextRead == next)
        return;
    std::unique_ptr<SQLMsg> msg(DBManager::instance().DirectQuery(
        "UPDATE warrior_skill_choices SET next_read=%llu WHERE player_id=%u AND skill_id=%u",
        static_cast<unsigned long long>(next), Pid(e), id));
    if (SqlOk(msg))
        row->nextRead = next;
    else
        Notice(e, "WEX_STORAGE_ERROR");
}
void SendState(entt::entity e)
{
    if (!EnsureLoaded(e))
    {
        ecs::ChatSystem::Send(e, CHAT_TYPE_COMMAND,
                              !IsEnabled() || ecs::PlayerRuntime::GetJob(e) > 3 ? "wex_disabled" : "wex_retry");
        return;
    }
    auto rows = State(e)->rows;
    int spent = 0;
    for (const auto &row : rows)
        spent += row.spent;
    std::ostringstream text;
    text << "wex_state 1 " << ecs::PointSystem::GetLevel(e) << ' ' << int(SkillSystem::GetSkillGroup(e)) << ' '
         << AvailablePoints(ecs::PointSystem::GetLevel(e), spent);
    for (const auto &row : rows)
        text << ' ' << row.skill << ' ' << int(row.level) << ' ' << row.reads << ' ' << row.nextRead;
    ecs::ChatSystem::Send(e, CHAT_TYPE_COMMAND, "%s", text.str().c_str());
    SkillSystem::SendSkillLevelPacket(e);
}
bool ReadBook(entt::entity e, uint32_t id, int64_t experienceCost)
{
    if (!IsChosen(e, id) || AffectSystem::IsPolymorphed(e))
        return false;
    auto *row = Find(e, id);
    if (!row || !BooksRequired(row->level))
        return false;
    bool skipDelay = false;
    if (get_global_time() < row->nextRead)
    {
        skipDelay = AffectSystem::FindAffect(e, AFFECT_SKILL_NO_BOOK_DELAY) != nullptr;
        if (!skipDelay && !(test_server && quest::CQuestManager::instance().GetEventFlag("no_read_delay")))
        {
            Notice(e, "WEX_READ_WAIT");
            return false;
        }
    }
    bool bonus = AffectSystem::FindAffect(e, AFFECT_SKILL_BOOK_BONUS) != nullptr;
    bool success = bonus || number(1, 100) <= 70;
    auto progress = AdvanceReading({row->level, row->reads}, success);
    auto next = get_global_time() + SKILLBOOK_DELAY_MIN;
    std::unique_ptr<SQLMsg> msg(DBManager::instance().DirectQuery(
        "UPDATE warrior_skill_choices SET skill_level=%u,successful_reads=%u,next_read=%llu WHERE player_id=%u AND "
        "skill_id=%u AND skill_level=%u AND successful_reads=%u AND next_read=%llu",
        progress.level, progress.successes, static_cast<unsigned long long>(next), Pid(e), id, row->level, row->reads,
        static_cast<unsigned long long>(row->nextRead)));
    if (!SqlOk(msg) || msg->Get()->uiAffectedRows != 1)
    {
        Notice(e, "WEX_STORAGE_ERROR");
        return false;
    }
    row->level = progress.level;
    row->reads = progress.successes;
    row->nextRead = next;
    if (skipDelay)
        AffectSystem::RemoveAffect(e, AFFECT_SKILL_NO_BOOK_DELAY);
    if (bonus)
        AffectSystem::RemoveAffect(e, AFFECT_SKILL_BOOK_BONUS);
    if (experienceCost > 0)
        ecs::PointSystem::Change(e, POINT_EXP, -experienceCost);
    SkillSystem::SetSkillLevel(e, id, progress.level);
    SkillSystem::SetSkillNextReadTime(e, id, next);
    ecs::SessionSystem::Save(e);
    ecs::PointSystem::Compute(e);
    Notice(e, success ? "WEX_READ_SUCCESS" : "WEX_READ_FAIL");
    SendState(e);
    return true;
}
bool ValidateCast(entt::entity e, uint32_t id, entt::entity target)
{
    if (!IsChosen(e, id) || !Alive(e) || !WeaponAllowed(e))
        return false;
    auto *state = State(e);
    auto now = get_dword_time();
    auto next = SkillSystem::GetNextSkillUseTime(e, id);
    if ((state->castingUntil && int32_t(state->castingUntil - now) > 0) || (next && int32_t(next - now) > 0))
        return false;
    if (ecs::IsAttrAt(ecs::PlayerRuntime::GetMapIndex(e), ecs::PlayerRuntime::GetX(e), ecs::PlayerRuntime::GetY(e),
                      ATTR_BANPK) &&
        !IsBuff(id))
        return false;
    const int range = id==184 ? BeamRange(SkillSystem::GetSkillLevel(e,id)) : Tuning(id).range;
    if (IsBuff(id)) return target==entt::null || target==e || (CanShare(id) && Friendly(e,target,range));
    if (IsSelfArea(id)) return true;
    if (IsArea(id) && state->ground) return PointAllowed(e,state->x,state->y,range);
    return TargetAllowed(e,target,range);
}
void AcceptedCast(entt::entity e, uint32_t id, entt::entity target)
{
    auto *state=State(e); if(!state || !state->loaded) return;
    const auto &cfg=Tuning(id);
    ++state->serial; state->castingUntil=get_dword_time()+cfg.windup+100;
    if(IsBuff(id) && (!CanShare(id) || target==entt::null)) target=e;
    if(IsSelfArea(id)) target=e;
    auto *info=AllocEventInfo<CastEvent>();
    info->caster=e; info->target=target; info->pid=Pid(e); info->weapon=Weapon(e);
    info->skill=id; info->map=ecs::PlayerRuntime::GetMapIndex(e); info->serial=state->serial;
    info->originX=ecs::PlayerRuntime::GetX(e); info->originY=ecs::PlayerRuntime::GetY(e);
    info->x=IsArea(id)&&state->ground&&!IsSelfArea(id)?state->x:ecs::PlayerRuntime::GetX(target);
    info->y=IsArea(id)&&state->ground&&!IsSelfArea(id)?state->y:ecs::PlayerRuntime::GetY(target);
    info->range=id==184?BeamRange(SkillSystem::GetSkillLevel(e,id)):cfg.range;
    info->travel=cfg.speed>0?int(Distance(info->originX,info->originY,info->x,info->y)/cfg.speed*1000):0;
    CastMessage(e,"wex_cast",id,ecs::IsCharacter(target)?ecs::PlayerRuntime::GetPacketVID(target):0,info->x,info->y,cfg.windup,info->travel,0);
    event_create(Impact,info,std::max<int32_t>(1,PASSES_PER_SEC(cfg.windup/1000.0)));
}

void OnGroupChanged(entt::entity e)
{
    for (uint32_t id = FirstSkill; id <= LastSkill; ++id)
        if (IsBuff(id) && !BuffForGroup(id, SkillSystem::GetSkillGroup(e), ecs::PlayerRuntime::GetJob(e)))
            AffectSystem::RemoveAffect(e, BuffAffect(id));
    SendState(e);
}
void Command(entt::entity e, const char *arguments)
{
    // Observers of every class need current buff/field visuals after entering view range.
    if(arguments && std::string(arguments)=="visuals" && IsEnabled() && ecs::PlayerRuntime::IsPC(e)) {
        auto *s=State(e);const auto now=get_dword_time();
        if(s && now-s->lastCommand>=150){s->lastCommand=now;Visuals(e);}
        return;
    }
    if (!EnsureLoaded(e))
    {
        ecs::ChatSystem::Send(e, CHAT_TYPE_COMMAND,
                              !IsEnabled() || ecs::PlayerRuntime::GetJob(e) > 3 ? "wex_disabled" : "wex_retry");
        return;
    }
    auto *state = State(e);
    uint32_t now = get_dword_time();
    if (now - state->lastCommand < 150)
        return;
    state->lastCommand = now;
    std::istringstream input(arguments ? arguments : "");
    std::string action;
    input >> action;
    if (action == "state")
    {
        SendState(e);
        return;
    }
    if (action == "visuals")
    {
        Visuals(e);
        return;
    }
    if (action == "ground")
    {
        uint32_t id;
        int64_t x, y;
        std::string trailing;
        if (!(input >> id >> x >> y) || (input >> trailing) || !IsArea(id) || x < std::numeric_limits<int32_t>::min() ||
            x > std::numeric_limits<int32_t>::max() || y < std::numeric_limits<int32_t>::min() ||
            y > std::numeric_limits<int32_t>::max())
            return;
        state->ground = true;
        state->x = int32_t(x);
        state->y = int32_t(y);
        SkillSystem::UseSkill(e, id, entt::null);
        if (g_registry.valid(e))
            State(e)->ground = false;
        return;
    }
    if (action != "choose")
        return;
    unsigned tier;
    uint32_t id;
    std::string trailing;
    if (!(input >> tier >> id) || (input >> trailing))
        return;
    auto error = ValidateChoice(ecs::PlayerRuntime::GetJob(e), SkillSystem::GetSkillGroup(e),
                                ecs::PointSystem::GetLevel(e), Selected(*state), tier, id);
    if (error != ChoiceError::None)
    {
        Notice(e, "WEX_CHOICE_REJECTED");
        SendState(e);
        return;
    }
    std::unique_ptr<SQLMsg> msg(DBManager::instance().DirectQuery(
        "INSERT INTO warrior_skill_choices(player_id,tier,skill_id,skill_level,successful_reads,next_read) "
        "VALUES(%u,%u,%u,1,0,0)",
        Pid(e), tier, id));
    if (!SqlOk(msg) || msg->Get()->uiAffectedRows != 1)
    {
        Notice(e, "WEX_STORAGE_ERROR");
        return;
    }
    State(e)->rows[tier].skill = id;
    SkillSystem::SetSkillLevel(e, id, 1);
    ecs::SessionSystem::Save(e);
    Notice(e, "WEX_CHOICE_SAVED");
    SendState(e);
}
} // namespace WarriorExpansion
#endif
ACMD(do_warrior_skill)
{
    WarriorExpansion::Command(character, argument);
}
