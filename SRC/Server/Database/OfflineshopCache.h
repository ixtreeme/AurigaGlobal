#ifndef __INCLUDE_HEADER_OFFLINESHOP_CACHE__
#define __INCLUDE_HEADER_OFFLINESHOP_CACHE__

#ifdef __ENABLE_NEW_OFFLINESHOP__
namespace offlineshop
{
	template <class T>
	void ZeroObject(T& obj) {
		obj = {};
	}

	template <class T>
	void CopyObject(T& objDest, const T& objSrc) {
		memcpy(&objDest, &objSrc, sizeof(objDest));
	}

	template <class T>
	void CopyContainer(T& objDest, const T& objSrc) {
		objDest = objSrc;
	}

	template <class T>
	void DeletePointersContainer(T& obj) {
		typename T::iterator it = obj.begin();
		for (; it != obj.end(); it++)
			delete(*it);
	}


	//typedefs
	struct SQueryInfoAddItem;
	struct SQueryInfoCreateShop;






	class CShopCache
	{
	public:
		typedef struct SShopCacheItemInfo{
			TPriceInfo		price;
			TItemInfoEx		item;
			bool			bLock;

			SShopCacheItemInfo()
			{
				ZeroObject(price);
				ZeroObject(item);

				bLock = false;
			}

		} TShopCacheItemInfo;


		typedef std::map<uint32_t, TShopCacheItemInfo> SHOPCACHE_MAP;
		typedef struct {
			uint32_t	dwDuration;
			char	szName[OFFLINE_SHOP_NAME_MAX_LEN];
#ifdef KASMIR_PAKET_SYSTEM
			uint32_t	dwKasmirNpc;
#endif
			SHOPCACHE_MAP 	itemsmap;
			SHOPCACHE_MAP	soldsmap;

		} TShopCacheInfo;

		typedef std::map<uint32_t, TShopCacheItemInfo>::iterator ITEM_ITER;



	public:
		CShopCache();
		~CShopCache();



		bool		Get(uint32_t dwOwnerID, TShopCacheInfo** ppCache) const;

		bool		AddItem(uint32_t dwOwnerID, const TShopCacheItemInfo& rItem);
		bool		RemoveItem(uint32_t dwOwnerID, uint32_t dwItemID);
		bool		SellItem(uint32_t dwOwnerID, uint32_t dwItemID);
		bool		LockSellItem(uint32_t dwOwnerID, uint32_t dwItemID, int64_t TotalPriceSeen); //patch seen price check
		bool		UnlockSellItem(uint32_t dwOwnerID, uint32_t dwItemID);//topatch
		bool		EditItem(uint32_t dwOwnerID, uint32_t dwItemID, const TPriceInfo& rItemPrice);

		bool		CloseShop(uint32_t dwOwnerID);
		bool		CreateShop(uint32_t dwOwnerID, uint32_t dwDuration, const char* szName, std::vector<TShopCacheItemInfo>& items
#ifdef KASMIR_PAKET_SYSTEM
		, uint32_t dwKasmirNpc
#endif
		);
		bool		CreateShopAddItem(SQueryInfoCreateShop* qi, const TShopCacheItemInfo& rItem);
		bool		ChangeShopName(uint32_t dwOwnerID, const char* szName);

		bool		PutItem(uint32_t dwOwnerID, uint32_t dwItemID, const TShopCacheItemInfo& rItem, bool isSold=false);
		bool		PutShop(uint32_t dwOwnerID, uint32_t dwDuration, const char* szName
#ifdef KASMIR_PAKET_SYSTEM
		, uint32_t dwKasmirNpc
#endif
		);

		uint32_t		GetCount() const {return m_shopsMap.size();}
		void		EncodeCache(CPeer* peer) const;
		uint32_t		GetItemCount() const;

		void		ShopDurationProcess();
		void		UpdateDurationQuery(uint32_t dwOwnerID, const TShopCacheInfo& rShop );

	private:
		typedef std::map<uint32_t, TShopCacheInfo>::iterator CACHEITER;
		typedef std::map<uint32_t, TShopCacheInfo>::const_iterator CONST_CACHEITER;
		std::map<uint32_t, TShopCacheInfo> m_shopsMap;
		


	};



	class CSafeboxCache
	{
	public:
		typedef struct {
			TValutesInfo	valutes;
			std::map<uint32_t, TItemInfoEx>
				itemsmap;
		} TSafeboxCacheInfo;

		typedef std::map<uint32_t, TSafeboxCacheInfo> CACHEMAP;
		typedef std::map<uint32_t, TSafeboxCacheInfo>::iterator CHACHEITER;
		typedef std::map<uint32_t, TSafeboxCacheInfo>::const_iterator CHACHECONSTITER;

	public:
		CSafeboxCache();
		~CSafeboxCache();

		bool				Get(uint32_t dwOwnerID, TSafeboxCacheInfo** ppSafebox) const;

		bool				PutSafebox(uint32_t dwOwnerID, const TSafeboxCacheInfo& rSafebox);
		bool				PutItem(uint32_t dwOwnerID, uint32_t dwItem, const TItemInfoEx& item);

