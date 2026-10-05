#pragma once

#include <cstdint>
#include <cstddef>
#include <string>

// Server-to-server link authentication.
//
// Mutual challenge-response: the accepting side generates a random nonce and
// a timestamp and sends them together with its own MAC; the connecting side
// verifies the server MAC and answers with its MAC over the very same
// challenge. The nonce is single-use per connection, so a captured frame
// cannot be replayed on another link.
//
// When no key is configured (HasKey() == false) the handshake is skipped,
// keeping the previous trust-based behaviour.
namespace PeerAuth
{
	constexpr int NONCE_SIZE = 16;
	constexpr int MAC_SIZE = 32;
	constexpr int TIMESTAMP_SIZE = 8;

	// A response older than this is rejected.
	constexpr int MAX_CLOCK_SKEW_SECONDS = 30;

	// Domain separation roles for the mutual authentication MACs.
	constexpr uint8_t ROLE_CLIENT = 'C';
	constexpr uint8_t ROLE_SERVER = 'S';

	void		SetKey(const std::string& key);
	bool		HasKey();

	void		MakeNonce(uint8_t nonce[NONCE_SIZE]);
	uint64_t	NowSeconds();
	void		EncodeTimestamp(uint64_t value, uint8_t out[TIMESTAMP_SIZE]);
	uint64_t	DecodeTimestamp(const uint8_t in[TIMESTAMP_SIZE]);
	bool		IsTimestampFresh(uint64_t timestamp);

	// MAC over: role byte || nonce || timestamp (little endian).
	void		ComputeAuthMac(uint8_t role, const uint8_t nonce[NONCE_SIZE], uint64_t timestamp, uint8_t out[MAC_SIZE]);
	bool		VerifyAuthMac(uint8_t role, const uint8_t nonce[NONCE_SIZE], uint64_t timestamp, const uint8_t mac[MAC_SIZE]);
}
