#pragma once

// Nexoria manual PvP benchmark: live PC-versus-PC telemetry and temporary
// level-170 equipment profiles. No damage simulation or database edits.
// Development test characters only: do not disconnect before /benchmark stop.

#include <entt/entt.hpp>
#include <cstdint>
#include <string>
#include <string_view>

namespace nexoria::benchmark {

class ScopedSkillContext {
public:
    explicit ScopedSkillContext(uint32_t skillVnum) noexcept;
    ~ScopedSkillContext() noexcept;
    ScopedSkillContext(const ScopedSkillContext&) = delete;
    ScopedSkillContext& operator=(const ScopedSkillContext&) = delete;
private:
    uint32_t previous_;
};

bool Start(entt::entity gm, int tier, std::string_view weaponType, std::string& message);
bool AddTarget(entt::entity gm, entt::entity target, std::string& message);
bool Reset(entt::entity gm, std::string& message);
bool Stop(entt::entity gm, std::string& message);
std::string Report(entt::entity gm);

// Called only AFTER the real GameServer changes the victim HP.
void RecordDamage(entt::entity attacker, entt::entity victim,
                  int64_t hpBefore, int64_t hpAfter, uint8_t damageType,
                  bool critical, bool penetrate) noexcept;
void RecordMiss(entt::entity attacker, entt::entity victim,
                uint8_t damageType, std::string_view reason) noexcept;

} // namespace nexoria::benchmark
