#pragma once
#include "Registry.hpp"
#include "components/identity_components.hpp"
#include <Core/resource_admission.h>
#include <chrono>
#include <initializer_list>
#include <iterator>

namespace ecs
{
struct NativeResourceAdmission
{
    explicit NativeResourceAdmission(entt::entity entity) : actor(entt::to_integral(entity)) {}
    auriga::resource_admission::Actor actor;
    std::optional<auriga::resource_admission::Pause> preparation;
    std::optional<auriga::resource_admission::Scope> scope;
    std::chrono::steady_clock::time_point deadline;
};
namespace resource_admission
{
namespace native = auriga::resource_admission;
inline bool EnsureActor(entt::entity entity)
{
    if (!g_registry.valid(entity))
        return false;
    if (!g_registry.all_of<NativeResourceAdmission>(entity))
    {
        NativeResourceAdmission value(entity);
        // emplace/get_or_emplace calls get AFTER on_construct, even if its
        // returned reference is discarded. A listener may destroy that actor.
        // A single-element move insert publishes without returning a reference.
        g_registry.insert<NativeResourceAdmission>(&entity, &entity + 1, std::make_move_iterator(&value));
    }
    return g_registry.valid(entity) && g_registry.all_of<NativeResourceAdmission>(entity);
}
inline std::optional<native::Permit> Existing(std::initializer_list<entt::entity> entities,
                                              native::Operation operation)
{
    if (!entities.size() || entities.size() > native::MaximumActors)
        return {};
    std::array<native::Actor *, native::MaximumActors> actors{};
    size_t count = 0;
    for (auto entity : entities)
    {
        auto *component =
            g_registry.valid(entity) ? g_registry.try_get<NativeResourceAdmission>(entity) : nullptr;
        const auto *pid = g_registry.valid(entity) ? g_registry.try_get<PlayerID>(entity) : nullptr;
        if (!component || !component->actor.MatchesEntity(entt::to_integral(entity), pid ? pid->pid : 0))
            return {};
        actors[count++] = &component->actor;
    }
    return native::Enter({actors.data(), count}, operation);
}
inline std::optional<native::Permit> Enter(std::initializer_list<entt::entity> entities,
                                           native::Operation operation)
{
    if (!entities.size() || entities.size() > native::MaximumActors)
        return {};
    // Component construction can call out. Finish every construction before
    // retaining any component address, and revalidate the complete participant set.
    for (auto entity : entities)
    {
        if (!EnsureActor(entity))
            return {};
    }
    // An aborted preparation/disconnected native session must not leave legacy
    // gameplay permanently blocked. Recover only as anonymous activity, after
    // the old operation has unwound. No Mission SQL submission exists here.
    for (auto entity : entities)
    {
        auto *component =
            g_registry.valid(entity) ? g_registry.try_get<NativeResourceAdmission>(entity) : nullptr;
        if (!component || !component->actor.RecoverLegacy(entt::to_integral(entity)))
            return {};
    }
    return Existing(entities, operation);
}
// Caller authenticates the live descriptor/native session first. Even an
// unbound legacy operation above prevents binding from its nested callbacks.
inline std::optional<native::Pause> Prepare(entt::entity entity, const auriga::player_session::Key &key,
                                            auriga::player_session::Token challenge)
{
    const auto *pid = g_registry.valid(entity) ? g_registry.try_get<PlayerID>(entity) : nullptr;
    if (!pid || pid->pid != key.character)
        return {};
    if (!EnsureActor(entity))
        return {};
    auto *component =
        g_registry.valid(entity) ? g_registry.try_get<NativeResourceAdmission>(entity) : nullptr;
    pid = g_registry.valid(entity) ? g_registry.try_get<PlayerID>(entity) : nullptr;
    if (!component || !pid || pid->pid != key.character ||
        !component->actor.Bind({key, entt::to_integral(entity)}))
        return {};
    const native::Scope scope{key, entt::to_integral(entity)};
    if (component->scope != scope)
    {
        component->preparation.reset();
        component->scope.reset();
    }
    std::array actors{&component->actor};
    return native::TryPause(actors, native::Supported, challenge);
}
inline bool StorePreparation(entt::entity entity, const auriga::player_session::Key &key,
                             native::Pause &&pause, std::chrono::steady_clock::time_point deadline)
{
    auto *component =
        g_registry.valid(entity) ? g_registry.try_get<NativeResourceAdmission>(entity) : nullptr;
    const native::Scope scope{key, entt::to_integral(entity)};
    const auto *pid = g_registry.valid(entity) ? g_registry.try_get<PlayerID>(entity) : nullptr;
    if (!component || !pid || pid->pid != key.character || component->preparation ||
        !pause.Matches(component->actor, scope, native::Supported))
        return false;
    component->scope = scope;
    component->deadline = deadline;
    component->preparation.emplace(std::move(pause));
    return true;
}
inline bool PreparationCurrent(entt::entity entity, const auriga::player_session::Key &key,
                               std::chrono::steady_clock::time_point now)
{
    const auto *component =
        g_registry.valid(entity) ? g_registry.try_get<NativeResourceAdmission>(entity) : nullptr;
    const native::Scope scope{key, entt::to_integral(entity)};
    const auto *pid = g_registry.valid(entity) ? g_registry.try_get<PlayerID>(entity) : nullptr;
    return component && pid && pid->pid == key.character && component->scope == scope &&
           component->preparation && now < component->deadline &&
           component->preparation->Matches(component->actor, scope, native::Supported);
}
inline void ReleasePreparation(entt::entity entity, const auriga::player_session::Key &key)
{
    if (auto *component =
            g_registry.valid(entity) ? g_registry.try_get<NativeResourceAdmission>(entity) : nullptr;
        component && component->scope == native::Scope{key, entt::to_integral(entity)})
    {
        component->preparation.reset();
        component->scope.reset();
    }
}
inline void Invalidate(entt::entity entity)
{
    if (auto *component =
            g_registry.valid(entity) ? g_registry.try_get<NativeResourceAdmission>(entity) : nullptr)
    {
        component->actor.Invalidate();
        component->preparation.reset();
        component->scope.reset();
    }
}
} // namespace resource_admission
} // namespace ecs
