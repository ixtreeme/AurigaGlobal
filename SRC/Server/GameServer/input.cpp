#include "stdafx.h"
#include "ecs/systems/PlayerRuntimeSystem.hpp"
#include "ecs/AIHelpers.hpp"
#include <sstream>

#include "desc.h"
#include "desc_manager.h"
#include "char_interface.hpp"
#include "ecs/CharacterAccessors.hpp"
#include "buffer_manager.h"
#include "config.h"
#include "profiler.h"
#include "p2p.h"
#include "log.h"
#include "db.h"
#include "questmanager.h"
#include "login_sim.h"
#include "fishing.h"
#include "priv_manager.h"
#include "dev_log.h"
#include <Core/Logging.hpp>

#include <common/CommonDefines.h>

#include "utils.h"

CInputProcessor::CInputProcessor() : m_pPacketInfo(nullptr), m_iBufferLeft(0)
{
	if (!m_pPacketInfo)
		BindPacketInfo(&m_packetInfoCG);
}

void CInputProcessor::BindPacketInfo(CPacketInfo * pPacketInfo)
{
	m_pPacketInfo = pPacketInfo;
}

int g_iLastPacket[2] = { -1, -1 };

bool CInputProcessor::Process(LPDESC lpDesc, const void * c_pvOrig, int iBytes, int & r_iBytesProceed)
{
	const char * c_pData = (const char *) c_pvOrig;
	int iPacketLen;

	if (!m_pPacketInfo)
	{
		LOG_ERROR("No packet info has been binded to");
		return true;
	}

#if defined(__IMPROVED_HANDSHAKE_PROCESS__)
	// Ignore input process if the host is an intruder
	if (lpDesc && DESC_MANAGER::instance().IsIntruder(lpDesc->GetHostName()))
	{
		// Set host close phase
		lpDesc->SetPhase(PHASE_CLOSE);
		return true;
	}
#endif

	for (m_iBufferLeft = iBytes; m_iBufferLeft > 0;)
	{
		uint8_t bHeader = (uint8_t) *(c_pData);
		const char * c_pszName = nullptr;

		if (bHeader == 0) // ��ȣȭ ó���� �����Ƿ� 0�� ����� ��ŵ�Ѵ�.
			iPacketLen = 1;
		else if (!m_pPacketInfo->Get(bHeader, &iPacketLen, &c_pszName))
		{

			LOG_ERROR("UNKNOWN HEADER: {}, LAST HEADER: {}({}), REMAIN BYTES: {}, fd: {}", static_cast<int>(bHeader), g_iLastPacket[0], g_iLastPacket[1], m_iBufferLeft, lpDesc->GetSocket());

			//printdata((uint8_t *) c_pvOrig, m_iBufferLeft);
			lpDesc->SetPhase(PHASE_CLOSE);
			return true;
		}

		if (m_iBufferLeft < iPacketLen)
			return true;

		int originalPacketSize = iPacketLen;
		uint8_t preAnalyzeSeq = *(uint8_t*)(c_pData + iPacketLen - sizeof(uint8_t));

		if (bHeader)
		{
			if (test_server && bHeader != HEADER_CG_MOVE)
				LOG_TRACE("Packet Analyze [Header {}][bufferLeft {}] ", static_cast<int>(bHeader), m_iBufferLeft);

			m_pPacketInfo->Start();

			int iExtraPacketSize = Analyze(lpDesc, bHeader, c_pData);

			if (iExtraPacketSize < 0)
				return true;

			iPacketLen += iExtraPacketSize;
			lpDesc->Log("%s %d", c_pszName, iPacketLen);
			m_pPacketInfo->End();
		}

		if (bHeader == HEADER_CG_PONG)
			LOG_TRACE("PONG! {}", static_cast<int>(*(uint8_t*)(c_pData + iPacketLen - sizeof(uint8_t))));

		c_pData	+= iPacketLen;
		m_iBufferLeft -= iPacketLen;
		r_iBytesProceed += iPacketLen;

		g_iLastPacket[1] = g_iLastPacket[0];
		g_iLastPacket[0] = bHeader;

		if (GetType() != lpDesc->GetInputProcessor()->GetType())
			return false;
	}

	return true;
}

void CInputProcessor::Pong(LPDESC d)
{
	d->SetPong(true);
}

void CInputProcessor::Handshake(LPDESC d, const char * c_pData)
{
	TPacketCGHandshake * p = (TPacketCGHandshake *) c_pData;

	if (d->GetHandshake() != p->dwHandshake)
	{
		LOG_ERROR("Invalid Handshake on {}", d->GetSocket());
		d->SetPhase(PHASE_CLOSE);
	}
	else
	{
		if (d->IsPhase(PHASE_HANDSHAKE))
		{
			if (d->HandshakeProcess(p->dwTime, p->lDelta, false))
			{
#ifdef _IMPROVED_PACKET_ENCRYPTION_
				d->SendKeyAgreement();
#else
				// Handshaking succeeded
				if (g_bAuthServer) {
					d->SetPhase(PHASE_AUTH);
				} else {
					d->SetPhase(PHASE_LOGIN);
				}
#endif // #ifdef _IMPROVED_PACKET_ENCRYPTION_
			}
		}
		else
			d->HandshakeProcess(p->dwTime, p->lDelta, true);
	}
}

