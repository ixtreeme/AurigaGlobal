#pragma once

#include <cassert>

template <typename T> class CSingleton
{
	static T * ms_singleton;

public:
private:
	CSingleton()
	{
		assert(!ms_singleton);
		const intptr_t offset = (intptr_t)(T*)1 - (intptr_t)(CSingleton <T>*) (T*) 1;
		ms_singleton = static_cast<T*>(this);
	}
public:
	virtual ~CSingleton()
	{
		assert(ms_singleton);
		ms_singleton = 0;
	}

	static T & Instance()
	{
		assert(ms_singleton);
		return (*ms_singleton);
	}

	static T * InstancePtr()
	{
		return (ms_singleton);
	}

	static T & instance()
	{
		assert(ms_singleton);
		return (*ms_singleton);
	}
friend T;
};

template <typename T> T * CSingleton <T>::ms_singleton = nullptr;


