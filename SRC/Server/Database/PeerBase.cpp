#include "stdafx.h"
#include "PeerBase.h"
#include <common/CommonDefines.h>

CPeerBase::CPeerBase() : m_host{}, m_wPort(0), m_BytesRemain(0), m_outBuffer(nullptr),
                         m_inBuffer(nullptr), m_socket(auriga::net::Io())
{
}

CPeerBase::~CPeerBase()
{
	Destroy();
}

void CPeerBase::Disconnect()
{
	if (m_socket.is_open())
	{
		boost::system::error_code ec;
		m_socket.close(ec);
	}
}

void CPeerBase::Destroy()
{
	if (m_bDestroyed)
		return;

	m_bDestroyed = true;

	Disconnect();

	if (m_outBuffer)
	{
		buffer_delete(m_outBuffer);
		m_outBuffer = nullptr;
	}

	if (m_inBuffer)
	{
		buffer_delete(m_inBuffer);
		m_inBuffer = nullptr;
	}
}

bool CPeerBase::Accept(auriga::net::tcp::socket&& sock)
{
	m_socket = std::move(sock);

	boost::system::error_code ec;
	const auto endpoint = m_socket.remote_endpoint(ec);

	if (ec)
	{
		Destroy();
		return false;
	}

	m_socket.set_option(boost::asio::socket_base::send_buffer_size(233016), ec);
	m_socket.set_option(boost::asio::socket_base::receive_buffer_size(233016), ec);

	strlcpy(m_host, endpoint.address().to_string().c_str(), sizeof(m_host));
	m_wPort = htons(endpoint.port());

	m_outBuffer = buffer_new(DEFAULT_PACKET_BUFFER_SIZE);
	m_inBuffer = buffer_new(MAX_INPUT_LEN);

	if (!m_outBuffer || !m_inBuffer)
	{
		Destroy();
		return false;
	}

	StartRead();

	OnAccept();
	LOG_INFO("ACCEPT FROM {}", m_host);
	return true;
}

void CPeerBase::Close()
{
	OnClose();
}

void CPeerBase::EncodeBYTE(uint8_t b)
{
	if (!m_outBuffer)
	{
		LOG_ERROR("Not ready to write");
		return;
	}

	buffer_write(m_outBuffer, &b, 1);
	StartWrite();
}

void CPeerBase::EncodeWORD(uint16_t w)
{
	if (!m_outBuffer)
	{
		LOG_ERROR("Not ready to write");
		return;
	}

	buffer_write(m_outBuffer, &w, 2);
	StartWrite();
}

void CPeerBase::EncodeDWORD(uint32_t dw)
{
	if (!m_outBuffer)
	{
		LOG_ERROR("Not ready to write");
		return;
	}

	buffer_write(m_outBuffer, &dw, 4);
	StartWrite();
}

void CPeerBase::Encode(const void* data, uint32_t size)
{
	if (!m_outBuffer)
	{
		LOG_ERROR("Not ready to write");
		return;
	}

	buffer_write(m_outBuffer, data, size);
	StartWrite();
}

void CPeerBase::RecvEnd(int proceed_bytes)
{
	buffer_read_proceed(m_inBuffer, proceed_bytes);
	m_BytesRemain = buffer_size(m_inBuffer);
}

int CPeerBase::GetRecvLength()
{
	return m_BytesRemain;
}

const void * CPeerBase::GetRecvBuffer()
{
	return buffer_read_peek(m_inBuffer);
}

int CPeerBase::GetSendLength()
{
	return buffer_size(m_outBuffer);
}

int CPeerBase::Send()
{
	if (!m_socket.is_open())
		return -1;

	StartWrite();
	return 0;
}

void CPeerBase::StartRead()
{
	if (m_readPending || m_bDestroyed || m_deletePending || !m_socket.is_open())
		return;

	m_readPending = true;
	++m_pendingAsyncOps;

	const uint64_t generation = m_generation;

	m_socket.async_read_some(
		boost::asio::buffer(m_readChunk),
		[this, generation](const boost::system::error_code & ec, std::size_t bytes)
		{
			--m_pendingAsyncOps;

			if (generation != m_generation || m_bDestroyed || m_deletePending)
				return;

			m_readPending = false;
			OnRead(ec, bytes);
		});
}

void CPeerBase::OnRead(const boost::system::error_code & ec, std::size_t bytes)
{
	if (ec)
	{
		if (ec != boost::asio::error::operation_aborted)
			OnDisconnect();
		return;
	}

	if (bytes == 0)
	{
		OnDisconnect();
		return;
	}

	if (m_inBuffer)
	{
		buffer_write(m_inBuffer, m_readChunk.data(), static_cast<int>(bytes));
		m_BytesRemain = buffer_size(m_inBuffer);
		OnRecvData();
	}

	StartRead();
}

void CPeerBase::StartWrite()
{
	if (m_writePending || m_bDestroyed || m_deletePending || !m_socket.is_open())
		return;

	const int bytes = m_outBuffer ? static_cast<int>(buffer_size(m_outBuffer)) : 0;

	if (bytes <= 0)
		return;

	const char * readPoint = static_cast<const char *>(buffer_read_peek(m_outBuffer));
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

			if (generation != m_generation || m_bDestroyed || m_deletePending)
				return;

			m_writePending = false;
			OnWrite(ec, bytes_transferred);
		});
}

void CPeerBase::OnWrite(const boost::system::error_code & ec, std::size_t bytes)
{
	if (ec)
	{
		if (ec != boost::asio::error::operation_aborted)
			OnDisconnect();
		return;
	}

	buffer_read_proceed(m_outBuffer, static_cast<int>(bytes));
	m_writeChunk.clear();

	if (buffer_size(m_outBuffer) > 0)
		StartWrite();
}
