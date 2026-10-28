#pragma once
// Memory Allocation Interceptor for Empirical Stress-Testing
// Allows measuring heap allocations strictly during scoped critical sections.

#include <cstddef>
#include <cstdlib>
#include <new>
#include <atomic>

namespace ghostrf_stress {

inline std::atomic<bool>   g_track_allocations{false};
inline std::atomic<size_t> g_allocation_count{0};
inline std::atomic<size_t> g_bytes_allocated{0};
inline std::atomic<size_t> g_deallocation_count{0};

class ScopedHeapMonitor {
private:
    bool _wasTracking;
    size_t _startCount;
    size_t _startBytes;
public:
    ScopedHeapMonitor() {
        _startCount = g_allocation_count.load();
        _startBytes = g_bytes_allocated.load();
        _wasTracking = g_track_allocations.exchange(true);
    }

    ~ScopedHeapMonitor() {
        if (!_wasTracking) {
            g_track_allocations.store(false);
        }
    }

    size_t getDeltaAllocations() const {
        return g_allocation_count.load() - _startCount;
    }

    size_t getDeltaBytes() const {
        return g_bytes_allocated.load() - _startBytes;
    }

    static void reset() {
        g_allocation_count.store(0);
        g_bytes_allocated.store(0);
        g_deallocation_count.store(0);
    }
};

} // namespace ghostrf_stress

// Global operator new/delete overrides
inline void* allocate_tracked(size_t size) {
    if (ghostrf_stress::g_track_allocations.load()) {
        ghostrf_stress::g_allocation_count.fetch_add(1, std::memory_order_relaxed);
        ghostrf_stress::g_bytes_allocated.fetch_add(size, std::memory_order_relaxed);
    }
    void* p = std::malloc(size);
    if (!p) throw std::bad_alloc();
    return p;
}

inline void deallocate_tracked(void* p) noexcept {
    if (ghostrf_stress::g_track_allocations.load()) {
        ghostrf_stress::g_deallocation_count.fetch_add(1, std::memory_order_relaxed);
    }
    std::free(p);
}

inline void* operator new(size_t size) {
    return allocate_tracked(size);
}

inline void* operator new[](size_t size) {
    return allocate_tracked(size);
}

inline void operator delete(void* p) noexcept {
    deallocate_tracked(p);
}

inline void operator delete[](void* p) noexcept {
    deallocate_tracked(p);
}

inline void operator delete(void* p, size_t) noexcept {
    deallocate_tracked(p);
}

inline void operator delete[](void* p, size_t) noexcept {
    deallocate_tracked(p);
}
