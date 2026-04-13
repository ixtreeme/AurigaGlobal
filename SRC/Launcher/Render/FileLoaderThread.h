#pragma once

#include <thread>
#include <mutex>
#include <condition_variable>
#include <deque>
#include <string>
#include <atomic>

#include "../Pack/EterPackManager.h"


class CFileLoaderThread
{
public:
    struct TData
    {
        DWORD dwSize;
        void* pvBuf;
        std::string stFileName;
        CMappedFile File;
    };

    CFileLoaderThread();
    ~CFileLoaderThread();

    bool Create(void* arg);
    void Shutdown();
    void Request(const std::string& filename);
    bool Fetch(TData** ppData);

private:
    void ThreadMain();
    void Process();

    std::thread m_thread;
    std::mutex m_requestMutex;
    std::mutex m_completeMutex;
    std::condition_variable m_cv;

    std::deque<TData*> m_requestDeque;
    std::deque<TData*> m_completeDeque;

    std::atomic<bool> m_shutdown{false};
    void* m_pArg;
};
