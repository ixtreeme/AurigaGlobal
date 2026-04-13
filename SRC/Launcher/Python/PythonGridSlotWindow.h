#pragma once

#include "PythonSlotWindow.h"

namespace UI
{
	class CGridSlotWindow : public CSlotWindow
	{
		public:
			static DWORD Type();

		public:
			CGridSlotWindow(PyObject * ppyObject);
			virtual ~CGridSlotWindow();

			void Destroy();

			void ArrangeGridSlot(uint32_t dwStartIndex, uint32_t dwxCount, uint32_t dwyCount, int ixSlotSize, int iySlotSize, int ixTemporarySize, int iyTemporarySize);

		protected:
			void __Initialize();

			bool GetPickedSlotPointer(TSlot ** ppSlot);
			bool GetPickedSlotList(int iWidth, int iHeight, std::list<TSlot*> * pSlotPointerList);
			bool GetGridSlotPointer(int ix, int iy, TSlot ** ppSlot);
			bool GetPickedGridSlotPosition(int ixLocal, int iyLocal, int * pix, int * piy);
			bool CheckMoving(uint32_t dwSlotNumber, uint32_t dwItemIndex, const std::list<TSlot*> & c_rSlotList);

			bool OnIsType(uint32_t dwType);

			void OnRefreshSlot();
			void OnRenderPickingSlot();

		protected:
			uint32_t m_dwxCount;
			uint32_t m_dwyCount;

			std::vector<TSlot *> m_SlotVector;
	};
};
