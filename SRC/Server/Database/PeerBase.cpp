#include "stdafx.h"
#include "PeerBase.h"
#include <common/CommonDefines.h>

CPeerBase::CPeerBase() : m_host{}, m_wPort(0), m_BytesRemain(0)
{
}

CPeerBase::~CPeerBase()
{
	Destroy();
}

void CPeerBase::Disconnect()
{
	CloseSocket();
}

void CPeerBase::Destroy()
{
	DestroySession();
}

bool CPeerBase::Accept(auriga::net::tcp::socket&& sock)
{
	AttachSocket(std::move(sock));

	boost::system::error_code ec;
	const auto endpoint = m_socket.remote_endpoint(ec);

	if (ec)
	{
		DestroySession();
		return false;
	}

	m_socket.set_option(boost::asio::socket_base::send_buffer_size(233016), ec);
	m_socket.set_option(boost::asio::socket_base::receive_buffer_size(233016), ec);

	strlcpy(m_host, endpoint.address().to_string().c_str(), sizeof(m_host));
	m_wPort = htons(endpoint.port());

	CreateBuffers(MAX_INPUT_LEN, DEFAULT_PACKET_BUFFER_SIZE);

	if (!m_lpInputBuffer || !m_lpOutputBuffer)
	{
		DestroySession();
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
	if (!m_lpOutputBuffer)
	{
		LOG_ERROR("Not ready to write");
		return;
	}

	buffer_write(m_lpOutputBuffer, &b, 1);
	StartWrite();
}

void CPeerBase::EncodeWORD(uint16_t w)
{
	if (!m_lpOutputBuffer)
	{
		LOG_ERROR("Not ready to write");
		return;
	}

	buffer_write(m_lpOutputBuffer, &w, 2);
	StartWrite();
}

void CPeerBase::EncodeDWORD(uint32_t dw)
{
	if (!m_lpOutputBuffer)
	{
		LOG_ERROR("Not ready to write");
		return;
	}

	buffer_write(m_lpOutputBuffer, &dw, 4);
	StartWrite();
}

void CPeerBase::Encode(const void* data, uint32_t size)
{
	if (!m_lpOutputBuffer)
	{
		LOG_ERROR("Not ready to write");
		return;
	}

	buffer_write(m_lpOutputBuffer, data, size);
	StartWrite();
}

void CPeerBase::RecvEnd(int proceed_bytes)
{
	buffer_read_proceed(m_lpInputBuffer, proceed_bytes);
	m_BytesRemain = static_cast<int>(buffer_size(m_lpInputBuffer));
}

int CPeerBase::GetRecvLength()
{
	return m_BytesRemain;
}

const void * CPeerBase::GetRecvBuffer()
{
	return buffer_read_peek(m_lpInputBuffer);
}

int CPeerBase::GetSendLength()
{
	return static_cast<int>(buffer_size(m_lpOutputBuffer));
}

int CPeerBase::Send()
{
	if (!IsConnected())
		return -1;

	StartWrite();
	return 0;
}

void CPeerBase::OnInput()
{
	m_BytesRemain = static_cast<int>(buffer_size(m_lpInputBuffer));
	OnRecvData();
}

void CPeerBase::OnPeerGone()
{
	OnDisconnect();
}
