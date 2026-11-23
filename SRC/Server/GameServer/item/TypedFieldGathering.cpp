#include "stdafx.h"
#include "TypedFieldGathering.hpp"
#include "ItemCatalogRuntime.hpp"
#include "ItemStore.hpp"
#include "TypedDrops.hpp"
#include "TypedFishProcessing.hpp"
#include "TypedInventorySync.hpp"
#include "TypedLoot.hpp"
#include "../ecs/Registry.hpp"
#include "../ecs/ResourceAdmission.hpp"
#include "../ecs/systems/ChatSystem.hpp"
#include "../ecs/systems/HerbalismSystem.hpp"
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "../ecs/systems/PointSystem.hpp"
#include "../ecs/systems/WoodcuttingSystem.hpp"
#include "../entity/char_manager.h"
#include "../mission_pass/MissionPassEvents.hpp"
#include "../network/desc.h"
#include <item/ToolRuntime.hpp>
#include <openssl/rand.h>

namespace {
namespace item = auriga::item;
namespace store = item::runtime;
namespace player = ecs::PlayerRuntime;
using Mission = AurigaMissionPass::Kind;

const item::ItemCatalogEntry* Reward(const item::ItemCatalog& catalog, std::string_view code)
{
    const auto& entries = catalog.Entries();
    const auto found = std::ranges::find_if(entries, [code](const auto& entry) {
        return entry.second.base.contentCode == code && !entry.second.base.retired &&
            entry.first.Category() == item::ItemCategory::Material;
    });
    return found != entries.end() && item::SupportsPersistentGrant(catalog, found->second)
        ? &found->second : nullptr;
}

const item::PersistentItem* Equipped(const item::ItemCatalog& catalog,
    const item::persistence::LoadedItemOwner& state, item::ToolKind kind)
{
    const auto found = std::ranges::find_if(state.items, [&](const auto& instance) {
        if (instance.destroyed || !instance.tool || instance.owner != state.fence.owner ||
            instance.placement != item::ItemPlacement{item::ItemStorageKind::Equipment, 1, 0})
            return false;
        const auto* entry = catalog.Find(instance.key);
        const auto* row = entry ? std::get_if<item::ItemToolRow>(&entry->category) : nullptr;
        return row && !entry->base.retired && row->kind == kind;
    });
    return found == state.items.end() ? nullptr : &*found;
}

void Feedback(entt::entity e, item::ToolKind kind, bool rewarded, std::uint32_t reward,
    std::uint32_t tool, std::uint32_t gained, bool ready)
{
    const bool wood = kind == item::ToolKind::Axe;
    const auto* command = wood ? "woodcut_notice %s" : "herb_notice %s";
    ecs::ChatSystem::Send(e, CHAT_TYPE_COMMAND, command,
        wood ? (rewarded ? "WOODCUT_LOG_SUCCESS" : "WOODCUT_NO_LOG")
             : (rewarded ? "HERB_SUCCESS" : "HERB_FAILED"));
    if (rewarded)
        AurigaMissionPass::Events::Emit(e, wood ? Mission::WoodGather : Mission::HerbGather, reward);
    if (gained)
        AurigaMissionPass::Events::Emit(e, Mission::ToolTraining, tool, gained);
    if (ready)
        ecs::ChatSystem::Send(e, CHAT_TYPE_COMMAND, command, wood ? "WOODCUT_READY" : "HERB_READY");
}

// Tool profile odds are expressed as an unsigned numerator and denominator.
std::optional<std::uint32_t> Draw(std::uint32_t bound)
{
    const auto limit = (std::uint64_t{UINT32_MAX} + 1) / bound * bound;
    std::uint32_t sample;
    do {
        if (RAND_bytes(reinterpret_cast<unsigned char*>(&sample), sizeof(sample)) != 1)
            return std::nullopt;
    } while (sample >= limit);
    return sample % bound + 1;
}
}

std::uint64_t EquippedFieldTool(entt::entity character, item::ToolKind kind)
{
    const auto catalog = store::CurrentCatalog();
    const auto* typed = g_registry.valid(character) ? g_registry.try_get<ecs::TypedItems>(character) : nullptr;
    const auto* instance = catalog && typed ? Equipped(*catalog, typed->state, kind) : nullptr;
    return instance ? instance->id.Value() : 0;
}

bool IsFieldGatheringNode(entt::entity node)
{
    return WoodcuttingSystem::IsTree(node) || HerbalismSystem::IsPlant(node);
}

bool TypedFieldGatheringActive(entt::entity character)
{
    return g_registry.valid(character) &&
        g_registry.any_of<WoodcuttingSystem::ChoppingState, HerbalismSystem::PruningState>(character);
}

bool FieldGatheringRewardExists(std::string_view code)
{
    const auto catalog = store::CurrentCatalog();
    return catalog && Reward(*catalog, code);
}

