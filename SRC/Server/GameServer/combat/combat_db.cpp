#include "stdafx.h"
#include <Core/Logging.hpp>
#include "input.h"
#include "../ecs/Registry.hpp"
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "char.h"
#include "packet.h"
#include "protocol.h"
#include "utils.h"
#include "../ecs/systems/SkillSystem.hpp"
#include "desc.h"
#include "../ecs/components/skill_components.hpp"

#ifdef __SKILL_COLOR_SYSTEM__
void CInputDB::SkillColorLoad(LPDESC desc, const char* data)
{
    if (!desc || !data)
        return;
    const auto player = desc->GetEntity();
    if (!ecs::PlayerRuntime::IsPC(player) || ecs::PlayerRuntime::GetDesc(player) != desc)
        return;
    ecs::SkillColor colors {};
    std::memcpy(colors.data, data, sizeof(colors.data));
    SkillSystem::SetSkillColors(player, colors);
}
#endif
