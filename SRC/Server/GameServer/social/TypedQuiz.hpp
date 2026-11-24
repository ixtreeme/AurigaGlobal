#pragma once

#include <entt/entity/fwd.hpp>
#include <cstdint>
#include <string>
#include <string_view>

void CheckTypedQuizAnswer(entt::entity character, std::string_view message);

// NPC rounds run for sixty seconds. Preset 0 carries a custom question, 100 and 1000 select localized prompts.
void StartQuestQuiz(entt::entity gm, std::int64_t answer, std::string_view rewardCode,
    std::uint32_t count, unsigned preset, std::string question);
void CloseQuestQuiz(entt::entity gm);
void PollExpiredQuiz();