void CompleteFieldGatheringHit(entt::entity character, std::uint64_t tool, item::ToolKind kind,
    std::string_view rewardCode, unsigned rewardPercent)
{
    if (!TypedGatheringAdmitted(character) || g_registry.all_of<ecs::TypedItemBusy>(character))
        return;
    auto admission = ecs::resource_admission::Enter({character}, auriga::resource_admission::Operation::ItemUse);
    if (!admission || !admission->Current())
        return;

    const auto catalog = store::CurrentCatalog();
    const auto& state = g_registry.get<ecs::TypedItems>(character).state;
    const auto* instance = catalog ? Equipped(*catalog, state, kind) : nullptr;
    const auto* reward = catalog ? Reward(*catalog, rewardCode) : nullptr;
    if (!instance || instance->id.Value() != tool || !reward)
        return;

    const auto& row = std::get<item::ItemToolRow>(catalog->Find(instance->key)->category);
    const auto& level = catalog->FindToolProfile(row.profile)->levels[instance->refinementLevel];
    const auto rewardRoll = Draw(100);
    if (!rewardRoll)
        return;

    item::ToolRuntimeActor actor{
        static_cast<std::uint16_t>(ecs::PointSystem::GetLevel(character)),
        static_cast<std::uint8_t>(player::GetJob(character)),
        static_cast<std::uint8_t>(player::GetSex(character)),
        true, player::GetPosition(character) == POS_FIGHTING, false, false, true
    };
    item::ToolRuntimeRequest request{instance->id, {}, item::ToolRuntimeAction::Practice, actor};
    if (!item::CheckToolRuntime(*catalog, state.fence.owner, state.items, request))
        return;
    if (instance->tool->trainingPoints < level.requiredPoints &&
        level.practiceNumerator > 0 && level.practiceNumerator < level.practiceDenominator)
    {
        const auto roll = Draw(level.practiceDenominator);
        if (!roll)
            return;
        request.roll = *roll;
    }
    std::array<std::uint8_t, 16> operation{};
    if (RAND_bytes(operation.data(), static_cast<int>(operation.size())) != 1)
        return;
    auto plan = item::MakeToolRuntimeCommit(*catalog, operation, state.fence, state.storages, state.items, request);
    if (!plan)
        return;

    const auto toolSubject = instance->key.LocalId();
    const auto rewardSubject = reward->base.key.LocalId();
    const auto gained = plan->state.trainingPoints - instance->tool->trainingPoints;
    const auto ready = gained && plan->state.trainingPoints == level.requiredPoints;
    const auto loot = *rewardRoll <= rewardPercent ? ReserveTypedLoot({reward->base.key, 0, 1},
        player::GetMapIndex(character), player::GetX(character), player::GetY(character),
        player::GetZ(character), character, 30'000) : 0u;
    if (!gained)
    {
        const bool published = loot && ResolveReservedTypedLoot(loot, true);
        Feedback(character, kind, published, rewardSubject, toolSubject, 0, false);
        return;
    }

    const auto pid = player::GetPlayerID(character);
    const auto token = g_registry.get<ecs::TypedItemLogin>(character).token;
    const auto fence = state.fence;
    auto held = std::make_shared<auriga::resource_admission::Permit>(std::move(*admission));
    g_registry.emplace<ecs::TypedItemBusy>(character);
    const bool queued = store::RequestCommit(fence.owner, fence.sessionGeneration, std::move(plan->commit),
        [pid, token, held, kind, loot, rewardSubject, toolSubject, gained, ready](store::CommitOutcome outcome)
        {
            const bool published = loot && ResolveReservedTypedLoot(loot, outcome.write.has_value());
            const auto e = CHARACTER_MANAGER::instance().FindEntityByPID(pid);
            if (!g_registry.valid(e))
                return;
            const auto* login = g_registry.try_get<ecs::TypedItemLogin>(e);
            if (!login || login->token != token)
                return;
            if (!outcome.state)
            {
                if (auto* desc = player::GetDesc(e))
                    desc->SetPhase(PHASE_CLOSE);
                return;
            }
            g_registry.emplace_or_replace<ecs::TypedItems>(e, std::move(*outcome.state));
            g_registry.remove<ecs::TypedItemBusy>(e);
            SendTypedInventory(e);
            Feedback(e, kind, published, rewardSubject, toolSubject, outcome.write ? gained : 0,
                outcome.write && ready);
            PumpTypedItemGrants(e);
        });
    if (!queued)
    {
        if (loot)
            ResolveReservedTypedLoot(loot, false);
        g_registry.remove<ecs::TypedItemBusy>(character);
        Feedback(character, kind, false, rewardSubject, toolSubject, 0, false);
        PumpTypedItemGrants(character);
    }
}
