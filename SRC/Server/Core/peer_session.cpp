#include "stdafx.h"
#include "peer_session.h"

PeerSession::PeerSession() : m_socket(auriga::net::Io()),
                             m_lastReceive(std::chrono::steady_clock::now()),
                             m_lastHeartbeat(std::chrono::steady_clock::now())
{
}

PeerSession::~PeerSession() = default;

socket_t PeerSession::GetSocketId() const
{
	return static_cast<socket_t>(const_cast<PeerSession*>(this)->m_socket.native_handle());
}

bool PeerSession::HasPendingIoWork() const
{
	return m_pendingAsyncOps.load(std::memory_order_acquire) > 0 ||
		m_queuedCompletions.load(std::memory_order_acquire) > 0;
}

int64_t PeerSession::IdleSeconds() const
{
	return std::chrono::duration_cast<std::chrono::seconds>(
		std::chrono::steady_clock::now() - m_lastReceive).count();
}

void PeerSession::SetHeartbeat(int intervalSeconds, int timeoutSeconds)
{
	m_heartbeatInterval = intervalSeconds;
	m_heartbeatTimeout = timeoutSeconds;
	m_lastHeartbeat = std::chrono::steady_clock::now();
}

void PeerSession::CheckHeartbeat()
{
	if (m_heartbeatInterval <= 0 || m_bSessionDestroyed || !m_ioConnected)
		return;

	const auto now = std::chrono::steady_clock::now();

	if (m_heartbeatTimeout > 0 &&
		now - m_lastReceive >= std::chrono::seconds(m_heartbeatTimeout))
	{
		OnPeerGone();
		return;
	}

	if (now - m_lastHeartbeat >= std::chrono::seconds(m_heartbeatInterval))
	{
		m_lastHeartbeat = now;
		OnHeartbeatSend();
	}
}

void PeerSession::CreateBuffers(int input_size, int output_size)
{
	m_lpInputBuffer = buffer_new(input_size);
	m_lpOutputBuffer = buffer_new(output_size);
}

void PeerSession::AttachSocket(auriga::net::tcp::socket&& sock)
{
	m_socket = std::move(sock);
	m_ioConnected = true;
	m_lastReceive = std::chrono::steady_clock::now();
	m_lastHeartbeat = m_lastReceive;
}

void PeerSession::ResetSession()
{
	++m_generation;
	m_bSessionDestroyed = false;
	m_writePending = false;
	m_writeChunk.clear();
	m_ioConnected = false;
	m_lastReceive = std::chrono::steady_clock::now();
	m_lastHeartbeat = m_lastReceive;
}

void PeerSession::CloseSocket()
{
	m_ioConnected = false;

	// The socket may only be touched on the io thread. When io work is
	// still outstanding (or a completion is queued for this session) the
	// close has to go through the io thread as well; the object is kept
	// alive by the extra pending token until that close ran.
	if (m_pendingAsyncOps.load(std::memory_order_acquire) > 0 ||
		m_queuedCompletions.load(std::memory_order_acquire) > 0)
	{
		const uint64_t generation = m_generation;
		m_pendingAsyncOps.fetch_add(1, std::memory_order_acq_rel);

		auriga::net::PostIo([this, generation]() { IoClose(generation); });
		return;
	}

	boost::system::error_code ec;
	m_socket.close(ec);
}

void PeerSession::DestroySession()
{
	if (m_bSessionDestroyed)
		return;

	m_bSessionDestroyed = true;

	CloseSocket();

	if (m_lpInputBuffer)
	{
		buffer_delete(m_lpInputBuffer);
		m_lpInputBuffer = nullptr;
	}

	if (m_lpOutputBuffer)
	{
		buffer_delete(m_lpOutputBuffer);
		m_lpOutputBuffer = nullptr;
	}
}

void PeerSession::IoClose(uint64_t generation)
{
	boost::system::error_code ec;
	m_socket.close(ec);

	// Consume the token that CloseSocket() acquired for this post.
	QueueGameCompletion(generation, []() {});
}

void PeerSession::StartRead()
{
	if (m_bSessionDestroyed || m_deletePending || !m_ioConnected)
		return;

	const uint64_t generation = m_generation;
	m_pendingAsyncOps.fetch_add(1, std::memory_order_acq_rel);

	auriga::net::PostIo([this, generation]() { ArmRead(generation); });
}

