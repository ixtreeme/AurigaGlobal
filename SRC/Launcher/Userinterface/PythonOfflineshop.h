#ifndef __INCLUDE_HEADER_PYTHON_OFFLINESHO__
#define __INCLUDE_HEADER_PYTHON_OFFLINESHO__

#ifdef __ENABLE_NEW_OFFLINESHOP__

#include "PythonBackground.h"
#include "InstanceBase.h"
#include "PythonCharacterManager.h"

#include <unordered_map>
#include <unordered_set>

namespace offlineshop
{
	template <class T>
	void ZeroObject(T& obj) {
		memset(&obj, 0, sizeof(obj));
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
		for (; it != obj.end(); ++it)
			delete(*it);
	}



	

	


	template <class T ,template <class> class S, typename S<T>::iterator >
	void ForEach(S<T>& container, std::function<void(T&)> func){
		typename S<T>::iterator it=container.begin(), iter;
		
		while((iter = it++) != container.end())
			func(*iter);
	}


	template <class T , class K, template <class,class> class S, typename S<K,T>::iterator >
	void ForEach(S<K,T>& container, std::function<void(T&)> func){
		typename S<K,T>::iterator it=container.begin(), iter;

		while((iter = it++) != container.end())
			func(iter->second);
	}
}




namespace offlineshop
{
	enum eConstOfflineshop {
		OFFLINESHOP_DURATION_MAX_DAYS	= 8,
		OFFLINESHOP_DURATION_MAX_HOURS	= 23,

		OFFLINESHOP_DURATION_MAX_MINUTES= OFFLINESHOP_DURATION_MAX_DAYS * 24 * 60,

		OFFLINESHOP_MAX_FILTER_HISTORY_SIZE = 50,
	};
}


#ifdef ENABLE_NEW_SHOP_IN_CITIES
namespace offlineshop
{
	class ShopInstance
	{
	public:
		ShopInstance() {
			m_dwVID = 0;
			m_iType = 0;
			m_stSign.clear();
		}


		~ShopInstance() {
			m_dwVID = 0;
			m_iType = 0;
			m_stSign.clear();
		}


		void SetVID(uint32_t dwVID) {
			m_dwVID = dwVID;
		}


		void SetShopType(int iType) {
			m_iType = iType;
		}


		void SetSign(const char* cpszSign) {
			m_stSign = cpszSign;
		}


		void Show(float x, float y, float z
#ifdef KASMIR_PAKET_SYSTEM
		, uint32_t dwKasmirNpc
#endif
		) {
#ifdef KASMIR_PAKET_SYSTEM
			m_thingInstance.SetActorType(CActorInstance::TYPE_NPC);
			m_thingInstance.SetRace(dwKasmirNpc);
			m_thingInstance.SetShape(0);
			m_thingInstance.SetMotionMode(CRaceMotionData::MODE_GENERAL);
			m_thingInstance.InterceptLoopMotion(CRaceMotionData::NAME_WAIT);
			m_thingInstance.RefreshActorInstance();
#else
			m_thingInstance.Clear();
			m_thingInstance.ReserveModelThing(1);
			m_thingInstance.ReserveModelInstance(1);
			std::string modelPath = "offlineshop/shop.gr2";
			m_thingInstance.RegisterModelThing(0, (CGraphicThing *)CResourceManager::Instance().GetResourcePointer(modelPath.c_str()));
			m_thingInstance.SetModelInstance(0, 0, 0);
#endif

			TPixelPosition c_rPixelPos;
			c_rPixelPos.x = x;
			c_rPixelPos.y = -y;
			c_rPixelPos.z = z;
			m_thingInstance.SetPixelPosition(c_rPixelPos);
#ifdef KASMIR_PAKET_SYSTEM
			m_thingInstance.INSTANCEBASE_Transform();
			m_thingInstance.INSTANCEBASE_Deform();
#else
			m_thingInstance.Show();
			m_thingInstance.Update();
			m_thingInstance.Transform();
			m_thingInstance.Deform();
#endif
		}

		uint32_t GetVID() const {
			return m_dwVID;
		}

		std::string GetSign() const {
			return m_stSign;
		}


		int GetType() const {
			return m_iType;
		}

		CActorInstance* GetThingInstancePtr() {
			return &m_thingInstance;
		}

		void Clear() {
			m_thingInstance.Clear();
			m_dwVID = 0;
			m_iType = 0;
			m_stSign.clear();
		}

		void Render()
		{
			m_thingInstance.Render();
		}

		void BlendRender()
		{
			m_thingInstance.BlendRender();
		}

		void Deform()
		{
			m_thingInstance.INSTANCEBASE_Deform();
		}

		void Update()
		{
			m_thingInstance.Update();
			m_thingInstance.MotionProcess(false);
		}

	private:
		CActorInstance	m_thingInstance;
		uint32_t					m_dwVID;
		int						m_iType;
		std::string				m_stSign;
	};
}
#endif









class CPythonOfflineshop : public CSingleton<CPythonOfflineshop>
{

public:
	typedef struct {
		uint8_t	bMinutes;
		uint8_t	bHour;

		uint8_t	bDay;
		uint8_t	bMonth;
		int		iYear;
	} TDatetime;


	typedef struct SFilterHistoryElement{
		TDatetime					datetime;
		uint32_t						dwCountItem;
		offlineshop::TFilterInfo	filter;

		SFilterHistoryElement()
		{
			memset(&datetime, 0, sizeof(datetime));
			dwCountItem = 0;
			memset(&filter, 0, sizeof(filter));
		}

		bool operator >(const SFilterHistoryElement& r) const
		{
			if(datetime.iYear != r.datetime.iYear)
				return datetime.iYear > r.datetime.iYear;

			if(datetime.bMonth != r.datetime.bMonth)
				return datetime.bMonth > r.datetime.bMonth;

			if(datetime.bDay != r.datetime.bDay)
				return datetime.bDay > r.datetime.bDay;

			if(datetime.bHour != r.datetime.bHour)
				return datetime.bHour > r.datetime.bHour;

			if(datetime.bMinutes != r.datetime.bMinutes)
				return datetime.bMinutes > r.datetime.bMinutes;

			return false;


		}

	} TFilterHistoryElement;


