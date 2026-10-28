#include "ItemCatalogFixture.hpp"
#include <iostream>
#include <source_location>
#include <stdexcept>

using namespace auriga::item;
namespace f = item_catalog_fixture;
namespace
{
std::size_t checks = 0;
void Check(bool condition, const char* message, const std::source_location where = std::source_location::current())
{
    ++checks;
    if (!condition) throw std::runtime_error(std::string(message) + " at line " + std::to_string(where.line()));
}
template <typename Mutate> void RejectCatalog(Mutate mutate, ItemCatalogErrorCode expected)
{
    auto source = f::Catalog(); mutate(source);
    const auto result = BuildItemCatalog(std::move(source));
    Check(!result && result.error().code == expected, "invalid token catalogue rejected with its reason");
}
constexpr std::uint64_t GoldMax = 999'000'000'000ULL; // today's GOLD_MAX, supplied by the runtime
void Redeeming()
{
    const auto catalog = BuildItemCatalog(f::Catalog()).value();
    const auto full = PlanTokenRedeem(*catalog, {f::GoldBar(), 10, 0, GoldMax}).value();
    Check(full.currency == WalletCurrency::Gold && full.unitsToConsume == 10 && full.amountToCredit == 50'000'000,
          "a whole stack redeems when the wallet has room");
    const auto partial = PlanTokenRedeem(*catalog, {f::GoldBar(), 10, GoldMax - 12'000'000, GoldMax}).value();
    Check(partial.unitsToConsume == 2 && partial.amountToCredit == 10'000'000,
          "only the units that fit are consumed; their value is never lost");
    Check(PlanTokenRedeem(*catalog, {f::GoldBar(), 10, GoldMax - 4'999'999, GoldMax}).error() == TokenRedeemError::WalletFull,
          "no unit is consumed when not even one fits");
    Check(PlanTokenRedeem(*catalog, {f::GoldBar(), 1, GoldMax, GoldMax}).error() == TokenRedeemError::WalletFull, "full wallet");
    Check(PlanTokenRedeem(*catalog, {f::GoldBar(), 1, GoldMax + 1, GoldMax}).error() == TokenRedeemError::InvalidWallet,
          "a balance above the cap is an invalid state, not a full wallet");
    Check(PlanTokenRedeem(*catalog, {f::GoldBar(), 0, 0, GoldMax}).error() == TokenRedeemError::EmptyStack, "empty stack");
    const auto limit = catalog->Find(f::GoldBar())->base.maxStack;
    Check(PlanTokenRedeem(*catalog, {f::GoldBar(), limit + 1, 0, UINT64_MAX}).error() == TokenRedeemError::InvalidStack,
          "overstacked token state cannot generate a redemption plan");
    // The old voucher branch consumed the whole stack even when the coin cap
    // clamped the credit. The plan applies the same whole-unit rule as gold.
    const auto coins = PlanTokenRedeem(*catalog, {f::CoinVoucher(), 200, 4'294'967'295ULL - 250, 4'294'967'295ULL}).value();
    Check(coins.currency == WalletCurrency::DragonCoin && coins.unitsToConsume == 2 && coins.amountToCredit == 200,
          "coin vouchers keep the units whose value does not fit");
    Check(PlanTokenRedeem(*catalog, {f::Sword(), 1, 0, GoldMax}).error() == TokenRedeemError::NotToken, "not a token");
    Check(PlanTokenRedeem(*catalog, {ItemKey::Create(ItemCategory::CurrencyToken, 99).value(), 1, 0, GoldMax}).error() ==
          TokenRedeemError::MissingItem, "unknown token");
    auto retired = f::Catalog();
    for (auto& row : retired.items) if (row.key == f::GoldBar()) row.retired = true;
    Check(PlanTokenRedeem(*BuildItemCatalog(std::move(retired)).value(), {f::GoldBar(), 1, 0, GoldMax}).error() ==
          TokenRedeemError::RetiredItem, "retired tokens are not redeemed");
    auto huge = f::Catalog();
    huge.currencyTokens[0].amountPerUnit = MaxTokenUnitAmount;
    for (auto& row : huge.items) if (row.key == f::GoldBar()) row.maxStack = UINT32_MAX;
    const auto large = PlanTokenRedeem(*BuildItemCatalog(std::move(huge)).value(), {f::GoldBar(), UINT32_MAX, 0, UINT64_MAX}).value();
    Check(large.unitsToConsume == 2 && large.amountToCredit == 2 * MaxTokenUnitAmount, "large values do not overflow");
}
void CatalogRules()
{
    RejectCatalog([](auto& s) { s.currencyTokens[0].amountPerUnit = 0; }, ItemCatalogErrorCode::InvalidProfile);
    RejectCatalog([](auto& s) { s.currencyTokens[0].amountPerUnit = MaxTokenUnitAmount + 1; }, ItemCatalogErrorCode::InvalidProfile);
    RejectCatalog([](auto& s) { s.currencyTokens[0].currency = static_cast<WalletCurrency>(3); }, ItemCatalogErrorCode::InvalidProfile);
    RejectCatalog([](auto& s) { s.currencyTokens[0].currency = static_cast<WalletCurrency>(0); }, ItemCatalogErrorCode::InvalidProfile);
    RejectCatalog([](auto& s) { s.currencyTokens.pop_back(); }, ItemCatalogErrorCode::MissingCategoryRow);
    RejectCatalog([](auto& s) { s.currencyTokens.push_back(s.currencyTokens[0]); }, ItemCatalogErrorCode::DuplicateCategoryRow);
    RejectCatalog([](auto& s) { s.currencyTokens[0].key = f::Sword(); }, ItemCatalogErrorCode::WrongCategory);
    RejectCatalog([](auto& s) { s.currencyTokens.push_back({ItemKey::Create(ItemCategory::CurrencyToken, 9).value(), WalletCurrency::Gold, 1}); },
                  ItemCatalogErrorCode::OrphanCategoryRow);
    RejectCatalog([](auto& s) { for (auto& row : s.items) if (row.key == f::GoldBar()) row.binding = BindingPolicy::OnEquip; },
                  ItemCatalogErrorCode::InvalidBase);
    RejectCatalog([](auto& s) { for (auto& row : s.items) if (row.key == f::CoinVoucher()) row.baseBonusProfile = 1; },
                  ItemCatalogErrorCode::InvalidBase);
    const auto catalog = BuildItemCatalog(f::Catalog()).value();
    Check(catalog->Size() == f::FixtureItems, "tokens are published prototypes");
    Check(std::get<ItemCurrencyTokenRow>(catalog->Find(f::CoinVoucher())->category).currency == WalletCurrency::DragonCoin,
          "token rows are typed category rows");
    const auto original = catalog->ContentHash();
    auto changed = f::Catalog(); ++changed.currencyTokens[0].amountPerUnit;
    Check(BuildItemCatalog(std::move(changed)).value()->ContentHash() != original, "token value participates in catalogue identity");
    auto swapped = f::Catalog(); swapped.currencyTokens[1].currency = WalletCurrency::Gold;
    Check(BuildItemCatalog(std::move(swapped)).value()->ContentHash() != original, "token currency participates in catalogue identity");
}
}
int main()
{
    try
    {
        Redeeming(); CatalogRules();
        std::cout << "Currency token checks passed: " << checks << '\n'; return 0;
    }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
