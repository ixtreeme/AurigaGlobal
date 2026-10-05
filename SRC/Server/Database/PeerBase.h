// vim:ts=8 sw=4
#ifndef __INC_PEERBASE_H__
#define __INC_PEERBASE_H__

#include <Core/asio_engine.h>

#include <array>
#include <vector>

#include "NetBase.h"

class CPeerBase : public CNetBase
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

	socket_t	GetFd() { return static_cast<socket_t>(m_socket.native_handle()); }
	bool		IsConnected() const { return m_socket.is_open(); }

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

	int		PendingAsyncOps() const { return m_pendingAsyncOps; }
	bool		IsDeletePending() const { return m_deletePending; }
	void		MarkDeletePending() { m_deletePending = true; }

    protected:
	void		StartRead();
	void		StartWrite();
	void		OnRead(const boost::system::error_code & ec, std::size_t bytes);
	void		OnWrite(const boost::system::error_code & ec, std::size_t bytes);

	char		m_host[MAX_HOST_LENGTH + 1];
	uint16_t	m_wPort;

    private:
	int		m_BytesRemain;
	LPBUFFER	m_outBuffer;
	LPBUFFER	m_inBuffer;

	auriga::net::tcp::socket	m_socket;
	bool		m_bDestroyed { false };
	bool		m_readPending { false };
	bool		m_writePending { false };
	int		m_pendingAsyncOps { 0 };
	bool		m_deletePending { false };
	uint64_t	m_generation { 0 };
	std::array<char, 65536>	m_readChunk;
	std::vector<char>	m_writeChunk;
};

#endif
