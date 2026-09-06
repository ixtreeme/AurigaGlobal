#include "../stdafx.h"
#include "EcsDiagnostics.hpp"

#include "Registry.hpp"
#include "components/identity_components.hpp"
#include "components/item_components.hpp"

#include <Core/Logging.hpp>

#include <cstdint>
#include <map>
#include <utility>

namespace ecs::diag {
namespace {

// Keyed by the source location. file_name() points at a string literal with
// static storage duration, so the pointer is a stable key and no copy is made.
using SiteKey = std::pair<const char*, uint32_t>;

std::map<SiteKey, uint64_t>& Sites()
{
    static std::map<SiteKey, uint64_t> sites;
    return sites;
}

// First occurrence, then every power of ten. A fault on a hot path stays
// visible without burying the rest of the log.
bool ShouldEmit(uint64_t count)
{
    if (count == 1)
        return true;
    for (uint64_t threshold = 10; threshold <= count; threshold *= 10)
        if (threshold == count)
            return true;
    return false;
}

// Strip the build path; the tail is what identifies the file.
const char* ShortFile(const char* path)
{
    const char* last = path;
    for (const char* p = path; *p; ++p)
        if (*p == '/' || *p == 92)
            last = p + 1;
    return last;
}

void AppendIdentity(std::string& out, entt::entity e)
{
    if (const auto* vid = g_registry.try_get<ecs::VIDComponent>(e))
        out += fmt::format(" vid={}", vid->value);
    if (const auto* pid = g_registry.try_get<ecs::PlayerID>(e))
        out += fmt::format(" pid={}", pid->pid);
    if (const auto* name = g_registry.try_get<ecs::PlayerName>(e); name && !name->value.empty())
        out += fmt::format(" '{}'", name->value);
    if (const auto* item = g_registry.try_get<ecs::ItemIdentity>(e))
        out += fmt::format(" item id={} vnum={}", item->id, item->vnum);

    if (g_registry.all_of<ecs::TagPC>(e))
        out += " PC";
    else if (g_registry.all_of<ecs::TagNPC>(e))
        out += " NPC";
    else if (g_registry.all_of<ecs::TagMonster>(e))
        out += " Mob";
    else if (g_registry.all_of<ecs::TagStone>(e))
        out += " Stone";
}

} // namespace

std::string Describe(entt::entity e)
{
    if (e == entt::null)
        return "null";

    const auto slot = static_cast<uint32_t>(entt::to_entity(e));
    const auto generation = static_cast<uint32_t>(entt::to_version(e));

    if (!g_registry.valid(e))
    {
        // current() is safe on any identifier: it answers with the tombstone
        // version when the slot is not live.
        const auto live = static_cast<uint32_t>(g_registry.current(e));
        // EnTT bumps the generation at destroy, not at recycle, so a handle to
        // anything that ever lived and then died always disagrees here.
        if (live != generation)
            return fmt::format("stale(slot={} gen={}, slot is now at gen={})", slot, generation, live);
        // Agreeing generations on an invalid handle means the slot never held
        // anything: the identifier was fabricated or is out of range.
        return fmt::format("never-lived(slot={} gen={})", slot, generation);
    }

    std::string out = fmt::format("live(slot={} gen={}", slot, generation);
    AppendIdentity(out, e);
    out += ')';
    return out;
}

void Report(const std::source_location& where, const std::string& message)
{
    const uint64_t count = ++Sites()[SiteKey { where.file_name(), where.line() }];
    if (!ShouldEmit(count))
        return;

    if (count == 1)
        LOG_ERROR("[ECS] {} at {}:{} in {}", message,
            ShortFile(where.file_name()), where.line(), where.function_name());
    else
        LOG_ERROR("[ECS] {} at {}:{} in {} (seen {} times)", message,
            ShortFile(where.file_name()), where.line(), where.function_name(), count);
}

bool Check(entt::entity e, std::string_view what, const std::source_location& where)
{
    if (e != entt::null && g_registry.valid(e))
        return true;

    Report(where, fmt::format("{} refused: entity is {}", what, Describe(e)));
    return false;
}

} // namespace ecs::diag
