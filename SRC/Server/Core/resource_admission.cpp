#include "resource_admission.h"
#include <algorithm>
#include <limits>
#include <mutex>
#include <thread>
#include <utility>

namespace auriga::resource_admission
{
struct State
{
    explicit State(uint64_t identity) : entity(identity), thread(std::this_thread::get_id()) {}
    uint64_t entity;
    const std::thread::id thread;
    mutable std::mutex mutex;
    bool valid = true;
    std::optional<Scope> scope;
    std::array<uint32_t, Operations.size()> active{};
    Mask paused = 0;
    player_session::Token challenge{};
};
namespace
{
bool Owned(const State &state)
{
    return state.valid && state.thread == std::this_thread::get_id();
}
bool Valid(const Scope &scope)
{
    const auto &key = scope.session;
    return key.Versioned() && player_session::Nonzero(key.session) && key.generation && key.account &&
           key.character;
}
struct Batch
{
    std::array<std::shared_ptr<State>, MaximumActors> states;
    std::array<std::unique_lock<std::mutex>, MaximumActors> locks;
    size_t count = 0;
    bool Lock()
    {
        std::sort(states.begin(), states.begin() + count,
                  [](const auto &a, const auto &b) { return std::less<State *>{}(a.get(), b.get()); });
        count = size_t(std::unique(states.begin(), states.begin() + count) - states.begin());
        for (size_t i = 0; i < count; ++i)
        {
            if (!states[i])
                return false;
            locks[i] = std::unique_lock(states[i]->mutex);
            if (!Owned(*states[i]))
                return false;
        }
        return true;
    }
};
} // namespace
Actor::Actor(uint64_t entity) : state_(std::make_shared<State>(entity)) {}
Actor::~Actor()
{
    Invalidate();
}
Actor::Actor(Actor &&other) noexcept : state_(other.state_.exchange(nullptr)) {}
Actor &Actor::operator=(Actor &&other) noexcept
{
    if (this != &other)
    {
        Invalidate();
        state_.store(other.state_.exchange(nullptr));
    }
    return *this;
}
bool Actor::Bind(Scope scope)
{
    const auto old = state_.load();
    if (!old || !Valid(scope))
        return false;
    std::lock_guard lock(old->mutex);
    if (old->thread != std::this_thread::get_id() || old->entity != scope.entity)
        return false;
    if (old->valid && old->scope == scope)
        return true;
    if (std::any_of(old->active.begin(), old->active.end(), [](auto count) { return count != 0; }))
        return false;
    auto replacement = std::make_shared<State>(scope.entity);
    replacement->scope = scope;
    old->valid = false;
    state_.store(std::move(replacement));
    return true;
}
bool Actor::Matches(Scope scope) const
{
    const auto state = state_.load();
    if (!state)
        return false;
    std::lock_guard lock(state->mutex);
    return Owned(*state) && state->scope == scope;
}
bool Actor::MatchesEntity(uint64_t entity, uint32_t player) const
{
    const auto state = state_.load();
    if (!state)
        return false;
    std::lock_guard lock(state->mutex);
    return Owned(*state) && state->entity == entity &&
           (!state->scope || state->scope->session.character == player);
}
bool Actor::RecoverLegacy(uint64_t entity)
{
    const auto old = state_.load();
    if (!old)
        return false;
    std::lock_guard lock(old->mutex);
    if (old->thread != std::this_thread::get_id() || old->entity != entity)
        return false;
    if (old->valid)
        return true;
    if (old->paused ||
        std::any_of(old->active.begin(), old->active.end(), [](auto count) { return count != 0; }))
        return false;
    state_.store(std::make_shared<State>(entity));
    return true;
}
void Actor::Invalidate()
{
    if (const auto state = state_.load())
    {
        std::lock_guard lock(state->mutex);
        state->valid = false;
    }
}
std::optional<Permit> Enter(std::span<Actor *const> actors, Operation operation)
{
    const auto mask = static_cast<Mask>(operation);
    const auto known = std::find(Operations.begin(), Operations.end(), operation);
    if (actors.empty() || actors.size() > MaximumActors || known == Operations.end())
        return {};
    Batch batch;
    for (auto *actor : actors)
    {
        if (!actor)
            return {};
        batch.states[batch.count++] = actor->state_.load();
    }
    const size_t domain = size_t(known - Operations.begin());
    if (!batch.Lock())
        return {};
    for (size_t i = 0; i < batch.count; ++i)
        if ((batch.states[i]->paused & mask) || batch.states[i]->active[domain] == UINT32_MAX)
            return {};
    for (size_t i = 0; i < batch.count; ++i)
        ++batch.states[i]->active[domain];
    return Permit(std::move(batch.states), batch.count, domain);
}
std::optional<Pause> TryPause(std::span<Actor *const> actors, Mask domains, player_session::Token challenge)
{
    if (actors.empty() || actors.size() > MaximumActors || !domains || (domains & ~Supported) ||
        !player_session::Nonzero(challenge))
        return {};
    Batch batch;
    for (auto *actor : actors)
    {
        if (!actor)
            return {};
        batch.states[batch.count++] = actor->state_.load();
    }
    if (!batch.Lock())
        return {};
    for (size_t i = 0; i < batch.count; ++i)
    {
        const auto &state = *batch.states[i];
        if (!state.scope || state.paused)
            return {};
        for (size_t domain = 0; domain < Operations.size(); ++domain)
            if ((domains & static_cast<Mask>(Operations[domain])) && state.active[domain])
                return {};
    }
    for (size_t i = 0; i < batch.count; ++i)
    {
        batch.states[i]->paused = domains;
        batch.states[i]->challenge = challenge;
    }
    return Pause(std::move(batch.states), batch.count, domains, challenge);
}
Permit::Permit(std::array<std::shared_ptr<State>, MaximumActors> states, size_t count, size_t domain)
    : states_(std::move(states)), count_(count), domain_(domain)
{
}
Permit::Permit(Permit &&other) noexcept
    : states_(std::move(other.states_)), count_(std::exchange(other.count_, 0)), domain_(other.domain_)
{
}
Permit &Permit::operator=(Permit &&other) noexcept
{
    if (this != &other)
    {
        Reset();
        states_ = std::move(other.states_);
        count_ = std::exchange(other.count_, 0);
        domain_ = other.domain_;
    }
    return *this;
}
Permit::~Permit()
{
    Reset();
}
bool Permit::Current() const
{
    if (!count_)
        return false;
    for (size_t i = 0; i < count_; ++i)
    {
        std::lock_guard lock(states_[i]->mutex);
        if (!Owned(*states_[i]))
            return false;
    }
    return true;
}
void Permit::Reset()
{
    for (size_t i = 0; i < count_; ++i)
    {
        std::lock_guard lock(states_[i]->mutex);
        --states_[i]->active[domain_];
    }
    count_ = 0;
    states_ = {};
}
Pause::Pause(std::array<std::shared_ptr<State>, MaximumActors> states, size_t count, Mask domains,
             player_session::Token challenge)
    : states_(std::move(states)), count_(count), domains_(domains), challenge_(challenge)
{
}
Pause::Pause(Pause &&other) noexcept
    : states_(std::move(other.states_)), count_(std::exchange(other.count_, 0)), domains_(other.domains_),
      challenge_(other.challenge_)
{
}
Pause &Pause::operator=(Pause &&other) noexcept
{
    if (this != &other)
    {
        Reset();
        states_ = std::move(other.states_);
        count_ = std::exchange(other.count_, 0);
        domains_ = other.domains_;
        challenge_ = other.challenge_;
    }
    return *this;
}
Pause::~Pause()
{
    Reset();
}
bool Pause::Current() const
{
    if (!count_)
        return false;
    for (size_t i = 0; i < count_; ++i)
    {
        std::lock_guard lock(states_[i]->mutex);
        if (!Owned(*states_[i]) || states_[i]->paused != domains_ || states_[i]->challenge != challenge_)
            return false;
    }
    return true;
}
bool Pause::Matches(const Actor &actor, Scope scope, Mask domains) const
{
    if (count_ != 1 || states_[0] != actor.state_.load() || domains_ != domains)
        return false;
    std::lock_guard lock(states_[0]->mutex);
    return Owned(*states_[0]) && states_[0]->scope == scope && states_[0]->paused == domains_ &&
           states_[0]->challenge == challenge_;
}
void Pause::Reset()
{
    for (size_t i = 0; i < count_; ++i)
    {
        std::lock_guard lock(states_[i]->mutex);
        if (states_[i]->challenge == challenge_ && states_[i]->paused == domains_)
        {
            states_[i]->paused = 0;
            states_[i]->challenge = {};
        }
    }
    count_ = 0;
    states_ = {};
}
} // namespace auriga::resource_admission
