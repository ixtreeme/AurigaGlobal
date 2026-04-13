#pragma once

class CFrameController
{
	public:
		CFrameController();
		virtual ~CFrameController();

		void Clear();

		void Update(float fElapsedTime);

		void SetCurrentFrame(uint32_t dwFrame);
		uint8_t GetCurrentFrame();

		void SetMaxFrame(uint32_t dwMaxFrame);
		void SetFrameTime(float fTime);
		void SetStartFrame(uint32_t dwStartFrame);
		void SetLoopFlag(bool bFlag);
		void SetLoopCount(int iLoopCount);

		void SetActive(bool bFlag);
		bool isActive(uint32_t dwMainFrame = 0);

	protected:
		// Dynamic
		bool m_isActive;
		uint32_t m_dwcurFrame;
		float m_fLastFrameTime;

		int m_iLoopCount;

		// Static
		bool m_isLoop;
		uint32_t m_dwMaxFrame;
		float m_fFrameTime;
		uint32_t m_dwStartFrame;
};
