#pragma once
#include <entt/entity/fwd.hpp>
bool HandleTypedCostumeTransferPacket(entt::entity,const char*);
// True for native owners, including a refused or empty selection.
bool OpenTypedCostumeTransfer(entt::entity);
bool CloseTypedCostumeTransfer(entt::entity);
