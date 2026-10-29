#pragma once
#include <cstdint>
namespace EclipseRaid {
// Private-instance effects must never survive logout in the account database.
// Names/UI are supplied by er_effect rather than the legacy skill affect table.
constexpr uint32_t GooAttackSlow=62040,GooCastSlow=62041,OozeMoveSlow=62042;
inline bool TransientAffect(uint32_t type){return type>=62040&&type<=62079;}
}
