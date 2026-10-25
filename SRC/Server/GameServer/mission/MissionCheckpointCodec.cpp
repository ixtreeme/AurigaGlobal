#include "MissionCheckpointCodec.hpp"
#include "MissionPackage.hpp"
#include <algorithm>
#include <stdexcept>

namespace auriga::mission
{
namespace
{
constexpr std::array<uint8_t, 8> Magic{'A', 'U', 'R', 'C', 'H', 'K', 'P', 0};
constexpr uint16_t Header = 64, Format = 1;
void Need(bool condition, const char *why)
{
    if (!condition)
        throw std::invalid_argument(why);
}
struct Writer
{
    std::vector<uint8_t> bytes;
    void Raw(std::span<const uint8_t> value)
    {
        Need(value.size() <= CheckpointBytes - bytes.size(), "Checkpoint byte limit");
        bytes.insert(bytes.end(), value.begin(), value.end());
    }
    void Number(uint64_t value, size_t count)
    {
        std::array<uint8_t, 8> data{};
        for (size_t i = 0; i < count; ++i)
            data[i] = uint8_t(value >> (8 * i));
        Raw({data.data(), count});
    }
    void Key(std::string_view value)
    {
        Need(IsIdentifier(value), "Invalid checkpoint identifier");
        Number(value.size(), 2);
        Raw({reinterpret_cast<const uint8_t *>(value.data()), value.size()});
    }
};
struct Reader
{
    std::span<const uint8_t> bytes;
    size_t position = 0;
    std::span<const uint8_t> Raw(size_t size)
    {
        Need(size <= bytes.size() - position, "Truncated checkpoint");
        auto value = bytes.subspan(position, size);
        position += size;
        return value;
    }
    uint64_t Number(size_t size)
    {
        uint64_t value = 0;
        auto data = Raw(size);
        for (size_t i = 0; i < size; ++i)
            value |= uint64_t(data[i]) << (8 * i);
        return value;
    }
    size_t Count(size_t maximum)
    {
        const auto size = Number(4);
        Need(size <= maximum && size <= bytes.size() - position, "Checkpoint container limit");
        return size_t(size);
    }
    std::string Key()
    {
        const auto size = Number(2);
        Need(size <= Limits::IdentifierBytes, "Checkpoint identifier limit");
        auto data = Raw(size_t(size));
        std::string value(reinterpret_cast<const char *>(data.data()), data.size());
        Need(IsIdentifier(value), "Invalid checkpoint identifier");
        return value;
    }
    Timestamp Time()
    {
        auto value = Number(8);
        Need(value <= uint64_t(INT64_MAX), "Checkpoint time outside Int64");
        return Timestamp(value);
    }
    bool Bool()
    {
        auto value = Number(1);
        Need(value <= 1, "Noncanonical checkpoint boolean");
        return value != 0;
    }
    ContentDigest Digest()
    {
        ContentDigest value{};
        auto data = Raw(value.size());
        std::copy(data.begin(), data.end(), value.begin());
        return value;
    }
};
ContentDigest Checksum(std::span<const uint8_t> bytes)
{
    Need(bytes.size() >= Header, "Missing checkpoint header");
    constexpr ContentDigest zero{};
    return ContentHashParts({bytes.first(24), zero, bytes.subspan(56)});
}
uint8_t Wire(MissionStatus status)
{
    switch (status)
    {
    case MissionStatus::Active:
        return 1;
    case MissionStatus::Completed:
        return 2;
    case MissionStatus::Failed:
        return 3;
    case MissionStatus::Abandoned:
        return 4;
    }
    throw std::invalid_argument("Unknown mission status");
}
MissionStatus MissionState(uint64_t status)
{
    switch (status)
    {
    case 1:
        return MissionStatus::Active;
    case 2:
        return MissionStatus::Completed;
    case 3:
        return MissionStatus::Failed;
    case 4:
        return MissionStatus::Abandoned;
    }
    throw std::invalid_argument("Unknown wire mission status");
}
uint8_t Wire(NodeStatus status)
{
    switch (status)
    {
    case NodeStatus::Dormant:
        return 1;
    case NodeStatus::Active:
        return 2;
    case NodeStatus::Completed:
        return 3;
    case NodeStatus::Failed:
        return 4;
    case NodeStatus::Cancelled:
        return 5;
    }
    throw std::invalid_argument("Unknown node status");
}
NodeStatus NodeState(uint64_t status)
{
    switch (status)
    {
    case 1:
        return NodeStatus::Dormant;
    case 2:
        return NodeStatus::Active;
    case 3:
        return NodeStatus::Completed;
    case 4:
        return NodeStatus::Failed;
    case 5:
        return NodeStatus::Cancelled;
    }
    throw std::invalid_argument("Unknown wire node status");
}
struct Document
{
    MissionRuntime::Checkpoint checkpoint;
    CheckpointScan metadata;
};
Document Read(std::span<const uint8_t> bytes)
{
    Need(bytes.size() >= Header && bytes.size() <= CheckpointBytes, "Checkpoint size limit");
    Reader r{bytes};
    auto magic = r.Raw(8);
    Need(std::equal(magic.begin(), magic.end(), Magic.begin()), "Checkpoint magic");
    Need(r.Number(2) == Format && r.Number(2) == Header, "Unsupported checkpoint format/header");
    Need(r.Number(4) == bytes.size() - Header, "Checkpoint payload length");
    Need(r.Number(8) == 0, "Nonzero checkpoint reserved header");
    const auto digest = r.Digest();
    Need(r.Number(8) == 0, "Nonzero checkpoint reserved footer");
    Need(Checksum(bytes) == digest, "Checkpoint SHA-256 mismatch");
    Document document;
    auto &c = document.checkpoint;
    auto &m = document.metadata;
    c.character = uint32_t(r.Number(4));
    c.format = uint32_t(r.Number(4));
    c.revision = r.Number(8);
    c.now = r.Time();
    Need(c.character && c.format == 1 && c.revision < UINT64_MAX, "Invalid checkpoint identity/version");
    m.character = c.character;
    m.revision = c.revision;
    m.now = c.now;
    m.digest = digest;
    const auto references = r.Count(Limits::Instances);
    for (size_t i = 0; i < references; ++i)
    {
        DefinitionReference ref{r.Key(), uint32_t(r.Number(4)), r.Digest()};
        Need(ref.version && (m.definitions.empty() ||
                             std::pair(m.definitions.back().mission, m.definitions.back().version) <
                                 std::pair(ref.mission, ref.version)),
             "Duplicate/unordered definition reference");
        m.definitions.push_back(std::move(ref));
    }
    const auto flags = r.Count(Limits::Flags);
    std::string previous;
    for (size_t i = 0; i < flags; ++i)
    {
        auto key = r.Key();
        Need(previous < key, "Duplicate/unordered flag");
        previous = key;
        c.facts.flags.insert(std::move(key));
    }
    const auto choices = r.Count(Limits::Flags);
    previous.clear();
    for (size_t i = 0; i < choices; ++i)
    {
        auto key = r.Key();
        auto value = r.Key();
        Need(previous < key, "Duplicate/unordered choice");
        previous = key;
        c.facts.choices.emplace(std::move(key), std::move(value));
    }
    const auto streams = r.Count(Limits::Streams);
    uint64_t previousStream = 0;
    for (size_t i = 0; i < streams; ++i)
    {
        const auto stream = r.Number(8);
        EventWindow window;
        window.highest = r.Number(8);
        Need(stream > previousStream && window.highest, "Duplicate/unordered/empty event stream");
        previousStream = stream;
        for (size_t byte = 0; byte < Limits::EventWindow / 8; ++byte)
        {
            auto bits = r.Number(1);
            for (size_t bit = 0; bit < 8; ++bit)
                window.seen[byte * 8 + bit] = (bits & (1u << bit)) != 0;
        }
        Need(window.seen[0], "Missing newest event marker");
        for (size_t bit = size_t(std::min<uint64_t>(window.highest, Limits::EventWindow));
             bit < Limits::EventWindow; ++bit)
            Need(!window.seen[bit], "Event window before stream start");
        c.streams.emplace(stream, std::move(window));
    }
    const auto instances = r.Count(Limits::Instances);
    uint64_t previousInstance = 0;
    size_t subjects = 0;
    std::set<size_t> used;
    for (size_t i = 0; i < instances; ++i)
    {
        MissionRuntime::SavedInstance instance;
        instance.id = r.Number(8);
        auto reference = r.Number(4);
        Need(instance.id > previousInstance && reference < m.definitions.size(),
             "Invalid instance/reference order");
        previousInstance = instance.id;
        used.insert(size_t(reference));
        const auto &ref = m.definitions[size_t(reference)];
        instance.mission = ref.mission;
        instance.version = ref.version;
        instance.status = MissionState(r.Number(1));
        const auto nodes = r.Count(Limits::Nodes);
        Need(nodes, "Empty instance nodes");
        instance.nodes.reserve(nodes);
        for (size_t k = 0; k < nodes; ++k)
        {
            NodeProgress p;
            p.status = NodeState(r.Number(1));
            p.counter = r.Number(8);
            p.selected = uint32_t(r.Number(4));
            Need(p.counter <= Limits::Counter && (p.selected == NoNode || p.selected <= Limits::Nodes),
                 "Node progress range");
            p.activatedAt = r.Time();
            p.deadline = r.Time();
            p.receipt = r.Bool();
            p.completedChildren = uint32_t(r.Number(4));
            p.failedChildren = uint32_t(r.Number(4));
            p.spawned = r.Bool();
            Need(p.completedChildren <= Limits::Nodes && p.failedChildren <= Limits::Nodes,
                 "Child count range");
            const auto distinct = r.Count(Limits::Distinct);
            Need(distinct <= Limits::DistinctPerCharacter - subjects, "Total distinct subject limit");
            subjects += distinct;
            previous.clear();
            for (size_t j = 0; j < distinct; ++j)
            {
                auto key = r.Key();
                Need(previous < key, "Duplicate/unordered subject");
                previous = key;
                p.subjects.insert(std::move(key));
            }
            instance.nodes.push_back(std::move(p));
        }
        c.instances.emplace(instance.id, std::move(instance));
    }
    Need(used.size() == m.definitions.size(), "Unused definition reference");
    Need(r.position == bytes.size(), "Trailing checkpoint data");
    return document;
}
bool Bound(const MissionRuntime::Checkpoint &c, const std::vector<DefinitionReference> &references,
           const DefinitionRegistry &registry)
{
    std::set<std::string> choices;
    for (const auto &reference : references)
    {
        const auto definition = registry.Get(reference.mission, reference.version);
        if (!definition || FingerprintDefinition(*definition) != reference.fingerprint)
            return false;
        for (const auto &[key, values] : definition->Source().choices)
            choices.insert(key);
    }
    for (const auto &[key, value] : c.facts.choices)
        if (!choices.contains(key))
            return false;
    return MissionRuntime::ValidateCheckpoint(registry, c);
}
} // namespace

CheckpointBuild EncodeCheckpoint(const MissionRuntime::Checkpoint &c, const DefinitionRegistry &registry)
{
    CheckpointBuild build;
    try
    {
        Need(MissionRuntime::ValidateCheckpoint(registry, c), "Invalid runtime checkpoint");
        std::map<std::pair<std::string, uint32_t>, ContentDigest> references;
        for (const auto &[id, saved] : c.instances)
        {
            auto key = std::pair(saved.mission, saved.version);
            if (!references.contains(key))
                references.emplace(key, FingerprintDefinition(*registry.Get(saved.mission, saved.version)));
        }
        std::vector<DefinitionReference> pins;
        for (const auto &[key, digest] : references)
            pins.push_back({key.first, key.second, digest});
        Need(Bound(c, pins, registry), "Checkpoint choice/domain binding missing");
        Writer w;
        w.Raw(Magic);
        w.Number(Format, 2);
        w.Number(Header, 2);
        w.Number(0, 4);
        w.Number(0, 8);
        w.Raw(ContentDigest{});
        w.Number(0, 8);
        w.Number(c.character, 4);
        w.Number(c.format, 4);
        w.Number(c.revision, 8);
        w.Number(uint64_t(c.now), 8);
        w.Number(pins.size(), 4);
        for (const auto &pin : pins)
        {
            w.Key(pin.mission);
            w.Number(pin.version, 4);
            w.Raw(pin.fingerprint);
        }
        w.Number(c.facts.flags.size(), 4);
        for (const auto &flag : c.facts.flags)
            w.Key(flag);
        w.Number(c.facts.choices.size(), 4);
        for (const auto &[key, value] : c.facts.choices)
        {
            w.Key(key);
            w.Key(value);
        }
        w.Number(c.streams.size(), 4);
        for (const auto &[stream, window] : c.streams)
        {
            w.Number(stream, 8);
            w.Number(window.highest, 8);
            for (size_t byte = 0; byte < Limits::EventWindow / 8; ++byte)
            {
                uint8_t bits = 0;
                for (size_t bit = 0; bit < 8; ++bit)
                    if (window.seen[byte * 8 + bit])
                        bits |= uint8_t(1u << bit);
                w.Number(bits, 1);
            }
        }
        w.Number(c.instances.size(), 4);
        for (const auto &[id, saved] : c.instances)
        {
            const auto position = references.find({saved.mission, saved.version});
            w.Number(id, 8);
            w.Number(size_t(std::distance(references.begin(), position)), 4);
            w.Number(Wire(saved.status), 1);
            w.Number(saved.nodes.size(), 4);
            for (const auto &p : saved.nodes)
            {
                w.Number(Wire(p.status), 1);
                w.Number(p.counter, 8);
                w.Number(p.selected, 4);
                w.Number(uint64_t(p.activatedAt), 8);
                w.Number(uint64_t(p.deadline), 8);
                w.Number(p.receipt, 1);
                w.Number(p.completedChildren, 4);
                w.Number(p.failedChildren, 4);
                w.Number(p.spawned, 1);
                w.Number(p.subjects.size(), 4);
                for (const auto &subject : p.subjects)
                    w.Key(subject);
            }
        }
        const auto length = w.bytes.size() - Header;
        for (size_t i = 0; i < 4; ++i)
            w.bytes[12 + i] = uint8_t(length >> (8 * i));
        const auto digest = Checksum(w.bytes);
        std::copy(digest.begin(), digest.end(), w.bytes.begin() + 24);
        build.bytes = std::move(w.bytes);
        build.metadata = {c.character, c.revision, c.now, digest, std::move(pins), {}};
    }
    catch (const std::exception &error)
    {
        build.metadata.errors.push_back({"checkpoint", error.what()});
    }
    return build;
}
CheckpointScan ScanCheckpoint(std::span<const uint8_t> bytes)
{
    try
    {
        return Read(bytes).metadata;
    }
    catch (const std::exception &error)
    {
        CheckpointScan result;
        result.errors.push_back({"checkpoint", error.what()});
        return result;
    }
}
CheckpointLoad DecodeCheckpoint(std::span<const uint8_t> bytes, const DefinitionRegistry &registry)
{
    try
    {
        auto document = Read(bytes);
        Need(Bound(document.checkpoint, document.metadata.definitions, registry),
             "Checkpoint definition content or state differs");
        return {std::move(document.checkpoint), {}};
    }
    catch (const std::exception &error)
    {
        return {std::nullopt, {{"checkpoint", error.what()}}};
    }
}
} // namespace auriga::mission
