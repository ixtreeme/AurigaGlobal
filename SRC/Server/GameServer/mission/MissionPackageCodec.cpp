#include "MissionPackage.hpp"
#include "MissionValidator.hpp"
#include <algorithm>
#include <fstream>
#include <openssl/evp.h>
#include <stdexcept>

namespace auriga::mission
{
namespace
{
constexpr std::array<uint8_t, 8> Magic = {'A', 'U', 'R', 'M', 'I', 'S', 'N', 0};
constexpr uint16_t HeaderSize = 64, Format = 1, Schema = 1;
constexpr uint32_t Required = 1;
enum class Section : uint16_t
{
    Manifest = 1,
    Graphs = 2,
    Requirements = 3,
    Presentation = 4
};
[[noreturn]] void Bad(std::string_view message)
{
    throw std::invalid_argument(std::string(message));
}
void Need(bool ok, std::string_view message)
{
    if (!ok)
        Bad(message);
}
struct Writer
{
    std::vector<uint8_t> bytes;
    void Raw(std::span<const uint8_t> data)
    {
        Need(data.size() <= PackageLimits::Bytes - bytes.size(), "Package byte limit");
        bytes.insert(bytes.end(), data.begin(), data.end());
    }
    void Number(uint64_t n, size_t count)
    {
        std::array<uint8_t, 8> data{};
        for (size_t i = 0; i < count; ++i)
            data[i] = uint8_t(n >> (8 * i));
        Raw({data.data(), count});
    }
    void U8(uint8_t n)
    {
        Number(n, 1);
    }
    void U16(uint16_t n)
    {
        Number(n, 2);
    }
    void U32(uint32_t n)
    {
        Number(n, 4);
    }
    void U64(uint64_t n)
    {
        Number(n, 8);
    }
    void Text(std::string_view text)
    {
        U32(uint32_t(text.size()));
        Raw({reinterpret_cast<const uint8_t *>(text.data()), text.size()});
    }
    void Strings(const std::vector<std::string> &values)
    {
        U32(uint32_t(values.size()));
        for (const auto &value : values)
            Text(value);
    }
    void AddSection(Section id, const Writer &data)
    {
        U16(uint16_t(id));
        U16(1);
        U32(Required);
        U32(uint32_t(data.bytes.size()));
        Raw(data.bytes);
    }
};
struct Reader
{
    std::span<const uint8_t> bytes;
    size_t position = 0;
    size_t Left() const
    {
        return bytes.size() - position;
    }
    std::span<const uint8_t> Raw(size_t count)
    {
        Need(count <= Left(), "Truncated package/section");
        const auto result = bytes.subspan(position, count);
        position += count;
        return result;
    }
    uint64_t Number(size_t count)
    {
        const auto data = Raw(count);
        uint64_t n = 0;
        for (size_t i = 0; i < count; ++i)
            n |= uint64_t(data[i]) << (8 * i);
        return n;
    }
    uint8_t U8()
    {
        return uint8_t(Number(1));
    }
    uint16_t U16()
    {
        return uint16_t(Number(2));
    }
    uint32_t U32()
    {
        return uint32_t(Number(4));
    }
    uint64_t U64()
    {
        return Number(8);
    }
    bool Bool()
    {
        const auto b = U8();
        Need(b <= 1, "Noncanonical boolean");
        return b != 0;
    }
    size_t Count(size_t max)
    {
        const auto n = U32();
        Need(n <= max && n <= Left(), "Container length limit/truncated data");
        return n;
    }
    std::string Text(size_t maximum = Limits::IdentifierBytes)
    {
        const auto n = U32();
        Need(n <= maximum, "String length limit");
        const auto data = Raw(n);
        return {reinterpret_cast<const char *>(data.data()), data.size()};
    }
    std::string Key()
    {
        auto value = Text();
        Need(IsIdentifier(value), "Invalid identifier");
        return value;
    }
    Timestamp Time()
    {
        const auto n = U64();
        Need(n <= uint64_t(INT64_MAX), "Time outside Int64 range");
        return Timestamp(n);
    }
    std::vector<std::string> Strings(size_t maximum)
    {
        const auto count = Count(maximum);
        std::vector<std::string> result;
        result.reserve(count);
        for (size_t i = 0; i < count; ++i)
            result.push_back(Key());
        return result;
    }
    Reader Part(size_t maximum)
    {
        const auto count = U32();
        Need(count <= maximum, "Section/graph byte limit");
        return {Raw(count)};
    }
    void End() const
    {
        Need(Left() == 0, "Trailing bytes in package/section");
    }
};
std::array<uint8_t, 32> Digest(std::span<const uint8_t> bytes)
{
    Need(bytes.size() >= HeaderSize, "Missing package header");
    const auto destroy = [](EVP_MD_CTX *ctx) { EVP_MD_CTX_free(ctx); };
    std::unique_ptr<EVP_MD_CTX, decltype(destroy)> ctx(EVP_MD_CTX_new(), destroy);
    std::array<uint8_t, 32> result{}, zero{};
    unsigned size = 0;
    if (!ctx || EVP_DigestInit_ex(ctx.get(), EVP_sha256(), nullptr) != 1 ||
        EVP_DigestUpdate(ctx.get(), bytes.data(), 24) != 1 ||
        EVP_DigestUpdate(ctx.get(), zero.data(), zero.size()) != 1 ||
        EVP_DigestUpdate(ctx.get(), bytes.data() + 56, bytes.size() - 56) != 1 ||
        EVP_DigestFinal_ex(ctx.get(), result.data(), &size) != 1 || size != result.size())
        throw std::runtime_error("SHA-256 package digest failed");
    return result;
}
void WriteCondition(Writer &w, const Condition &c)
{
    w.U32(NativeType(c.kind));
    w.U16(1);
    switch (c.kind)
    {
    case ConditionKind::Always:
        break;
    case ConditionKind::AllOf:
    case ConditionKind::AnyOf:
    case ConditionKind::AtLeastN:
    case ConditionKind::ExactlyN:
    case ConditionKind::Not:
    case ConditionKind::Xor:
        w.U32(c.required);
        w.U32(uint32_t(c.children.size()));
        for (const auto &child : c.children)
            WriteCondition(w, child);
        break;
    case ConditionKind::HasFlag:
    case ConditionKind::NotFlag:
    case ConditionKind::Subgraph:
        w.Text(c.key);
        break;
    case ConditionKind::ChoiceIs:
        w.Text(c.key);
        w.Text(c.value);
        break;
    case ConditionKind::TimeWindow:
        w.U64(uint64_t(c.beginsAt));
        w.U64(uint64_t(c.endsAt));
        break;
    }
}
Condition ReadCondition(Reader &r, size_t &conditions, size_t depth = 0)
{
    Need(++conditions <= Limits::Conditions && depth <= Limits::Depth, "Predicate count/depth limit");
    Condition c;
    const auto type = r.U32();
    const auto version = r.U16();
    c.kind = NativeCondition(type, version);
    switch (c.kind)
    {
    case ConditionKind::Always:
        break;
    case ConditionKind::AllOf:
    case ConditionKind::AnyOf:
    case ConditionKind::AtLeastN:
    case ConditionKind::ExactlyN:
    case ConditionKind::Not:
    case ConditionKind::Xor:
    {
        c.required = r.U32();
        const auto count = r.Count(64);
        for (size_t i = 0; i < count; ++i)
            c.children.push_back(ReadCondition(r, conditions, depth + 1));
        break;
    }
    case ConditionKind::HasFlag:
    case ConditionKind::NotFlag:
    case ConditionKind::Subgraph:
        c.key = r.Key();
        break;
    case ConditionKind::ChoiceIs:
        c.key = r.Key();
        c.value = r.Key();
        break;
    case ConditionKind::TimeWindow:
        c.beginsAt = r.Time();
        c.endsAt = r.Time();
        break;
    }
    return c;
}
void WriteNode(Writer &w, const Node &n)
{
    w.Text(n.id);
    w.U32(NativeType(n.kind));
    w.U16(1);
    w.Strings(n.completionFlags);
    switch (n.kind)
    {
    case NodeKind::Objective:
        w.U32(NativeType(n.objective.event));
        w.U16(1);
        w.Text(n.objective.subject);
        w.U64(n.objective.goal);
        w.U8(n.objective.distinctSubjects);
        break;
    case NodeKind::Condition:
        WriteCondition(w, n.condition);
        break;
    case NodeKind::Branch:
        w.U32(uint32_t(n.branches.size()));
        for (const auto &arm : n.branches)
        {
            WriteCondition(w, arm.when);
            w.Text(arm.child);
        }
        w.Text(n.fallback);
        break;
    case NodeKind::Parallel:
        w.U8(uint8_t(n.parallel));
        [[fallthrough]];
    case NodeKind::Sequence:
    case NodeKind::SubMission:
    case NodeKind::Recovery:
        w.Strings(n.children);
        break;
    case NodeKind::Timer:
        w.U64(uint64_t(n.duration));
        break;
    case NodeKind::Reward:
        w.Text(n.reward);
        break;
    case NodeKind::Success:
    case NodeKind::Failure:
        break;
    }
}
Node ReadNode(Reader &r, size_t &conditions)
{
    Node n;
    n.id = r.Key();
    const auto type = r.U32();
    const auto version = r.U16();
    n.kind = NativeNode(type, version);
    n.completionFlags = r.Strings(Limits::Flags);
    switch (n.kind)
    {
    case NodeKind::Objective:
    {
        const auto event = r.U32();
        const auto schema = r.U16();
        n.objective.event = NativeEvent(event, schema);
        n.objective.subject = r.Text();
        n.objective.goal = r.U64();
        n.objective.distinctSubjects = r.Bool();
        break;
    }
    case NodeKind::Condition:
        n.condition = ReadCondition(r, conditions);
        break;
    case NodeKind::Branch:
    {
        const auto count = r.Count(Limits::Nodes);
        for (size_t i = 0; i < count; ++i)
        {
            auto c = ReadCondition(r, conditions);
            auto child = r.Key();
            n.branches.push_back({std::move(c), std::move(child)});
        }
        n.fallback = r.Key();
        break;
    }
    case NodeKind::Parallel:
        n.parallel = ParallelPolicy(r.U8());
        [[fallthrough]];
    case NodeKind::Sequence:
    case NodeKind::SubMission:
    case NodeKind::Recovery:
        n.children = r.Strings(Limits::Nodes);
        break;
    case NodeKind::Timer:
        n.duration = r.Time();
        break;
    case NodeKind::Reward:
        n.reward = r.Key();
        break;
    case NodeKind::Success:
    case NodeKind::Failure:
        break;
    }
    return n;
}
Writer WriteDefinition(const Definition &d)
{
    Writer w;
    w.Text(d.id);
    w.U32(d.version);
    w.Text(d.root);
    w.U8(d.repeatable);
    WriteCondition(w, d.prerequisite);
    w.U32(uint32_t(d.predicates.size()));
    for (const auto &[key, c] : d.predicates)
    {
        w.Text(key);
        WriteCondition(w, c);
    }
    w.U32(uint32_t(d.choices.size()));
    for (const auto &[key, values] : d.choices)
    {
        w.Text(key);
        w.Strings(values);
    }
    w.U32(uint32_t(d.nodes.size()));
    for (const auto &node : d.nodes)
        WriteNode(w, node);
    Need(w.bytes.size() <= PackageLimits::GraphBytes, "Graph byte limit");
    return w;
}
Definition ReadDefinition(Reader &r)
{
    Definition d;
    d.id = r.Key();
    d.version = r.U32();
    d.root = r.Key();
    d.repeatable = r.Bool();
    size_t conditions = 0;
    d.prerequisite = ReadCondition(r, conditions);
    const auto predicates = r.Count(Limits::Conditions);
    for (size_t i = 0; i < predicates; ++i)
    {
        auto key = r.Key();
        auto c = ReadCondition(r, conditions);
        Need(d.predicates.emplace(std::move(key), std::move(c)).second, "Duplicate predicate key");
    }
    const auto choices = r.Count(Limits::Choices);
    for (size_t i = 0; i < choices; ++i)
    {
        auto key = r.Key();
        auto values = r.Strings(Limits::ChoiceValues);
        Need(d.choices.emplace(std::move(key), std::move(values)).second, "Duplicate choice key");
    }
    const auto nodes = r.Count(Limits::Nodes);
    for (size_t i = 0; i < nodes; ++i)
        d.nodes.push_back(ReadNode(r, conditions));
    r.End();
    return d;
}
Writer WritePresentation(const PresentationCatalog &p)
{
    Writer w;
    w.U32(uint32_t(p.locales.size()));
    for (const auto &[locale, texts] : p.locales)
    {
        w.Text(locale);
        w.U32(uint32_t(texts.size()));
        for (const auto &[key, text] : texts)
        {
            w.Text(key);
            w.Text(text);
        }
    }
    w.U32(uint32_t(p.missions.size()));
    for (const auto &[id, mission] : p.missions)
    {
        w.Text(id);
        w.Text(mission.titleKey);
        w.Text(mission.summaryKey);
        w.U32(uint32_t(mission.objectives.size()));
        for (const auto &objective : mission.objectives)
        {
            w.Text(objective.node);
            w.Text(objective.titleKey);
        }
    }
    return w;
}
PresentationCatalog ReadPresentation(Reader &r)
{
    PresentationCatalog p;
    const auto locales = r.Count(PackageLimits::Locales);
    size_t total = 0;
    for (size_t i = 0; i < locales; ++i)
    {
        auto locale = r.Key();
        const auto count = r.Count(PackageLimits::Texts);
        Need(count <= PackageLimits::Texts - total, "Localization count limit");
        total += count;
        std::map<std::string, std::string> texts;
        for (size_t k = 0; k < count; ++k)
        {
            auto key = r.Key();
            auto text = r.Text(PackageLimits::TextBytes);
            Need(texts.emplace(std::move(key), std::move(text)).second, "Duplicate localization key");
        }
        Need(p.locales.emplace(std::move(locale), std::move(texts)).second, "Duplicate locale");
    }
    const auto missions = r.Count(PackageLimits::Missions);
    for (size_t i = 0; i < missions; ++i)
    {
        auto key = r.Key();
        MissionPresentation m;
        m.titleKey = r.Key();
        m.summaryKey = r.Key();
        const auto objectives = r.Count(Limits::Nodes);
        for (size_t k = 0; k < objectives; ++k)
        {
            auto node = r.Key();
            auto text = r.Key();
            m.objectives.push_back({std::move(node), std::move(text)});
        }
        Need(p.missions.emplace(std::move(key), std::move(m)).second, "Duplicate mission presentation");
    }
    r.End();
    return p;
}
} // namespace
ContentDigest FingerprintDefinition(const CompiledDefinition &definition)
{
    return ContentHash(WriteDefinition(definition.Source()).bytes);
}

struct PackageCodec
{
    static std::shared_ptr<MissionPackage> Validate(const PackageSource &source)
    {
        Need(IsIdentifier(source.manifest.id) && source.manifest.version &&
                 IsIdentifier(source.manifest.defaultLocale),
             "Invalid package identity/version/default locale");
        Need(!source.missions.empty() && source.missions.size() <= PackageLimits::Missions,
             "Mission count limit");
        auto p = std::make_shared<MissionPackage>();
        p->manifest_ = source.manifest;
        p->presentation_ = source.presentation;
        std::set<std::string> ids;
        NativeRequirements derived;
        // Also validates cross-definition persistent choice domains atomically.
        DefinitionRegistry probe;
        const auto publish = probe.Activate(source.missions);
        if (publish.status != Publication::Status::Published)
            Bad(publish.errors.empty() ? "Invalid catalog"
                                       : publish.errors.front().path + ": " + publish.errors.front().message);
        for (const auto &definition : source.missions)
        {
            Need(ids.insert(definition.id).second, "Duplicate mission ID");
            p->graphs_.push_back(probe.Get(definition.id, definition.version));
            for (const auto &[type, version] : RequiredNativeTypes(definition))
                derived.emplace(type, version);
        }
        Need(source.manifest.requiresTypes.size() <= PackageLimits::NativeTypes,
             "Native requirement count limit");
        for (const auto &[type, version] : source.manifest.requiresTypes)
        {
            const auto *schema = FindNativeSchema(type);
            Need(schema && schema->version == version, "Unknown/unsupported mandatory native type version");
        }
        for (const auto &[type, version] : derived)
        {
            const auto found = source.manifest.requiresTypes.find(type);
            Need(found != source.manifest.requiresTypes.end() && found->second == version,
                 "Missing mandatory native type requirement");
        }
        std::sort(p->graphs_.begin(), p->graphs_.end(),
                  [](const auto &a, const auto &b) { return a->Source().id < b->Source().id; });
        const auto &presentation = source.presentation;
        Need(!presentation.locales.empty() && presentation.locales.size() <= PackageLimits::Locales &&
                 presentation.locales.contains(source.manifest.defaultLocale),
             "Missing/default locale or locale limit");
        Need(presentation.missions.size() == source.missions.size(),
             "Every mission requires its presentation");
        size_t textCount = 0;
        for (const auto &[locale, texts] : presentation.locales)
        {
            Need(IsIdentifier(locale) && texts.size() <= PackageLimits::Texts - textCount,
                 "Localization count limit");
            textCount += texts.size();
            for (const auto &[key, text] : texts)
                Need(IsIdentifier(key) && IsPresentationText(text), "Invalid localization key/UTF-8 text");
        }
        for (const auto &graph : p->graphs_)
        {
            const auto found = presentation.missions.find(graph->Source().id);
            Need(found != presentation.missions.end(), "Missing mission presentation");
            const auto &m = found->second;
            const auto textKey = [&](const std::string &key)
            {
                Need(IsIdentifier(key), "Invalid presentation text key");
                for (const auto &[locale, texts] : presentation.locales)
                    Need(texts.contains(key), "Missing localization key in a declared locale");
            };
            textKey(m.titleKey);
            textKey(m.summaryKey);
            Need(m.objectives.size() <= Limits::Nodes, "Presentation objective count limit");
            std::set<std::string> nodes;
            for (const auto &objective : m.objectives)
            {
                Need(graph->Find(objective.node) != NoNode && nodes.insert(objective.node).second,
                     "Unknown/duplicate presentation node");
                textKey(objective.titleKey);
            }
        }
        return p;
    }
    static PackageBuild Build(const PackageSource &input)
    {
        PackageBuild result;
        try
        {
            auto source = input;
            for (const auto &definition : source.missions)
            {
                const auto compiled = CompileDefinition(definition);
                if (!compiled)
                    Bad(compiled.errors.front().path + ": " + compiled.errors.front().message);
                for (const auto &[type, version] : RequiredNativeTypes(definition))
                {
                    auto [it, inserted] = source.manifest.requiresTypes.emplace(type, version);
                    Need(inserted || it->second == version, "Conflicting native requirement version");
                }
            }
            const auto package = Validate(source);
            Writer manifest;
            manifest.Text(package->manifest_.id);
            manifest.U32(package->manifest_.version);
            manifest.Text(package->manifest_.defaultLocale);
            Writer requirements;
            requirements.U32(uint32_t(package->manifest_.requiresTypes.size()));
            for (const auto &[type, version] : package->manifest_.requiresTypes)
            {
                requirements.U32(type);
                requirements.U16(version);
            }
            Writer graphs;
            graphs.U32(uint32_t(package->graphs_.size()));
            for (const auto &graph : package->graphs_)
            {
                const auto data = WriteDefinition(graph->Source());
                graphs.U32(uint32_t(data.bytes.size()));
                graphs.Raw(data.bytes);
            }
            Writer payload;
            payload.AddSection(Section::Manifest, manifest);
            payload.AddSection(Section::Graphs, graphs);
            payload.AddSection(Section::Requirements, requirements);
            payload.AddSection(Section::Presentation, WritePresentation(package->presentation_));
            Writer w;
            w.Raw(Magic);
            w.U16(Format);
            w.U16(HeaderSize);
            w.U32(uint32_t(payload.bytes.size()));
            w.U16(Schema);
            w.U16(Schema);
            w.U32(4);
            w.Raw(std::array<uint8_t, 32>{});
            w.U64(0);
            w.Raw(payload.bytes);
            const auto digest = Digest(w.bytes);
            std::copy(digest.begin(), digest.end(), w.bytes.begin() + 24);
            result.bytes = std::move(w.bytes);
        }
        catch (const std::exception &e)
        {
            result.errors.push_back({"package", e.what()});
        }
        return result;
    }
    static PackageLoad Load(std::span<const uint8_t> bytes)
    {
        PackageLoad result;
        try
        {
            Need(bytes.size() >= HeaderSize && bytes.size() <= PackageLimits::Bytes,
                 "Package byte limit/header");
            Reader r{bytes};
            const auto magic = r.Raw(Magic.size());
            Need(std::equal(magic.begin(), magic.end(), Magic.begin()), "Wrong .aurmission magic");
            Need(r.U16() == Format && r.U16() == HeaderSize, "Unsupported package format/header version");
            const auto payloadSize = r.U32();
            Need(payloadSize == bytes.size() - HeaderSize, "Package length mismatch");
            Need(r.U16() == Schema && r.U16() == Schema, "Unsupported graph/native registry schema");
            const auto sections = r.U32();
            Need(sections <= PackageLimits::Sections, "Section count limit");
            std::array<uint8_t, 32> expected{};
            const auto hash = r.Raw(expected.size());
            std::copy(hash.begin(), hash.end(), expected.begin());
            Need(r.U64() == 0, "Nonzero reserved header");
            Need(Digest(bytes) == expected, "Package SHA-256 mismatch");
            std::map<uint16_t, std::span<const uint8_t>> parts;
            std::set<uint16_t> seen;
            for (size_t i = 0; i < sections; ++i)
            {
                const auto type = r.U16();
                const auto version = r.U16();
                const auto flags = r.U32();
                Need((flags & ~Required) == 0 && seen.insert(type).second, "Invalid flags/duplicate section");
                auto part = r.Part(PackageLimits::Bytes);
                if (type >= uint16_t(Section::Manifest) && type <= uint16_t(Section::Presentation))
                {
                    Need(version == 1 && flags == Required, "Unsupported/malformed required section");
                    parts.emplace(type, part.bytes);
                }
                else
                    Need(!(flags & Required), "Unknown mandatory section");
            }
            r.End();
            Need(parts.size() == 4, "Missing required section");
            PackageSource source;
            Reader manifest{parts.at(uint16_t(Section::Manifest))};
            source.manifest.id = manifest.Key();
            source.manifest.version = manifest.U32();
            source.manifest.defaultLocale = manifest.Key();
            manifest.End();
            Reader requirements{parts.at(uint16_t(Section::Requirements))};
            const auto count = requirements.Count(PackageLimits::NativeTypes);
            for (size_t i = 0; i < count; ++i)
            {
                const auto type = requirements.U32();
                const auto version = requirements.U16();
                Need(source.manifest.requiresTypes.emplace(type, version).second,
                     "Duplicate native requirement");
                const auto *schema = FindNativeSchema(type);
                Need(schema && schema->version == version, "Unsupported mandatory native capability");
            }
            requirements.End();
            Reader graphs{parts.at(uint16_t(Section::Graphs))};
            const auto missions = graphs.Count(PackageLimits::Missions);
            for (size_t i = 0; i < missions; ++i)
            {
                auto graph = graphs.Part(PackageLimits::GraphBytes);
                source.missions.push_back(ReadDefinition(graph));
            }
            graphs.End();
            Reader presentation{parts.at(uint16_t(Section::Presentation))};
            source.presentation = ReadPresentation(presentation);
            auto package = Validate(source);
            package->digest_ = expected;
            result.package = std::move(package);
        }
        catch (const std::exception &e)
        {
            result.errors.push_back({"package", e.what()});
        }
        return result;
    }
};
PackageBuild CompilePackage(const PackageSource &source)
{
    return PackageCodec::Build(source);
}
PackageLoad LoadPackage(std::span<const uint8_t> bytes)
{
    return PackageCodec::Load(bytes);
}
PackageLoad LoadPackageFile(const std::filesystem::path &path)
{
    try
    {
        if (std::filesystem::file_size(path) > PackageLimits::Bytes)
            Bad("Package file byte limit");
        std::ifstream file(path, std::ios::binary);
        if (!file)
            Bad("Cannot open package");
        std::vector<uint8_t> bytes(PackageLimits::Bytes + 1);
        file.read(reinterpret_cast<char *>(bytes.data()), std::streamsize(bytes.size()));
        if (file.bad())
            Bad("Cannot read package");
        bytes.resize(size_t(file.gcount()));
        return LoadPackage(bytes);
    }
    catch (const std::exception &e)
    {
        return {nullptr, {{"package/file", e.what()}}};
    }
}
} // namespace auriga::mission
