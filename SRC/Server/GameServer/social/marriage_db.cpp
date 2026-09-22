#include "stdafx.h"
#include <Core/Logging.hpp>
#include "input.h"
#include "packet.h"
#include "protocol.h"
#include "char.h"
#include "constants.h"
#include "utils.h"
#include "marriage.h"
#include "wedding.h"
#include "char_manager.h"
#include "../ecs/systems/InventorySystem.hpp"
#include "../ecs/systems/ItemSystem.hpp"
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "../ecs/systems/PointSystem.hpp"
#include "../ecs/systems/SkillSystem.hpp"
#include "../ecs/systems/SocialSystem.hpp"
#include "../ecs/systems/StatSystem.hpp"
#include "desc.h"
#include "desc_client.h"

void CInputDB::MarriageAdd(TPacketMarriageAdd * p)
{
	LOG_INFO("MarriageAdd {} {} {} {} {}", p->dwPID1, p->dwPID2, (uint32_t)p->tMarryTime, p->szName1, p->szName2);
	marriage::CManager::instance().Add(p->dwPID1, p->dwPID2, p->tMarryTime, p->szName1, p->szName2);
}

void CInputDB::MarriageUpdate(TPacketMarriageUpdate * p)
{
	LOG_INFO("MarriageUpdate {} {} {} {}", p->dwPID1, p->dwPID2, p->iLovePoint, p->byMarried);
	marriage::CManager::instance().Update(p->dwPID1, p->dwPID2, p->iLovePoint, p->byMarried);
}

void CInputDB::MarriageRemove(TPacketMarriageRemove * p)
{
	LOG_INFO("MarriageRemove {} {}", p->dwPID1, p->dwPID2);
	marriage::CManager::instance().Remove(p->dwPID1, p->dwPID2);
}

void CInputDB::WeddingRequest(TPacketWeddingRequest* p)
{
	marriage::WeddingManager::instance().Request(p->dwPID1, p->dwPID2);
}

void CInputDB::WeddingReady(TPacketWeddingReady* p)
{
	LOG_INFO("WeddingReady {} {} {}", p->dwPID1, p->dwPID2, p->dwMapIndex);
	marriage::CManager::instance().WeddingReady(p->dwPID1, p->dwPID2, p->dwMapIndex);
}

void CInputDB::WeddingStart(TPacketWeddingStart* p)
{
	LOG_INFO("WeddingStart {} {}", p->dwPID1, p->dwPID2);
	marriage::CManager::instance().WeddingStart(p->dwPID1, p->dwPID2);
}

void CInputDB::WeddingEnd(TPacketWeddingEnd* p)
{
	LOG_INFO("WeddingEnd {} {}", p->dwPID1, p->dwPID2);
	marriage::CManager::instance().WeddingEnd(p->dwPID1, p->dwPID2);
}
