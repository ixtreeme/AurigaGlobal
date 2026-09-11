#pragma once

#include <entt/entt.hpp>
#include <string_view>

#include "../../typedef.h"
#include "../components/session_components.hpp"

class CArena;
class CPetSystem;
class CNewPetSystem;

namespace ecs::PlayerRuntime {

entt::entity FindByPlayerID(uint32_t playerID);
entt::entity FindByPlayerName(std::string_view name);
entt::entity FindByVID(uint32_t vid);
entt::entity FindSpecifyPC(uint32_t jobFlag, int32_t mapIndex, entt::entity except,
	int minLevel, int maxLevel);
LPDESC GetDesc(entt::entity e);
uint32_t GetPlayerID(entt::entity e);
uint32_t GetAccountID(entt::entity e);
#ifdef ENABLE_ITEMSHOP
uint32_t GetDragonCoin(entt::entity e);
void SetDragonCoin(entt::entity e, uint32_t amount);
void SetProtectTime(entt::entity e, std::string_view flag, int value);
int GetProtectTime(entt::entity e, std::string_view flag);
#endif
uint8_t GetEmpire(entt::entity e);
void SetEmpire(entt::entity e, uint8_t empire);
int ChangeEmpire(entt::entity e, uint8_t empire);
int GetChangeEmpireCount(entt::entity e);
void IncrementChangeEmpireCount(entt::entity e);
uint8_t GetGMLevel(entt::entity e);
void RefreshGMLevel(entt::entity e);
uint8_t GetBlockMode(entt::entity e);
bool IsBlockMode(entt::entity e, uint8_t flag);
void SetBlockMode(entt::entity e, uint8_t flag);
void StartDestroyWhenIdleEvent(entt::entity e);
void SetBlockModeForce(entt::entity e, uint8_t flags);
uint32_t GetPacketVID(entt::entity e);
uint32_t GetRaceNum(entt::entity e);
uint8_t GetSex(entt::entity e);
void BuffOnAttr_AddBuffsFromItem(entt::entity e, entt::entity item);
void BuffOnAttr_RemoveBuffsFromItem(entt::entity e, entt::entity item);
void BuffOnAttr_ClearAll(entt::entity e);
void BuffOnAttr_Destroy(entt::entity e);
void BuffOnAttr_ValueChange(entt::entity e, uint8_t bType, uint8_t bOldValue, uint8_t bNewValue);
#ifdef __HIGHLIGHT_SYSTEM__
void SetItem(entt::entity e, TItemPos Cell, entt::entity item, bool isHighLight = false);
#else
void SetItem(entt::entity e, TItemPos Cell, entt::entity item);
#endif
void SetWear(entt::entity e, uint8_t bCell, entt::entity item);
bool IsRaceFlag(entt::entity e, uint32_t dwBit);
int64_t GetHP(entt::entity e);
int GetHPPct(entt::entity e);
void SetDungeonTicketExtraMetin(entt::entity e, bool value);
bool IsDungeonTicketExtraMetin(entt::entity e);
bool IsPet(entt::entity e);
bool IsNewPet(entt::entity e);
void SetProto(entt::entity e, const CMob* pkMob);
void SetDungeon(entt::entity e, LPDUNGEON pkDungeon);
void SetCoward(entt::entity e);
void SetNoAttackShinsu(entt::entity e);
void SetNoAttackChunjo(entt::entity e);
void SetNoAttackJinno(entt::entity e);
void SetAttackMob(entt::entity e);
bool IsReviver(entt::entity e);
int GetWheelFreeCount(entt::entity e);
void SetWheelFreeCount(entt::entity e, int count);
uint32_t GetAIFlag(entt::entity e);
void SetLastSyncTime(entt::entity e, const timeval& tv);
const timeval& GetLastSyncTime(entt::entity e);
const TMobTable* GetMobTable(entt::entity e);
uint8_t GetJob(entt::entity e);
bool ChangeSex(entt::entity e);
bool SetRace(entt::entity e, uint8_t race);
bool SetCostumeHidden(entt::entity e, uint8_t part, bool hidden, bool skipPersistence = false);
bool IsCostumeHidden(entt::entity e, uint8_t part);
std::string_view GetName(entt::entity e);
std::string_view GetPendingName(entt::entity e);
void SetPendingName(entt::entity e, std::string_view name);
int RequestNameChange(entt::entity e, std::string_view name);
int32_t GetMapIndex(entt::entity e);
int32_t GetX(entt::entity e);
int32_t GetY(entt::entity e);
int32_t GetZ(entt::entity e);
float GetRotation(entt::entity e);
LPSECTREE GetSectree(entt::entity e);
// The entity exists in the registry. This is what the legacy `if (!ch)`
// guards were approximating: they resolved a pointer purely to test that
// something was still there.
bool IsValid(entt::entity e);

bool IsPC(entt::entity e);
bool IsNPC(entt::entity e);
// Not IsNPC: CHARACTER::IsGuardNPC tests m_bCharType != CHAR_TYPE_PC, which
// includes monsters and stones, while IsNPC above is the TagNPC component.
bool IsGuardNPC(entt::entity e);
// The remaining two CharacterType tests the AI states need. CHARACTER
// carried these as inline reads of m_bCharType; the component is what the
// factory fills and what IsGuardNPC above already trusts.
int64_t GetSP(entt::entity e);
int GetQuestFlag(entt::entity e, const std::string& flag);
void SetQuestFlag(entt::entity e, const std::string& flag, int value);
int GetPetEnchant(entt::entity e);
void SetPetEnchant(entt::entity e, int value);
void SetUseSeedOrMoonBottleTime(entt::entity e);
std::list<TPlayerBattlePassMission*>& GetBattlePassMissions(entt::entity e);
uint32_t GetBattlePassAdjustedTotal(entt::entity e, uint32_t dwMissionID,
                                    uint32_t dwBattlePassID, uint32_t dwBaseTotal);
void ApplyBattlePassBoostRecalc(entt::entity e, uint8_t bBattlePassId);
int GetCmdAntiFloodPulse(entt::entity e);
void SetCmdAntiFloodPulse(entt::entity e, int pulse);
uint32_t IncreaseCmdAntiFloodCount(entt::entity e);
void SetCmdAntiFloodCount(entt::entity e, uint32_t count);
void SetItemUseAntiFloodPulse(entt::entity e, int pulse);
uint32_t IncreaseItemUseAntiFloodCount(entt::entity e);
void SetItemUseAntiFloodCount(entt::entity e, uint32_t count);
uint32_t GetBoxUseTime(entt::entity e);
void SetBoxUseTime(entt::entity e, uint32_t when);
bool HasBattlePassBoost(entt::entity e, uint8_t bBattlePassId);
int GetSecondsTillNextMonth();
bool IsWarping(entt::entity e);
bool IsImmortal(entt::entity e);
uint32_t GetMonsterDrainSPPoint(entt::entity e);
bool IsWarp(entt::entity e);
bool IsBuilding(entt::entity e);
// Is this character a mount creature, not a rider - StatusFlags.isMount.
bool IsMount(entt::entity e);
bool IsGoto(entt::entity e);
void SetPotionLimit(entt::entity e, int count);

// The appearance part write. CHARACTER::SetPart was already nothing but
// this - EnsureAppearancePartsComponent then one array store.
void SetPart(entt::entity e, uint8_t partPos, uint16_t value);
uint16_t GetPart(entt::entity e, uint8_t bPartPos);
uint16_t GetOriginalPart(entt::entity e, uint8_t bPartPos);
uint16_t GetRuneEffect(entt::entity e);

// The point writers and readers PointSystem::Change needs.
void SetHP(entt::entity e, int64_t hp);
void SetMaxHP(entt::entity e, int64_t value);
void SetSP(entt::entity e, int64_t sp);
void SetMaxSP(entt::entity e, int64_t value);
void SetStamina(entt::entity e, int64_t value);
void SetMaxStamina(entt::entity e, int64_t value);
int GetStamina(entt::entity e);
int64_t GetMaxStamina(entt::entity e);
void SetExp(entt::entity e, uint32_t exp);
uint32_t GetExp(entt::entity e);
uint32_t GetNextExp(entt::entity e);
void SetGold(entt::entity e, int64_t gold);
void SetLevel(entt::entity e, uint8_t level);
uint32_t GetImmuneFlag(entt::entity e);
void SetImmuneFlag(entt::entity e, uint32_t value);

// The three timed events that used to be CHARACTER members. Slot names the
// one being addressed; Cancel runs event_cancel on it, Set stores or clears.
enum class CharEvent : uint8_t { Dead, Stun, Recovery, Fishing, Timed, Warp, WarpNPC,
                                 BattlePassStayOnline, Drop, Mining,
                                 DestroyWhenIdle, Save };
LPEVENT GetCharEvent(entt::entity e, CharEvent slot);
void SetCharEvent(entt::entity e, CharEvent slot, LPEVENT ev);
void CancelCharEvent(entt::entity e, CharEvent slot);

#ifdef ENABLE_BLOCK_MULTIFARM
void BlockProcessed(entt::entity e);
void BlockDrop(entt::entity e);
void UnblockDrop(entt::entity e);
void SetDropStatus(entt::entity e);
#endif

// Test-server monster diagnostics. Plain text, no varargs: the callers
// that needed formatting are still CHARACTER::MonsterLog.
void MonsterLog(entt::entity e, const char* text);

void SetPosition(entt::entity e, int pos);
void StartRecoveryEvent(entt::entity e);
int GetPotionLimit(entt::entity e);
bool IsStone(entt::entity e);
bool IsMonster(entt::entity e);
uint8_t GetMobRank(entt::entity e);
int GetPremiumRemainSeconds(entt::entity e, uint8_t premiumType);
uint8_t GetBattlePassId(entt::entity e);
bool IsPCBang(entt::entity e);
bool IsObserverMode(entt::entity e);
bool IsArenaObserverMode(entt::entity e);
void SetItemAwardVnum(entt::entity e, uint32_t vnum);
uint32_t GetItemAwardVnum(entt::entity e);
void SetItemAwardCommand(entt::entity e, const char* command);
const char* GetItemAwardCommand(entt::entity e);
CArena* GetArena(entt::entity e);
void SetArenaObserverMode(entt::entity e, bool flag);
bool GetArenaObserverMode(entt::entity e);
const ecs::MobileAuth& GetMobileAuth(entt::entity e);
void SetMobilePhone(entt::entity e, const char* phone);
bool IsBattlePassLoaded(entt::entity e);
void SetBattlePassLoaded(entt::entity e, bool loaded);
void SetArena(entt::entity e, CArena* arena);
bool CanWarp(entt::entity e);
bool IsHack(entt::entity e, bool sendMessage, bool checkShopOwner, int limitTime);
bool IsHack(entt::entity e, bool sendMessage = true, bool checkShopOwner = true);
int GetDuelOption(entt::entity e, const char* option);
void SetDuelOption(entt::entity e, const char* option, int value);
int GetPosition(entt::entity e);
entt::entity GetQuestNPC(entt::entity e);
uint32_t GetQuestNPCID(entt::entity e);
bool SetQuestNPCID(entt::entity e, uint32_t id);
uint32_t GetQuestBy(entt::entity e);
bool SetQuestBy(entt::entity e, uint32_t questVnum);
void DestroyCharacter(entt::entity e);
#ifdef __PET_SYSTEM__
CPetSystem* GetPetSystem(entt::entity e);
#endif
#ifdef __NEWPET_SYSTEM__
CNewPetSystem* GetNewPetSystem(entt::entity e);
void SetEggVID(entt::entity e, int vid);
int GetEggVID(entt::entity e);
#endif
#ifdef __DUNGEON_INFO_SYSTEM__
uint64_t GetQuestDamage(entt::entity e, int race);
#endif
#ifdef ENABLE_BATTLE_PASS
uint8_t GetBattlePassID(entt::entity e);
uint32_t GetMissionProgress(entt::entity e, uint32_t missionID, uint32_t battlePassID);
bool IsCompletedMission(entt::entity e, uint8_t missionType);
bool UpdateMissionProgress(entt::entity e, uint32_t missionID, uint32_t battlePassID,
    uint32_t updateValue, uint32_t totalValue, bool overrideValue = false);
#endif

#ifdef ENABLE_VOTE4BUFF
int64_t GetVoteCoin(entt::entity e);
bool SetVoteCoin(entt::entity e, int64_t amount);
#endif

#ifdef ENABLE_RANKING
uint32_t GetMobDropItemVnum(entt::entity e);
uint32_t GetPolymorphItemVnum(entt::entity e);
uint32_t GetLastShoutPulse(entt::entity e);
void SetLastShoutPulse(entt::entity e, uint32_t pulse);
entt::entity GetQuestItem(entt::entity e);
void SetQuestItem(entt::entity e, entt::entity item);
int64_t GetRankPoints(entt::entity e, int category);
bool SetRankPoints(entt::entity e, int category, int64_t value);
#endif

} // namespace ecs::PlayerRuntime
