#include "ArenaAllocator.hpp"

#include <cassert>
#include <cstdint>
#include <cstdio>

using namespace rtb;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static bool is_aligned(void* ptr, size_t alignment) {
    return reinterpret_cast<size_t>(ptr) % alignment == 0;
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

static void test_basic_allocation() {
    ArenaAllocator arena(256);

    void* p = arena.allocate(8, 1);
    assert(p != nullptr && "allocation returned null");
    assert(arena.used() >= 8 && "used() should be at least 8 after allocating 8 bytes");

    printf("PASS test_basic_allocation\n");
}

static void test_used_starts_at_zero() {
    ArenaAllocator arena(256);
    assert(arena.used() == 0 && "fresh arena should report 0 bytes used");

    printf("PASS test_used_starts_at_zero\n");
}

static void test_alignment_single_byte() {
    // alignment=1 means no padding ever — used() should equal size exactly
    ArenaAllocator arena(256);
    arena.allocate(1, 1);
    assert(arena.used() == 1 && "1-byte alloc with align=1 should use exactly 1 byte");

    printf("PASS test_alignment_single_byte\n");
}

static void test_alignment_double() {
    // Allocate 1 byte first to misalign the cursor, then allocate a double.
    // The arena must pad to 8-byte alignment before handing out the double.
    ArenaAllocator arena(256);
    arena.allocate(1, 1);               // cursor now at offset 1 — misaligned for double
    void* p = arena.allocate(8, 8);     // needs 8-byte alignment
    assert(is_aligned(p, 8) && "double allocation must be 8-byte aligned");

    printf("PASS test_alignment_double\n");
}

static void test_alignment_uint32() {
    ArenaAllocator arena(256);
    void* p = arena.allocate(4, 4);
    assert(is_aligned(p, 4) && "uint32 allocation must be 4-byte aligned");

    printf("PASS test_alignment_uint32\n");
}

static void test_multiple_allocations_no_overlap() {
    ArenaAllocator arena(256);

    void* a = arena.allocate(8, 8);
    void* b = arena.allocate(8, 8);

    // b must start at or after the end of a
    uintptr_t end_of_a = reinterpret_cast<uintptr_t>(a) + 8;
    assert(reinterpret_cast<uintptr_t>(b) >= end_of_a && "allocations must not overlap");

    printf("PASS test_multiple_allocations_no_overlap\n");
}

static void test_reset_resets_used() {
    ArenaAllocator arena(256);
    arena.allocate(64, 1);
    assert(arena.used() > 0);

    arena.reset();
    assert(arena.used() == 0 && "used() must be 0 after reset");

    printf("PASS test_reset_resets_used\n");
}

static void test_reset_allows_reallocation() {
    ArenaAllocator arena(256);

    arena.allocate(200, 1);
    arena.reset();

    // After reset, the full slab is available again
    void* p = arena.allocate(200, 1);
    assert(p != nullptr && "should be able to allocate 200 bytes again after reset");

    printf("PASS test_reset_allows_reallocation\n");
}

static void test_reset_same_address() {
    // After reset, the first allocation should return the same base address
    // as before the reset (cursor went back to start).
    ArenaAllocator arena(256);

    void* first = arena.allocate(1, 1);
    arena.reset();
    void* after_reset = arena.allocate(1, 1);

    assert(first == after_reset && "first alloc after reset should return same address as before");

    printf("PASS test_reset_same_address\n");
}

static void test_exact_capacity_fills() {
    // Allocating exactly capacity bytes with alignment=1 should succeed.
    ArenaAllocator arena(64);
    void* p = arena.allocate(64, 1);
    assert(p != nullptr && "exact-capacity allocation should succeed");
    assert(arena.used() == 64);

    printf("PASS test_exact_capacity_fills\n");
}

static void test_used_increases_monotonically() {
    ArenaAllocator arena(256);

    size_t prev = arena.used();
    for (int i = 0; i < 5; ++i) {
        arena.allocate(8, 8);
        size_t curr = arena.used();
        assert(curr > prev && "used() must strictly increase with each allocation");
        prev = curr;
    }

    printf("PASS test_used_increases_monotonically\n");
}

static void test_written_value_survives() {
    // Write a known value into allocated memory, read it back — verifies
    // the returned pointer is genuinely usable memory.
    ArenaAllocator arena(256);

    double* d = static_cast<double*>(arena.allocate(sizeof(double), alignof(double)));
    *d = 3.14159;
    assert(*d == 3.14159 && "value written into arena memory must survive a read-back");

    printf("PASS test_written_value_survives\n");
}

static void test_two_values_independent() {
    // Two separate allocations must not alias each other.
    ArenaAllocator arena(256);

    double* a = static_cast<double*>(arena.allocate(sizeof(double), alignof(double)));
    double* b = static_cast<double*>(arena.allocate(sizeof(double), alignof(double)));

    *a = 1.0;
    *b = 2.0;

    assert(*a == 1.0 && "first allocation must not be clobbered by second");
    assert(*b == 2.0 && "second allocation must hold its own value");

    printf("PASS test_two_values_independent\n");
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

int main() {
    test_used_starts_at_zero();
    test_basic_allocation();
    test_alignment_single_byte();
    test_alignment_double();
    test_alignment_uint32();
    test_multiple_allocations_no_overlap();
    test_reset_resets_used();
    test_reset_allows_reallocation();
    test_reset_same_address();
    test_exact_capacity_fills();
    test_used_increases_monotonically();
    test_written_value_survives();
    test_two_values_independent();

    printf("\nAll ArenaAllocator tests passed.\n");
    return 0;
}