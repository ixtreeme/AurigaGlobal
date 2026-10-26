#pragma once
#include "player_session.h"
#include <atomic>
#include <memory>

namespace auriga::resource_admission
{
enum class Operation : uint8_t
{
    Exchange = 1,
    InventoryMove = 2,
    Equipment = 4,
    Pickup = 8,
    ItemUse = 16,
    GroundDrop = 32
};
using Mask = uint8_t;
inline constexpr std::array Operations{Operation::Exchange, Operation::InventoryMove, Operation::Equipment,
                                       Operation::Pickup,   Operation::ItemUse,       Operation::GroundDrop};
inline constexpr Mask Supported = 63;
inline constexpr size_t MaximumActors = 8;
struct Scope
{
    player_session::Key session;
    uint64_t entity = 0;
    bool operator==(const Scope &) const = default;
};
struct State;
class Actor;
class Permit;
class Pause;
std::optional<Permit> Enter(std::span<Actor *const> actors, Operation operation);
std::optional<Pause> TryPause(std::span<Actor *const> actors, Mask domains, player_session::Token challenge);

// Construct/use on the native owner thread, which must outlive its actors.
// Tracks activity before a native session is bound too. Binding while a known
// operation is running is forbidden, including from a nested gameplay callback.
class Actor final
{
  public:
    explicit Actor(uint64_t entity);
    ~Actor();
    Actor(Actor &&other) noexcept;
    Actor &operator=(Actor &&other) noexcept;
    Actor(const Actor &) = delete;
    Actor &operator=(const Actor &) = delete;
    bool Bind(Scope scope);
    bool Matches(Scope scope) const;
    bool MatchesEntity(uint64_t entity, uint32_t player) const;
    // Legacy/pre-submit recovery only: no native identity or writer lease is
    // restored. An outstanding operation or pause must finish first.
    bool RecoverLegacy(uint64_t entity);
    void Invalidate();

  private:
    friend class Pause;
    friend std::optional<Permit> Enter(std::span<Actor *const>, Operation);
    friend std::optional<Pause> TryPause(std::span<Actor *const>, Mask, player_session::Token);
    std::atomic<std::shared_ptr<State>> state_;
};
// A permit owns state, never an ECS pointer. All participating actors enter or
// none do. Hold across callbacks and through the entire operation/publication.
class Permit final
{
  public:
    Permit(Permit &&other) noexcept;
    Permit &operator=(Permit &&other) noexcept;
    Permit(const Permit &) = delete;
    Permit &operator=(const Permit &) = delete;
    ~Permit();
    bool Current() const;
    void Reset();

  private:
    friend std::optional<Permit> Enter(std::span<Actor *const>, Operation);
    Permit(std::array<std::shared_ptr<State>, MaximumActors> states, size_t count, size_t domain);
    std::array<std::shared_ptr<State>, MaximumActors> states_;
    size_t count_ = 0, domain_ = 0;
};
// Only pauses the named, instrumented operation domains. This is NOT a complete
// source-stop, a cache receipt, a Mission writer lease, or a SQL submission API.
class Pause final
{
  public:
    Pause(Pause &&other) noexcept;
    Pause &operator=(Pause &&other) noexcept;
    Pause(const Pause &) = delete;
    Pause &operator=(const Pause &) = delete;
    ~Pause();
    bool Current() const;
    // Exact single-actor ownership, including the current state instance.
    bool Matches(const Actor &actor, Scope scope, Mask domains) const;
    void Reset();

  private:
    friend std::optional<Pause> TryPause(std::span<Actor *const>, Mask, player_session::Token);
    Pause(std::array<std::shared_ptr<State>, MaximumActors> states, size_t count, Mask domains,
          player_session::Token challenge);
    std::array<std::shared_ptr<State>, MaximumActors> states_;
    size_t count_ = 0;
    Mask domains_ = 0;
    player_session::Token challenge_{};
};
} // namespace auriga::resource_admission
