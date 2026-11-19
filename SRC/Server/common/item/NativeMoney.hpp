#pragma once
#include "ItemKey.hpp"
#include "CurrencyTokens.hpp"
#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
namespace auriga::item {
class ItemCatalog;
struct ItemCommit;
struct ItemOwnerFence;
struct ItemWallet;
struct PersistentItem;
struct ItemChange;
enum class CurrencyTokenMode : std::uint8_t { None=0,Fixed=1,DynamicGold=2,GroundGold=3 };
struct CurrencyTokenPolicy {
    ItemKey key;
    CurrencyTokenMode mode{};
};
// Trusted account award, unrelated to an item socket or client-supplied money.
// Claiming the row and creating its concrete native pouch share one transaction.
struct CurrencyAwardClaim {
    std::uint64_t id=0,amount=0;
    std::string login;
    bool operator==(const CurrencyAwardClaim&) const=default;
};
struct CurrencyDropIntent {
    std::array<std::uint8_t,16> operation{};
    std::string core;
    std::uint64_t run=0,owner=0,amount=0,createdAtMs=0,expiresAtMs=0,nowAtMs=0;
    std::int32_t map=0,x=0,y=0,z=0;
    bool create=false;
    bool operator==(const CurrencyDropIntent&) const=default;
};
inline constexpr std::uint16_t GroundCurrencyCreditOutcome=0x2D01;
inline constexpr std::uint16_t GroundCurrencyDropOutcome=0x2D02;
inline constexpr std::uint16_t QuestGoldChangeOutcome=0x3801;
inline constexpr std::uint16_t QuestGayaChangeOutcome=0x3802;
bool ValidQuestGoldChange(const ItemCatalog&,const ItemCommit&);
bool ValidQuestGayaChange(const ItemCatalog&,const ItemCommit&);
enum class MoneyError { Invalid,Unavailable,WrongKind,WalletFull,Denied };
std::expected<ItemCommit,MoneyError> MakeQuestGoldChange(const ItemCatalog&,std::array<std::uint8_t,16>,ItemOwnerFence,const ItemWallet&,std::int64_t delta,std::uint64_t cap);
std::expected<ItemCommit,MoneyError> MakeQuestGayaChange(const ItemCatalog&,std::array<std::uint8_t,16>,ItemOwnerFence,const ItemWallet&,std::int64_t delta,std::uint64_t cap);
// A ground/instant Yang reward credits the authoritative wallet directly.
// No inventory unit or transient fake token is created.
std::expected<ItemCommit,MoneyError> MakeGroundCurrencyCredit(const ItemCatalog&,
    std::array<std::uint8_t,16> operation,ItemOwnerFence,const ItemWallet&,
    ItemKey groundCurrency,std::uint64_t amount,std::uint64_t cap);
bool ValidGroundCurrencyCredit(const ItemCatalog&,const ItemCommit&);
bool ValidCurrencyDropIntent(const CurrencyDropIntent&);
bool ValidCurrencyDropWrite(const ItemCatalog&,const ItemCommit&);
std::expected<ItemCommit,MoneyError> MakeCurrencyDropCommit(const ItemCatalog&,
    std::array<std::uint8_t,16> operation,ItemOwnerFence,const ItemWallet&,CurrencyDropIntent);
bool MoneyPassiveAccessoryDebit(const ItemChange&);
std::optional<std::string> NormalizeCurrencyAwardLogin(std::string_view);
bool ValidCurrencyAwardClaim(const ItemCatalog&,const ItemCommit&);
std::optional<std::uint64_t> CurrencyUnitAmount(const ItemCatalog&,const PersistentItem&);
}
