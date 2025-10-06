#include "MemoryManager.h"
#include <iostream>
#include <unordered_map>
#include <mutex>

namespace ScotlandYard {
namespace Memory {

struct MemoryStats {
    size_t totalAllocated = 0;
    size_t allocationCount = 0;
};

static std::unordered_map<MemoryTag, MemoryStats> s_map_MemoryStats;
static std::mutex s_mtx_Memory;
static bool s_b_Initialized = false;

void MemoryManager::Initialize() {
    std::lock_guard<std::mutex> lock(s_mtx_Memory);

    if (s_b_Initialized) {
        return;
    }

    s_map_MemoryStats[MemoryTag::GENERAL] = MemoryStats{};
    s_map_MemoryStats[MemoryTag::GRAPHICS] = MemoryStats{};
    s_map_MemoryStats[MemoryTag::GAME_LOGIC] = MemoryStats{};
    s_map_MemoryStats[MemoryTag::AI] = MemoryStats{};
    s_map_MemoryStats[MemoryTag::AUDIO] = MemoryStats{};
    s_map_MemoryStats[MemoryTag::NETWORK] = MemoryStats{};
    s_map_MemoryStats[MemoryTag::TEMPORARY] = MemoryStats{};

    s_b_Initialized = true;
}

void MemoryManager::Shutdown() {
    std::lock_guard<std::mutex> lock(s_mtx_Memory);

    if (!s_b_Initialized) {
        return;
    }

    PrintStatistics();

    s_map_MemoryStats.clear();
    s_b_Initialized = false;
}

void* MemoryManager::Allocate(size_t size, MemoryTag tag) {
    void* ptr = ::operator new(size);

    {
        std::lock_guard<std::mutex> lock(s_mtx_Memory);
        auto& stats = s_map_MemoryStats[tag];
        stats.totalAllocated += size;
        stats.allocationCount++;
    }

    return ptr;
}

void MemoryManager::Free(void* ptr, MemoryTag tag) {
    if (!ptr) return;

    ::operator delete(ptr);

    {
        std::lock_guard<std::mutex> lock(s_mtx_Memory);
        auto& stats = s_map_MemoryStats[tag];
        stats.allocationCount--;
    }
}

size_t MemoryManager::GetAllocatedMemory(MemoryTag tag) {
    std::lock_guard<std::mutex> lock(s_mtx_Memory);
    return s_map_MemoryStats[tag].totalAllocated;
}

void MemoryManager::PrintStatistics() {
    const char* tagNames[] = {
        "GENERAL",
        "GRAPHICS",
        "GAME_LOGIC",
        "AI",
        "AUDIO",
        "NETWORK",
        "TEMPORARY"
    };

    size_t totalMemory = 0;

    std::cout << "\n=== Memory Statistics ===" << std::endl;
    for (const auto& [tag, stats] : s_map_MemoryStats) {
        int tagIndex = static_cast<int>(tag);
        std::cout << tagNames[tagIndex] << ": "
                  << stats.totalAllocated / 1024.0f << " KB ("
                  << stats.allocationCount << " allocations)" << std::endl;
        totalMemory += stats.totalAllocated;
    }
    std::cout << "TOTAL: " << totalMemory / 1024.0f << " KB" << std::endl;
    std::cout << "=========================" << std::endl;
}

} // namespace Memory
} // namespace ScotlandYard
