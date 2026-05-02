#pragma once

#include <cstdint>
#include <memory>
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

class CExchange;
class CWheelDestiny;

#ifdef __ENABLE_NEW_OFFLINESHOP__
namespace offlineshop {
class CShop;
class CAuction;
}
#endif

namespace ecs {

struct SocialRefs {
    LPPARTY party { nullptr };
    CGuild* guild { nullptr };
};

struct ExchangeRef {
    CExchange* exchange { nullptr };
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
    LPCHARACTER shopOwner { nullptr };
    LPSHOP myShop { nullptr };
#ifdef __ENABLE_NEW_OFFLINESHOP__
    offlineshop::CShop* offlineShopGuest { nullptr };
    offlineshop::CAuction* auctionGuest { nullptr };
    int offlineShopUseTime { 0 };
#endif
#if defined(ENABLE_CHRISTMAS_WHEEL_OF_DESTINY)
    std::shared_ptr<CWheelDestiny> wheelDestiny {};
#endif
    std::string shopSign;
    bool noOpenedShop;
    bool underRefine;
    int refineCell;
    uint32_t refineNPCVID;
};

struct WarpBlockState {
    int safeboxLoadTime { 0 };
    int exchangeTime { 0 };
    int myShopTime { 0 };
    int refineTime { 0 };
};

struct MountState {
    uint32_t mountVnum { 0 };
    uint32_t mountTime { 0 };
    uint8_t sendHorseLevel { 0 };
    uint8_t sendHorseHealthGrade { 0 };
    uint8_t sendHorseStaminaGrade { 0 };
    int mountPulse { 0 };
    bool horseRiding { false };
};

} // namespace ecs
