#include <Core/resource_admission.h>
#include "../../SRC/Server/GameServer/ecs/ResourceAdmission.hpp"
#include <iostream>
#include <stdexcept>
#include <thread>
#include <type_traits>

entt::registry g_registry;
namespace gate = auriga::resource_admission;
namespace session = auriga::player_session;
namespace native = ecs::resource_admission;
namespace
{
int checks = 0;
void Check(bool value, const char *why)
{
    ++checks;
    if (!value)
        throw std::runtime_error(why);
}
session::Token Token(uint8_t n)
{
    session::Token result{};
    result.fill(n);
    return result;
}
session::Key Key(uint32_t pid = 100, uint64_t generation = 1)
{
    return {Token(1), Token(2), Token(3), generation, 900, pid};
}
void Activity()
{
    static_assert(!std::is_copy_constructible_v<gate::Actor> && !std::is_copy_constructible_v<gate::Permit> &&
                  !std::is_copy_constructible_v<gate::Pause>);
    gate::Actor a(0), b(1);
    std::array one{&a};
    std::array pair{&a, &b};
    std::array duplicate{&a, &a};
    Check(!gate::TryPause(one, gate::Supported, Token(4)), "unbound actor granted native pause");
    auto permit = gate::Enter(duplicate, gate::Operation::Exchange);
    Check(permit && permit->Current() && !a.Bind({Key(), 0}), "legacy activity allowed native binding");
    auto nested = gate::Enter(pair, gate::Operation::InventoryMove);
    Check(nested && nested->Current(), "nested multi-actor activity refused");
    permit.reset();
    nested.reset();
    Check(a.Bind({Key(), 0}) && b.Bind({Key(101), 1}), "quiescent native binding failed");
    auto pause = gate::TryPause(duplicate, gate::Supported, Token(4));
    Check(pause && pause->Current() && !gate::Enter(pair, gate::Operation::Exchange),
          "paused actor did not prevent the complete operation");
    std::array right{&b};
    auto otherPause = gate::TryPause(right, gate::Supported, Token(5));
    Check(otherPause && otherPause->Current(), "failed multi-entry left the other actor active");
    otherPause.reset();
    pause.reset();
    permit = gate::Enter(one, gate::Operation::InventoryMove);
    Check(!gate::TryPause(pair, gate::Supported, Token(4)), "pause succeeded during active operation");
    auto rightPermit = gate::Enter(right, gate::Operation::Exchange);
    Check(rightPermit && rightPermit->Current(), "failed multi-pause froze an unrelated actor");
    rightPermit.reset();
    permit.reset();
    pause = gate::TryPause(pair, uint8_t(gate::Operation::Exchange), Token(4));
    Check(pause && !gate::Enter(pair, gate::Operation::Exchange) &&
              gate::Enter(pair, gate::Operation::InventoryMove).has_value(),
          "pause crossed operation domains");
    Check(!gate::TryPause(pair, gate::Supported, Token(4)), "duplicate pause created two unlocking owners");
    pause.reset();
    Check(gate::TryPause(pair, gate::Supported, Token(4)).has_value(), "RAII release leaked a pause");
    for (int i = 0; i < 256; ++i)
    {
        auto hold = gate::Enter(duplicate, gate::Operation::Exchange);
        Check(hold && !gate::TryPause(pair, gate::Supported, Token(4)), "active duplicate admission escaped");
        hold.reset();
        Check(gate::TryPause(pair, gate::Supported, Token(4)).has_value(),
              "duplicate counted more than once");
    }
}
void Lifetimes()
{
    gate::Actor actor(7);
    std::array one{&actor};
    Check(actor.Bind({Key(), 7}), "initial binding failed");
    auto old = gate::TryPause(one, gate::Supported, Token(4));
    Check(old && actor.Bind({Key(100, 2), 7}) && !old->Current(), "new session inherited old pause");
    auto next = gate::TryPause(one, gate::Supported, Token(5));
    Check(next && next->Current(), "replacement session could not pause");
    old.reset();
    Check(next->Current() && !gate::Enter(one, gate::Operation::Exchange),
          "old release unlocked new session");
    gate::Actor moved(std::move(actor));
    Check(next->Current() && moved.Matches({Key(100, 2), 7}) && !actor.Matches({Key(100, 2), 7}),
          "ECS component relocation changed identity");
    next.reset();
    std::array relocated{&moved};
    auto live = gate::Enter(relocated, gate::Operation::Exchange);
    moved.Invalidate();
    Check(!live->Current() && !moved.Bind({Key(100, 3), 7}), "invalidated active operation allowed rebind");
    live.reset();
    Check(moved.Bind({Key(100, 3), 7}), "idle invalidated actor could not rebind");
    bool foreignGranted = false;
    std::thread foreign(
        [&]
        {
            foreignGranted = gate::Enter(relocated, gate::Operation::Exchange).has_value() ||
                             gate::TryPause(relocated, gate::Supported, Token(4)).has_value();
        });
    foreign.join();
    Check(!foreignGranted && gate::TryPause(relocated, gate::Supported, Token(4)).has_value(),
          "foreign thread gained native writer admission");
    std::optional<gate::Pause> retired;
    {
        gate::Actor temporary(8);
        temporary.Bind({Key(101), 8});
        std::array owners{&temporary};
        retired = gate::TryPause(owners, gate::Supported, Token(6));
    }
    Check(retired && !retired->Current(), "destroyed actor left a live pause proof");
    auto held = gate::TryPause(relocated, gate::Supported, Token(7));
    moved.Invalidate();
    Check(!moved.RecoverLegacy(7), "legacy recovery released an owned pause");
    held.reset();
    Check(moved.RecoverLegacy(7) && !moved.Matches({Key(100, 3), 7}) &&
              !gate::TryPause(relocated, gate::Supported, Token(7)),
          "legacy recovery restored native identity");
    live = gate::Enter(relocated, gate::Operation::InventoryMove);
    moved.Invalidate();
    Check(!moved.RecoverLegacy(7), "legacy recovery revived an active cancelled operation");
    live.reset();
    Check(moved.RecoverLegacy(7) && gate::Enter(relocated, gate::Operation::Exchange).has_value(),
          "idle legacy recovery left gameplay blocked");
}
void OperationDomains()
{
    // Every published domain must participate in both activity checks and the
    // preparation pause. In particular equipment is not an inventory drag.
    for (auto operation : gate::Operations)
    {
        gate::Actor first(1), second(2);
        std::array one{&first};
        std::array pair{&first, &second};
        auto active = gate::Enter(one, operation);
        Check(active && !first.Bind({Key(), 1}), "domain activity allowed first native bind");
        active.reset();
        Check(first.Bind({Key(), 1}) && second.Bind({Key(101), 2}), "domain binding failed");
        active = gate::Enter(one, operation);
        Check(!gate::TryPause(pair, gate::Supported, Token(4)), "active domain escaped full pause");
        std::array right{&second};
        Check(gate::Enter(right, operation).has_value(), "busy full pause partly froze another actor");
        active.reset();
        for (int mask = 1; mask <= gate::Supported; ++mask)
        {
            auto pause = gate::TryPause(pair, uint8_t(mask), Token(4));
            const bool covered = (mask & uint8_t(operation)) != 0;
            Check(pause && gate::Enter(pair, operation).has_value() != covered,
                  "operation pause mask crossed or missed a domain");
        }
        active = gate::Enter(one, operation);
        first.Invalidate();
        Check(!active->Current() && !first.Bind({Key(100, 2), 1}) && !first.RecoverLegacy(1),
              "cancelled domain rebound before its callback finished");
        active.reset();
        Check(first.RecoverLegacy(1) && !gate::TryPause(one, gate::Supported, Token(4)),
              "domain recovery restored native authority");
    }
}
void InvalidAndCapacity()
{
    gate::Actor actor(1);
    for (int field = 0; field < 7; ++field)
    {
        auto scope = gate::Scope{Key(), 1};
        if (field == 0)
            scope.session.link = {};
        if (field == 1)
            scope.session.request = {};
        if (field == 2)
            scope.session.session = {};
        if (field == 3)
            scope.session.generation = 0;
        if (field == 4)
            scope.session.account = 0;
        if (field == 5)
            scope.session.character = 0;
        if (field == 6)
            scope.entity = 2;
        Check(!actor.Bind(scope), "invalid native scope accepted");
    }
    Check(actor.Bind({Key(), 1}), "valid actor scope rejected");
    std::array one{&actor};
    Check(!gate::Enter({}, gate::Operation::Exchange) && !gate::TryPause({}, gate::Supported, Token(4)),
          "ownerless operation/pause accepted");
    for (int mask = 0; mask < 256; ++mask)
    {
        const bool allowed = mask > 0 && mask <= gate::Supported;
        Check(gate::TryPause(one, uint8_t(mask), Token(4)).has_value() == allowed,
              "invalid domain mask accepted");
        Check(gate::Enter(one, static_cast<gate::Operation>(mask)).has_value() ==
                  (mask == 1 || mask == 2 || mask == 4),
              "invalid operation accepted");
    }
    Check(!gate::TryPause(one, gate::Supported, {}), "zero preparation challenge accepted");
    std::array<gate::Actor *, 1> missing{nullptr};
    Check(!gate::Enter(missing, gate::Operation::Exchange), "missing actor accepted");
    std::vector<gate::Actor> owners;
    owners.reserve(9);
    std::vector<gate::Actor *> refs;
    for (uint32_t i = 0; i < 9; ++i)
    {
        owners.emplace_back(i);
        owners.back().Bind({Key(100 + i), i});
        refs.push_back(&owners.back());
    }
    Check(!gate::Enter(refs, gate::Operation::Exchange) && !gate::TryPause(refs, gate::Supported, Token(4)),
          "oversized participant set accepted");
    refs.pop_back();
    const bool entered = gate::Enter(refs, gate::Operation::Exchange).has_value();
    Check(entered && gate::TryPause(refs, gate::Supported, Token(4)).has_value(),
          "participant limit refused");
}
void NativePreparation()
{
    using Clock = std::chrono::steady_clock;
    const auto now = Clock::time_point(std::chrono::seconds(100));
    const auto spare = g_registry.create();
    Check(native::Enter({spare}, gate::Operation::Exchange).has_value(), "unbound activity fixture failed");
    const auto entity = g_registry.create();
    g_registry.emplace<ecs::PlayerID>(entity, 100u);
    auto activity = native::Enter({entity}, gate::Operation::InventoryMove);
    Check(activity && !native::Prepare(entity, Key(), Token(4)),
          "nested initial native bind escaped activity");
    activity.reset();
    auto pause = native::Prepare(entity, Key(), Token(4));
    Check(pause && pause->Current(), "native actor could not prepare");
    pause.reset();
    {
        gate::Actor impostor(entt::to_integral(entity));
        Check(impostor.Bind({Key(), entt::to_integral(entity)}), "foreign instance fixture failed");
        std::array owners{&impostor};
        auto foreign = gate::TryPause(owners, gate::Supported, Token(4));
        Check(foreign && !native::StorePreparation(entity, Key(), std::move(*foreign), now) &&
                  foreign->Current() && native::Enter({entity}, gate::Operation::Exchange).has_value(),
              "another actor instance's pause impersonated this ECS actor");
    }
    pause = native::Prepare(entity, Key(), Token(4));
    Check(pause && native::StorePreparation(entity, Key(), std::move(*pause), now + std::chrono::seconds(10)),
          "native preparation not owned by ECS component");
    Check(native::PreparationCurrent(entity, Key(), now) &&
              !native::Enter({entity}, gate::Operation::InventoryMove) &&
              !native::Enter({entity}, gate::Operation::Exchange),
          "native preparation did not hold both domains");
    g_registry.destroy(spare); // Dense storage relocates the still-paused component.
    Check(native::PreparationCurrent(entity, Key(), now), "ECS relocation lost native pause ownership");
    g_registry.remove<ecs::PlayerID>(entity);
    Check(!native::PreparationCurrent(entity, Key(), now),
          "missing live PID retained a positive preparation");
    g_registry.emplace<ecs::PlayerID>(entity, 100u);
    native::ReleasePreparation(entity, Key(100, 2));
    Check(native::PreparationCurrent(entity, Key(), now),
          "stale native release unlocked current preparation");
    Check(!native::PreparationCurrent(entity, Key(), now + std::chrono::seconds(10)) &&
              !native::Enter({entity}, gate::Operation::Exchange),
          "expiry silently resumed before invalidation");
    native::ReleasePreparation(entity, Key());
    Check(native::Enter({entity}, gate::Operation::InventoryMove).has_value(),
          "abort did not resume native operation");
    activity = native::Enter({entity}, gate::Operation::Exchange);
    native::Invalidate(entity);
    Check(!activity->Current() && !native::Prepare(entity, Key(100, 2), Token(4)),
          "disconnect allowed rebind before active callback completed");
    Check(!native::Enter({entity}, gate::Operation::InventoryMove),
          "disconnect recovered activity before the cancelled callback completed");
    activity.reset();
    Check(native::Enter({entity}, gate::Operation::InventoryMove).has_value() &&
              !native::PreparationCurrent(entity, Key(), now),
          "disconnect recovery blocked legacy gameplay or restored old preparation");
    pause = native::Prepare(entity, Key(100, 2), Token(4));
    Check(pause && pause->Current(), "new native session could not prepare");
    g_registry.destroy(entity);
    Check(!pause->Current(), "retired ECS generation retained pause authority");
    const auto replacement = g_registry.create();
    g_registry.emplace<ecs::PlayerID>(replacement, 100u);
    Check(replacement != entity && !native::Prepare(entity, Key(), Token(4)) &&
              native::Prepare(replacement, Key(100, 3), Token(4)).has_value(),
          "recycled ECS entity inherited old scope");
    g_registry.clear();
}
void NativeConstruction()
{
    const auto first = g_registry.create(), second = g_registry.create();
    g_registry.emplace<ecs::PlayerID>(first, 100u);
    g_registry.emplace<ecs::PlayerID>(second, 101u);
    struct Callback
    {
        entt::entity victim, trigger;
        void Construct(entt::registry &registry, entt::entity entity)
        {
            if (entity == trigger)
                registry.destroy(victim);
        }
    } callback{first, second};
    entt::scoped_connection connection =
        g_registry.on_construct<ecs::NativeResourceAdmission>().connect<&Callback::Construct>(callback);
    Check(!native::Enter({first, second}, gate::Operation::Exchange) && !g_registry.valid(first),
          "component construction retained a retired participant");
    Check(native::Prepare(second, Key(101), Token(4)).has_value(),
          "failed participant construction left the surviving actor busy");
    connection.release();
    g_registry.clear();
    for (bool prepare : {false, true})
    {
        for (bool removeOnly : {false, true})
        {
            const auto current = g_registry.create();
            g_registry.emplace<ecs::PlayerID>(current, 100u);
            struct SelfRemoval
            {
                bool removeOnly;
                void Construct(entt::registry &registry, entt::entity entity)
                {
                    if (removeOnly)
                        registry.remove<ecs::NativeResourceAdmission>(entity);
                    else
                        registry.destroy(entity);
                }
            } self{removeOnly};
            entt::scoped_connection selfConnection =
                g_registry.on_construct<ecs::NativeResourceAdmission>().connect<&SelfRemoval::Construct>(
                    self);
            const bool entered = prepare ? native::Prepare(current, Key(), Token(4)).has_value()
                                         : native::Enter({current}, gate::Operation::Equipment).has_value();
            Check(!entered, "self-removing admission construction succeeded");
            selfConnection.release();
            if (removeOnly)
                Check(native::Prepare(current, Key(), Token(4)).has_value(),
                      "removed admission could not be retried");
            g_registry.clear();
        }
    }
}
} // namespace
int main()
{
    try
    {
        Activity();
        Lifetimes();
        OperationDomains();
        InvalidAndCapacity();
        NativePreparation();
        NativeConstruction();
        std::cout << "Resource admission checks passed: " << checks << '\n';
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
