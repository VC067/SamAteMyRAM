#include "memory_manager.h"

#include <chrono>
#include <string>

namespace {
constexpr uint64_t kOneGB = 1024ULL * 1024ULL * 1024ULL;
constexpr uint64_t kPageFileReserve = 2ULL * kOneGB;
}

MemoryManager::MemoryManager()
    : m_isSacrificing(false),
      m_stopRequested(false),
      m_isAccepted(false),
      m_hadError(false),
      m_speed(AllocationSpeed::Normal),
      m_safetyMode(SafetyMode::Safe4GB),
      m_appAllocatedBytes(0),
      m_startingAvailPhysBytes(0),
      m_lowMemoryNotification(CreateMemoryResourceNotification(LowMemoryResourceNotification)) {
}

MemoryManager::~MemoryManager() {
    ReclaimAll();
    if (m_lowMemoryNotification) {
        CloseHandle(m_lowMemoryNotification);
        m_lowMemoryNotification = nullptr;
    }
}

void MemoryManager::SetStatus(const std::string& msg) {
    std::lock_guard<std::mutex> lock(m_stringMutex);
    m_statusMessage = msg;
}

void MemoryManager::SetError(const std::string& msg) {
    std::lock_guard<std::mutex> lock(m_stringMutex);
    m_errorMessage = msg;
    m_hadError.store(true);
}

void MemoryManager::ClearMessages() {
    std::lock_guard<std::mutex> lock(m_stringMutex);
    m_errorMessage.clear();
    m_statusMessage.clear();
    m_hadError.store(false);
}

uint64_t MemoryManager::ComputeReserve(const MEMORYSTATUSEX& memStatus) const {
    if (m_safetyMode.load() == SafetyMode::Experimental1GB) {
        // 1 GB floor, raised with machine size so a huge box is not eaten down to a sliver.
        // Capped at 8 GB so experimental mode still does something on workstation-class RAM.
        uint64_t dynamic = memStatus.ullTotalPhys / 16; // 6.25%
        if (dynamic < kOneGB) {
            dynamic = kOneGB;
        }
        if (dynamic > 8 * kOneGB) {
            dynamic = 8 * kOneGB;
        }
        return dynamic;
    }
    return 4 * kOneGB;
}

size_t MemoryManager::TouchMemory(void* ptr, size_t size) {
    if (!ptr || size == 0) {
        return 0;
    }

    volatile char* bytePtr = static_cast<volatile char*>(ptr);
    constexpr size_t pageSize = 4096;
    size_t touched = 0;
    for (size_t offset = 0; offset < size; offset += pageSize) {
        if ((offset & 0xFFFF) == 0 && m_stopRequested.load()) {
            break;
        }
        bytePtr[offset] = static_cast<char>((offset / pageSize) & 0xFF);
        touched = offset + pageSize;
    }
    return touched;
}

bool MemoryManager::StartSacrifice() {
    std::lock_guard<std::mutex> lock(m_apiMutex);
    if (m_isSacrificing.load()) {
        return false;
    }
    if (m_workerThread.joinable()) {
        m_workerThread.join();
    }

    MEMORYSTATUSEX memStatus{};
    memStatus.dwLength = sizeof(memStatus);
    if (GlobalMemoryStatusEx(&memStatus)) {
        m_startingAvailPhysBytes.store(memStatus.ullAvailPhys);
    } else {
        m_startingAvailPhysBytes.store(8 * kOneGB);
    }

    m_isAccepted.store(false);
    ClearMessages();
    m_stopRequested.store(false);
    m_isSacrificing.store(true);
    m_workerThread = std::thread(&MemoryManager::WorkerLoop, this);
    return true;
}

void MemoryManager::StopSacrifice() {
    // Signal only. Joining here would freeze the UI for the rest of the current chunk.
    // ReclaimAll, StartSacrifice, and the destructor join the worker before freeing memory.
    m_stopRequested.store(true);
    m_isSacrificing.store(false);
}

void MemoryManager::ReclaimAll() {
    std::lock_guard<std::mutex> lock(m_apiMutex);
    m_stopRequested.store(true);
    if (m_workerThread.joinable()) {
        m_workerThread.join();
    }
    m_isSacrificing.store(false);
    ReleaseAllChunksInternal();
    m_isAccepted.store(false);
    ClearMessages();
}

void MemoryManager::RequestEmergencyReclaim() {
    // Wake the worker out of a long page-touch before the join inside ReclaimAll.
    m_stopRequested.store(true);
    ReclaimAll();
}

void MemoryManager::ReleaseAllChunksInternal() {
    std::lock_guard<std::mutex> lock(m_chunksMutex);
    for (void* ptr : m_allocatedChunks) {
        if (ptr) {
            VirtualFree(ptr, 0, MEM_RELEASE);
        }
    }
    m_allocatedChunks.clear();
    m_appAllocatedBytes.store(0);
}

