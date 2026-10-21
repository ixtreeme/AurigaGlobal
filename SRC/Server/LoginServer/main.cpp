#include <Core/stdafx.h>
#include <Core/Logging.hpp>
#include <Core/asio_engine.h>
#include <Core/peer_auth.h>
#include <Core/tls.h>
#include <Core/server_config.h>
#include <iostream>

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
    auriga::config::StartupOptions options;
    auriga::config::Section section;
    std::string configError;
    if (!auriga::config::ParseArguments(auriga::config::Role::Login, argc, argv, options, configError))
    { std::cerr << configError << '\n'; auriga::config::PrintUsage(auriga::config::Role::Login); return 1; }
    if (options.help) { auriga::config::PrintUsage(auriga::config::Role::Login); return 0; }
    LoginConfig config;
    if (!auriga::config::Load(options, auriga::config::Role::Login, section, configError) || !LoadLoginConfig(section, config, configError))
    { std::cerr << "LoginServer configuration: " << configError << '\n'; return 1; }
    if (options.checkOnly) { auriga::config::PrintChecked(section, auriga::config::Role::Login); return 0; }
    if (!auriga::config::EnterWorkingDirectory(section, configError)) { std::cerr << configError << '\n'; return 1; }

	if (!thecore_init(25, EmptyBeat))
		return 1;
	// SQL/session destructors still log while unwinding every exit path.
	// The core (and its logger) must outlive those objects.
	struct CoreCleanup
	{
		~CoreCleanup() { thecore_destroy(); }
	} coreCleanup;

	LOG_INFO("LoginServer: starting (port {}, locale {})", config.listenPort, config.locale);

	if (!config.interserverKey.empty())
		PeerAuth::SetKey(config.interserverKey);

	auriga::net::Start();

	LoginAuth auth;

	if (!auth.Init(config))
	{
		LOG_ERROR("LoginServer: account database is not available");
		auriga::net::Stop();
		return 1;
	}

	LoginDBLink dbLink;
	dbLink.Configure(config.dbLink.host, config.dbLink.port);
	auth.SetLink(&dbLink);

	// Client connections are TLS only.
	const std::unique_ptr<auriga::net::TlsServerContext> tls =
		auriga::net::TlsServerContext::FromFiles(config.tls.certFile, config.tls.keyFile);

	if (!tls)
	{
		LOG_ERROR("LoginServer: startup aborted, the TLS certificate could not be loaded");
		auriga::net::Stop();
		return 1;
	}

	LOG_INFO("LoginServer: client connections use TLS ({})", config.tls.certFile);

	LoginListener listener(config.listenPort, auth, *tls);
	auth.SetLimiter(&listener.Limiter());

	if (!listener.IsOpen())
	{
		LOG_ERROR("LoginServer: listener could not be opened");
		auriga::net::Stop();
		return 1;
	}

	listener.Start();

	while (!auriga::config::ShutdownRequested() && thecore_idle())
	{
		auriga::net::DrainCompletions();
		auth.Process();
		dbLink.Tick();
		listener.Collect();
	}

	// Join the io thread before the listener (and its accept handler) goes
	// away.
	auriga::net::Stop();

	return 0;
}
