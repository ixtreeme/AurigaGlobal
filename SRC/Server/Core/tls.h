#pragma once

#include <boost/asio/ssl/context.hpp>

#include <memory>
#include <string>
#include <string_view>

namespace auriga::net
{
	// Server-side TLS settings of one listener: the certificate chain and the
	// private key (PEM), TLS 1.2 or newer, no renegotiation. Built at boot and
	// shared by every session accepted on that listener.
	class TlsServerContext
	{
	public:
		// From PEM files. Returns null and logs why when they cannot be used.
		static std::unique_ptr<TlsServerContext> FromFiles(const std::string& certChainFile, const std::string& privateKeyFile);

		// From PEM text. Returns null and logs why when it cannot be used.
		static std::unique_ptr<TlsServerContext> FromPem(std::string_view certChain, std::string_view privateKey);

		boost::asio::ssl::context& Native() { return m_context; }

	private:
		TlsServerContext();

		boost::asio::ssl::context m_context;
	};
}
