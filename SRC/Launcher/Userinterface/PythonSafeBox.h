#pragma once

class CPythonSafeBox : public CSingleton<CPythonSafeBox>
{
	public:
		enum
		{
			SAFEBOX_SLOT_X_COUNT = 16,
			SAFEBOX_SLOT_Y_COUNT = 9,
			SAFEBOX_PAGE_SIZE = SAFEBOX_SLOT_X_COUNT * SAFEBOX_SLOT_Y_COUNT,
		};
		typedef std::vector<TItemData> TItemInstanceVector;

	public:
		CPythonSafeBox();
		virtual ~CPythonSafeBox();

		void OpenSafeBox(int iSize);
		void SetItemData(uint32_t dwSlotIndex, const TItemData & rItemData);
		void DelItemData(uint32_t dwSlotIndex);

		void SetMoney(uint32_t dwMoney);
		uint32_t GetMoney();

		bool GetSlotItemID(uint32_t dwSlotIndex, uint32_t* pdwItemID);

		int GetCurrentSafeBoxSize();
		bool GetItemDataPtr(uint32_t dwSlotIndex, TItemData ** ppInstance);

		// MALL
		void OpenMall(int iSize);
		void SetMallItemData(uint32_t dwSlotIndex, const TItemData & rItemData);
		void DelMallItemData(uint32_t dwSlotIndex);
		bool GetMallItemDataPtr(uint32_t dwSlotIndex, TItemData ** ppInstance);
		bool GetSlotMallItemID(uint32_t dwSlotIndex, uint32_t * pdwItemID);
		uint32_t GetMallSize();

	protected:
		TItemInstanceVector m_ItemInstanceVector;
		TItemInstanceVector m_MallItemInstanceVector;
		uint32_t m_dwMoney;
};