#pragma once
#define LEADERBOARD_RAZOR93

template <typename T>
class TAbstractSingleton
{
	static T * ms_singleton;

public:
#ifdef LEADERBOARD_RAZOR93
	TAbstractSingleton()
	{
		assert(!ms_singleton);
		ms_singleton = static_cast<T*>(this);
	}
#else

	TAbstractSingleton()
	{
		assert(!ms_singleton);
		intptr_t offset = (intptr_t) (T*) 1 - (intptr_t) (CSingleton <T>*) (T*) 1;
		ms_singleton = (T*) ((intptr_t) this + offset);
	}
#endif
	virtual ~TAbstractSingleton()
	{
		assert(ms_singleton);
		ms_singleton = 0;
	}
#ifdef LEADERBOARD_RAZOR93
	static bool HasSingleton()
	{
		return ms_singleton != nullptr;
	}

	static void DestroySingleton()
	{
		if (ms_singleton)
		{
			delete ms_singleton;
			ms_singleton = nullptr;
		}
	}
	static void CreateSingleton()
	{
		if (!ms_singleton)
			ms_singleton = new T();
	}
#endif
	__forceinline static T & GetSingleton()
	{
		//assert(ms_singleton!=NULL);
		return (*ms_singleton);
	}
};


template <typename T> T * TAbstractSingleton <T>::ms_singleton = nullptr;