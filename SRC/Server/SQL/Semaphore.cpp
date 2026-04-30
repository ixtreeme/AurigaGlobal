#include "stdafx.h"
#include "Semaphore.h"
#include "Core/Logging.hpp"

#ifndef _WIN32

CSemaphore::CSemaphore() : m_hSem(NULL)
{
	Initialize();
}

CSemaphore::~CSemaphore()
{
	Destroy();
}

int CSemaphore::Initialize()
{
	Clear();

	m_hSem = new sem_t;

	if (sem_init(m_hSem, 0, 0) == -1)
	{
		LOG_ERROR("{}: {}", "sem_init", strerror(errno));
		return false;
	}

	return true;
}

void CSemaphore::Destroy()
{
	Clear();
}

void CSemaphore::Clear()
{
	if (m_hSem)
	{
		sem_destroy(m_hSem);
		delete m_hSem;
	}

	m_hSem = NULL;
}

int CSemaphore::Wait()
{
	if (!m_hSem)
		return true;

	int re = sem_wait(m_hSem);

	if (re == -1)
	{
		if (errno == EINTR)
			return Wait();

		LOG_ERROR("{}: {}", "sem_wait", strerror(errno));
		return false;
	}

	return true;
}

int CSemaphore::Release(int count)
{
	if (!m_hSem)
		return false;

	for (int i = 0; i < count; ++i)
		sem_post(m_hSem);

	return true;
}

#else

CSemaphore::CSemaphore() : m_hSem(nullptr)
{
	Initialize();
}

CSemaphore::~CSemaphore()
{
	Destroy();
}

int CSemaphore::Initialize()
{
	Clear();

	m_hSem = ::CreateSemaphore(nullptr, 0, 32, nullptr);

	if (m_hSem == nullptr) {
		return false;
	}

	return true;
}

void CSemaphore::Destroy()
{
	Clear();
}

void CSemaphore::Clear()
{
	if (m_hSem == nullptr) {
		return;
	}
	::CloseHandle(m_hSem);
	m_hSem = nullptr;
}

int CSemaphore::Wait()
{
	if (!m_hSem)
		return true;

	uint32_t dwWaitResult = ::WaitForSingleObject(m_hSem, INFINITE);

	switch (dwWaitResult) {
		case WAIT_OBJECT_0:
			return true;
		default:
			break;
	}
	return false;
}

int CSemaphore::Release(int count)
{
	if (!m_hSem)
		return false;

	::ReleaseSemaphore(m_hSem, count, nullptr);

	return true;
}

#endif
