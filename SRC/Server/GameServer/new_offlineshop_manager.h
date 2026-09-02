#ifndef __INCLUDE_HEADER_OFFLINESHOP_MANAGER__
#define __INCLUDE_HEADER_OFFLINESHOP_MANAGER__

#ifdef __ENABLE_NEW_OFFLINESHOP__
#define SUBTYPE_NOSET 255
#define OFFLINESHOP_DURATION_UPDATE_TIME PASSES_PER_SEC(60)
#define OFFLINESHOP_AUCTION_RAISE_PERCENTAGE 10 //to raise the offer will be 10% more than best offer

namespace offlineshop
{
	

	class CShopManager : public singleton<CShopManager>
	{
	public:

#ifdef ENABLE_NEW_SHOP_IN_CITIES
		typedef std::map<uint32_t, ShopEntity*> SHOPENTITIES_MAP;

		typedef struct SCityShopInfo {
			SHOPENTITIES_MAP	entitiesByPID;
			SHOPENTITIES_MAP	entitiesByVID;


			void Clear()
			{
				entitiesByPID.clear();
				entitiesByVID.clear();
			}


			SCityShopInfo()
			{
				Clear();
			}


			SCityShopInfo(const SCityShopInfo& rCopy)
			{
				CopyContainer(entitiesByPID, rCopy.entitiesByPID);
				CopyContainer(entitiesByVID, rCopy.entitiesByVID);
			}

		} TCityShopInfo;
#endif

		typedef std::map<uint32_t,CShop>					 SHOPMAP;
		typedef std::map<uint32_t,CShopSafebox>			 SAFEBOXMAP;
		typedef std::map<uint32_t,std::vector<TOfferInfo> > OFFERSMAP;
		typedef std::map<uint32_t, uint32_t>					 SEARCHTIMEMAP;
		typedef std::map<uint32_t, CAuction>				 AUCTIONMAP;

#ifdef ENABLE_NEW_SHOP_IN_CITIES
		typedef std::vector<TCityShopInfo>				 CITIESVEC;
#endif
		


		CShopManager();
		~CShopManager();
		



		//booting
		offlineshop::CShop*		PutsNewShop(TShopInfo * pInfo);
		void					PutsAuction(const TAuctionInfo& auction);
		void					PutsAuctionOffer(const TAuctionOfferInfo& offer);

		offlineshop::CShop*		GetShopByOwnerID(uint32_t dwPID);
		CShopSafebox*			GetShopSafeboxByOwnerID(uint32_t dwPID);
		CAuction*				GetAuctionByOwnerID(uint32_t dwPID);
		//offers
		bool					PutsNewOffer(const TOfferInfo* pInfo);

		void					RemoveSafeboxFromCache(uint32_t dwOwnerID);
		void					RemoveGuestFromShops(entt::entity character);



#ifdef ENABLE_NEW_SHOP_IN_CITIES
	public:
		void		CreateNewShopEntities(offlineshop::CShop& rShop);
		void		DestroyNewShopEntities(const offlineshop::CShop& rShop);

		void		EncodeInsertShopEntity(ShopEntity& shop, entt::entity character);
		void		EncodeRemoveShopEntity(ShopEntity& shop, entt::entity character);

	private:
		bool		__CanUseCity(size_t index);
		bool		__CheckEntitySpawnPos(const int32_t x, const int32_t y, const TCityShopInfo& city);
		void		__UpdateEntity(const offlineshop::CShop& rShop);
#endif


