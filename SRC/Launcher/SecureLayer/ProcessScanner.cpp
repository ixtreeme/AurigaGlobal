#include "ProcessScanner.h"

#include <Windows.h>
#include <tlhelp32.h>
#include <vector>
#include <unordered_map>
#include <thread>
#include <mutex>
#include <atomic>
#include <random>
#include <string>
#include <chrono>

#include "../Base/CRC32.h"

struct UniqueHandle {
	HANDLE handle{ INVALID_HANDLE_VALUE };
	UniqueHandle() = default;
	explicit UniqueHandle(HANDLE h) : handle(h) {}
	~UniqueHandle() { close(); }

	void close() {
		if (handle != INVALID_HANDLE_VALUE && handle != nullptr) {
			CloseHandle(handle);
			handle = INVALID_HANDLE_VALUE;
		}
	}

	operator HANDLE() const { return handle; }
	bool valid() const { return handle != INVALID_HANDLE_VALUE && handle != nullptr; }
};

// -----------------------------------------------------------
// Global state
// -----------------------------------------------------------
static std::vector<CRCPair> g_crcResults;
static std::mutex g_dataMutex;
static std::thread g_workerThread;
static std::atomic g_stopRequested{ false };
static std::atomic g_threadExited{ false };

// -----------------------------------------------------------
// Helpers
// -----------------------------------------------------------
static std::string WideToUtf8(const std::wstring& wstr)
{
	if (wstr.empty())
		return {};
	int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), nullptr, 0, nullptr, nullptr);
	std::string result(sizeNeeded, 0);
	WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), &result[0], sizeNeeded, nullptr, nullptr);
	return result;
}

// -----------------------------------------------------------
// Main scanning routine
// -----------------------------------------------------------
static void ScanProcessList(std::unordered_map<uint32_t, uint32_t>& crcMap,
	std::vector<CRCPair>& outPairs)
{
	UniqueHandle hSnapshot(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
	if (!hSnapshot.valid())
		return;

	PROCESSENTRY32W pe{};
	pe.dwSize = sizeof(pe);

	if (!Process32FirstW(hSnapshot, &pe))
		return;

	do {
		UniqueHandle hModSnap(CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pe.th32ProcessID));
		if (!hModSnap.valid())
			continue;

		MODULEENTRY32W me{};
		me.dwSize = sizeof(me);

		if (!Module32FirstW(hModSnap, &me))
			continue;

		do {
			std::string exePath = WideToUtf8(me.szExePath);
			if (exePath.empty())
				continue;

			uint32_t pathCrc = GetCRC32(exePath.c_str(), (uint32_t)exePath.size());
			if (crcMap.find(pathCrc) == crcMap.end()) {
				uint32_t fileCrc = GetFileCRC32(exePath.c_str());
				crcMap[pathCrc] = fileCrc;
				outPairs.emplace_back(fileCrc, exePath);
			}
		} while (Module32NextW(hModSnap, &me));

	} while (Process32NextW(hSnapshot, &pe));
}

// -----------------------------------------------------------
// Thread loop
// -----------------------------------------------------------
static void ProcessScanner_Thread()
{
	std::unordered_map<uint32_t, uint32_t> crcCache;
	std::vector<CRCPair> localPairs;

	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_int_distribution<uint32_t> dist(1000, 11000);

	while (!g_stopRequested.load()) {
		localPairs.clear();
		ScanProcessList(crcCache, localPairs);

		{
			std::lock_guard<std::mutex> lock(g_dataMutex);
			g_crcResults.insert(g_crcResults.end(), localPairs.begin(), localPairs.end());
		}

		// Randomized delay between scans
		std::this_thread::sleep_for(std::chrono::milliseconds(dist(gen)));
	}

	g_threadExited.store(true);
}

// -----------------------------------------------------------
// Public API
// -----------------------------------------------------------
bool ProcessScanner_Create()
{
	g_stopRequested = false;
	g_threadExited = false;

	try {
		g_workerThread = std::thread(ProcessScanner_Thread);
		SetThreadPriority(g_workerThread.native_handle(), THREAD_PRIORITY_NORMAL);
	}
	catch (...) {
		return false;
	}

	return true;
}

void ProcessScanner_ReleaseQuitEvent()
{
	g_stopRequested.store(true);
}

void ProcessScanner_Destroy()
{
	ProcessScanner_ReleaseQuitEvent();

	if (g_workerThread.joinable())
		g_workerThread.join();

	g_threadExited.store(true);

	{
		std::lock_guard lock(g_dataMutex);
		g_crcResults.clear();
	}
}

bool ProcessScanner_PopProcessQueue(std::vector<CRCPair>* outPairs)
{
	if (!outPairs)
		return false;

	std::lock_guard lock(g_dataMutex);
	if (g_crcResults.empty())
		return false;

	*outPairs = std::move(g_crcResults);
	g_crcResults.clear();
	return true;
}
