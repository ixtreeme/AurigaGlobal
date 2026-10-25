#include "MissionPackage.hpp"
#include <algorithm>
#include <initializer_list>
#include <nlohmann/json.hpp>
#include <stdexcept>

namespace auriga::mission
{
bool IsPresentationText(std::string_view text)
{
    if (text.empty() || text.size() > PackageLimits::TextBytes)
        return false;
    // Strict UTF-8, including overlong forms, surrogates and C0/C1 controls.
    for (size_t i = 0; i < text.size();)
    {
        const auto first = uint8_t(text[i++]);
        uint32_t code = first;
        size_t continuation = 0;
        uint32_t minimum = 0;
        if (first >= 0xc2 && first <= 0xdf)
        {
            code &= 0x1f;
            continuation = 1;
            minimum = 0x80;
        }
        else if (first >= 0xe0 && first <= 0xef)
        {
            code &= 0x0f;
            continuation = 2;
            minimum = 0x800;
        }
        else if (first >= 0xf0 && first <= 0xf4)
        {
            code &= 7;
            continuation = 3;
            minimum = 0x10000;
        }
        else if (first >= 0x80)
            return false;
        if (continuation > text.size() - i)
            return false;
        for (size_t k = 0; k < continuation; ++k)
        {
            const auto next = uint8_t(text[i++]);
            if ((next & 0xc0) != 0x80)
                return false;
            code = (code << 6) | (next & 0x3f);
        }
        if (code < minimum || code > 0x10ffff || (code >= 0xd800 && code <= 0xdfff) ||
            (code < 32 && code != '\t' && code != '\n' && code != '\r') || (code >= 0x7f && code <= 0x9f))
            return false;
    }
    return true;
}
namespace
{
using Json = nlohmann::json;
void Need(bool ok, std::string_view message)
{
    if (!ok)
        throw std::invalid_argument(std::string(message));
}
void Fields(const Json &j, std::initializer_list<std::string_view> required,
            std::initializer_list<std::string_view> optional = {})
{
    Need(j.is_object(), "Expected source object");
    for (const auto key : required)
        Need(j.contains(key), "Missing source field: " + std::string(key));
    for (auto it = j.begin(); it != j.end(); ++it)
        Need(std::find(required.begin(), required.end(), it.key()) != required.end() ||
                 std::find(optional.begin(), optional.end(), it.key()) != optional.end(),
             "Unknown source field: " + it.key());
}
std::string Key(const Json &j)
{
    Need(j.is_string(), "Expected identifier");
    const auto key = j.get<std::string>();
    Need(IsIdentifier(key), "Invalid source identifier");
    return key;
}
uint32_t Number(const Json &j, uint32_t max = UINT32_MAX)
{
    Need(j.is_number_unsigned() || (j.is_number_integer() && j.get<int64_t>() >= 0),
         "Expected nonnegative integer");
    const auto value = j.get<uint64_t>();
    Need(value <= max, "Source integer outside range");
    return uint32_t(value);
}
} // namespace
PackageSourceResult ParsePackageSource(std::string_view text)
{
    PackageSourceResult result;
    try
    {
        Need(text.size() <= PackageLimits::Bytes, "Source byte limit");
        std::vector<std::set<std::string>> keys;
        auto callback = [&](int depth, Json::parse_event_t event, Json &value)
        {
            Need(depth <= int(2 * Limits::Depth + 16), "Source JSON depth limit");
            if (event == Json::parse_event_t::object_start)
                keys.emplace_back();
            if (event == Json::parse_event_t::key)
                Need(keys.back().insert(value.get<std::string>()).second, "Duplicate source object key");
            if (event == Json::parse_event_t::object_end)
                keys.pop_back();
            return true;
        };
        const auto j = Json::parse(text.begin(), text.end(), callback);
        Fields(j, {"schema", "package", "version", "default_locale", "missions", "presentation"},
               {"requires"});
        Need(Number(j.at("schema")) == 1, "Unsupported authoring schema");
        auto &source = result.source;
        source.manifest.id = Key(j.at("package"));
        source.manifest.version = Number(j.at("version"));
        source.manifest.defaultLocale = Key(j.at("default_locale"));
        if (j.contains("requires"))
        {
            Need(j.at("requires").is_array() && j.at("requires").size() <= PackageLimits::NativeTypes,
                 "Invalid native requirement table");
            for (const auto &entry : j.at("requires"))
            {
                Fields(entry, {"type_id", "version"});
                const auto type = Number(entry.at("type_id"));
                const auto version = uint16_t(Number(entry.at("version"), UINT16_MAX));
                Need(source.manifest.requiresTypes.emplace(type, version).second,
                     "Duplicate native requirement");
            }
        }
        Need(j.at("missions").is_array() && j.at("missions").size() <= PackageLimits::Missions,
             "Source mission count limit");
        for (const auto &entry : j.at("missions"))
        {
            auto parsed = ParseDefinition(entry.dump());
            if (!parsed)
                throw std::invalid_argument(parsed.errors.front().path + ": " +
                                            parsed.errors.front().message);
            source.missions.push_back(std::move(parsed.value));
        }
        const auto &presentation = j.at("presentation");
        Fields(presentation, {"locales", "missions"});
        const auto &locales = presentation.at("locales");
        Need(locales.is_object() && locales.size() <= PackageLimits::Locales, "Source locale count limit");
        size_t total = 0;
        for (auto locale = locales.begin(); locale != locales.end(); ++locale)
        {
            Need(IsIdentifier(locale.key()) && locale.value().is_object() &&
                     locale.value().size() <= PackageLimits::Texts - total,
                 "Invalid localization table");
            total += locale.value().size();
            auto &texts = source.presentation.locales[locale.key()];
            for (auto it = locale.value().begin(); it != locale.value().end(); ++it)
            {
                Need(IsIdentifier(it.key()) && it.value().is_string(), "Invalid localization key/value");
                auto value = it.value().get<std::string>();
                Need(IsPresentationText(value), "Invalid UTF-8 text");
                texts.emplace(it.key(), std::move(value));
            }
        }
        const auto &missions = presentation.at("missions");
        Need(missions.is_object() && missions.size() <= PackageLimits::Missions,
             "Presentation mission count limit");
        for (auto it = missions.begin(); it != missions.end(); ++it)
        {
            Need(IsIdentifier(it.key()), "Invalid presentation mission ID");
            Fields(it.value(), {"title", "summary", "objectives"});
            MissionPresentation m;
            m.titleKey = Key(it.value().at("title"));
            m.summaryKey = Key(it.value().at("summary"));
            const auto &objectives = it.value().at("objectives");
            Need(objectives.is_array() && objectives.size() <= Limits::Nodes,
                 "Presentation objective count limit");
            for (const auto &objective : objectives)
            {
                Fields(objective, {"node", "title"});
                m.objectives.push_back({Key(objective.at("node")), Key(objective.at("title"))});
            }
            source.presentation.missions.emplace(it.key(), std::move(m));
        }
    }
    catch (const std::exception &error)
    {
        result.source = {};
        result.errors.push_back({"source", error.what()});
    }
    return result;
}
} // namespace auriga::mission
