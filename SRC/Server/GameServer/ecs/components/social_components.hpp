#pragma once

#include <cstdint>
#include <string>

#include "../../guild.h"
#include "../../party.h"
#include "../../dungeon.h"
#include "../../war_map.h"
#include "../../shop.h"
#include "../../typedef.h"

namespace marriage {
class WeddingMap;
}

namespace ecs {

struct SocialRefs {
    LPPARTY party { nullptr };
    CGuild* guild { nullptr };
};

struct PartyMembership {
    LPPARTY party { nullptr };
    uint32_t lastDeadTime;
};

struct GuildMembership {
    CGuild* guild { nullptr };
    uint32_t underWarInfoMessageTime;
};

struct DungeonMembership {
    LPDUNGEON dungeon { nullptr };
    int eventAttr;
    CWarMap* warMap { nullptr };
};

struct MarriageState {
    LPCHARACTER partner { nullptr };
    marriage::WeddingMap* weddingMap { nullptr };
};

struct ShopState {
    LPSHOP currentShop { nullptr };
    LPSHOP myShop { nullptr };
    std::string shopSign;
    bool noOpenedShop;
    bool underRefine;
    int refineCell;
    uint32_t refineNPCVID;
};

struct MountState {
    uint32_t mountVnum;
    uint32_t mountTime;
    uint8_t sendHorseLevel;
    uint8_t sendHorseHealthGrade;
    uint8_t sendHorseStaminaGrade;
    int mountPulse;
};

} // namespace ecs
