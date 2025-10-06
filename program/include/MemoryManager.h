#ifndef SCOTLANDYARD_MEMORY_MEMORYMANAGER_H
#define SCOTLANDYARD_MEMORY_MEMORYMANAGER_H

#include <memory>
#include <cstddef>

namespace ScotlandYard {
namespace Memory {

enum class MemoryTag {
    GENERAL,
    GRAPHICS,
    GAME_LOGIC,
    AI,
    AUDIO,
    NETWORK,
    TEMPORARY
};

class MemoryManager {
public:
    static void Initialize();
    static void Shutdown();
    static void* Allocate(size_t size, MemoryTag tag = MemoryTag::GENERAL);
    static void Free(void* ptr, MemoryTag tag = MemoryTag::GENERAL);
    static size_t GetAllocatedMemory(MemoryTag tag);
    static void PrintStatistics();

private:
    MemoryManager() = delete;
    ~MemoryManager() = delete;
};

template<typename T, typename... Args>
std::unique_ptr<T> MakeUnique(Args&&... args) {
    return std::make_unique<T>(std::forward<Args>(args)...);
}

template<typename T, typename... Args>
std::shared_ptr<T> MakeShared(Args&&... args) {
    return std::make_shared<T>(std::forward<Args>(args)...);
}

} // namespace Memory
} // namespace ScotlandYard

#endif // SCOTLANDYARD_MEMORY_MEMORYMANAGER_H
