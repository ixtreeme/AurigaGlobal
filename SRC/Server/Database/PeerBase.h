// vim:ts=8 sw=4
#ifndef __INC_PEERBASE_H__
#define __INC_PEERBASE_H__

#include <Core/peer_session.h>

#include "NetBase.h"

class CPeerBase : public CNetBase, public PeerSession
{
    public:
	enum
	{
	    MAX_HOST_LENGTH		= 30,
	    MAX_INPUT_LEN		= 1024 * 1024 * 2,
	    DEFAULT_PACKET_BUFFER_SIZE	= 1024 * 1024 * 2
	};

    protected:
	virtual void	OnAccept() = 0;
	virtual void	OnRecvData() = 0;
	virtual void	OnDisconnect() = 0;
	virtual void	OnClose() = 0;

    public:
	bool		Accept(auriga::net::tcp::socket&& sock);
	void		Close();

    public:
	CPeerBase();
	virtual ~CPeerBase();

	void		Disconnect();
	void		Destroy();

	socket_t	GetFd() { return GetSocketId(); }

	void		EncodeBYTE(uint8_t b);
	void		EncodeWORD(uint16_t w);
	void		EncodeDWORD(uint32_t dw);
	void		Encode(const void* data, uint32_t size);
	int		Send();

	void		RecvEnd(int proceed_bytes);
	int		GetRecvLength();
	const void *	GetRecvBuffer();

	int		GetSendLength();

	const char *	GetHost() { return m_host; }
	uint16_t	GetPort() { return m_wPort; }

    protected:
	virtual void	OnInput() override;
	virtual void	OnPeerGone() override;

	char		m_host[MAX_HOST_LENGTH + 1];
	uint16_t	m_wPort;

    private:
	int		m_BytesRemain;
};

#endif
