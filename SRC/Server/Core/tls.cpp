#include "stdafx.h"
#include "tls.h"

#include <Core/Logging.hpp>

#include <openssl/ssl.h>

namespace auriga::net
{
	TlsServerContext::TlsServerContext() : m_context(boost::asio::ssl::context::tls_server)
	{
		namespace ssl = boost::asio::ssl;
		m_context.set_options(ssl::context::default_workarounds | ssl::context::no_sslv2 |
			ssl::context::no_sslv3 | ssl::context::no_tlsv1 | ssl::context::no_tlsv1_1 |
			ssl::context::single_dh_use);
		SSL_CTX_set_min_proto_version(m_context.native_handle(), TLS1_2_VERSION);
		SSL_CTX_set_options(m_context.native_handle(), SSL_OP_NO_RENEGOTIATION);
	}

	std::unique_ptr<TlsServerContext> TlsServerContext::FromFiles(const std::string& certChainFile, const std::string& privateKeyFile)
	{
		std::unique_ptr<TlsServerContext> context(new TlsServerContext());
		boost::system::error_code ec;

		context->m_context.use_certificate_chain_file(certChainFile, ec);
		if (ec)
		{
			LOG_ERROR("TLS: cannot load the certificate chain '{}': {}", certChainFile, ec.message());
			return nullptr;
		}

		context->m_context.use_private_key_file(privateKeyFile, boost::asio::ssl::context::pem, ec);
		if (ec)
		{
			LOG_ERROR("TLS: cannot load the private key '{}': {}", privateKeyFile, ec.message());
			return nullptr;
		}

		if (SSL_CTX_check_private_key(context->m_context.native_handle()) != 1)
		{
			LOG_ERROR("TLS: the private key '{}' does not match the certificate '{}'", privateKeyFile, certChainFile);
			return nullptr;
		}

		return context;
	}

	std::unique_ptr<TlsServerContext> TlsServerContext::FromPem(std::string_view certChain, std::string_view privateKey)
	{
		std::unique_ptr<TlsServerContext> context(new TlsServerContext());
		boost::system::error_code ec;

		context->m_context.use_certificate_chain(boost::asio::buffer(certChain.data(), certChain.size()), ec);
		if (ec)
		{
			LOG_ERROR("TLS: cannot use the certificate chain: {}", ec.message());
			return nullptr;
		}

		context->m_context.use_private_key(boost::asio::buffer(privateKey.data(), privateKey.size()),
			boost::asio::ssl::context::pem, ec);
		if (ec)
		{
			LOG_ERROR("TLS: cannot use the private key: {}", ec.message());
			return nullptr;
		}

		if (SSL_CTX_check_private_key(context->m_context.native_handle()) != 1)
		{
			LOG_ERROR("TLS: the private key does not match the certificate");
			return nullptr;
		}

		return context;
	}
}