void CInputProcessor::Version(entt::entity ch, const char* c_pData)
{
	if (!ecs::PlayerRuntime::IsValid(ch))
		return;

	TPacketCGClientVersion * p = (TPacketCGClientVersion *) c_pData;
	LOG_INFO("VERSION: {} {} {}", ecs::PlayerRuntime::GetName(ch).data(), p->timestamp, p->filename);
	ecs::PlayerRuntime::GetDesc(ch)->SetClientVersion(p->timestamp);
}

void LoginFailure(LPDESC d, const char * c_pszStatus)
{
	if (!d)
		return;

	TPacketGCLoginFailure failurePacket;

	failurePacket.header = HEADER_GC_LOGIN_FAILURE;
	strlcpy(failurePacket.szStatus, c_pszStatus, sizeof(failurePacket.szStatus));

	d->Packet(&failurePacket, sizeof(failurePacket));
}

CInputHandshake::CInputHandshake()
{
	CPacketInfoCG * pkPacketInfo = M2_NEW CPacketInfoCG;

	m_pMainPacketInfo = m_pPacketInfo;
	BindPacketInfo(pkPacketInfo);
}

CInputHandshake::~CInputHandshake()
{
	if(nullptr != m_pPacketInfo )
	{
		M2_DELETE(m_pPacketInfo);
		m_pPacketInfo = nullptr;
	}
}


std::map<uint32_t, CLoginSim *> g_sim;
std::map<uint32_t, CLoginSim *> g_simByPID;
std::vector<TPlayerTable> g_vec_save;

// BLOCK_CHAT
ACMD(do_block_chat);
// END_OF_BLOCK_CHAT

int CInputHandshake::Analyze(LPDESC d, uint8_t bHeader, const char * c_pData)
{
	if (bHeader == 10) // ���ʹ� ����
		return 0;

	/*if (bHeader == HEADER_CG_TEXT) {
		return 0;
	}
	else */if (bHeader == HEADER_CG_MARK_LOGIN)
	{
		if (!guild_mark_server)
		{
			// �������! - ��ũ ������ �ƴѵ� ��ũ�� ��û�Ϸ���?
			LOG_ERROR("Guild Mark login requested but i'm not a mark server!");
			d->SetPhase(PHASE_CLOSE);
			return 0;
		}

		// ������ ���� --;
		LOG_INFO("MARK_SERVER: Login");
		d->SetPhase(PHASE_LOGIN);
		return 0;
	}
	else if (bHeader == HEADER_CG_STATE_CHECKER)
	{
		if (d->isChannelStatusRequested()) {
			return 0;
		}
		d->SetChannelStatusRequested(true);
		db_clientdesc->DBPacket(HEADER_GD_REQUEST_CHANNELSTATUS, d->GetHandle(), nullptr, 0);
	}
	else if (bHeader == HEADER_CG_PONG)
		Pong(d);
	else if (bHeader == HEADER_CG_HANDSHAKE)
		Handshake(d, c_pData);
#ifdef _IMPROVED_PACKET_ENCRYPTION_
	else if (bHeader == HEADER_CG_KEY_AGREEMENT)
	{
		// Send out the key agreement completion packet first
		// to help client to enter encryption mode
		d->SendKeyAgreementCompleted();
		// Flush socket output before going encrypted
		d->ProcessOutput();

		TPacketKeyAgreement* p = (TPacketKeyAgreement*)c_pData;
		if (!d->IsCipherPrepared())
		{
			LOG_ERROR("Cipher isn't prepared. {} maybe a Hacker.", inet_ntoa(d->GetAddr().sin_addr));
			d->DelayedDisconnect(5);
			return 0;
		}
		if (d->FinishHandshake(p->wAgreedLength, p->data, p->wDataLength)) {
			// Handshaking succeeded
			if (g_bAuthServer) {
				d->SetPhase(PHASE_AUTH);
			} else {
				d->SetPhase(PHASE_LOGIN);
			}
		} else {
			LOG_ERROR("[CInputHandshake] Key agreement failed: al={} dl={}", p->wAgreedLength, p->wDataLength);
			d->SetPhase(PHASE_CLOSE);
		}
	}
#endif // _IMPROVED_PACKET_ENCRYPTION_
	else {
//#ifdef ENABLE_AUTH_PERFORMANCE
		d->SetPhase(PHASE_CLOSE);
		LOG_ERROR("Handshake phase does not handle packet {} (fd {})", static_cast<int>(bHeader), d->GetSocket());
		return 0;
//#else
//		LOG_ERROR("Handshake phase does not handle packet {} (fd {})", static_cast<int>(bHeader), d->GetSocket());
//#endif
	}

	return 0;
}



