#pragma once
#include "MissionNativeSchema.hpp"
#include <array>
#include <filesystem>
#include <thread>

namespace auriga::mission
{
struct PackageLimits
{
    static constexpr size_t Bytes = 16 * 1024 * 1024, GraphBytes = 1024 * 1024;
    static constexpr size_t Missions = 128, Locales = 8, Texts = 16'384;
    static constexpr size_t TextBytes = 4096, NativeTypes = 256, Sections = 16;
};
struct PackageManifest
{
    std::string id;
    uint32_t version = 0;
    std::string defaultLocale;
    NativeRequirements requiresTypes;
};
struct ObjectivePresentation
{
    std::string node, titleKey;
};
struct MissionPresentation
{
    std::string titleKey, summaryKey;
    std::vector<ObjectivePresentation> objectives;
};
struct PresentationCatalog
{
    std::map<std::string, std::map<std::string, std::string>> locales;
    std::map<std::string, MissionPresentation> missions;
};
struct PackageSource
{
    PackageManifest manifest;
    std::vector<Definition> missions;
    PresentationCatalog presentation;
};
struct PackageSourceResult
{
    PackageSource source;
    std::vector<Diagnostic> errors;
    explicit operator bool() const
    {
        return errors.empty();
    }
};
PackageSourceResult ParsePackageSource(std::string_view json);
bool IsPresentationText(std::string_view text);

class MissionPackage final
{
  public:
    const PackageManifest &Manifest() const
    {
        return manifest_;
    }
    const auto &Graphs() const
    {
        return graphs_;
    }
    const PresentationCatalog &Presentation() const
    {
        return presentation_;
    }
    const std::array<uint8_t, 32> &Digest() const
    {
        return digest_;
    }

  private:
    friend struct PackageCodec;
    PackageManifest manifest_;
    std::vector<std::shared_ptr<const CompiledDefinition>> graphs_;
    PresentationCatalog presentation_;
    std::array<uint8_t, 32> digest_{};
};
struct PackageBuild
{
    std::vector<uint8_t> bytes;
    std::vector<Diagnostic> errors;
    explicit operator bool() const
    {
        return !bytes.empty() && errors.empty();
    }
};
struct PackageLoad
{
    std::shared_ptr<const MissionPackage> package;
    std::vector<Diagnostic> errors;
    explicit operator bool() const
    {
        return package != nullptr && errors.empty();
    }
};
// No timestamps, native ABI dumps, script code, compression or generated C++.
PackageBuild CompilePackage(const PackageSource &source);
PackageLoad LoadPackage(std::span<const uint8_t> bytes);
PackageLoad LoadPackageFile(const std::filesystem::path &path);

struct PresentedMission
{
    std::string mission, locale, title, summary;
    struct Objective
    {
        std::string node, title;
    };
    std::vector<Objective> objectives;
};
// The future gameplay/dialogue adapter must authorize mission and node IDs.
// This projection copies only explicitly allowed presentation fields, never graphs.
std::optional<PresentedMission> ProjectPresentation(const MissionPackage &package, std::string_view mission,
                                                    std::string_view locale,
                                                    const std::set<std::string> &allowedNodes);

class MissionPackageManager final
{
  public:
    enum class Mode
    {
        Gameplay,
        DomainSimulation
    };
    enum class Status
    {
        Activated,
        NoChange,
        Invalid,
        VersionConflict,
        NothingToRollback
    };
    struct Result
    {
        Status status = Status::Invalid;
        std::vector<Diagnostic> errors;
    };
    explicit MissionPackageManager(DefinitionRegistry &registry, Mode mode = Mode::Gameplay,
                                   NativeRequirements verifiedGameplayBindings = {});
    // Call at an authoritative safe point. Staging/decoding can happen off-thread.
    Result Activate(const PackageLoad &staged);
    Result Rollback();
    std::shared_ptr<const MissionPackage> Active() const;

  private:
    void CheckThread() const;
    Result Select(const std::shared_ptr<const MissionPackage> &package);
    DefinitionRegistry &registry_;
    std::thread::id thread_;
    Mode mode_;
    NativeRequirements bindings_;
    std::shared_ptr<const MissionPackage> active_, previous_;
    std::map<std::string, uint32_t> highest_;
};
} // namespace auriga::mission