void PeerSession::ArmRead(uint64_t generation)
{
	m_socket.async_read_some(
		boost::asio::buffer(m_readChunk),
		[this, generation](const boost::system::error_code & ec, std::size_t bytes)
		{
			const bool rearm = (!ec && bytes > 0);

			// Acquire the token for the follow-up read before the handoff,
			// so the deferred-delete check can never observe a zero count
			// while the next read is already in flight.
			if (rearm)
				m_pendingAsyncOps.fetch_add(1, std::memory_order_acq_rel);

			std::vector<char> data;

			if (rearm)
				data.assign(m_readChunk.data(), m_readChunk.data() + bytes);

			QueueGameCompletion(generation,
				[this, ec, data = std::move(data)]() mutable
				{
					OnRead(ec, std::move(data));
				});

			// The re-arm stays on the io thread: the live stream does not
			// wait for the next game tick.
			if (rearm)
				ArmRead(generation);
		});
}

void PeerSession::OnRead(const boost::system::error_code & ec, std::vector<char> data)
{
	if (ec)
	{
		if (ec != boost::asio::error::operation_aborted)
			OnPeerGone();
		return;
	}

	if (data.empty())
	{
		OnPeerGone();
		return;
	}

	m_lastReceive = std::chrono::steady_clock::now();

	if (m_lpInputBuffer)
	{
		buffer_write(m_lpInputBuffer, data.data(), static_cast<int>(data.size()));
		OnInput();
	}
}

void PeerSession::QueueOutput(const void * data, int size)
{
	if (!m_lpOutputBuffer || size <= 0)
		return;

	buffer_write(m_lpOutputBuffer, data, size);
	StartWrite();
}

void PeerSession::StartWrite()
{
	if (m_writePending || m_bSessionDestroyed || m_deletePending || !m_ioConnected)
		return;

	const int bytes = m_lpOutputBuffer ? static_cast<int>(buffer_size(m_lpOutputBuffer)) : 0;

	if (bytes <= 0)
		return;

	// The staging chunk is prepared on the game thread; the io thread only
	// reads it while the write is in flight (m_writePending guards reuse).
	const char * readPoint = static_cast<const char *>(buffer_read_peek(m_lpOutputBuffer));
	m_writeChunk.assign(readPoint, readPoint + bytes);

	m_writePending = true;
	const uint64_t generation = m_generation;
	m_pendingAsyncOps.fetch_add(1, std::memory_order_acq_rel);

	auriga::net::PostIo([this, generation]() { ArmWrite(generation); });
}

void PeerSession::ArmWrite(uint64_t generation)
{
	boost::asio::async_write(
		m_socket,
		boost::asio::buffer(m_writeChunk),
		[this, generation](const boost::system::error_code & ec, std::size_t bytes_transferred)
		{
			QueueGameCompletion(generation,
				[this, ec, bytes_transferred]()
				{
					m_writePending = false;
					OnWrite(ec, bytes_transferred);
				});
		});
}

void PeerSession::OnWrite(const boost::system::error_code & ec, std::size_t bytes)
{
	if (ec)
	{
		m_writeChunk.clear();

		if (ec != boost::asio::error::operation_aborted)
			OnPeerGone();
		return;
	}

	OnBytesWritten(bytes);

	buffer_read_proceed(m_lpOutputBuffer, static_cast<int>(bytes));
	m_writeChunk.clear();

	if (buffer_size(m_lpOutputBuffer) > 0)
		StartWrite();

	OnOutput();
}

void PeerSession::QueueGameCompletion(uint64_t generation, std::function<void()> fn)
{
	// The queued counter is bumped before the completion becomes visible to
	// the game thread and released when the game loop ran it, so the
	// deferred-delete check can never free the session while a completion
	// still references it.
	m_queuedCompletions.fetch_add(1, std::memory_order_acq_rel);

	auriga::net::PostCompletion(
		[this, generation, fn = std::move(fn)]() mutable
		{
			m_queuedCompletions.fetch_sub(1, std::memory_order_acq_rel);

			if (generation != m_generation || m_bSessionDestroyed || m_deletePending)
				return;

			fn();
		});

	m_pendingAsyncOps.fetch_sub(1, std::memory_order_acq_rel);
}
