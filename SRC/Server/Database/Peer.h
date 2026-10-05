#pragma once

#include <Core/peer_auth.h>

#include "PeerBase.h"
#include <common/CommonDefines.h>

class CPeer : public CPeerBase
{
    protected:
	virtual void OnAccept();
	virtual void OnClose();
	virtual void OnRecvData();
	virtual void OnDisconnect();

    public:
#pragma pack(1)
	typedef struct _header
	{
	    uint8_t    bHeader;
	    uint32_t   dwHandle;
	    uint32_t   dwSize;
	} HEADER;
#pragma pack()
	enum EState
	{
	    STATE_CLOSE = 0,
	    STATE_PLAYING = 1
	};

	CPeer();
	virtual ~CPeer();

	void	EncodeHeader(uint8_t header, uint32_t dwHandle, uint32_t dwSize);
	bool 	PeekPacket(int & iBytesProceed, uint8_t & header, uint32_t & dwHandle, uint32_t & dwLength, const char ** data);
	void	EncodeReturn(uint8_t header, uint32_t dwHandle);

	void	ProcessInput();
	int	Send();

	uint32_t	GetHandle();
	uint32_t	GetUserCount();
	void	SetUserCount(uint32_t dwCount);

	bool	IsAuthenticated() const	{ return m_bAuthenticated; }
	void	SetAuthenticated(bool b);

	void	SendAuthChallenge();
	bool	VerifyAuthResponse(const uint8_t mac[PeerAuth::MAC_SIZE]);

	void	SetPublicIP(const char * ip)	{ m_stPublicIP = ip; }
	const char * GetPublicIP()		{ return m_stPublicIP.c_str(); }

	void	SetChannel(uint8_t bChannel)	{ m_bChannel = bChannel; }
	uint8_t	GetChannel()			{ return m_bChannel; }

	void	SetListenPort(uint16_t wPort) { m_wListenPort = wPort; }
	uint16_t	GetListenPort() { return m_wListenPort; }

	void	SetP2PPort(uint16_t wPort);
	uint16_t	GetP2PPort() { return m_wP2PPort; }

	void	SetMaps(int32_t* pl);
	int32_t*	GetMaps() { return &m_alMaps[0]; }

	bool	SetItemIDRange(TItemIDRangeTable itemRange);
	bool	SetSpareItemIDRange(TItemIDRangeTable itemRange);
	bool	CheckItemIDRangeCollision(TItemIDRangeTable itemRange);
	void	SendSpareItemIDRange();

    private:
	int	m_state;
	bool	m_bAuthenticated { false };
	bool	m_bAuthChallengeSent { false };
	uint8_t	m_authNonce[PeerAuth::NONCE_SIZE] {};
	uint64_t m_authTimestamp { 0 };

	uint8_t	m_bChannel;
	uint32_t	m_dwHandle;
	uint32_t	m_dwUserCount;
	uint16_t	m_wListenPort;
	uint16_t	m_wP2PPort;
	int32_t	m_alMaps[MAP_ALLOW_LIMIT];

	TItemIDRangeTable m_itemRange;
	TItemIDRangeTable m_itemSpareRange;

	std::string m_stPublicIP;
};

