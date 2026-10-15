#include <Core/stdafx.h>
#include <Core/Logging.hpp>
#include <Core/asio_engine.h>
#include <Core/peer_auth.h>
#include <Core/tls.h>

#include "login_auth.h"
#include "login_config.h"
#include "login_db_link.h"
#include "login_listener.h"

#include <memory>

// LoginServer: standalone login/auth server, next to Database and GameServer.
//
// Own boot, own configuration and own listener; it never links GameServer
// code. The login protocol and the account DB layer are built on top of this
// skeleton in the following steps.
namespace
{
	void EmptyBeat(LPHEART, int)
	{
	}
}

int main(int argc, char** argv)
{
	const char* configPath = (argc > 1) ? argv[1] : "conf.json";

	thecore_init(25, EmptyBeat);

	LoginConfig config;

	if (!LoadLoginConfig(configPath, config))
	{
		LOG_ERROR("LoginServer: startup aborted, invalid configuration");
		thecore_destroy();
		return 1;
	}

	LOG_INFO("LoginServer: starting (port {}, locale {})", config.listenPort, config.locale);

	if (!config.interserverKey.empty())
		PeerAuth::SetKey(config.interserverKey);

	auriga::net::Start();

	LoginAuth auth;

	if (!auth.Init(config))
	{
		LOG_ERROR("LoginServer: account database is not available");
	}

	LoginDBLink dbLink;
	dbLink.Configure(config.dbLink.host, config.dbLink.port);
	auth.SetLink(&dbLink);

	// Client connections are TLS when configured; the legacy packet cipher
	// runs on the plain socket otherwise.
	std::unique_ptr<auriga::net::TlsServerContext> tls;

	if (config.tls.enabled)
	{
		tls = auriga::net::TlsServerContext::FromFiles(config.tls.certFile, config.tls.keyFile);

		if (!tls)
		{
			LOG_ERROR("LoginServer: startup aborted, the TLS certificate could not be loaded");
			auriga::net::Stop();
			thecore_destroy();
			return 1;
		}

		LOG_INFO("LoginServer: client connections use TLS ({})", config.tls.certFile);
	}
	else
		LOG_INFO("LoginServer: client connections use the legacy packet cipher");

	LoginListener listener(config.listenPort, auth, tls.get());
	auth.SetLimiter(&listener.Limiter());

	if (!listener.IsOpen())
	{
		LOG_ERROR("LoginServer: listener could not be opened");
		auriga::net::Stop();
		thecore_destroy();
		return 1;
	}

	listener.Start();

	while (thecore_idle())
	{
		auriga::net::DrainCompletions();
		auth.Process();
		dbLink.Tick();
		listener.Collect();
	}

	// Join the io thread before the listener (and its accept handler) goes
	// away.
	auriga::net::Stop();

	thecore_destroy();
	return 0;
}