		bool				RemoveItem(uint32_t dwOwner, uint32_t dwItemID);
		bool				AddItem(uint32_t dwOwnerID, const TItemInfoEx& item);

		bool				AddValutes(uint32_t dwOwnerID, const TValutesInfo& val);
		bool				RemoveValutes(uint32_t dwOwnerID, const TValutesInfo& val);
		void				AddValutesAsQuery(uint32_t dwOwnerID, const TValutesInfo& val);

		TSafeboxCacheInfo*	CreateSafebox(uint32_t dwOwnerID);
		uint32_t				GetCount() const		{return m_safeboxMap.size();}
		uint32_t				GetItemCount() const;

		TSafeboxCacheInfo*	LoadSafebox(uint32_t dwPID);

		//patch 08-03-2020
		void				ItemExpirationProcess();


	private:
		CACHEMAP	m_safeboxMap;

	};



	class COfferCache
	{
	public:
		typedef struct {
			uint32_t		dwItemID,dwOwnerID, dwOffererID;
			TPriceInfo	price;
			bool		bNoticed,bAccepted;

			//offlineshop-updated 03/08/19
			char		szBuyerName[CHARACTER_NAME_MAX_LEN+1];
		} TOfferCacheInfo;

		typedef std::map<uint32_t, TOfferCacheInfo> CACHEMAP;
		typedef CACHEMAP::iterator CACHEITER;
		typedef CACHEMAP::const_iterator CONST_CACHEITER;
		typedef std::vector<uint32_t> OFFERIDVEC;
		typedef std::map<uint32_t, OFFERIDVEC>    FINDOFFERBYSHOP;

	public:
		COfferCache();
		~COfferCache();

		bool	Puts(uint32_t dwOfferID, const TOfferCacheInfo& rOffer);
		bool	Get(uint32_t dwOfferID, TOfferCacheInfo** ppOffer) const;
		bool	GetOffersByShopOwner(uint32_t dwOwnerID ,  COfferCache::OFFERIDVEC** ppVec);
		bool	GetOffersByItemID(uint32_t dwItemID, std::vector<uint32_t>& offerIDs);

		bool	AddOffer(const TOfferCacheInfo& rOffer);
		bool	NoticedOffer(uint32_t dwOfferID);
		bool	AcceptOffer(uint32_t dwOfferID);
		bool	CancelOffer(uint32_t dwOfferID,uint32_t dwOwnerID);
		bool	RemoveOffersByShopOwner(uint32_t dwShopOwnerID);

		uint32_t	GetCount() {return m_mapOffer.size();}
		void	EncodeCache(CPeer* peer) const;

	private:
		CACHEMAP			m_mapOffer;
		FINDOFFERBYSHOP		m_findOffersByShop;

	};






	class CAuctionCache 
	{
	public:
		typedef std::vector<TAuctionOfferInfo>						AUCTIONOFFERVEC;
		typedef std::map<uint32_t, TAuctionInfo>						AUCTIONMAP;
		typedef std::map<uint32_t,AUCTIONOFFERVEC >	AUCTIONOFFERMAP;


	public:
		CAuctionCache();
		~CAuctionCache();


		bool	PutsAuction(const TAuctionInfo& auction);
		bool	PutsAuctionOffer(const TAuctionOfferInfo& auctionOffer);

		bool	AddAuction(const TAuctionInfo& auction);
		bool	AddOffer(const TAuctionOfferInfo& auctionOffer, bool quering = true);
		bool	ExpiredAuction(uint32_t dwOwnerID);
		bool	AuctionDurationProcess();

		uint32_t	GetBestBuyer(uint32_t dwOwnerID, TAuctionOfferInfo** ppOffer);

		bool	Get(uint32_t dwOwnerID, TAuctionInfo** ppInfo);
		bool	GetOffers(uint32_t dwOwnerID, AUCTIONOFFERVEC** ppVec);

		uint32_t	GetCount();
		uint32_t	GetOffersCount();

		void	EncodeCache(CPeer* peer);

	private:
		void	__UpdateDuration(const TAuctionInfo& auction);



	private:
		AUCTIONMAP			m_mapAuction;
		AUCTIONOFFERMAP		m_mapAuctionOffer;


	};








	//QUERY INFO STRUCT
	//shopcache
	struct SQueryInfoAddItem {
		uint32_t dwOwnerID;
		CShopCache::TShopCacheItemInfo item;
	};



	struct SQueryInfoCreateShop {
		uint32_t dwOwnerID;
		uint32_t dwDuration;
		char  szName[OFFLINE_SHOP_NAME_MAX_LEN];
#ifdef KASMIR_PAKET_SYSTEM
		uint32_t	dwKasmirNpc;
#endif
		std::vector<CShopCache::TShopCacheItemInfo> items;
		std::vector<uint32_t> ids;
		uint32_t dwItemIndex;
	};




	//safeboxcache
	struct SQueryInfoSafeboxAddItem {
		uint32_t		dwOwnerID;
		TItemInfoEx item;
	};


	//offer cache
	struct SQueryInfoOfferAdd {
		COfferCache::TOfferCacheInfo offer;
	};
}

	



#endif //__ENABLE_NEW_OFFLINESHOP__
#endif //__INCLUDE_HEADER_OFFLINESHOP_CACHE__