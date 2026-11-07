#pragma once
#include <entt/entt.hpp>
#include <functional>
#include <cstdint>
#include <item/BookLearning.hpp>
bool HandleTypedBooksPacket(entt::entity,const char*);
bool ApplyTypedBookLogin(entt::entity);
// Ordinary progression callbacks wait for the indivisible item/skill write.
bool DeferTypedBookMutation(entt::entity,std::function<void()>);
bool DeferTypedBookLogin(std::uint32_t,std::function<void()>);
std::optional<auriga::item::BookCharacter> TypedBookCharacter(entt::entity);
// Shared progression barrier for pet study spending a typed book-aid charge.
// Finish after installing authoritative items: apply checkpoint, replay mutations.
std::function<void(bool,bool)> HoldTypedBookCheckpoint(entt::entity);