	typedef struct {
		TDatetime	datetime_lastuse ;
		std::string stName;
		offlineshop::TFilterInfo filter;
	} TFilterPatternInfo;

	typedef std::vector<TFilterHistoryElement> FILTERHISTORY;
	typedef FILTERHISTORY::iterator FILTERHISTORY_ITER;

	typedef std::map<int, TFilterPatternInfo> FILTERMAP;
	typedef FILTERMAP::iterator FILTERMAP_ITER;


	static void GetNowAsDatetime(TDatetime& datetime)
	{
		SYSTEMTIME time;
		GetLocalTime(&time);

		datetime.bMinutes	= (uint8_t) time.wMinute;
		datetime.bHour		= (uint8_t) time.wHour;
		datetime.bDay		= (uint8_t) time.wDay;

		datetime.bMonth		= (uint8_t) time.wMonth;
		datetime.iYear		= (int)	 time.wYear;
	}


	static int AllocPatternID()
	{
		static int id=0;
		return id++;
	}

public:
	CPythonOfflineshop();
	~CPythonOfflineshop();

	//starts
	void		SetWindowObjectPointer(PyObject* poWindow);
	PyObject*	GetOfflineshopBoard();

	//GC
	void	ShopListAddItem(const offlineshop::TShopInfo& shop);
	void	ShopListShow();
	void	ShopListClear();

	void	BuyFromSearch(uint32_t dwOwnerID, uint32_t dwItemID);

	void	OpenShop(const offlineshop::TShopInfo& shop, const std::vector<offlineshop::TItemInfo>& vec);
	void	OpenShopOwner(	const offlineshop::TShopInfo& shop, 
							const std::vector<offlineshop::TItemInfo>& vec , 
							const std::vector<offlineshop::TItemInfo>& solds,
							const std::vector<offlineshop::TOfferInfo>& offers
	);
	void	OpenShopOwnerNoShop();

	void	ShopClose();
	void	ShopFilterResult(const std::vector<offlineshop::TItemInfo>& vec);
	void	OfferListReceived(const std::vector<offlineshop::TOfferInfo>& offers, const std::vector<offlineshop::TMyOfferExtraInfo>& items);//offlineshop-updated 03/08/19
	void	SafeboxRefresh(const offlineshop::TValutesInfo& valute,const std::vector<uint32_t>& ids, const std::vector<offlineshop::TItemInfoEx>& item); 


	void	AuctionList(const std::vector<offlineshop::TAuctionListElement>& auctions, bool owner);
	void	MyAuctionOpen(const offlineshop::TAuctionInfo& auction, const std::vector<offlineshop::TAuctionOfferInfo>& offers);
	void	AuctionOpen(const offlineshop::TAuctionInfo& auction, const std::vector<offlineshop::TAuctionOfferInfo>& offers);
	void	OpenMyAuctionNoAuction();

	//filter caching
	void	LoadFilterHistory();
	void	LoadFilterPatterns();

	void	SaveFilterHistory();
	void	SaveFilterPatterns();

	void	AddNewFilterToHistory(TFilterHistoryElement& element);
	void	PopOldestFilterHistoryElement();

	void	AddNewFilterPattern(TFilterPatternInfo& pattern);

	void	UpdateFilterPatternLastUse(int iPatternID);
	
	//interfaces methods
	void	RefreshFilterHistory();
	void	RefreshFilterPatterns();
	void	RefreshItemNameMap();
	void	EnableGuiRefreshSymbol();


	//create shop methods
	void	ShopBuilding_AddInventoryItem(int iSlot);
	void	AuctionBuilding_AddInventoryItem(int iSlot);

	void	ShopBuilding_AddItem(int iWin, int iSlot);
	void	AuctionBuilding_AddItem(int iWin, int iSlot);


#ifdef ENABLE_NEW_SHOP_IN_CITIES
	void	InsertEntity(uint32_t dwVID , int iType, const char* szName, int32_t x, int32_t y, int32_t z
#ifdef KASMIR_PAKET_SYSTEM
	, uint32_t dwKasmirNpc
#endif
	);
	void	RemoveEntity(uint32_t dwVID);

	void	RenderEntities();
	void	UpdateEntities();
	void	DeformEntities();

	bool	GetShowNameFlag();
	void	SetShowNameFlag(bool flag);

	void	DeleteEntities();

	void	MarkShopViewedByName(const std::string& ownerName);
	void	UpdateShopOwnerMapping(uint32_t dwVID, const std::string& ownerName);
#endif

private:
	PyObject* m_poWindow;

	FILTERHISTORY	m_filterHistory;
	FILTERMAP		m_filterPatterns;

#ifdef ENABLE_NEW_SHOP_IN_CITIES
	std::vector<offlineshop::ShopInstance*>	m_vecShopInstance;
	bool			m_bIsShowName;

	std::unordered_map<uint32_t, std::string> m_shopOwnerNameByVid;
	std::unordered_map<std::string, std::vector<uint32_t>> m_shopVidsByOwnerName;
	std::unordered_set<std::string> m_viewedShopOwners;
#endif

};


extern void initofflineshop();



#endif //__ENABLE_NEW_OFFLINESHOP__

#endif //__INCLUDE_HEADER_PYTHON_OFFLINESHOP__