void MemoryManager::WorkerLoop() {
    while (!m_stopRequested.load()) {
        MEMORYSTATUSEX memStatus{};
        memStatus.dwLength = sizeof(memStatus);
        if (!GlobalMemoryStatusEx(&memStatus)) {
            SetError("Failed to query system memory status.");
            break;
        }

        const uint64_t reserve = ComputeReserve(memStatus);
        if (memStatus.ullAvailPhys <= reserve) {
            SetStatus("Safety reserve reached. Allocation stopped.");
            m_isAccepted.store(true);
            break;
        }

        BOOL lowMemory = FALSE;
        if (m_lowMemoryNotification &&
            QueryMemoryResourceNotification(m_lowMemoryNotification, &lowMemory) &&
            lowMemory) {
            SetStatus("Windows reported memory pressure. Allocation stopped early.");
            m_isAccepted.store(true);
            break;
        }

        if (memStatus.ullAvailPageFile <= kPageFileReserve) {
            SetStatus("Commit charge is too low to continue safely.");
            m_isAccepted.store(true);
            break;
        }

        size_t currentChunk = 64 * 1024 * 1024;
        int sleepMs = 125;
        switch (m_speed.load()) {
            case AllocationSpeed::Slow:
                currentChunk = 32 * 1024 * 1024;
                sleepMs = 250;
                break;
            case AllocationSpeed::Normal:
                currentChunk = 64 * 1024 * 1024;
                sleepMs = 125;
                break;
            case AllocationSpeed::Fast:
                currentChunk = 64 * 1024 * 1024;
                sleepMs = 60;
                break;
        }

        const uint64_t physHeadroom = memStatus.ullAvailPhys - reserve;
        const uint64_t pageHeadroom = memStatus.ullAvailPageFile - kPageFileReserve;
        uint64_t headroom = physHeadroom < pageHeadroom ? physHeadroom : pageHeadroom;
        if (headroom < 4096) {
            SetStatus("Safety reserve reached. Allocation stopped.");
            m_isAccepted.store(true);
            break;
        }
        if (static_cast<uint64_t>(currentChunk) > headroom) {
            currentChunk = static_cast<size_t>(headroom);
        }
        currentChunk &= ~static_cast<size_t>(4095);
        if (currentChunk < 4096) {
            SetStatus("Safety reserve reached. Allocation stopped.");
            m_isAccepted.store(true);
            break;
        }

        const auto started = std::chrono::steady_clock::now();
        void* ptr = VirtualAlloc(nullptr, currentChunk, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (!ptr) {
            const DWORD err = GetLastError();
            if (err == ERROR_COMMITMENT_LIMIT || err == ERROR_NOT_ENOUGH_MEMORY) {
                SetStatus("Windows refused further commitment. Allocation stopped safely.");
                m_isAccepted.store(true);
            } else {
                SetError("VirtualAlloc failed with error " + std::to_string(err) + ".");
            }
            break;
        }

        const size_t touched = TouchMemory(ptr, currentChunk);
        const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - started).count();

        {
            std::lock_guard<std::mutex> lock(m_chunksMutex);
            m_allocatedChunks.push_back(ptr);
            m_appAllocatedBytes.fetch_add(touched);
        }

        if (m_stopRequested.load()) {
            break;
        }

        if (elapsedMs > 750) {
            SetStatus("Allocation slowed sharply, so the sacrifice stopped to keep the system responsive.");
            m_isAccepted.store(true);
            break;
        }

        for (int slept = 0; slept < sleepMs; slept += 10) {
            if (m_stopRequested.load()) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    m_isSacrificing.store(false);
}

MemoryStats MemoryManager::GetStats() {
    MemoryStats stats;
    stats.isSacrificing = m_isSacrificing.load();
    stats.isAccepted = m_isAccepted.load();
    stats.hadError = m_hadError.load();
    stats.speed = m_speed.load();
    stats.safety = m_safetyMode.load();
    stats.appAllocatedBytes = m_appAllocatedBytes.load();
    stats.startingAvailPhysBytes = m_startingAvailPhysBytes.load();

    {
        std::lock_guard<std::mutex> lock(m_stringMutex);
        stats.errorMessage = m_errorMessage;
        stats.statusMessage = m_statusMessage;
    }

    MEMORYSTATUSEX memStatus{};
    memStatus.dwLength = sizeof(memStatus);
    if (GlobalMemoryStatusEx(&memStatus)) {
        stats.totalPhysBytes = memStatus.ullTotalPhys;
        stats.availPhysBytes = memStatus.ullAvailPhys;
        stats.reserveBytes = ComputeReserve(memStatus);
    } else {
        stats.reserveBytes = (m_safetyMode.load() == SafetyMode::Experimental1GB) ? kOneGB : 4 * kOneGB;
    }

    PROCESS_MEMORY_COUNTERS_EX pmc{};
    if (K32GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc), sizeof(pmc))) {
        stats.appWorkingSetBytes = pmc.WorkingSetSize;
    }

    if (stats.startingAvailPhysBytes > stats.reserveBytes) {
        stats.targetAllocBytes = stats.startingAvailPhysBytes - stats.reserveBytes;
    } else {
        stats.targetAllocBytes = 0;
    }

    if (stats.targetAllocBytes > 0) {
        float progress = static_cast<float>(stats.appAllocatedBytes) / static_cast<float>(stats.targetAllocBytes);
        if (progress < 0.0f) {
            progress = 0.0f;
        } else if (progress > 1.0f) {
            progress = 1.0f;
        }
        stats.sacrificeProgress = progress;
    } else {
        stats.sacrificeProgress = 0.0f;
    }

    if (stats.isAccepted) {
        stats.sacrificeProgress = 1.0f;
    }

    return stats;
}
