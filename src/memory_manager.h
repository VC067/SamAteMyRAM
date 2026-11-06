#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif

#include <windows.h>
#include <psapi.h>

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

enum class AllocationSpeed : int {
    Slow = 0,    // ~128 MB/s
    Normal = 1,  // ~512 MB/s
    Fast = 2     // ~1 GB/s
};

enum class SafetyMode : int {
    Safe4GB = 0,         // Leave at least 4 GB of available physical RAM
    Experimental1GB = 1  // Aggressive target; still raises the floor on large machines
};

struct MemoryStats {
    uint64_t totalPhysBytes = 0;
    uint64_t availPhysBytes = 0;
    uint64_t appAllocatedBytes = 0;
    uint64_t appWorkingSetBytes = 0;
    uint64_t startingAvailPhysBytes = 0;
    uint64_t targetAllocBytes = 0;
    uint64_t reserveBytes = 0;
    float sacrificeProgress = 0.0f; // 0.0 to 1.0
    bool isSacrificing = false;
    bool isAccepted = false;
    bool hadError = false;
    AllocationSpeed speed = AllocationSpeed::Normal;
    SafetyMode safety = SafetyMode::Safe4GB;
    std::string errorMessage;
    std::string statusMessage;
};

// Allocates and commits real process memory on a worker thread.
// StartSacrifice, StopSacrifice, ReclaimAll, and RequestEmergencyReclaim
// are safe to call from any thread, including while the UI thread is blocked.
class MemoryManager {
public:
    MemoryManager();
    ~MemoryManager();

    MemoryManager(const MemoryManager&) = delete;
    MemoryManager& operator=(const MemoryManager&) = delete;

    void SetSpeed(AllocationSpeed speed) { m_speed.store(speed); }
    AllocationSpeed GetSpeed() const { return m_speed.load(); }

    void SetSafetyMode(SafetyMode mode) { m_safetyMode.store(mode); }
    SafetyMode GetSafetyMode() const { return m_safetyMode.load(); }

    bool StartSacrifice();
    void StopSacrifice();
    void ReclaimAll();
    void RequestEmergencyReclaim();

    MemoryStats GetStats();
    bool IsSacrificing() const { return m_isSacrificing.load(); }
    bool IsAccepted() const { return m_isAccepted.load(); }

private:
    void WorkerLoop();
    size_t TouchMemory(void* ptr, size_t size);
    void ReleaseAllChunksInternal();
    uint64_t ComputeReserve(const MEMORYSTATUSEX& memStatus) const;
    void SetStatus(const std::string& msg);
    void SetError(const std::string& msg);
    void ClearMessages();

    std::atomic<bool> m_isSacrificing;
    std::atomic<bool> m_stopRequested;
    std::atomic<bool> m_isAccepted;
    std::atomic<bool> m_hadError;
    std::atomic<AllocationSpeed> m_speed;
    std::atomic<SafetyMode> m_safetyMode;
    std::atomic<uint64_t> m_appAllocatedBytes;
    std::atomic<uint64_t> m_startingAvailPhysBytes;

    std::mutex m_apiMutex;
    std::mutex m_chunksMutex;
    std::mutex m_stringMutex;
    std::thread m_workerThread;
    std::vector<void*> m_allocatedChunks;

    std::string m_errorMessage;
    std::string m_statusMessage;
    HANDLE m_lowMemoryNotification;
};
