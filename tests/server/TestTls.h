#pragma once

// Shared by the TLS tests: a certificate made at run time and client
// operations that give up instead of hanging when the server stops answering.

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>

#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/x509.h>

#include <chrono>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>

namespace test_tls {

struct Pem { std::string cert, key; };

inline std::string Drain(BIO* bio)
{
    char* data = nullptr;
    const long size = BIO_get_mem_data(bio, &data);
    return std::string(data, static_cast<size_t>(size));
}

// A self-signed P-256 certificate, made fresh for the run.
inline Pem SelfSigned(const char* commonName)
{
    EVP_PKEY* key = EVP_EC_gen("P-256");
    X509* cert = X509_new();
    X509_set_version(cert, 2);
    ASN1_INTEGER_set(X509_get_serialNumber(cert), 1);
    X509_gmtime_adj(X509_getm_notBefore(cert), -60);
    X509_gmtime_adj(X509_getm_notAfter(cert), 3600);
    X509_set_pubkey(cert, key);
    X509_NAME* name = X509_get_subject_name(cert);
    X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, reinterpret_cast<const unsigned char*>(commonName), -1, -1, 0);
    X509_set_issuer_name(cert, name);
    X509_sign(cert, key, EVP_sha256());

    BIO* certBio = BIO_new(BIO_s_mem());
    BIO* keyBio = BIO_new(BIO_s_mem());
    PEM_write_bio_X509(certBio, cert);
    PEM_write_bio_PrivateKey(keyBio, key, nullptr, nullptr, 0, nullptr, nullptr);
    Pem pem { Drain(certBio), Drain(keyBio) };
    BIO_free(certBio);
    BIO_free(keyBio);
    X509_free(cert);
    EVP_PKEY_free(key);
    return pem;
}

using Done = std::function<void(const boost::system::error_code&)>;

// Runs one asynchronous client operation for at most five seconds and returns
// its result: a server that stops answering fails the scenario instead of
// hanging it.
inline boost::system::error_code Within(boost::asio::io_context& io, const std::function<void(Done)>& start)
{
    std::optional<boost::system::error_code> result;
    start([&](const boost::system::error_code& ec) { result = ec; });
    io.restart();
    io.run_for(std::chrono::seconds(5));
    if (!result)
        throw std::runtime_error("the server did not answer within five seconds");
    return *result;
}

inline void Expect(const boost::system::error_code& ec, const char* what)
{
    if (ec)
        throw std::runtime_error(std::string(what) + ": " + ec.message());
}

// Read/write completion adapted to Within.
inline auto Transfer(Done done, std::size_t* bytes = nullptr)
{
    return [done, bytes](const boost::system::error_code& ec, std::size_t transferred) {
        if (bytes)
            *bytes = transferred;
        done(ec);
    };
}

}
