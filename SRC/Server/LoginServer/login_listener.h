#pragma once

#include <Core/asio_engine.h>

#include "login_limiter.h"

#include <cstdint>
#include <vector>

class LoginAuth;
class LoginSession;

namespace auriga::net
{
	class TlsServerContext;
}

// Client acceptor for the login server. Accepted connections become TLS
// login sessions; closed sessions are reaped by Collect() from the server
// loop once their outstanding io work has drained.
class LoginListener
{
public:
	LoginListener(uint16_t port, LoginAuth& auth, auriga::net::TlsServerContext& tls);
	~LoginListener();

	LoginListener(const LoginListener&) = delete;
	LoginListener& operator=(const LoginListener&) = delete;

	bool IsOpen() const { return m_acceptor.is_open(); }
	void Start();

	// Game loop: drops sessions past their TLS handshake deadline and
	// deletes those whose socket teardown has completed.
	void Collect();

	LoginLimiter& Limiter() { return m_limiter; }

private:
	void DoAccept();

	// Game thread: accepts are handed over from the io thread, so session
	// creation and the limiter never race the server loop.
	void OnAccept(auriga::net::tcp::socket& socket);

	auriga::net::tcp::acceptor m_acceptor;
	std::vector<LoginSession*> m_sessions;
	LoginAuth& m_auth;
	auriga::net::TlsServerContext& m_tls;
	LoginLimiter m_limiter;
};
