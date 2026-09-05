#include "../../SRC/Server/GameServer/stdafx.h"
#include "../../SRC/Server/GameServer/event_queue.h"
#include "../../SRC/Server/GameServer/ecs/components/status_components.hpp"
#include <Core/Logging.hpp>
#include <entt/entt.hpp>
#include <functional>
#include <iostream>
#include <stdexcept>

namespace {
HEART testHeart{};
int checks = 0;
void Check(bool value, const char* why) {
    ++checks;
    if (!value) throw std::runtime_error(why);
}
struct CallbackInfo : event_info_data {
    std::function<int32_t(LPEVENT, int32_t)> callback;
};
EVENTFUNC(TestCallback) {
    return static_cast<CallbackInfo*>(event->info)->callback(event, processing_time);
}
LPEVENT Schedule(std::function<int32_t(LPEVENT, int32_t)> callback, int delay = 1) {
    auto* info = AllocEventInfo<CallbackInfo>();
    info->callback = std::move(callback);
    return event_create(TestCallback, info, delay);
}
int Run(int pulse) {
    testHeart.pulse = pulse;
    return event_process(pulse);
}
void Reset() {
    event_destroy();
    testHeart.pulse = 0;
}
void SelfCancellationChecks() {
    Reset();
    LPEVENT owner;
    owner = Schedule([&](LPEVENT running, int elapsed) {
        Check(elapsed == 1, "processing delay is captured before queue deletion");
        // The real scheduler has already deleted the queue entry here. This
        // cancellation used to write through its stale q_el backlink.
        event_cancel(&owner);
        Check(!owner && running->is_force_to_end && !running->q_el,
            "self-cancellation clears ownership without a freed queue access");
        return 100;
    });
    auto retained = owner;
    Check(Run(1) == 1 && event_count() == 0, "self-cancelled event is not rescheduled");
    event_cancel(&retained);
    Check(!retained, "retained finished event can be cancelled again");
}
void ComponentChecks() {
    Reset();
    entt::registry registry;
    auto entity = registry.create();
    auto timer = Schedule([&](LPEVENT running, int) {
        registry.destroy(entity);
        Check(running->is_force_to_end && !running->q_el,
            "destroying an ECS owner during its timer cancels safely");
        return 10;
    });
    registry.emplace<ecs::AffectTickState>(entity).timer = timer;
    Check(Run(1) == 1 && event_count() == 0 && !registry.valid(entity),
        "RAII cancellation prevents another tick");

    entity = registry.create();
    const auto survivor = registry.create();
    registry.emplace<ecs::AffectTickState>(entity).timer = Schedule([](LPEVENT, int) { return 0; }, 5);
    auto survivingTimer = Schedule([](LPEVENT, int) { return 0; }, 5);
    registry.emplace<ecs::AffectTickState>(survivor).timer = survivingTimer;
    registry.remove<ecs::AffectTickState>(entity);
    Check(survivingTimer->q_el && !survivingTimer->q_el->bCancel,
        "EnTT relocation keeps the surviving real queue entry active");
    registry.clear<ecs::AffectTickState>();
    Check(survivingTimer->q_el->bCancel, "shutdown clear cancels queued entries");
    event_destroy();
    Check(!survivingTimer->q_el && !timer->q_el, "shutdown leaves no dangling queue backlinks");
}
void RepeatAndResetChecks() {
    Reset();
    int calls = 0;
    auto timer = Schedule([&](LPEVENT running, int elapsed) {
        Check(!running->q_el, "running callback has no queued entry");
        Check(elapsed == (calls ? 3 : 2), "repeat processing delay preserved");
        return ++calls < 2 ? 3 : 0;
    }, 2);
    Check(Run(2) == 1 && event_count() == 1 && timer->q_el && !timer->is_processing,
        "repeating event receives a new queue entry");
    Check(event_time(timer) == 3 && Run(5) == 1 && calls == 2 && !timer->q_el,
        "repeat terminates with cleared backlink");
    event_cancel(&timer);

    Reset(); calls = 0;
    timer = Schedule([&](LPEVENT, int elapsed) { ++calls; Check(elapsed == 5, "reset elapsed delay"); return 0; }, 1);
    event_reset_time(timer, 5);
    const auto* replacement = timer->q_el;
    Check(Run(1) == 0 && timer->q_el == replacement && event_time(timer) == 4,
        "deleting cancelled old queue entry cannot clear replacement backlink");
    Check(Run(5) == 1 && calls == 1 && !timer->q_el, "reset timer executes exactly once");
}
void ShutdownChecks() {
    Reset();
    auto timer = Schedule([](LPEVENT, int) { throw std::runtime_error("cancelled timer ran"); return 0; });
    auto retained = timer;
    event_cancel(&timer);
    Check(Run(1) == 0 && !retained->q_el, "cancelled dequeue clears retained event backlink");
    event_cancel(&retained);

    timer = Schedule([](LPEVENT, int) { return 0; });
    event_destroy();
    Check(event_count() == 0 && !timer->q_el, "event_destroy clears retained event backlink");
    event_cancel(&timer);
    Check(!timer, "late owner cancellation after queue shutdown is safe");

    LPEVENT localEvent(new event);
    {
        CEventQueue localQueue;
        localEvent->q_el = localQueue.Enqueue(localEvent, 1, 0);
    }
    Check(!localEvent->q_el, "queue destructor clears externally retained event backlink");
}
} // namespace

// Only the clock and fatal-error/logging hooks are doubles. Allocation, queue
// entries, scheduling, event callbacks and cancellation use production code.
LPHEART thecore_heart = &testHeart;
void ContinueOnFatalError() { throw std::runtime_error("unexpected fatal event error"); }
void ShutdownOnFatalError() { throw std::runtime_error("unexpected shutdown"); }
std::shared_ptr<spdlog::logger> logging::GetErrorLogger() { return spdlog::default_logger(); }

int main() {
    try {
        SelfCancellationChecks(); ComponentChecks(); RepeatAndResetChecks(); ShutdownChecks();
        event_destroy();
        std::cout << "Event checks passed: " << checks << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        event_destroy();
        return 1;
    }
}
