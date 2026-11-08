#include <item/StorageRuntime.hpp>
#include <item/TypedItemDetails.hpp>
#include <algorithm>
namespace auriga::item {
std::expected<std::vector<std::byte>,TypedInventoryWireError> EncodeStorageSnapshot(const ItemCatalog& c,const StorageSnapshot& s){
    using E=TypedInventoryWireError;if(s.status>7 || unsigned(s.route)<1 || unsigned(s.route)>6 || s.command>6 || s.message.size()>512 || s.items.size()>65535)return std::unexpected(E::BadValue);
    std::vector<std::byte> b;const auto put=[&](std::uint64_t v,unsigned n){for(unsigned j=0;j<n;j++)b.push_back(std::byte(v>>(8*j)));};
    put(201,1);put(0,4);put(1,1);put(s.status,1);put(unsigned(s.route),1);put(s.command,1);put(s.nonce,8);put(s.subject,8);put(s.units,4);put(s.cell,4);put(s.recipient,4);put(s.amount,8);put(s.coins,8);put(s.walletRevision,8);put(unsigned(s.owner.kind),1);put(s.owner.id,8);put(s.canDeposit,1);put(s.canWithdraw,1);
    std::vector<ItemGrant> denominations;for(const auto& [key,e]:c.Entries())if(const auto* r=std::get_if<ItemCurrencyTokenRow>(&e.category);r && r->currency==WalletCurrency::DragonCoin && !e.base.retired)denominations.push_back(ItemGrant{key,0,1});if(denominations.size()>65535)return std::unexpected(E::TooLarge);
    std::ranges::sort(denominations,{},[](const auto& g){return g.key;});put(denominations.size(),2);for(const auto& g:denominations){put(g.key.LocalId(),4);put(std::get<ItemCurrencyTokenRow>(c.Find(g.key)->category).amountPerUnit,8);}
    const ItemOwner actor{ItemOwnerKind::Character,s.actor.pid};ItemStorageLayout layout{{actor,ItemStorageKind::Inventory,1},5,9,3};if(!s.layouts.empty()){layout.columns=s.layouts[0].columns;layout.rows=s.layouts[0].rows;layout.pages=s.layouts[0].pages;}
    std::vector<PersistentItem> projected;put(s.items.size(),2);for(const auto& i:s.items){if(!i.placement || i.destroyed || i.owner!=s.owner)return std::unexpected(E::BadValue);put(i.id.Value(),8);put(unsigned(i.placement->kind),1);put(i.placement->storageId,8);put(i.placement->anchor,4);auto copy=i;copy.owner=actor;copy.placement->kind=ItemStorageKind::Inventory;copy.placement->storageId=1;projected.push_back(copy);}
    const auto inventory=EncodeTypedInventory(c,std::span(&layout,1),{},projected);const auto details=EncodeTypedItemDetails(c,projected,false,denominations);if(!inventory || !details)return std::unexpected(E::BadValue);
    put(inventory->size(),4);b.insert(b.end(),inventory->begin(),inventory->end());put(details->size(),4);b.insert(b.end(),details->begin(),details->end());put(s.message.size(),2);for(unsigned char ch:s.message)put(ch,1);
    if(b.size()>16*1024*1024)return std::unexpected(E::TooLarge);for(unsigned j=0;j<4;j++)b[1+j]=std::byte(b.size()>>(8*j));return b;
}
}
