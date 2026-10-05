#include "stdafx.h"
#include "Peer.h"
#include "ClientManager.h"
#include "ItemIDRangeManager.h"

CPeer::CPeer()
{
	m_state = 0;
	m_bChannel = 0;
	m_dwHandle = 0;
	m_dwUserCount = 0;
	m_wListenPort = 0;
	m_wP2PPort = 0;

	memset(m_alMaps, 0, sizeof(m_alMaps));

	m_itemRange.dwMin = m_itemRange.dwMax = m_itemRange.dwUsableItemIDMin = 0;
	m_itemSpareRange.dwMin = m_itemSpareRange.dwMax = m_itemSpareRange.dwUsableItemIDMin = 0;
}

CPeer::~CPeer()
{
	Close();
}

void CPeer::OnAccept()
{
	m_state = STATE_PLAYING;

	static uint32_t current_handle = 0;
	m_dwHandle = ++current_handle;

	LOG_INFO("Connection accepted. (host: {} handle: {} fd: {})", m_host, m_dwHandle, GetFd());
}

void CPeer::OnRecvData()
{
	CClientManager::instance().ProcessPackets(this);
}

void CPeer::OnDisconnect()
{
	CClientManager::instance().RemovePeer(this);
}

void CPeer::OnClose()
{
	m_state = STATE_CLOSE;

	LOG_INFO("Connection closed. (host: {})", m_host);
	LOG_INFO("ItemIDRange: returned. {} ~ {}", m_itemRange.dwMin, m_itemRange.dwMax);

	CItemIDRangeManager::instance().UpdateRange(m_itemRange.dwMin, m_itemRange.dwMax);

	m_itemRange.dwMin = 0;
	m_itemRange.dwMax = 0;
	m_itemRange.dwUsableItemIDMin = 0;
}

uint32_t CPeer::GetHandle()
{
	return m_dwHandle;
}

uint32_t CPeer::GetUserCount()
{
	return m_dwUserCount;
}

void CPeer::SetUserCount(uint32_t dwCount)
{
	m_dwUserCount = dwCount;
}

void CPeer::SetAuthenticated(bool b)
{
	m_bAuthenticated = b;

	// Until the challenge is verified only the auth timeout runs; after a
	// successful handshake the regular keepalive takes over.
	SetHeartbeat(b ? 30 : 0, b ? 90 : 10);
}

void CPeer::SendAuthChallenge()
{
	uint8_t nonce[PeerAuth::NONCE_SIZE];
	uint8_t timestamp[PeerAuth::TIMESTAMP_SIZE];
	uint8_t mac[PeerAuth::MAC_SIZE];

	PeerAuth::MakeNonce(nonce);
	memcpy(m_authNonce, nonce, sizeof(m_authNonce));

	m_authTimestamp = PeerAuth::NowSeconds();
	PeerAuth::EncodeTimestamp(m_authTimestamp, timestamp);
	PeerAuth::ComputeAuthMac(PeerAuth::ROLE_SERVER, nonce, m_authTimestamp, mac);

	m_bAuthChallengeSent = true;

	EncodeHeader(HEADER_DG_AUTH_CHALLENGE, 0,
		PeerAuth::NONCE_SIZE + PeerAuth::TIMESTAMP_SIZE + PeerAuth::MAC_SIZE);
	Encode(nonce, PeerAuth::NONCE_SIZE);
	Encode(timestamp, PeerAuth::TIMESTAMP_SIZE);
	Encode(mac, PeerAuth::MAC_SIZE);
	Send();
}

bool CPeer::VerifyAuthResponse(const uint8_t mac[PeerAuth::MAC_SIZE])
{
	if (!m_bAuthChallengeSent || !PeerAuth::IsTimestampFresh(m_authTimestamp))
		return false;

	const bool ok = PeerAuth::VerifyAuthMac(PeerAuth::ROLE_CLIENT, m_authNonce, m_authTimestamp, mac);

	// The challenge is single use.
	memset(m_authNonce, 0, sizeof(m_authNonce));
	m_authTimestamp = 0;
	m_bAuthChallengeSent = false;

	return ok;
}

