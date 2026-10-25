#include "MissionPackage.hpp"
#include <stdexcept>

namespace auriga::mission
{
std::optional<PresentedMission> ProjectPresentation(const MissionPackage &package, std::string_view mission,
                                                    std::string_view locale,
                                                    const std::set<std::string> &allowed)
{
    const auto &catalog = package.Presentation();
    const auto found = catalog.missions.find(std::string(mission));
    if (found == catalog.missions.end())
        return std::nullopt;
    auto language = catalog.locales.find(std::string(locale));
    if (language == catalog.locales.end())
        language = catalog.locales.find(package.Manifest().defaultLocale);
    if (language == catalog.locales.end())
        return std::nullopt;
    const auto &texts = language->second;
    const auto &p = found->second;
    PresentedMission view{
        std::string(mission), language->first, texts.at(p.titleKey), texts.at(p.summaryKey), {}};
    for (const auto &objective : p.objectives)
        if (allowed.contains(objective.node))
            view.objectives.push_back({objective.node, texts.at(objective.titleKey)});
    return view;
}
MissionPackageManager::MissionPackageManager(DefinitionRegistry &registry, Mode mode,
                                             NativeRequirements bindings)
    : registry_(registry), thread_(std::this_thread::get_id()), mode_(mode), bindings_(std::move(bindings))
{
    if (mode_ != Mode::Gameplay && mode_ != Mode::DomainSimulation)
        throw std::invalid_argument("Unknown package activation mode");
    for (const auto &[type, version] : bindings_)
    {
        const auto *schema = FindNativeSchema(type);
        if (!schema || schema->version != version)
            throw std::invalid_argument("Unknown verified gameplay binding");
    }
}
void MissionPackageManager::CheckThread() const
{
    if (thread_ != std::this_thread::get_id())
        throw std::logic_error("Package activation outside its authoritative owner thread");
}
std::shared_ptr<const MissionPackage> MissionPackageManager::Active() const
{
    CheckThread();
    return active_;
}
MissionPackageManager::Result
MissionPackageManager::Select(const std::shared_ptr<const MissionPackage> &package)
{
    if (mode_ == Mode::Gameplay)
        for (const auto &[type, version] : package->Manifest().requiresTypes)
        {
            const auto *schema = FindNativeSchema(type);
            if (schema->category == NativeCategory::Event || type == NativeType(NodeKind::Reward))
            {
                const auto binding = bindings_.find(type);
                if (binding == bindings_.end() || binding->second != version)
                    return {
                        Status::Invalid,
                        {{"bindings", "Missing verified gameplay binding: " + std::string(schema->name)}}};
            }
        }
    std::vector<Definition> definitions;
    for (const auto &graph : package->Graphs())
        definitions.push_back(graph->Source());
    auto result = registry_.Activate(definitions);
    if (result.status != Publication::Status::Published)
        return {Status::Invalid, std::move(result.errors)};
    previous_ = active_;
    active_ = package;
    return {Status::Activated, {}};
}
MissionPackageManager::Result MissionPackageManager::Activate(const PackageLoad &staged)
{
    CheckThread();
    if (!staged)
        return {Status::Invalid, staged.errors};
    const auto &manifest = staged.package->Manifest();
    if (active_ && active_->Digest() == staged.package->Digest())
        return {Status::NoChange, {}};
    const auto highest = highest_.find(manifest.id);
    if ((highest != highest_.end() && manifest.version <= highest->second) ||
        (highest == highest_.end() && highest_.size() >= PackageLimits::Missions))
        return {Status::VersionConflict, {{"package", "version is not newer; use explicit rollback"}}};
    auto nextHighest = highest_;
    nextHighest[manifest.id] = manifest.version;
    auto result = Select(staged.package);
    if (result.status == Status::Activated)
        highest_.swap(nextHighest);
    return result;
}
MissionPackageManager::Result MissionPackageManager::Rollback()
{
    CheckThread();
    if (!previous_)
        return {Status::NothingToRollback, {}};
    const auto previous = previous_;
    return Select(previous);
}
} // namespace auriga::mission
