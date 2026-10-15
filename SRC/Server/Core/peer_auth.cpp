#include "stdafx.h"
#include "peer_auth.h"

#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>

#include <chrono>
#include <cstring>
#include <stdexcept>

namespace
{
	std::string g_key;

	// HMAC-SHA256 over the message with the interserver key.
	void ComputeMac(const uint8_t * data, size_t length, uint8_t out[PeerAuth::MAC_SIZE])
	{
		unsigned int written = 0;

		if (!HMAC(EVP_sha256(), g_key.data(), static_cast<int>(g_key.size()), data, length, out, &written) ||
			written != PeerAuth::MAC_SIZE)
			throw std::runtime_error("PeerAuth: HMAC-SHA256 failed");
	}
}

void PeerAuth::SetKey(const std::string& key)
{
	g_key = key;

	if (!g_key.empty())
		LOG_INFO("PeerAuth: interserver authentication enabled");
}

bool PeerAuth::HasKey()
{
	return !g_key.empty();
}

void PeerAuth::MakeNonce(uint8_t nonce[NONCE_SIZE])
{
	// A nonce from a failing generator would make the challenge replayable.
	if (RAND_bytes(nonce, NONCE_SIZE) != 1)
		throw std::runtime_error("PeerAuth: the random generator failed");
}

uint64_t PeerAuth::NowSeconds()
{
	return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(
		std::chrono::system_clock::now().time_since_epoch()).count());
}

void PeerAuth::EncodeTimestamp(uint64_t value, uint8_t out[TIMESTAMP_SIZE])
{
	for (int i = 0; i < TIMESTAMP_SIZE; ++i)
		out[i] = static_cast<uint8_t>((value >> (8 * i)) & 0xFF);
}

uint64_t PeerAuth::DecodeTimestamp(const uint8_t in[TIMESTAMP_SIZE])
{
	uint64_t value = 0;

	for (int i = 0; i < TIMESTAMP_SIZE; ++i)
		value |= static_cast<uint64_t>(in[i]) << (8 * i);

	return value;
}

bool PeerAuth::IsTimestampFresh(uint64_t timestamp)
{
	const uint64_t now = NowSeconds();

	if (timestamp > now + 5)
		return false;

	return now - timestamp <= MAX_CLOCK_SKEW_SECONDS;
}

void PeerAuth::ComputeAuthMac(uint8_t role, const uint8_t nonce[NONCE_SIZE], uint64_t timestamp, uint8_t out[MAC_SIZE])
{
	uint8_t message[1 + NONCE_SIZE + TIMESTAMP_SIZE];

	message[0] = role;
	memcpy(message + 1, nonce, NONCE_SIZE);
	EncodeTimestamp(timestamp, message + 1 + NONCE_SIZE);

	ComputeMac(message, sizeof(message), out);
}

bool PeerAuth::VerifyAuthMac(uint8_t role, const uint8_t nonce[NONCE_SIZE], uint64_t timestamp, const uint8_t mac[MAC_SIZE])
{
	if (!HasKey())
		return true;

	uint8_t expected[MAC_SIZE];
	ComputeAuthMac(role, nonce, timestamp, expected);

	// Constant time comparison.
	uint8_t diff = 0;

	for (int i = 0; i < MAC_SIZE; ++i)
		diff |= static_cast<uint8_t>(expected[i] ^ mac[i]);

	return diff == 0;
}
