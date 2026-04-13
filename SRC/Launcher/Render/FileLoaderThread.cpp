#include "FileLoaderThread.h"
CFileLoaderThread::CFileLoaderThread() : m_pArg(nullptr) {}

CFileLoaderThread::~CFileLoaderThread() {
    Shutdown();
}

bool CFileLoaderThread::Create(void* arg)
{
    m_pArg = arg;
    m_thread = std::thread(&CFileLoaderThread::ThreadMain, this);
    return true;
}

void CFileLoaderThread::Shutdown()
{
    {
        std::unique_lock<std::mutex> lock(m_requestMutex);
        m_shutdown = true;
    }
    m_cv.notify_all();

    if (m_thread.joinable())
        m_thread.join();

    // cleanup
    for (auto* p : m_requestDeque)
        delete p;
    for (auto* p : m_completeDeque)
        delete p;
}

void CFileLoaderThread::Request(const std::string& filename)
{
    auto* pData = new TData;
    pData->dwSize = 0;
    pData->pvBuf = nullptr;
    pData->stFileName = filename;

    {
        std::lock_guard<std::mutex> lock(m_requestMutex);
        m_requestDeque.push_back(pData);
    }

    m_cv.notify_one(); // Értesíti a háttér threadet, hogy új munka van
}

bool CFileLoaderThread::Fetch(TData** ppData)
{
    std::lock_guard<std::mutex> lock(m_completeMutex);

    if (m_completeDeque.empty())
        return false;

    *ppData = m_completeDeque.front();
    m_completeDeque.pop_front();
    return true;
}

void CFileLoaderThread::ThreadMain()
{
    while (true)
    {
        TData* pData = nullptr;

        // Várakozás, amíg van munka vagy shutdown
        {
            std::unique_lock<std::mutex> lock(m_requestMutex);
            m_cv.wait(lock, [this] {
                return !m_requestDeque.empty() || m_shutdown;
                });

            if (m_shutdown && m_requestDeque.empty())
                break;

            pData = m_requestDeque.front();
            m_requestDeque.pop_front();
        }

        // Dolgozás (Process)
        const void* pvBuf = nullptr;
        if (CEterPackManager::Instance().Get(pData->File, pData->stFileName.c_str(), &pvBuf))
        {
            pData->dwSize = pData->File.Size();
            pData->pvBuf = new char[pData->dwSize];
            memcpy(pData->pvBuf, pvBuf, pData->dwSize);
        }

        // Áthelyezés a complete listába
        {
            std::lock_guard<std::mutex> lock(m_completeMutex);
            m_completeDeque.push_back(pData);
        }

#ifdef ENABLE_LOADING_DELAY
        Sleep(g_iLoadingDelayTime);
#endif
    }
}
