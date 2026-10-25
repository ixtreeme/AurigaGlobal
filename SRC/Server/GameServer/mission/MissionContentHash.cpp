#include "MissionContentHash.hpp"
#include <memory>
#include <openssl/evp.h>
#include <stdexcept>

namespace auriga::mission
{
ContentDigest ContentHashParts(std::initializer_list<std::span<const uint8_t>> parts)
{
    std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> context(EVP_MD_CTX_new(), EVP_MD_CTX_free);
    if (!context || EVP_DigestInit_ex(context.get(), EVP_sha256(), nullptr) != 1)
        throw std::runtime_error("Cannot initialize mission content hash");
    for (const auto part : parts)
        if (!part.empty() && EVP_DigestUpdate(context.get(), part.data(), part.size()) != 1)
            throw std::runtime_error("Cannot update mission content hash");
    ContentDigest digest{};
    unsigned int size = 0;
    if (EVP_DigestFinal_ex(context.get(), digest.data(), &size) != 1 || size != digest.size())
        throw std::runtime_error("Cannot finish mission content hash");
    return digest;
}
ContentDigest ContentHash(std::span<const uint8_t> bytes)
{
    return ContentHashParts({bytes});
}
} // namespace auriga::mission
