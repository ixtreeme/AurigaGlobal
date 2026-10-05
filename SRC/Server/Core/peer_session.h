#pragma once

#include <Core/asio_engine.h>
#include <Core/buffer.h>
#include <Core/typedef.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <vector>

// Shared transport session for the server-to-server links.
//
// Owns the socket, the input/output byte buffers and the asynchronous
// read/write loop. The concrete peer classes (GameServer DESC, Database
// CPeerBase) only implement the protocol layer on top:
//
//   OnInput()     - parse the bytes appended to m_lpInputBuffer
//   OnPeerGone()  - EOF/error, start the subclass teardown
//   OnOutput()    - optional post-write hook
//
// Deferred deletion is handled here: every pending completion counts in
// m_pendingAsyncOps, so the owners can keep the object alive until the
// aborted handlers have run (see MarkDeletePending()).
class PeerSession
{
public:
	PeerSession();
	virtual ~PeerSession();

	PeerSession(const PeerSession&) = delete;
	PeerSession& operator=(const PeerSession&) = delete;

	bool		IsConnected() const		{ return m_socket.is_open(); }
	socket_t	GetSocketId() const;
	int		PendingAsyncOps() const	{ return m_pendingAsyncOps; }
	bool		IsDeletePending() const	{ return m_deletePending; }
	void		MarkDeletePending()		{ m_deletePending = true; }
	int64_t		IdleSeconds() const;

	// Link level liveness handling. The owner loops must call
	// CheckHeartbeat() periodically; interval == 0 disables sending,
	// timeout == 0 disables the idle timeout.
	void		SetHeartbeat(int intervalSeconds, int timeoutSeconds);
	void		CheckHeartbeat();

protected:
	void		CreateBuffers(int input_size, int output_size);
	void		AttachSocket(auriga::net::tcp::socket&& sock);

	void		StartRead();
	void		StartWrite();
	void		QueueOutput(const void * data, int size);
	void		CloseSocket();
	void		DestroySession();
	void		ResetSession();

	virtual void	OnInput() = 0;
	virtual void	OnPeerGone() = 0;
	virtual void	OnOutput() {}
	virtual void	OnBytesWritten(std::size_t bytes) { (void)bytes; }

	// Called when the link was idle for the configured interval; the
	// subclass sends the protocol specific ping packet.
	virtual void	OnHeartbeatSend() {}

	auriga::net::tcp::socket	m_socket;
	LPBUFFER	m_lpInputBuffer { nullptr };
	LPBUFFER	m_lpOutputBuffer { nullptr };

protected:
	// Subclasses may start their own asynchronous operations (for example
	// the connecting client descriptors), so these bookkeeping members are
	// available to them as well.
	void		OnRead(const boost::system::error_code & ec, std::size_t bytes);
	void		OnWrite(const boost::system::error_code & ec, std::size_t bytes);

	std::array<char, 65536>	m_readChunk;
	std::vector<char>	m_writeChunk;

	bool		m_readPending { false };
	bool		m_writePending { false };
	int		m_pendingAsyncOps { 0 };
	bool		m_deletePending { false };
	bool		m_bSessionDestroyed { false };
	uint64_t	m_generation { 0 };

	int		m_heartbeatInterval { 0 };
	int		m_heartbeatTimeout { 0 };

	std::chrono::steady_clock::time_point	m_lastReceive;
	std::chrono::steady_clock::time_point	m_lastHeartbeat;
};
