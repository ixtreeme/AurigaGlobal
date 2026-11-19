#pragma once
#include <entt/entity/entity.hpp>
#include <string>
bool InitializeSocialHistory(const std::string& keyFile,std::string& error);
bool SocialHistoryEnabled();
bool SavePrivateHistory(entt::entity sender,const std::string& recipient,const std::string& body);
bool SaveGuildHistory(entt::entity sender,const std::string& formatted);
void SendSocialHistoryIndex(entt::entity viewer);
bool HandleSocialHistory(entt::entity viewer,const char* arguments);
