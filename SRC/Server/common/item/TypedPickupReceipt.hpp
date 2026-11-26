#pragma once
#include "PersistentItems.hpp"
#include <optional>
#include <string>
#include <string_view>

namespace auriga::item
{
// GC_CHAT_NEW index 102: count#canonical typed link, never a prototype or name lookup.
inline std::optional<std::string> TypedPickupReceipt(const ItemGrant& grant, std::string_view name)
{
    if (!grant.count || name.empty() || name.size()>160 ||
        name.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_.-")!=std::string_view::npos)
        return std::nullopt;
    return std::to_string(grant.count)+"#|Hti:"+std::to_string(static_cast<unsigned>(grant.key.Category()))+
        ":"+std::to_string(grant.key.LocalId())+":"+std::to_string(grant.refinementLevel)+
        ":"+std::to_string(grant.count)+":0::|h["+std::string(name)+"]|h";
}
}
