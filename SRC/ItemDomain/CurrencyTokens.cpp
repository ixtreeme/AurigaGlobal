#include <item/CurrencyTokens.hpp>
#include <item/ItemCatalog.hpp>
#include <algorithm>

namespace auriga::item
{
std::expected<TokenRedeemPlan, TokenRedeemError> PlanTokenRedeem(const ItemCatalog& catalog, const TokenRedeemRequest& request)
{
    const auto* entry = catalog.Find(request.token);
    if (!entry) return std::unexpected(TokenRedeemError::MissingItem);
    const auto* token = std::get_if<ItemCurrencyTokenRow>(&entry->category);
    if (!token) return std::unexpected(TokenRedeemError::NotToken);
    if (entry->base.retired) return std::unexpected(TokenRedeemError::RetiredItem);
    if (!request.unitsInStack) return std::unexpected(TokenRedeemError::EmptyStack);
    if (request.unitsInStack > entry->base.maxStack) return std::unexpected(TokenRedeemError::InvalidStack);
    if (request.walletBalance > request.walletCap) return std::unexpected(TokenRedeemError::InvalidWallet);
    const auto units = std::min<std::uint64_t>(request.unitsInStack, (request.walletCap - request.walletBalance) / token->amountPerUnit);
    if (!units) return std::unexpected(TokenRedeemError::WalletFull);
    // units * amount <= free space, so the product cannot overflow.
    return TokenRedeemPlan{request.token, token->currency, static_cast<std::uint32_t>(units), units * token->amountPerUnit};
}
} // namespace auriga::item