bool CPeer::PeekPacket(int & iBytesProceed, uint8_t & header, uint32_t & dwHandle, uint32_t & dwLength, const char ** data)
{
	if (GetRecvLength() < iBytesProceed + 9)
		return false;

	const char * buf = (const char *) GetRecvBuffer();
	buf += iBytesProceed;

	header	= *(buf++);

	dwHandle	= *((uint32_t *) buf);
	buf		+= sizeof(uint32_t);

	dwLength	= *((uint32_t *) buf);
	buf		+= sizeof(uint32_t);

	//LOG_INFO("{} header {} handle {} length {}", GetRecvLength(), header, dwHandle, dwLength);
	if (iBytesProceed + dwLength + 9 > (uint32_t) GetRecvLength())
	{
		LOG_INFO("PeekPacket: not enough buffer size: len {}, recv {}", 9+dwLength, GetRecvLength()-iBytesProceed);
		return false;
	}

	*data = buf;
	iBytesProceed += dwLength + 9;
	return true;
}

void CPeer::EncodeHeader(uint8_t header, uint32_t dwHandle, uint32_t dwSize)
{
	HEADER h;

	LOG_TRACE("EncodeHeader {} handle {} size {}", header, dwHandle, dwSize);

	h.bHeader = header;
	h.dwHandle = dwHandle;
	h.dwSize = dwSize;

	Encode(&h, sizeof(HEADER));
}

void CPeer::EncodeReturn(uint8_t header, uint32_t dwHandle)
{
	EncodeHeader(header, dwHandle, 0);
}

int CPeer::Send()
{
	if (m_state == STATE_CLOSE)
		return -1;

	return (CPeerBase::Send());
}

void CPeer::SetP2PPort(uint16_t wPort)
{
	m_wP2PPort = wPort;
}

void CPeer::SetMaps(int32_t * pl)
{
	memcpy(m_alMaps, pl, sizeof(m_alMaps));
}

void CPeer::SendSpareItemIDRange()
{
	if (m_itemSpareRange.dwMin == 0 || m_itemSpareRange.dwMax == 0 || m_itemSpareRange.dwUsableItemIDMin == 0)
	{
		EncodeHeader(HEADER_DG_ACK_SPARE_ITEM_ID_RANGE, 0, sizeof(TItemIDRangeTable));
		Encode(&m_itemSpareRange, sizeof(TItemIDRangeTable));
	}
	else
	{
		SetItemIDRange(m_itemSpareRange);

		if (SetSpareItemIDRange(CItemIDRangeManager::instance().GetRange()) == false)
		{
			LOG_INFO("ItemIDRange: spare range set error");
			m_itemSpareRange.dwMin = m_itemSpareRange.dwMax = m_itemSpareRange.dwUsableItemIDMin = 0;
		}

		EncodeHeader(HEADER_DG_ACK_SPARE_ITEM_ID_RANGE, 0, sizeof(TItemIDRangeTable));
		Encode(&m_itemSpareRange, sizeof(TItemIDRangeTable));
	}
}

bool CPeer::SetItemIDRange(TItemIDRangeTable itemRange)
{
	if (itemRange.dwMin == 0 || itemRange.dwMax == 0 || itemRange.dwUsableItemIDMin == 0) return false;

	m_itemRange = itemRange;
	LOG_INFO("ItemIDRange: SET {} {} ~ {} start: {}", GetPublicIP(), m_itemRange.dwMin, m_itemRange.dwMax, m_itemRange.dwUsableItemIDMin);

	return true;
}

bool CPeer::SetSpareItemIDRange(TItemIDRangeTable itemRange)
{
	if (itemRange.dwMin == 0 || itemRange.dwMax == 0 || itemRange.dwUsableItemIDMin == 0) return false;

	m_itemSpareRange = itemRange;
	LOG_INFO("ItemIDRange: SPARE SET {} {} ~ {} start: {}", GetPublicIP(), m_itemSpareRange.dwMin, m_itemSpareRange.dwMax, m_itemSpareRange.dwUsableItemIDMin);

	return true;
}

bool CPeer::CheckItemIDRangeCollision(TItemIDRangeTable itemRange)
{
	if (m_itemRange.dwMin < itemRange.dwMax && m_itemRange.dwMax > itemRange.dwMin)
	{
		LOG_ERROR("ItemIDRange: Collision!! this {} ~ {} check {} ~ {}", m_itemRange.dwMin, m_itemRange.dwMax, itemRange.dwMin, itemRange.dwMax);
		return false;
	}

	if (m_itemSpareRange.dwMin < itemRange.dwMax && m_itemSpareRange.dwMax > itemRange.dwMin)
	{
		LOG_ERROR("ItemIDRange: Collision with spare range this {} ~ {} check {} ~ {}", m_itemSpareRange.dwMin, m_itemSpareRange.dwMax, itemRange.dwMin, itemRange.dwMax);
		return false;
	}

	return true;
}


