#pragma once
#include <entt/entt.hpp>
#include <functional>
#include <cstdint>
bool HandleTypedBooksPacket(entt::entity,const char*);
bool ApplyTypedBookLogin(entt::entity);
// Ordinary progression callbacks wait for the indivisible item/skill write.
bool DeferTypedBookMutation(entt::entity,std::function<void()>);
bool DeferTypedBookLogin(std::uint32_t,std::function<void()>);
