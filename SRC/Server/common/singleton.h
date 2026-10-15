#pragma once

#include <cstdio>
#include <cstdlib>
#include <typeinfo>

// One live instance of T at a time, constructed and destroyed explicitly
// (normally a local in main). The checks hold in release builds too: a
// second instance, or a use while none is alive, stops the process with the
// type's name instead of silently replacing the instance or dereferencing
// null. Copies are refused at compile time - a copy never registered itself,
// yet its destructor would have cleared the live instance.
template <typename T> class singleton
{
	public:
		static T * ms_singleton;

		singleton()
		{
			if (ms_singleton)
				Fail("a second instance was constructed");
			ms_singleton = static_cast<T*>(this);
		}

		virtual ~singleton()
		{
			ms_singleton = nullptr;
		}

		singleton(const singleton&) = delete;
		singleton& operator=(const singleton&) = delete;

		static T & instance()
		{
			if (!ms_singleton) [[unlikely]]
				Fail("used while no instance exists");
			return *ms_singleton;
		}

		static T & Instance()
		{
			return instance();
		}

		static T * instance_ptr()
		{
			return ms_singleton;
		}

	private:
		[[noreturn]] static void Fail(const char * what)
		{
			std::fprintf(stderr, "singleton<%s>: %s\n", typeid(T).name(), what);
			std::fflush(stderr);
			std::abort();
		}
};

template <typename T> T * singleton <T>::ms_singleton = nullptr;
