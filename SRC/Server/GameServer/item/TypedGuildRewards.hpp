#pragma once
#include <item/ItemKey.hpp>
#include <cstdint>

// Server-authored quest operation; recipients are resolved and checked in SQL.
bool QueueTypedGuildReward(std::uint32_t guild,auriga::item::ItemKey,std::uint16_t level,std::uint32_t count);
