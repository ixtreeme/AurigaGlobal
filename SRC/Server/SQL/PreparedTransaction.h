#pragma once
#include "SqlParams.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

// Only trusted, fixed server SQL belongs here. Values must use SqlParams.
// All accessed tables (including trigger targets) must be transactional.
// This is a queue ownership/commit boundary, not an SQL sandbox.
struct SqlTransactionStep
{
    std::string query;
    SqlParams params;
    std::optional<uint64_t> expectedAffectedRows;
};

using SqlTransaction = std::vector<SqlTransactionStep>;

enum class SqlTransactionOutcome : uint8_t
{
    NotStarted,
    Committed,
    RolledBack,
    Conflict,
    Failed,
    // COMMIT was attempted but its acknowledgement was lost or failed.
    // Reconcile a durable operation ID on a fresh connection; never blindly replay.
    CommitUnknown
};

inline constexpr uint32_t SqlTransactionConflict = 0xffff0001u;
inline constexpr uint32_t SqlTransactionInvalid = 0xffff0002u;
inline constexpr size_t SqlTransactionNoStep = static_cast<size_t>(-1);

namespace SqlTransactionLimits
{
inline constexpr size_t Steps = 128;
inline constexpr size_t Parameters = 4096;
inline constexpr size_t QueryBytes = 65536;
inline constexpr size_t TotalBytes = 16 * 1024 * 1024;
} // namespace SqlTransactionLimits