	public:
//packets exchanging db
//ITEMS
/*db*/	void		SendShopBuyDBPacket(uint32_t dwBuyerID, uint32_t dwOwnerID,uint32_t dwItemID);
/*db*/	void		SendShopLockBuyItemDBPacket(uint32_t dwBuyerID, uint32_t dwOwnerID,uint32_t dwItemID, int64_t TotalPriceSeen);

/*db*/	bool		RecvShopLockedBuyItemDBPacket(uint32_t dwBuyerID, uint32_t dwOwnerID,uint32_t dwItemID);
/*db*/	bool		RecvShopBuyDBPacket(uint32_t dwBuyerID, uint32_t dwOwnerID,uint32_t dwItemID);
/*db*/	void		SendShopCannotBuyLockedItemDBPacket(uint32_t dwOwnerID, uint32_t dwItemID); //topatch

/*db*/	void		SendShopEditItemDBPacket(uint32_t dwOwnerID, uint32_t dwItemID, const TPriceInfo& rPrice);
/*db*/	bool		RecvShopEditItemDBPacket(uint32_t dwOwnerID, uint32_t dwItemID, const TPriceInfo& rPrice);
			

/*db*/	void		SendShopRemoveItemDBPacket(uint32_t dwOwnerID, uint32_t dwItemID);
/*db*/	bool		RecvShopRemoveItemDBPacket(uint32_t dwOwnerID, uint32_t dwItemID);
		

/*db*/	void		SendShopAddItemDBPacket(uint32_t dwOwnerID, const TItemInfo& rItemInfo);
/*db*/	bool		RecvShopAddItemDBPacket(uint32_t dwOwnerID, const TItemInfo& rItemInfo);
		
//SHOPS 
/*db*/	void		SendShopForceCloseDBPacket(uint32_t dwPID);
/*db*/	bool		RecvShopForceCloseDBPacket(uint32_t dwPID);
/*db*/	bool		RecvShopExpiredDBPacket(uint32_t dwPID);
		
/*db*/	void		SendShopCreateNewDBPacket(const TShopInfo& , std::vector<TItemInfo>& vec);
/*db*/	bool		RecvShopCreateNewDBPacket(const TShopInfo& , std::vector<TItemInfo>& vec);

/*db*/	void		SendShopChangeNameDBPacket(uint32_t dwOwnerID, const char* szName);
/*db*/	bool		RecvShopChangeNameDBPacket(uint32_t dwOwnerID, const char* szName);




//OFFER
/*db*/	void		SendShopOfferNewDBPacket(const TOfferInfo& offer);
/*db*/	bool		RecvShopOfferNewDBPacket(const TOfferInfo& offer);
		
/*db*/	void		SendShopOfferNotifiedDBPacket(uint32_t dwOfferID, uint32_t dwOwnerID);
/*db*/	bool		RecvShopOfferNotifiedDBPacket(uint32_t dwOfferID, uint32_t dwOwnerID);

/*db*/	void		SendShopOfferAcceptDBPacket(const TOfferInfo& offer);
/*db*/	bool		RecvShopOfferAcceptDBPacket(uint32_t dwOfferID, uint32_t dwOwnerID);

/*db*/	void		SendShopOfferCancelDBPacket(const TOfferInfo& offer);
/*db*/	bool		RecvShopOfferCancelDBPacket(uint32_t dwOfferID, uint32_t dwOwnerID, bool isRemovingItem);//offlineshop-updated 05/08/19

		
/*db*/	void		SendShopSafeboxGetItemDBPacket(uint32_t dwOwnerID, uint32_t dwItemID);
/*db*/	void		SendShopSafeboxGetValutesDBPacket(uint32_t dwOwnerID, const TValutesInfo& valutes);
/*db*/  bool		SendShopSafeboxAddItemDBPacket(uint32_t dwOwnerID, const CShopItem& item);
/*db*/	bool		RecvShopSafeboxAddItemDBPacket(uint32_t dwOwnerID, uint32_t dwItemID, const TItemInfoEx& item);
/*db*/	bool		RecvShopSafeboxAddValutesDBPacket(uint32_t dwOwnerID, const TValutesInfo& valute);
/*db*/	bool		RecvShopSafeboxLoadDBPacket(uint32_t dwOwnerID, const TValutesInfo& valute, const std::vector<uint32_t>& ids, const std::vector<TItemInfoEx>& items);
//patch 08-03-2020
/*db*/  bool		RecvShopSafeboxExpiredItemDBPacket(uint32_t dwOwnerID, uint32_t dwItemID);


//AUCTION
/*db*/	void		SendAuctionCreateDBPacket(const TAuctionInfo& auction);
/*db*/	void		SendAuctionAddOfferDBPacket(const TAuctionOfferInfo& offer);

/*db*/	bool		RecvAuctionCreateDBPacket(const TAuctionInfo& auction);
/*db*/	bool		RecvAuctionAddOfferDBPacket(const TAuctionOfferInfo& offer);
/*db*/	bool		RecvAuctionExpiredDBPacket(uint32_t dwID);


//packets echanging clients
//SHOPS
/*cli.*/bool		RecvShopCreateNewClientPacket(entt::entity character, TShopInfo& rShopInfo, std::vector<TShopItemInfo> & vec);
/*cli.*/bool		RecvShopChangeNameClientPacket(entt::entity character, const char* szName);
/*cli.*/bool		RecvShopForceCloseClientPacket(entt::entity character);
/*cli.*/bool		RecvShopRequestListClientPacket(entt::entity character);
/*cli.*/bool		RecvShopOpenClientPacket(entt::entity character, uint32_t dwOwnerID);
/*cli.*/bool		RecvShopOpenMyShopClientPacket(entt::entity character);
/*cli.*/bool		RecvShopBuyItemClientPacket(entt::entity character, uint32_t dwOwnerID, uint32_t dwItemID, bool isSearch, int64_t TotalPriceSeen);
#ifdef ENABLE_NEW_SHOP_IN_CITIES
/*cli.*/bool		RecvShopClickEntity(entt::entity character, uint32_t dwShopEntityVID);
#endif

/*cli.*/void		SendShopListClientPacket(entt::entity character);
/*cli.*/void		SendShopOpenClientPacket(entt::entity character, CShop* pkShop);
/*cli.*/void		SendShopOpenMyShopClientPacket(entt::entity character);
/*cli.*/void		SendShopOpenMyShopNoShopClientPacket(entt::entity character);
/*cli.*/void		SendShopBuyItemFromSearchClientPacket(entt::entity character, uint32_t dwOwnerID, uint32_t dwItemID);
		
/*cli.*/void		SendShopForceClosedClientPacket(uint32_t dwOwnerID);


