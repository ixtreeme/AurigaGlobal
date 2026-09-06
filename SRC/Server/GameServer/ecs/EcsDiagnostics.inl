#pragma once

#include "Registry.hpp"

#include <fmt/format.h>

namespace ecs::diag {

template <typename T>
T* Require(entt::entity e, std::string_view what, const std::source_location& where)
{
    if (!Check(e, what, where))
        return nullptr;

    if (T* component = g_registry.try_get<T>(e))
        return component;

    Report(where, fmt::format("{}: {} has no {}", what,
        Describe(e), entt::type_name<T>::value()));
    return nullptr;
}

} // namespace ecs::diag
