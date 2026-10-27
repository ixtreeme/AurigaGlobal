#include "MissionResourcePlan.hpp"
#include "MissionActions.hpp"
#include <algorithm>
#include <limits>
#include <nlohmann/json.hpp>
#include <set>
#include <stdexcept>
#include <tuple>

namespace auriga::mission
{
namespace
{
using Json = nlohmann::json;
void Need(bool condition, const char *message)
{
    if (!condition)
        throw std::invalid_argument(message);
}
uint64_t UInt(const Json &value, uint64_t maximum = UINT64_MAX)
{
    Need(value.is_number_integer() && (value.is_number_unsigned() || value.get<int64_t>() >= 0),
         "Expected unsigned resource value");
    const auto number = value.get<uint64_t>();
    Need(number <= maximum, "Resource value exceeds field width");
    return number;
}
bool Boolean(const Json &value)
{
    Need(value.is_boolean(), "Expected resource boolean");
    return value.get<bool>();
}
void Array(const Json &value, size_t size)
{
    Need(value.is_array() && value.size() == size, "Invalid resource tuple");
}
std::string Hex(const ContentDigest &digest)
{
    std::string text;
    for (uint8_t value : digest)
    {
        text += "0123456789abcdef"[value >> 4];
        text += "0123456789abcdef"[value & 15];
    }
    return text;
}
Json Progress(const ResourceProgress &p)
{
    return Json::array({p.level, p.step, p.experience, p.stat, p.skill, p.subSkill, p.randomHP, p.randomSP,
                        p.job, p.skillGroup, p.experienceBlocked});
}
Json Snapshot(const ResourceSnapshot &snapshot)
{
    Json items = Json::array();
    for (const auto &item : snapshot.items)
        items.push_back(Json::array({item.id, item.owner, item.vnum, item.count, uint8_t(item.window),
                                     item.category, item.cell, item.flags}));
    return {{"character", snapshot.character},
            {"revision", snapshot.revision},
            {"progress", Progress(snapshot.progress)},
            {"gold", snapshot.gold},
            {"items", items}};
}
Json Rules(const ResourceRules &rules)
{
    Json items = Json::array();
    for (const auto &[vnum, item] : rules.items)
        items.push_back(Json::array({vnum, item.maximumGrant, item.maximumConsume}));
    return {{"version", rules.version},
            {"maximum_level", rules.maximumLevel},
            {"last_stat_level", rules.lastStatLevel},
            {"last_stat_step", rules.lastStatStep},
            {"stat_cap", rules.statCap},
            {"job", rules.job},
            {"hp", Json::array({rules.hpMinimum, rules.hpMaximum})},
            {"sp", Json::array({rules.spMinimum, rules.spMaximum})},
            {"gold_exclusive_limit", rules.goldExclusiveLimit},
            {"force_experience", rules.forceExperience},
            {"next_experience", rules.nextExperience},
            {"items", items}};
}
ResourceRules ReadRules(const Json &value)
{
    Need(value.is_object() && value.size() == 12, "Invalid resource rules shape");
    ResourceRules rules;
    rules.version = uint32_t(UInt(value.at("version"), UINT32_MAX));
    rules.maximumLevel = uint16_t(UInt(value.at("maximum_level"), 250));
    rules.lastStatLevel = uint16_t(UInt(value.at("last_stat_level"), 250));
    rules.lastStatStep = uint8_t(UInt(value.at("last_stat_step"), 3));
    rules.statCap = uint16_t(UInt(value.at("stat_cap"), INT16_MAX));
    rules.job = uint8_t(UInt(value.at("job"), UINT8_MAX));
    Array(value.at("hp"), 2);
    Array(value.at("sp"), 2);
    rules.hpMinimum = uint16_t(UInt(value["hp"][0], INT16_MAX));
    rules.hpMaximum = uint16_t(UInt(value["hp"][1], INT16_MAX));
    rules.spMinimum = uint16_t(UInt(value["sp"][0], INT16_MAX));
    rules.spMaximum = uint16_t(UInt(value["sp"][1], INT16_MAX));
    rules.goldExclusiveLimit = int64_t(UInt(value.at("gold_exclusive_limit"), INT64_MAX));
    rules.forceExperience = Boolean(value.at("force_experience"));
    const auto &curve = value.at("next_experience");
    Need(curve.is_array() && curve.size() <= 251, "Invalid resource XP curve size");
    for (const auto &entry : curve)
        rules.nextExperience.push_back(uint32_t(UInt(entry, UINT32_MAX)));
    const auto &items = value.at("items");
    Need(items.is_array() && items.size() <= RewardLimits::ItemEntries, "Invalid resource prototype count");
    for (const auto &item : items)
    {
        Array(item, 3);
        Need(rules.items
                 .emplace(uint32_t(UInt(item[0], UINT32_MAX)),
                          ResourceItemRule{uint32_t(UInt(item[1], RewardLimits::ItemCount)),
                                           uint32_t(UInt(item[2], RewardLimits::ItemCount))})
                 .second,
             "Duplicate resource prototype");
    }
    return rules;
}
ResourceSnapshot ReadSnapshot(const Json &value)
{
    Need(value.is_object() && value.size() == 5, "Invalid resource snapshot shape");
    ResourceSnapshot snapshot;
    snapshot.character = uint32_t(UInt(value.at("character"), UINT32_MAX));
    snapshot.revision = UInt(value.at("revision"));
    snapshot.gold = int64_t(UInt(value.at("gold"), INT64_MAX));
    const auto &p = value.at("progress");
    Array(p, 11);
    snapshot.progress = {uint16_t(UInt(p[0], 250)),
                         uint8_t(UInt(p[1], 3)),
                         uint32_t(UInt(p[2], UINT32_MAX)),
                         uint16_t(UInt(p[3], INT16_MAX)),
                         uint16_t(UInt(p[4], INT16_MAX)),
                         uint16_t(UInt(p[5], INT16_MAX)),
                         uint16_t(UInt(p[6], INT16_MAX)),
                         uint16_t(UInt(p[7], INT16_MAX)),
                         uint8_t(UInt(p[8], UINT8_MAX)),
                         uint8_t(UInt(p[9], UINT8_MAX)),
                         Boolean(p[10])};
    const auto &items = value.at("items");
    Need(items.is_array() && items.size() <= PreparedResources::MaximumItems, "Resource inventory too large");
    for (const auto &item : items)
    {
        Array(item, 8);
        snapshot.items.push_back({uint32_t(UInt(item[0], UINT32_MAX)), uint32_t(UInt(item[1], UINT32_MAX)),
                                  uint32_t(UInt(item[2], UINT32_MAX)),
                                  uint32_t(UInt(item[3], RewardLimits::ItemCount)),
                                  ResourceWindow(UInt(item[4], 8)), uint16_t(UInt(item[5], UINT16_MAX)),
                                  uint16_t(UInt(item[6], UINT16_MAX)), uint8_t(UInt(item[7], 15))});
    }
    return snapshot;
}
void Canonical(ResourceSnapshot &snapshot)
{
    std::sort(snapshot.items.begin(), snapshot.items.end(),
              [](const auto &a, const auto &b) { return a.id < b.id; });
}
void Validate(const ResourceSnapshot &snapshot, const ResourceRules &rules)
{
    Need(snapshot.character && snapshot.revision && snapshot.revision < UINT64_MAX && snapshot.gold >= 0 &&
             snapshot.gold < rules.goldExclusiveLimit &&
             snapshot.items.size() <= PreparedResources::MaximumItems,
         "Invalid resource snapshot identity/revision/gold/budget");
    const auto &p = snapshot.progress;
    Need(p.level && p.level <= rules.maximumLevel && p.step <= 3 && p.job == rules.job &&
             p.stat <= rules.statCap && p.skill <= INT16_MAX && p.subSkill <= INT16_MAX &&
             p.randomHP <= INT16_MAX && p.randomSP <= INT16_MAX,
         "Invalid resource progression");
    if (p.level < rules.maximumLevel)
    {
        Need(rules.nextExperience.size() > p.level, "Missing resource XP threshold");
        Need(p.experience < rules.nextExperience[p.level], "Resource XP already exceeds its level");
    }
    std::set<uint32_t> ids;
    std::set<std::tuple<uint8_t, uint16_t, uint16_t>> cells;
    for (const auto &item : snapshot.items)
        Need(item.id && item.owner == snapshot.character && item.vnum && item.count &&
                 item.count <= RewardLimits::ItemCount && uint8_t(item.window) >= 1 &&
                 uint8_t(item.window) <= 8 && item.window != ResourceWindow::Reserved4 &&
                 item.flags <= 15 && ids.insert(item.id).second &&
                 cells.emplace(uint8_t(item.window), item.category, item.cell).second,
             "Invalid/duplicate resource item identity/placement");
}
uint16_t Increase(uint16_t value, uint16_t amount)
{
    Need(uint32_t(value) + amount <= INT16_MAX, "Resource progression short overflow");
    return uint16_t(value + amount);
}
// Preserve the current PointSystem's float32 quarter thresholds, including
// rounding for large XP tables. Full level transition still uses exact next XP.
uint8_t Step(uint32_t experience, uint32_t next)
{
    if (experience == next)
        return 4;
    const auto quarter = uint32_t(float(next) / 4.0f);
    if (experience >= quarter * 3)
        return 3;
    if (experience >= quarter * 2)
        return 2;
    return experience >= quarter ? 1 : 0;
}
std::vector<uint8_t> Encode(const PreparedResources &plan)
{
    const auto &a = plan.Action();
    Json rolls = Json::array(), consumption = Json::array();
    for (const auto &roll : plan.LevelRolls())
        rolls.push_back(Json::array({roll.fromLevel, roll.hp, roll.sp}));
    for (const auto &item : plan.Consumption())
        consumption.push_back(Json::array({item.id, item.vnum, item.before, item.consumed, item.after}));
    Json body = {{"action", Json::array({a.character, a.mission, a.version, a.instance, a.node, a.reward})},
                 {"definition", Hex(plan.DefinitionDigest())},
                 {"payload", Hex(plan.PayloadDigest())},
                 {"rules", Rules(plan.Rules())},
                 {"before", Snapshot(plan.Before())},
                 {"after", Snapshot(plan.After())},
                 {"rolls", rolls},
                 {"consumption", consumption},
                 {"refill_vitals", plan.RefillVitals()}};
    const auto text = body.dump();
    Need(text.size() <= PreparedResources::MaximumBytes - 64, "Resource decision exceeds byte budget");
    std::vector<uint8_t> bytes(64, 0);
    const std::string magic = "AURRES01";
    std::copy(magic.begin(), magic.end(), bytes.begin());
    bytes[8] = 1;
    bytes[10] = 64;
    const uint32_t size = uint32_t(text.size() + 64);
    for (unsigned int i = 0; i < 4; ++i)
        bytes[12 + i] = uint8_t(size >> (i * 8));
    bytes.insert(bytes.end(), text.begin(), text.end());
    const auto digest = ContentHash(bytes);
    std::copy(digest.begin(), digest.end(), bytes.begin() + 24);
    return bytes;
}
} // namespace
ResourcePreparation PrepareResources(const PinnedReward &reward, ResourceSnapshot before, ResourceRules rules,
                                     std::vector<ResourceLevelRoll> rolls)
{
    try
    {
        Need(ValidRewardBundle(reward.Payload()), "Invalid pinned resource payload");
        Need(rules.version == 1 && rules.maximumLevel && rules.maximumLevel <= 250 &&
                 rules.nextExperience.size() == size_t(rules.maximumLevel) + 1 && !rules.nextExperience[0] &&
                 rules.lastStatLevel <= 250 && rules.lastStatStep <= 3 && rules.statCap <= INT16_MAX &&
                 rules.hpMinimum <= rules.hpMaximum && rules.hpMaximum <= INT16_MAX &&
                 rules.spMinimum <= rules.spMaximum && rules.spMaximum <= INT16_MAX &&
                 rules.goldExclusiveLimit > 0 && rules.items.size() <= RewardLimits::ItemEntries &&
                 rolls.size() <= 249,
             "Invalid or unsupported resource rules");
        for (size_t level = 1; level < rules.maximumLevel; ++level)
            Need(rules.nextExperience[level] >= 4, "Zero/degenerate resource XP threshold");
        for (const auto &[vnum, item] : rules.items)
            Need(vnum && (item.maximumGrant || item.maximumConsume) &&
                     item.maximumGrant <= RewardLimits::ItemCount &&
                     item.maximumConsume <= RewardLimits::ItemCount,
                 "Invalid resource prototype capability");
        Need(before.items.size() <= PreparedResources::MaximumItems, "Resource inventory too large");
        Canonical(before);
        Validate(before, rules);
        Need(before.revision < UINT64_MAX - 1, "Resource revision exhausted");
        Need(before.character == reward.Action().character, "Resource owner differs from reward owner");
        PreparedResources plan;
        plan.action_ = reward.Action();
        plan.definition_ = reward.DefinitionDigest();
        plan.payload_ = reward.PayloadDigest();
        plan.before_ = before;
        plan.after_ = before;
        ++plan.after_.revision;
        plan.rules_ = rules;
        plan.rolls_ = rolls;
        for (const auto *entries : {&reward.Payload().items, &reward.Payload().consumeItems})
            for (const auto &[vnum, count] : *entries)
            {
                auto rule = rules.items.find(vnum);
                Need(rule != rules.items.end() &&
                         count <= (entries == &reward.Payload().items ? rule->second.maximumGrant
                                                                      : rule->second.maximumConsume),
                     "Missing resource prototype/count binding");
            }
        // Stable selection from eligible PRE-existing stacks. The granted item
        // list never enters this inventory and cannot pay any of these costs.
        std::vector<size_t> candidates;
        for (size_t i = 0; i < before.items.size(); ++i)
            if (!before.items[i].flags && (before.items[i].window == ResourceWindow::Inventory ||
                                           before.items[i].window == ResourceWindow::ExtraInventory))
                candidates.push_back(i);
        std::sort(candidates.begin(), candidates.end(),
                  [&](size_t a, size_t b)
                  {
                      const auto &x = before.items[a], &y = before.items[b];
                      return std::tie(x.window, x.category, x.cell, x.id) <
                             std::tie(y.window, y.category, y.cell, y.id);
                  });
        for (const auto &[vnum, required] : reward.Payload().consumeItems)
        {
            uint32_t remaining = required;
            for (const auto i : candidates)
            {
                const auto &item = before.items[i];
                if (!remaining || item.vnum != vnum)
                    continue;
                const auto used = std::min(remaining, item.count);
                plan.consumption_.push_back({item.id, item.vnum, item.count, used, item.count - used});
                plan.after_.items[i].count -= used;
                remaining -= used;
            }
            Need(!remaining, "Insufficient eligible pre-existing cost items");
        }
        std::erase_if(plan.after_.items, [](const auto &item) { return !item.count; });
        const auto amount = reward.Payload().gold;
        Need(amount < uint64_t(rules.goldExclusiveLimit - before.gold),
             "Gold reward exceeds exclusive limit");
        plan.after_.gold += int64_t(amount);
        auto &progress = plan.after_.progress;
        uint64_t experience = reward.Payload().experience;
        Need(!experience || !progress.experienceBlocked || rules.forceExperience, "Experience is blocked");
        size_t rollIndex = 0;
        while (experience)
        {
            Need(progress.level < rules.maximumLevel, "Experience reward exceeds maximum level");
            const uint32_t next = rules.nextExperience[progress.level];
            const auto add = uint32_t(std::min(experience, uint64_t(next - progress.experience)));
            progress.experience += add;
            experience -= add;
            const auto target = Step(progress.experience, next);
            while (progress.step < target)
            {
                ++progress.step;
                plan.refillVitals_ = true;
                if (progress.step < 4)
                {
                    if (progress.level < rules.lastStatLevel ||
                        (progress.level == rules.lastStatLevel && progress.step <= rules.lastStatStep))
                        progress.stat = std::min(
                            rules.statCap, Increase(progress.stat, progress.stat < rules.statCap ? 1 : 0));
                }
                else
                {
                    Need(rollIndex < rolls.size(), "Missing archived level-up roll");
                    const auto &roll = rolls[rollIndex++];
                    Need(roll.fromLevel == progress.level && roll.hp >= rules.hpMinimum &&
                             roll.hp <= rules.hpMaximum && roll.sp >= rules.spMinimum &&
                             roll.sp <= rules.spMaximum,
                         "Invalid level-up roll");
                    progress.randomHP = Increase(progress.randomHP, roll.hp);
                    progress.randomSP = Increase(progress.randomSP, roll.sp);
                    if (progress.skillGroup)
                    {
                        if (progress.level >= 5)
                            progress.skill = Increase(progress.skill, 1);
                        if (progress.level >= 9)
                            progress.subSkill = Increase(progress.subSkill, 1);
                    }
                    ++progress.level;
                    progress.step = 0;
                    progress.experience = 0;
                    break;
                }
            }
        }
        Need(rollIndex == rolls.size(), "Unused/unrelated level-up rolls");
        Validate(plan.after_, rules);
        plan.bytes_ = Encode(plan);
        std::copy(plan.bytes_.begin() + 24, plan.bytes_.begin() + 56, plan.digest_.begin());
        return {std::move(plan), {}};
    }
    catch (const std::exception &error)
    {
        return {{}, {{"resources", error.what()}}};
    }
}
ResourcePreparation DecodeResources(std::span<const uint8_t> bytes, const PinnedReward &reward)
{
    try
    {
        Need(bytes.size() >= 64 && bytes.size() <= PreparedResources::MaximumBytes &&
                 std::equal(bytes.begin(), bytes.begin() + 8, "AURRES01") && bytes[8] == 1 && !bytes[9] &&
                 bytes[10] == 64 && !bytes[11],
             "Invalid resource decision envelope");
        uint32_t size = 0;
        for (unsigned int i = 0; i < 4; ++i)
            size |= uint32_t(bytes[12 + i]) << (8 * i);
        Need(size == bytes.size(), "Resource decision size mismatch");
        for (size_t i = 16; i < 24; ++i)
            Need(!bytes[i], "Reserved resource header field");
        for (size_t i = 56; i < 64; ++i)
            Need(!bytes[i], "Reserved resource header field");
        std::vector<uint8_t> checked(bytes.begin(), bytes.end());
        std::fill(checked.begin() + 24, checked.begin() + 56, 0);
        const auto digest = ContentHash(checked);
        Need(std::equal(digest.begin(), digest.end(), bytes.begin() + 24),
             "Resource decision digest mismatch");
        size_t nodes = 0;
        const auto body =
            Json::parse(bytes.begin() + 64, bytes.end(),
                        [&](int depth, Json::parse_event_t, Json &)
                        {
                            Need(depth <= 16 && ++nodes <= 65536, "Resource JSON decoding budget exceeded");
                            return true;
                        });
        Need(body.is_object() && body.size() == 9, "Invalid resource decision fields");
        auto before = ReadSnapshot(body.at("before"));
        auto rules = ReadRules(body.at("rules"));
        const auto &stored = body.at("rolls");
        Need(stored.is_array() && stored.size() <= 249, "Resource roll count exceeded");
        std::vector<ResourceLevelRoll> rolls;
        for (const auto &value : stored)
        {
            Array(value, 3);
            rolls.push_back({uint16_t(UInt(value[0], 250)), uint16_t(UInt(value[1], INT16_MAX)),
                             uint16_t(UInt(value[2], INT16_MAX))});
        }
        auto decoded = PrepareResources(reward, std::move(before), std::move(rules), std::move(rolls));
        Need(bool(decoded) && decoded.plan->Bytes().size() == bytes.size() &&
                 std::equal(bytes.begin(), bytes.end(), decoded.plan->Bytes().begin()),
             "Resource decision is noncanonical or differs from its pinned reward/recomputed result");
        return decoded;
    }
    catch (const std::exception &error)
    {
        return {{}, {{"resources", error.what()}}};
    }
}
ResourceApplication InspectResourceApplication(const PreparedResources &plan, ResourceSnapshot current,
                                               std::optional<ResourceEffectReceipt> receipt)
{
    try
    {
        Need(plan.Bytes().size() >= 64 && current.revision && current.revision < UINT64_MAX &&
                 current.items.size() <= PreparedResources::MaximumItems,
             "Invalid/moved resource plan or state");
        Canonical(current);
        if (current.character != plan.Before().character)
            return ResourceApplication::Invalid;
        if (receipt)
        {
            if (receipt->action != ActionIdentity(plan.Action()) || receipt->decision != plan.Digest() ||
                receipt->appliedRevision != plan.After().revision ||
                current.revision < receipt->appliedRevision)
                return ResourceApplication::Invalid;
            if (current.revision == receipt->appliedRevision && current != plan.After())
                return ResourceApplication::Invalid;
            // Later authoritative revisions may use newer progression/config
            // rules. The old receipt still proves that this effect happened;
            // it is not a validator for that unrelated later state.
            return ResourceApplication::AlreadyApplied;
        }
        Validate(current, plan.Rules());
        if (current == plan.Before())
            return ResourceApplication::Ready;
        if (current == plan.After())
            return ResourceApplication::Reconcile;
        return ResourceApplication::Stale;
    }
    catch (const std::exception &)
    {
        return ResourceApplication::Invalid;
    }
}
} // namespace auriga::mission