		//ITEMS
/*cli.*/bool		RecvShopAddItemClientPacket(entt::entity character, const TItemPos& item, const TPriceInfo& price);
/*cli.*/bool		RecvShopRemoveItemClientPacket(entt::entity character, uint32_t dwItemID);
/*cli.*/bool		RecvShopEditItemClientPacket(entt::entity character, uint32_t dwItemID, const TPriceInfo& price);

		//FILTER
/*cli.*/bool		RecvShopFilterRequestClientPacket(entt::entity character, const TFilterInfo& filter);
/*cli.*/void		SendShopFilterResultClientPacket(entt::entity character, const std::vector<TItemInfo>& items);


		//OFFERS
/*cli.*/bool		RecvShopCreateOfferClientPacket(entt::entity character, TOfferInfo& offer);
/*cli.*/bool		RecvShopEditOfferClientPacket(entt::entity character, const TOfferInfo& offer);/*unused*/
/*cli.*/bool		RecvShopAcceptOfferClientPacket(entt::entity character, uint32_t dwOfferID);
/*cli.*/bool		RecvShopCancelOfferClientPacket(entt::entity character, uint32_t dwOfferID, uint32_t dwOwnerID);
/*cli.*/bool		RecvOfferListRequestPacket(entt::entity character);

		
		//SAFEBOX
/*cli.*/bool		RecvShopSafeboxOpenClientPacket(entt::entity character);
/*cli.*/bool		RecvShopSafeboxGetItemClientPacket(entt::entity character, uint32_t dwItemID);
/*cli.*/bool		RecvShopSafeboxGetValutesClientPacket(entt::entity character, const TValutesInfo& valutes);
/*cli.*/bool		RecvShopSafeboxCloseClientPacket(entt::entity character);
		

		//AUCTION
/*cli.*/bool		RecvAuctionListRequestClientPacket(entt::entity character, bool owner = false);
/*cli.*/bool		RecvAuctionOpenRequestClientPacket(entt::entity character, uint32_t dwOwnerID);
/*cli.*/bool		RecvMyAuctionOpenRequestClientPacket(entt::entity character);
/*cli.*/bool		RecvAuctionCreateClientPacket(entt::entity character, uint32_t dwDuration, const TPriceInfo& init_price, const TItemPos& pos);
/*cli.*/bool		RecvAuctionAddOfferClientPacket(entt::entity character, uint32_t dwOwnerID, const TPriceInfo& price);
/*cli.*/bool		RecvAuctionExitFromAuction(entt::entity character, uint32_t dwOwnerID);

/*cli.*/void		SendAuctionListClientPacket(entt::entity character, const std::vector<TAuctionListElement>& auctionVec, bool owner);
/*cli.*/void		SendAuctionOpenAuctionClientPacket(entt::entity character, const TAuctionInfo& auction, const std::vector<TAuctionOfferInfo>& vec); 
/*cli.*/void		SendAuctionOpenMyAuctionNoAuctionClientPacket(entt::entity character);


/*cli.*/void		SendShopSafeboxRefresh(entt::entity character, const TValutesInfo& valute, const std::vector<CShopItem>& vec);
		
/*cli.*/void		RecvCloseBoardClientPacket(entt::entity character);
		void		RecvCloseMyAuction(entt::entity character);
		//other
		void		UpdateShopsDuration();
		void		UpdateAuctionsDuration();

		//search time map (checking to avoid search abouse)
		void		ClearSearchTimeMap();
		bool		CheckSearchTime(uint32_t dwPID);

		//*new check about auction offers
		bool		CheckLastOfferTime(uint32_t dwPID);
		void		ClearOfferTimeMap();

		//topatch 29-10
		void		CheckOfferOnItem(uint32_t dwOwnerID, uint32_t dwItemID);
	
		// fix flooding offers
		bool		CheckOfferCooldown(uint32_t dwPID);
		
			
		void		Destroy();

	private:
		SHOPMAP			m_mapShops;
		SAFEBOXMAP		m_mapSafeboxs;
		OFFERSMAP		m_mapOffer;

		LPEVENT			m_eventShopDuration;
		SEARCHTIMEMAP	m_searchTimeMap;
		AUCTIONMAP		m_mapAuctions;

		//*new check about auction offers
		SEARCHTIMEMAP	m_offerTimeMap;

		// fix flooding offers
		SEARCHTIMEMAP	m_offerCooldownMap;

#ifdef ENABLE_NEW_SHOP_IN_CITIES
		CITIESVEC		m_vecCities;
#endif
	};




	offlineshop::CShopManager& GetManager();
}

#endif//__ENABLE_NEW_OFFLINESHOP__
#endif //__INCLUDE_HEADER_OFFLINESHOP_MANAGER__
