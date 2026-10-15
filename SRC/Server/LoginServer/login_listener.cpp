#include "login_listener.h"

#include "login_auth.h"
#include "login_session.h"

#include <Core/Logging.hpp>

#include <boost/asio.hpp>

#include <memory>

namespace
{
	uint32_t NextHandshakeValue()
	{
		static uint32_t s_counter = 1;
		return s_counter++;
	}
}

LoginListener::LoginListener(uint16_t port, LoginAuth& auth, auriga::net::TlsServerContext& tls)
	: m_acceptor(auriga::net::Io()), m_auth(auth), m_tls(tls)
{
	boost::system::error_code ec;

	const auriga::net::tcp::endpoint endpoint(auriga::net::tcp::v4(), port);

	m_acceptor.open(endpoint.protocol(), ec);

	if (!ec)
		m_acceptor.set_option(boost::asio::socket_base::reuse_address(true), ec);

	if (!ec)
		m_acceptor.bind(endpoint, ec);

	if (!ec)
		m_acceptor.listen(boost::asio::socket_base::max_listen_connections, ec);

	if (ec)
	{
		LOG_ERROR("LoginServer: cannot listen on port {} ({})", port, ec.message().c_str());
		return;
	}

	LOG_INFO("LoginServer: listening on port {}", port);
}

LoginListener::~LoginListener()
{
	boost::system::error_code ec;
	m_acceptor.close(ec);

	// The engine is stopped before this destructor runs.
	for (LoginSession* session : m_sessions)
		delete session;

	m_sessions.clear();
}

void LoginListener::Start()
{
	DoAccept();
}

void LoginListener::Collect()
{
	auto it = m_sessions.begin();

	while (it != m_sessions.end())
	{
		LoginSession* session = *it;

		// A TLS peer that never finishes its handshake is dropped here.
		if (!session->IsClosed())
			session->CheckHeartbeat();

		if (session->IsClosed() && !session->HasPendingIoWork() && !session->HasPendingDbWork())
		{
			m_auth.OnSessionClosed(session);
			m_limiter.SessionClosed(session->GetHost());
			it = m_sessions.erase(it);
			delete session;
		}
		else
			++it;
	}
}

void LoginListener::DoAccept()
{
	if (!m_acceptor.is_open())
		return;

	auto socket = std::make_shared<auriga::net::tcp::socket>(auriga::net::Io());

	m_acceptor.async_accept(*socket,
		[this, socket](const boost::system::error_code& ec)
		{
			if (!ec)
			{
				// Game logic (limiter, session, auth) runs on the game
				// thread; the accept chain re-arms on the io thread.
				auriga::net::PostCompletion([this, socket]() { OnAccept(*socket); });
			}
			else if (ec != boost::asio::error::operation_aborted)
			{
				LOG_ERROR("LoginServer: accept failed ({})", ec.message().c_str());
			}

			DoAccept();
		});
}

void LoginListener::OnAccept(auriga::net::tcp::socket& socket)
{
	std::string host;

	boost::system::error_code endpointEc;
	const auto endpoint = socket.remote_endpoint(endpointEc);

	if (!endpointEc)
		host = endpoint.address().to_string();

	if (!host.empty() && !m_limiter.AllowConnect(host))
	{
		LOG_INFO("LoginServer: connection from {} rejected (connection limit)", host);

		boost::system::error_code ignored;
		socket.close(ignored);
		return;
	}

	auto* session = new LoginSession(NextHandshakeValue(), &m_auth);
	session->SetHost(host);

	if (!session->Accept(std::move(socket), m_tls))
		delete session;
	else
	{
		if (!host.empty())
			m_limiter.SessionOpened(host);

		LOG_INFO("LoginServer: session opened");
		m_sessions.push_back(session);
	}
}
