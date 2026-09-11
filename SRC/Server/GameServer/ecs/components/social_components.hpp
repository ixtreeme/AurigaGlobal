#pragma once
#include "../../exchange.h"

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

class CWheelDestiny;
class CArena;

#ifdef __ENABLE_NEW_OFFLINESHOP__
namespace offlineshop {
class CShop;
class CAuction;
class CShopSafebox;
}
#endif

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

struct GuildDepositState {
    int nextAllowedPulse { 0 };
};

struct DungeonMembership {
    LPDUNGEON dungeon { nullptr };
    int eventAttr;
    CWarMap* warMap { nullptr };
};

// Guild ids whose name this viewer has already been sent. The client only
// needs each name once; this is the dedup set that used to be
// CHARACTER::m_known_guild.
struct KnownGuilds {
    std::set<uint32_t> ids;
};

struct ArenaMembership {
    CArena* arena { nullptr };
    int potionLimit { 0 };
};

struct MarriageState {
    // The partner, while both are online. It says nothing about whether the
    // two are engaged or married: marriage::CManager is keyed on the player id
    // and is the only thing that knows that.
    entt::entity partner { entt::null };
    marriage::WeddingMap* weddingMap { nullptr };
};

struct ShopState {
    LPSHOP currentShop { nullptr };
    entt::entity shopOwner { entt::null };
    LPSHOP myShop { nullptr };
#ifdef __ENABLE_NEW_OFFLINESHOP__
    // The offline shop this character owns, and the one being browsed.
    offlineshop::CShop* offlineShop { nullptr };
    offlineshop::CShop* offlineShopGuest { nullptr };
    offlineshop::CShopSafebox* shopSafebox { nullptr };
    offlineshop::CAuction* auction { nullptr };
    offlineshop::CAuction* auctionGuest { nullptr };
    int offlineShopUseTime { 0 };
    bool lookingOfferList { false };
#endif
#if defined(ENABLE_CHRISTMAS_WHEEL_OF_DESTINY)
    std::shared_ptr<CWheelDestiny> wheelDestiny {};
#endif
    std::string shopSign;
    // Whether the saved price list still has to be fetched before the
    // personal shop can open. It had no initialiser while CHARACTER still
    // held the authoritative copy; it is the only copy now.
    bool noOpenedShop { false };
    bool underRefine { false };
    int refineCell { -1 };
    entt::entity refineNPC { entt::null };
#ifdef KASMIR_PAKET_SYSTEM
    // LPENTITY.4-fixup.2.g: mirror of legacy m_bKasmirPaketBaslik so native
    // EntityNetworkDispatch shop sign packet matches legacy bytes.
    uint8_t kasmirTitle { 0 };
    // Whether this open is a kasmir package.
    bool kasmirPaket { false };
#endif
};

// How recently this character bought or sold. Both were CHARACTER members
// that only the shop code read, so nothing entity-native could rate-limit
// a purchase. myShopTime already lives in WarpBlockState below.
struct ShopTimers {
    int lastBuyPulse { 0 };
    uint32_t lastBuySellTime { 0 };
};

struct WarpBlockState {
    int safeboxLoadTime { 0 };
    int exchangeTime { 0 };
    int myShopTime { 0 };
    int refineTime { 0 };
};

// The horse a rider currently has summoned, as an entity. Null when none is
// out. Spawned and destroyed by CHARACTER::HorseSummon, which owns its life.
struct SummonedHorse {
    entt::entity horse { entt::null };
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
