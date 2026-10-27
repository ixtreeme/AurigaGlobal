-- Interpretation fixture only, not production content. Loaded after v6;
-- mirrors the currency tokens of tests/server/ItemCatalogFixture.hpp.
INSERT INTO protos.item_base VALUES
 (16,1,'currency_token.gold_bar','item.gold_bar.name','item.gold_bar.description','icons/gold_bar.png',
  1,1,200,100,0,1,2,1,1,NULL,NULL,1,0,0,0),
 (16,2,'currency_token.coin_voucher','item.coin_voucher.name','item.coin_voucher.description','icons/coin_voucher.png',
  1,1,200,100,0,1,2,1,1,NULL,NULL,1,0,0,0);
INSERT INTO protos.item_currency_token VALUES(16,1,1,5000000),(16,2,2,100);
