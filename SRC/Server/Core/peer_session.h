#pragma once

#include <Core/asio_engine.h>
#include <Core/buffer.h>
#include <Core/typedef.h>

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace auriga::net
{
	class TlsServerContext;
}

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
// Threading: the socket is only touched by the dedicated io thread. Game
// logic only ever runs on the game thread: io completions copy their result
// and enqueue a callback through QueueGameCompletion(), which the game loop
// processes in DrainCompletions(). Socket mutations requested by the game
// thread (initial read arm, write starts, close) are posted to the io thread.
//
// Deferred deletion is handled here: an object may only be destroyed when
// HasPendingIoWork() is false, i.e. no asynchronous operation and no queued
// completion references it anymore (see MarkDeletePending()).
class PeerSession
{
public:
	PeerSession();
	virtual ~PeerSession();

	PeerSession(const PeerSession&) = delete;
	PeerSession& operator=(const PeerSession&) = delete;

	bool		IsConnected() const		{ return m_ioConnected; }
	socket_t	GetSocketId() const;
	int		PendingAsyncOps() const		{ return m_pendingAsyncOps.load(std::memory_order_acquire); }
	bool		HasPendingIoWork() const;
	bool		IsDeletePending() const		{ return m_deletePending; }
	void		MarkDeletePending()		{ m_deletePending = true; }
	int64_t		IdleSeconds() const;

	// Link level liveness handling. The owner loops must call
	// CheckHeartbeat() periodically; interval == 0 disables sending,
	// timeout == 0 disables the idle timeout. Runs on the game thread.
	void		SetHeartbeat(int intervalSeconds, int timeoutSeconds);
	void		CheckHeartbeat();

	// TLS sessions only: a peer that has not finished the handshake within
	// this time is dropped by CheckHeartbeat() (default 10 seconds).
	void		SetTlsHandshakeTimeout(std::chrono::milliseconds timeout) { m_tlsHandshakeTimeout = timeout; }
	bool		IsTls() const			{ return m_tls != nullptr; }
	bool		IsTlsReady() const		{ return m_tlsReady; }

protected:
	void		CreateBuffers(int input_size, int output_size);
	void		AttachSocket(auriga::net::tcp::socket&& sock);

	// Wraps an accepted socket in a server TLS stream and starts the
	// handshake on the io thread. Until it is done, queued output stays in
	// the buffer and StartRead() only takes note; both go once the stream is
	// up. A failed handshake ends in OnPeerGone(). A session stays TLS for
	// its whole lifetime.
	void		AttachTlsSocket(auriga::net::tcp::socket&& sock, auriga::net::TlsServerContext& context);

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

	// Cached native socket handle for logging/status queries. The game
	// thread must never read m_socket itself while the io thread owns it.
	socket_t	m_socketHandle { static_cast<socket_t>(INVALID_SOCKET) };

	LPBUFFER	m_lpInputBuffer { nullptr };
	LPBUFFER	m_lpOutputBuffer { nullptr };

protected:
	// Subclasses may start their own asynchronous operations (for example
	// the connecting client descriptors), so these bookkeeping members are
	// available to them as well.
	//
	// QueueGameCompletion() runs on the io thread, consumes exactly one
	// pending token and enqueues fn for the game thread. The game loop calls
	// OnRead/OnWrite with the copied completion result.
	void		QueueGameCompletion(uint64_t generation, std::function<void()> fn);

	void		OnRead(const boost::system::error_code & ec, std::vector<char> data);
	void		OnWrite(const boost::system::error_code & ec, std::size_t bytes);

	std::array<char, 65536>	m_readChunk;
	std::vector<char>	m_writeChunk;

	bool		m_writePending { false };
	std::atomic<int>	m_pendingAsyncOps { 0 };
	std::atomic<int>	m_queuedCompletions { 0 };
	bool		m_deletePending { false };
	bool		m_bSessionDestroyed { false };
	bool		m_ioConnected { false };
	uint64_t	m_generation { 0 };

	int		m_heartbeatInterval { 0 };
	int		m_heartbeatTimeout { 0 };

	std::chrono::steady_clock::time_point	m_lastReceive;
	std::chrono::steady_clock::time_point	m_lastHeartbeat;

private:
	// io thread entry points.
	void		ArmRead(uint64_t generation);
	void		ArmWrite(uint64_t generation);
	void		ArmTlsHandshake(uint64_t generation);
	void		IoClose(uint64_t generation);

	void		OnTlsHandshake(const boost::system::error_code & ec);
	void		CloseTransport();

	// The TLS stream owns the socket of a TLS session; m_socket stays unused
	// then. Assigned on the game thread before any io work is posted.
	struct TlsStream;
	std::unique_ptr<TlsStream>	m_tls;
	bool		m_tlsReady { false };
	bool		m_readAfterTls { false };
	std::chrono::steady_clock::time_point	m_tlsStarted;
	std::chrono::milliseconds	m_tlsHandshakeTimeout { 10000 };
};
