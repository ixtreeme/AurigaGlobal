#include "stdafx.h"
#include "TypedWikiSources.hpp"
#include "ItemCatalogRuntime.hpp"
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "../network/desc.h"

#include <Headers/Header_GC.h>
#include <item/ItemCatalog.hpp>

#include <array>
#include <limits>
#include <optional>
#include <string_view>
#include <vector>

namespace
{
namespace items = auriga::item;
enum class Status : std::uint8_t { Ready, Unavailable, Changed, Unchanged };

struct Appearance
{
    std::uint8_t slot = 0;
    std::uint8_t variant = 0;
    std::uint32_t race = 0;
};

Appearance BrowseAppearance(const items::ItemCatalog& catalog, const items::ItemCategoryRow& row)
{
    using namespace items;
    if (const auto* weapon = std::get_if<ItemWeaponRow>(&row))
        return {static_cast<std::uint8_t>(weapon->slot)};
    if (const auto* armor = std::get_if<ItemArmorRow>(&row))
        return {static_cast<std::uint8_t>(armor->slot)};
    if (const auto* accessory = std::get_if<ItemAccessoryRow>(&row))
        return {static_cast<std::uint8_t>(accessory->slot)};
    if (const auto* costume = std::get_if<ItemCostumeRow>(&row))
    {
        const auto* form = costume->companionForm ? catalog.FindCompanionForm(*costume->companionForm) : nullptr;
        return {0, static_cast<std::uint8_t>(costume->kind), form ? form->mobTemplateId : 0};
    }
    if (const auto* companion = std::get_if<ItemCompanionRow>(&row))
    {
        const auto* form = catalog.FindCompanionForm(companion->form);
        return {0, static_cast<std::uint8_t>(companion->kind), form ? form->mobTemplateId : 0};
    }
    if (const auto* regalia = std::get_if<ItemRegaliaRow>(&row))
        return {regalia->slot, static_cast<std::uint8_t>(regalia->kind)};
    if (const auto* container = std::get_if<ItemContainerRow>(&row))
        return {0, static_cast<std::uint8_t>(container->kind)};
    if (const auto* quest = std::get_if<ItemQuestRow>(&row))
    {
        if (const auto* hatch = catalog.FindPetHatchFor(quest->key))
        {
            const auto* target = catalog.Find(hatch->target);
            const auto* companion = target ? std::get_if<ItemCompanionRow>(&target->category) : nullptr;
            const auto* form = companion ? catalog.FindCompanionForm(companion->form) : nullptr;
            return {0, static_cast<std::uint8_t>(CompanionKind::GrowthPet), form ? form->mobTemplateId : 0};
        }
    }
    return {};
}

std::optional<std::uint16_t> FirstAvailableLevel(const items::ItemCatalog& catalog, const items::ItemBaseRow& base)
{
    if (base.retired) return {};
    const auto* profile = catalog.FindRefinement(base.refinementProfile);
    if (!profile) return {};
    for (const auto& state : profile->Data().levels)
    {
        const auto* rule = catalog.FindDefinitionRules(base.key, state.level);
        if (!rule || rule->available)
            return state.minimumLevel.value_or(base.minimumLevel);
    }
    return {};
}
}

void HandleTypedWikiCatalog(entt::entity player, const char* data)
{
    if (!ecs::PlayerRuntime::IsPC(player)) return;
    auto* desc = ecs::PlayerRuntime::GetDesc(player);
    if (!desc) return;

    const auto* request = reinterpret_cast<const std::uint8_t*>(data);
    const auto read = [&](std::size_t offset, unsigned size) {
        std::uint64_t value = 0;
        for (unsigned i = 0; i < size; ++i)
            value |= std::uint64_t{request[offset + i]} << (8 * i);
        return value;
    };
    const auto id = read(2, 4);
    const auto prior = read(6, 8);
    const auto category = static_cast<std::uint16_t>(read(14, 2));
    const auto local = static_cast<std::uint32_t>(read(16, 4));
    const auto after = items::ItemKey::Create(category, local);
    const bool hasCursor = category != 0 || local != 0;
    if (request[1] != 1 || !id || (hasCursor && (!after || !prior))) return;

    const auto catalog = items::runtime::CurrentCatalog();
    std::uint64_t fingerprint = 0;
    if (catalog)
    {
        for (unsigned i = 0; i < 8; ++i)
            fingerprint |= std::uint64_t{catalog->ContentHash()[i]} << (8 * i);
        if (!fingerprint) fingerprint = 1;
    }

    Status status = !catalog ? Status::Unavailable : Status::Ready;
    if (catalog && hasCursor && fingerprint != prior) status = Status::Changed;
    if (catalog && !hasCursor && fingerprint == prior) status = Status::Unchanged;

    std::vector<std::uint8_t> packet;
    const auto put = [&](std::uint64_t value, unsigned width) {
        for (unsigned i = 0; i < width; ++i)
            packet.push_back(static_cast<std::uint8_t>(value >> (8 * i)));
    };
    put(HEADER_GC_TYPED_WIKI_CATALOG, 1);
    put(0, 4);
    put(1, 1);
    put(id, 4);
    put(0, 1);
    put(fingerprint, 8);
    put(0, 1);
    put(0, 2);
    constexpr std::size_t headerSize = 22;

    std::uint16_t count = 0;
    bool more = false;
    if (status == Status::Ready)
    {
        const auto& entries = catalog->Entries();
        for (auto it = hasCursor ? entries.upper_bound(*after) : entries.begin(); it != entries.end(); ++it)
        {
            const auto& [key, entry] = *it;
            const auto level = FirstAvailableLevel(*catalog, entry.base);
            if (!level) continue;

            const auto& base = entry.base;
            const std::array<std::string_view, 4> fields{base.nameKey, base.contentCode, base.iconAsset, base.descriptionKey};
            std::size_t rowSize = 23;
            for (const auto field : fields)
            {
                if (field.size() > std::numeric_limits<std::uint16_t>::max())
                {
                    status = Status::Unavailable;
                    break;
                }
                rowSize += field.size();
            }
            if (status != Status::Ready) break;
            if (count == 256 || packet.size() + rowSize > (1 << 20))
            {
                more = true;
                break;
            }

            const auto appearance = BrowseAppearance(*catalog, entry.category);
            const auto* policy = catalog->FindUsePolicy(base.usePolicy);
            put(static_cast<std::uint16_t>(key.Category()), 2);
            put(key.LocalId(), 4);
            put(appearance.slot, 1);
            put(appearance.variant, 1);
            put(policy ? policy->allowedClasses : 0, 1);
            put(*level, 2);
            put(appearance.race, 4);
            for (const auto field : fields)
            {
                put(field.size(), 2);
                packet.insert(packet.end(), field.begin(), field.end());
            }
            ++count;
        }
    }
    if (status != Status::Ready)
    {
        count = 0;
        more = false;
        packet.resize(headerSize);
    }
    for (unsigned i = 0; i < 4; ++i)
        packet[1 + i] = static_cast<std::uint8_t>(packet.size() >> (8 * i));
    packet[10] = static_cast<std::uint8_t>(status);
    packet[19] = more ? 1 : 0;
    packet[20] = static_cast<std::uint8_t>(count);
    packet[21] = static_cast<std::uint8_t>(count >> 8);
    if (packet.size() > 65535)
        desc->LargePacket(packet.data(), static_cast<int>(packet.size()));
    else
        desc->Packet(packet.data(), static_cast<int>(packet.size()));
}
