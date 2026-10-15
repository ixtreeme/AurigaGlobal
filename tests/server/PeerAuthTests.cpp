#include <Core/peer_auth.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>

// The server-to-server MAC must stay bit-identical across implementations:
// a Database and a GameServer built at different times have to agree on it.
// The expected values were produced by the Crypto++ implementation and match
// Python's hmac/sha256 for the same message.
#define EXPECTED_SERVER_MAC "12d7028a586c8bf3ec3406d25ce6d6a9a1a2115df9899c764f242ffd8cb3c573"
#define EXPECTED_CLIENT_MAC "82386595db759712c046884bbb27e94cf8efd2716f12e0f25161e31c5e145f9e"
namespace {
int checks = 0;
void Check(bool value, const char* why) { ++checks; if (!value) throw std::runtime_error(why); }

std::string Hex(const uint8_t* data, size_t size)
{
    std::string out;
    char byte[3];
    for (size_t i = 0; i < size; ++i) {
        std::snprintf(byte, sizeof(byte), "%02x", data[i]);
        out += byte;
    }
    return out;
}

void KnownMac()
{
    PeerAuth::SetKey("aurigaglobal-interserver-key");
    uint8_t nonce[PeerAuth::NONCE_SIZE];
    for (int i = 0; i < PeerAuth::NONCE_SIZE; ++i)
        nonce[i] = static_cast<uint8_t>(i * 17 + 3);
    uint8_t mac[PeerAuth::MAC_SIZE];
    PeerAuth::ComputeAuthMac(PeerAuth::ROLE_SERVER, nonce, 0x0102030405060708ull, mac);
    const std::string hex = Hex(mac, sizeof(mac));
    Check(hex == EXPECTED_SERVER_MAC, "the server MAC changed");

    PeerAuth::ComputeAuthMac(PeerAuth::ROLE_CLIENT, nonce, 0x0102030405060708ull, mac);
    Check(Hex(mac, sizeof(mac)) == EXPECTED_CLIENT_MAC, "the client MAC changed");

    Check(PeerAuth::VerifyAuthMac(PeerAuth::ROLE_CLIENT, nonce, 0x0102030405060708ull, mac), "a valid MAC was rejected");
    mac[7] ^= 0x10;
    Check(!PeerAuth::VerifyAuthMac(PeerAuth::ROLE_CLIENT, nonce, 0x0102030405060708ull, mac), "a flipped MAC was accepted");
    mac[7] ^= 0x10;
    Check(!PeerAuth::VerifyAuthMac(PeerAuth::ROLE_SERVER, nonce, 0x0102030405060708ull, mac), "the roles were not separated");
    Check(!PeerAuth::VerifyAuthMac(PeerAuth::ROLE_CLIENT, nonce, 0x0102030405060709ull, mac), "the timestamp was not covered");
}

void Nonces()
{
    uint8_t first[PeerAuth::NONCE_SIZE] {}, second[PeerAuth::NONCE_SIZE] {};
    PeerAuth::MakeNonce(first);
    PeerAuth::MakeNonce(second);
    const uint8_t zero[PeerAuth::NONCE_SIZE] {};
    Check(std::memcmp(first, second, sizeof(first)) != 0, "two nonces were equal");
    Check(std::memcmp(first, zero, sizeof(first)) != 0 && std::memcmp(second, zero, sizeof(second)) != 0,
        "a nonce was left zero");
}

void Timestamps()
{
    uint8_t encoded[PeerAuth::TIMESTAMP_SIZE];
    PeerAuth::EncodeTimestamp(0x0102030405060708ull, encoded);
    Check(encoded[0] == 0x08 && encoded[7] == 0x01, "the timestamp is not little endian");
    Check(PeerAuth::DecodeTimestamp(encoded) == 0x0102030405060708ull, "the timestamp did not round trip");
    Check(PeerAuth::IsTimestampFresh(PeerAuth::NowSeconds()), "now was not fresh");
    Check(!PeerAuth::IsTimestampFresh(PeerAuth::NowSeconds() - PeerAuth::MAX_CLOCK_SKEW_SECONDS - 5), "an old timestamp was fresh");
}

void NoKey()
{
    PeerAuth::SetKey("");
    uint8_t nonce[PeerAuth::NONCE_SIZE] {}, mac[PeerAuth::MAC_SIZE] {};
    Check(!PeerAuth::HasKey() && PeerAuth::VerifyAuthMac(PeerAuth::ROLE_SERVER, nonce, 1, mac),
        "without a key the handshake is not skipped");
}
}

int main()
{
    try {
        KnownMac();
        Nonces();
        Timestamps();
        NoKey();
    } catch (const std::exception& error) {
        std::cerr << "peer auth test failed: " << error.what() << '\n';
        return 1;
    }
    std::cout << "peer auth: " << checks << " checks passed\n";
    return 0;
}
