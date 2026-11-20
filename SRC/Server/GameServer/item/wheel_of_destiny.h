#pragma once

#if defined(ENABLE_CHRISTMAS_WHEEL_OF_DESTINY)
class CWheelDestiny
{
public:
    explicit CWheelDestiny(entt::entity owner);
    void TurnWheel();
    void FinishAnimation() noexcept { showingResult = false; }
    bool CanClose() const noexcept { return !pending && !showingResult; }

private:
    entt::entity owner;
    unsigned turns=0;
    bool pending=false;
    bool showingResult=false;
};
#endif
