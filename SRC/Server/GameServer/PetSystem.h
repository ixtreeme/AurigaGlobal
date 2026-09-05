#ifndef __HEADER_PET_SYSTEM__
#define __HEADER_PET_SYSTEM__

#include <array>
#include <memory>
#include <string>
#include <unordered_map>
#include <common/tables.h>
#include <entt/entt.hpp>
#include "event.h"

class CPetActor
{
public:
    enum EPetOptions
    {
        EPetOption_Followable = 1 << 0,
        EPetOption_Mountable = 1 << 1,
        EPetOption_Summonable = 1 << 2,
        EPetOption_Combatable = 1 << 3,
    };

    CPetActor(entt::entity owner, uint32_t vnum, uint32_t options = EPetOption_Followable | EPetOption_Summonable);
    virtual ~CPetActor();
    CPetActor(const CPetActor&) = delete;
    CPetActor& operator=(const CPetActor&) = delete;

    entt::entity GetCharacter() const { return m_character; }
    entt::entity GetOwner() const { return m_owner; }
    entt::entity GetSummonItem() const { return m_summonItem; }
    uint32_t GetVID() const { return m_vid; }
    // Stable identity/map key; a costume changes only the spawned creature.
    uint32_t GetVnum() const { return m_vnum; }
    bool HasOption(EPetOptions option) const { return (m_options & option) != 0; }
    bool IsSummoned() const;

    void SetName(const char* petName);
    bool Mount();
    void Unmount();
    uint32_t Summon(const char* petName, entt::entity item, bool spawnFar = false);
    void Unsummon();
    void GiveBuff();
    void ClearBuff();
#ifdef ENABLE_COSTUME_PET
    void UpdatePetSkin();
#endif

protected:
    friend class CPetSystem;
    virtual bool Update(uint32_t deltaTime);
    virtual bool UpdateFollowAI();

private:
    bool Follow(float minDistance);
    void SetSummonItem(entt::entity item);
    bool CanGiveBuff() const;

    entt::entity m_owner { entt::null };
    entt::entity m_character { entt::null };
    entt::entity m_summonItem { entt::null };
    uint32_t m_vnum;
    uint32_t m_options;
    uint32_t m_vid { 0 };
    uint32_t m_ridingVnum { 0 };
    // Cleanup cannot dereference a deleted/recycled item or its new owner.
    std::array<TItemApply, ITEM_APPLY_MAX_NUM> m_buffApplies {};
};

class CPetSystem
{
public:
    using TPetActorMap = std::unordered_map<uint32_t, std::unique_ptr<CPetActor>>;
    explicit CPetSystem(entt::entity owner);
    virtual ~CPetSystem();
    CPetSystem(const CPetSystem&) = delete;
    CPetSystem& operator=(const CPetSystem&) = delete;

    CPetActor* GetByVID(uint32_t vid) const;
    CPetActor* GetByVnum(uint32_t vnum) const;
    entt::entity GetOwner() const { return m_owner; }
    bool IsUpdateEvent(const LPEVENT& event) const { return event && event == m_updateEvent; }
    bool Update(uint32_t deltaTime);
    void Destroy();
    size_t CountSummoned() const;
    void SetUpdatePeriod(uint32_t ms);
    CPetActor* Summon(uint32_t vnum, entt::entity item, const char* petName, bool spawnFar,
        uint32_t options = CPetActor::EPetOption_Followable | CPetActor::EPetOption_Summonable);
    void Unsummon(uint32_t vnum, bool deleteFromList = false);
    void Unsummon(CPetActor* actor, bool deleteFromList = false);
    void UnsummonAll();
    void DeletePet(uint32_t vnum);
    void DeletePet(CPetActor* actor);
    void RefreshBuff();
#ifdef ENABLE_COSTUME_PET
    void UpdatePetSkin();
#endif

private:
    TPetActorMap m_petActorMap;
    entt::entity m_owner { entt::null };
    uint32_t m_updatePeriod { 400 };
    uint32_t m_lastUpdateTime { 0 };
    LPEVENT m_updateEvent;
    bool m_destroying { false };
};

#endif
