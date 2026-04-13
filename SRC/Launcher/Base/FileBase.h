#pragma once

#include <windows.h>
#include <cstdint>

class CFileBase
{
	public:
		enum EFileMode
		{
			FILEMODE_READ = (1 << 0),
			FILEMODE_WRITE = (1 << 1)
		};

		CFileBase();
		virtual	~CFileBase();

		virtual void			Destroy();
		void			Close();

		BOOL			Create(const char* filename, EFileMode mode);
		virtual DWORD			Size();
		void			SeekCur(uint32_t size);
		void			Seek(uint32_t offset);
		virtual uint32_t			GetPosition();

		virtual BOOL	Write(const void* src, int bytes);
		virtual BOOL			Read(void* dest, int bytes);

		char*			GetFileName();
		BOOL			IsNull();

	protected:
		int				m_mode;
		char			m_filename[MAX_PATH+1];
		HANDLE			m_hFile;
		uint32_t			m_dwSize;
};
