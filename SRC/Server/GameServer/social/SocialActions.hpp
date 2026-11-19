#pragma once
#include <entt/entity/entity.hpp>
#include <string>
void HandleSocialCommand(entt::entity character,const char* arguments);
void SendSocialSnapshot(entt::entity character);
bool SocialBlocked(const std::string& first,const std::string& second);
bool SaveSocialFriendRequest(const std::string& requester,const std::string& recipient);
bool ResolveSocialFriendRequest(const std::string& recipient,const std::string& requester,bool deny);
void NotifySocialChanged(const std::string& name);
void SendSocialCommand(const std::string& name,const std::string& command);
