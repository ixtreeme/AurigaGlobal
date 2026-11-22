#pragma once

#if defined(ENABLE_CHRISTMAS_WHEEL_OF_DESTINY)
#include <array>
#include <optional>
#include <item/ItemKey.hpp>

class CWheelDestiny
{
public:
    static constexpr std::size_t SlotCount = 16;

    explicit CWheelDestiny(entt::entity character) : owner(character) {}
    bool Open();
    void Close();
    void TurnWheel();
    void FinishAnimation() noexcept { showingResult = false; }
    bool CanClose() const noexcept { return !pending && !showingResult; }

private:
    entt::entity owner;
    std::array<std::optional<auriga::item::ItemKey>, SlotCount> slots;
    unsigned turns = 0;
    bool pending = false;
    bool showingResult = false;
};
#endif
