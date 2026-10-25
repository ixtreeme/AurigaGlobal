#include "MissionDefinition.hpp"
#include "MissionValidator.hpp"
#include <mutex>

namespace auriga::mission
{
Publication DefinitionRegistry::Activate(const std::vector<Definition> &sources)
{
    return StoreCatalog(sources, true);
}
Publication DefinitionRegistry::Retain(const std::vector<Definition> &sources)
{
    return StoreCatalog(sources, false);
}
Publication DefinitionRegistry::StoreCatalog(const std::vector<Definition> &sources, bool activate)
{
    if (sources.empty() || sources.size() > (activate ? 128 : Limits::Instances))
        return {Publication::Status::Invalid, {{"catalog", "catalog/history batch size limit"}}};
    std::map<std::pair<std::string, uint32_t>, std::shared_ptr<const CompiledDefinition>> compiled;
    std::set<std::string> activeIds;
    size_t catalogDependencies = 0;
    const auto dependencyLimit = activate ? Limits::CatalogDependencies : Limits::RegistryDependencies;
    for (const auto &source : sources)
    {
        auto c = CompileDefinition(source);
        if (!c)
            return {Publication::Status::Invalid, std::move(c.errors)};
        const auto dependencies = c.definition->DependencyEntries();
        if (dependencies > dependencyLimit - catalogDependencies)
            return {Publication::Status::CapacityExceeded, {{"catalog", "derived dependency limit"}}};
        catalogDependencies += dependencies;
        if ((activate && !activeIds.insert(source.id).second) ||
            !compiled.emplace(std::pair(source.id, source.version), std::move(c.definition)).second)
            return {Publication::Status::Invalid, {{"catalog", "duplicate mission ID"}}};
    }
    std::unique_lock lock(mutex_);
    auto nextDefinitions = definitions_;
    auto nextChoices = choices_;
    decltype(active_) nextActive;
    size_t nextCount = versions_;
    size_t nextDependencies = dependencyEntries_;
    for (const auto &[key, definition] : compiled)
    {
        const auto &source = definition->Source();
        const auto &id = source.id;
        auto &versions = nextDefinitions[id];
        if (const auto old = versions.find(source.version); old != versions.end())
        {
            if (old->second->Source() != source)
                return {Publication::Status::Invalid,
                        {{"catalog/" + id, "immutable version content differs"}}};
        }
        else
        {
            if (nextCount == 4096 ||
                definition->DependencyEntries() > Limits::RegistryDependencies - nextDependencies)
                return {Publication::Status::CapacityExceeded, {}};
            versions.emplace(source.version, definition);
            ++nextCount;
            nextDependencies += definition->DependencyEntries();
        }
        for (const auto &[key, values] : source.choices)
        {
            const std::set<std::string> domain(values.begin(), values.end());
            const auto old = nextChoices.find(key);
            if (old != nextChoices.end() && old->second != domain)
                return {Publication::Status::Invalid, {{"choices/" + key, "persistent domain differs"}}};
            nextChoices.emplace(key, domain);
        }
        nextActive.emplace(id, source.version);
    }
    definitions_.swap(nextDefinitions);
    choices_.swap(nextChoices);
    if (activate)
        active_.swap(nextActive);
    versions_ = nextCount;
    dependencyEntries_ = nextDependencies;
    catalogManaged_ = true;
    return {Publication::Status::Published, {}};
}
} // namespace auriga::mission
