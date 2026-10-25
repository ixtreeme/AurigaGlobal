#pragma once
#include "MissionContentHash.hpp"
#include "MissionRuntime.hpp"
#include <span>

namespace auriga::mission
{
inline constexpr size_t CheckpointBytes = 16 * 1024 * 1024;
struct DefinitionReference
{
    std::string mission;
    uint32_t version = 0;
    ContentDigest fingerprint{};
    auto operator<=>(const DefinitionReference &) const = default;
};
struct CheckpointScan
{
    CharacterId character = 0;
    uint64_t revision = 0;
    Timestamp now = 0;
    ContentDigest digest{};
    std::vector<DefinitionReference> definitions;
    std::vector<Diagnostic> errors;
    explicit operator bool() const
    {
        return errors.empty() && character != 0;
    }
};
struct CheckpointBuild
{
    std::vector<uint8_t> bytes;
    CheckpointScan metadata;
    explicit operator bool() const
    {
        return !bytes.empty() && bool(metadata);
    }
};
struct CheckpointLoad
{
    std::optional<MissionRuntime::Checkpoint> checkpoint;
    std::vector<Diagnostic> errors;
    explicit operator bool() const
    {
        return checkpoint.has_value() && errors.empty();
    }
};

// Scan checks the complete envelope/encoding/hash and exposes only archive pins,
// never executable progress. Load additionally validates exact graph content and
// all runtime invariants, without changing the registry or advancing time.
CheckpointBuild EncodeCheckpoint(const MissionRuntime::Checkpoint &checkpoint,
                                 const DefinitionRegistry &registry);
CheckpointScan ScanCheckpoint(std::span<const uint8_t> bytes);
CheckpointLoad DecodeCheckpoint(std::span<const uint8_t> bytes, const DefinitionRegistry &registry);
} // namespace auriga::mission
