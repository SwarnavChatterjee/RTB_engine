#pragma once

#include <cstddef>
#include <cstdint>
#include <cassert>

namespace rtb {

class ArenaAllocator {
public:
    // Allocates the backing slab on the heap at construction time.
    explicit ArenaAllocator(size_t capacity);

    // Frees the backing slab.
    ~ArenaAllocator();

    // No copying — two arenas sharing the same slab would be a disaster.
    ArenaAllocator(const ArenaAllocator&) = delete;
    ArenaAllocator& operator=(const ArenaAllocator&) = delete;

    // Allocate `size` bytes aligned to `alignment`.
    // Asserts on overflow — see design decision in CLAUDE.md.
    void* allocate(size_t size, size_t alignment = alignof(std::max_align_t));

    // Resets position to start. O(1). Does NOT zero memory.
    void reset();

    // How many bytes have been used since last reset.
    size_t used() const;

private:
    char*  slab_;      // backing memory, heap-allocated once
    size_t capacity_;  // total size of slab in bytes
    size_t offset_;    // current position from start of slab
};

} // namespace rtb