#include "ArenaAllocator.hpp"

#include <cstddef>
#include <cassert>

namespace rtb {

ArenaAllocator::ArenaAllocator(size_t capacity)
    : slab_(new char[capacity])
    , capacity_(capacity)
    , offset_(0)
{}

ArenaAllocator::~ArenaAllocator() {
    delete[] slab_;
}

void* ArenaAllocator::allocate(size_t size, size_t alignment) {
    // Step 1: find the next address that satisfies alignment.
    // Current address is slab_ + offset_.
    // We need to round it UP to the nearest multiple of alignment.
    size_t current_addr = reinterpret_cast<size_t>(slab_ + offset_);
    size_t aligned_addr = (current_addr + alignment - 1) & ~(alignment - 1);

    // Step 2: how many padding bytes did alignment require?
    size_t padding = aligned_addr - current_addr;

    // Step 3: check we won't overflow the slab.
    assert(offset_ + padding + size <= capacity_ && "ArenaAllocator overflow");

    // Step 4: advance offset past the padding and the allocation.
    offset_ += padding + size;

    // Step 5: return the aligned address.
    return reinterpret_cast<void*>(aligned_addr);
}

void ArenaAllocator::reset() {
    offset_ = 0;
    // Note: memory is NOT zeroed. Callers must not assume clean memory
    // after reset — every field must be explicitly written before use.
}

size_t ArenaAllocator::used() const {
    return offset_;
}

} // namespace rtb