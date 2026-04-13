#pragma once

#include "ReferenceObject.h"
#include <string>

class CResource : public CReferenceObject
{
	public:
		typedef uint32_t TType;

		enum EState
		{
			STATE_EMPTY,
			STATE_ERROR,
			STATE_EXIST,
			STATE_LOAD,
			STATE_FREE
		};

		void			Clear();

		static TType	StringToType(const char* c_szType);
		static TType	Type();

		void			Load();
		void			Reload();
		int				ConvertPathName(const char * c_szPathName, char * pszRetPathName, int retLen);

		virtual bool	CreateDeviceObjects();
		virtual void	DestroyDeviceObjects();

		explicit CResource(const char* c_szFileName);
		~CResource() override;

		static void		SetDeleteImmediately(bool isSet = false);

		// is loaded?
		[[nodiscard]] bool IsData() const;
		[[nodiscard]] bool IsEmpty() const;
		bool			IsType(TType type);

		uint32_t			GetLoadCostMilliSecond() const { return m_dwLoadCostMiliiSecond;	}

		[[nodiscard]] const char* GetFileName() const			{ return m_stFileName.c_str();				}
		[[nodiscard]] const std::string& GetFileNameString() const { return m_stFileName;	}

		virtual bool	OnLoad(int iSize, const void * c_pvBuf) = 0;

	protected:
		void			SetFileName(const char* c_szFileName);

		virtual void	OnClear() = 0;
		virtual bool	OnIsEmpty() const = 0;
		virtual bool	OnIsType(TType type) = 0;

		void	OnConstruct() override;
		void	OnSelfDestruct() override;

		std::string		m_stFileName;
		uint32_t			m_dwLoadCostMiliiSecond;
		EState			me_state;

		static bool		ms_bDeleteImmediately;
};
