#include "stdafx.h"
#include "peer_session.h"

PeerSession::PeerSession() : m_socket(auriga::net::Io()), m_lastActivity(std::chrono::steady_clock::now())
{
}

PeerSession::~PeerSession() = default;

socket_t PeerSession::GetSocketId() const
{
	return static_cast<socket_t>(const_cast<PeerSession*>(this)->m_socket.native_handle());
}

int64_t PeerSession::IdleSeconds() const
{
	return std::chrono::duration_cast<std::chrono::seconds>(
		std::chrono::steady_clock::now() - m_lastActivity).count();
}

void PeerSession::CreateBuffers(int input_size, int output_size)
{
	m_lpInputBuffer = buffer_new(input_size);
	m_lpOutputBuffer = buffer_new(output_size);
}

void PeerSession::AttachSocket(auriga::net::tcp::socket&& sock)
{
	m_socket = std::move(sock);
	m_lastActivity = std::chrono::steady_clock::now();
}

void PeerSession::ResetSession()
{
	++m_generation;
	m_bSessionDestroyed = false;
	m_readPending = false;
	m_writePending = false;
	m_writeChunk.clear();
	m_lastActivity = std::chrono::steady_clock::now();
}

void PeerSession::CloseSocket()
{
	if (m_socket.is_open())
	{
		boost::system::error_code ec;
		m_socket.close(ec);
	}
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

void PeerSession::StartRead()
{
	if (m_readPending || m_bSessionDestroyed || m_deletePending || !m_socket.is_open())
		return;

	m_readPending = true;
	++m_pendingAsyncOps;

	const uint64_t generation = m_generation;

	m_socket.async_read_some(
		boost::asio::buffer(m_readChunk),
		[this, generation](const boost::system::error_code & ec, std::size_t bytes)
		{
			--m_pendingAsyncOps;

			if (generation != m_generation || m_bSessionDestroyed || m_deletePending)
				return;

			m_readPending = false;
			OnRead(ec, bytes);
		});
}

void PeerSession::OnRead(const boost::system::error_code & ec, std::size_t bytes)
{
	if (ec)
	{
		if (ec != boost::asio::error::operation_aborted)
			OnPeerGone();
		return;
	}

	if (bytes == 0)
	{
		OnPeerGone();
		return;
	}

	m_lastActivity = std::chrono::steady_clock::now();

	if (m_lpInputBuffer)
	{
		buffer_write(m_lpInputBuffer, m_readChunk.data(), static_cast<int>(bytes));
		OnInput();
	}

	if (IsConnected())
		StartRead();
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
	if (m_writePending || m_bSessionDestroyed || m_deletePending || !m_socket.is_open())
		return;

	const int bytes = m_lpOutputBuffer ? static_cast<int>(buffer_size(m_lpOutputBuffer)) : 0;

	if (bytes <= 0)
		return;

	const char * readPoint = static_cast<const char *>(buffer_read_peek(m_lpOutputBuffer));
	m_writeChunk.assign(readPoint, readPoint + bytes);

	m_writePending = true;
	++m_pendingAsyncOps;

	const uint64_t generation = m_generation;

	boost::asio::async_write(
		m_socket,
		boost::asio::buffer(m_writeChunk),
		[this, generation](const boost::system::error_code & ec, std::size_t bytes_transferred)
		{
			--m_pendingAsyncOps;

			if (generation != m_generation || m_bSessionDestroyed || m_deletePending)
				return;

			m_writePending = false;
			OnWrite(ec, bytes_transferred);
		});
}

void PeerSession::OnWrite(const boost::system::error_code & ec, std::size_t bytes)
{
	if (ec)
	{
		if (ec != boost::asio::error::operation_aborted)
			OnPeerGone();
		return;
	}

	m_lastActivity = std::chrono::steady_clock::now();

	OnBytesWritten(bytes);

	buffer_read_proceed(m_lpOutputBuffer, static_cast<int>(bytes));
	m_writeChunk.clear();

	if (buffer_size(m_lpOutputBuffer) > 0)
		StartWrite();

	OnOutput();
}
