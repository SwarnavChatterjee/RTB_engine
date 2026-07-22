// tests/test_floorgate.cpp
#include "FloorGate.hpp"
#include <cstdio>
#include <limits>

static int g_pass = 0;
static int g_fail = 0;

#define CHECK(cond, msg) \
    do { \
        if (cond) { g_pass++; } \
        else { g_fail++; std::printf("FAIL: %s (line %d)\n", msg, __LINE__); } \
    } while (0)

int main() {
    // --- Basic pass / reject ---
    CHECK(FloorGate::passes(100.0, 50) == true,
          "bid clearly above floor -> passes");
    CHECK(FloorGate::passes(30.0, 50) == false,
          "bid clearly below floor -> rejected");

    // --- Boundary: exactly equal to floor ---
    // Locked design decision: >= means an exact match clears the floor.
    // If this decision is ever flipped to strict >, this test is the one
    // that should fail first and flag the change.
    CHECK(FloorGate::passes(50.0, 50) == true,
          "bid exactly equal to floor -> passes (locked >= decision)");

    // --- Zero floor: everything should pass, including a zero bid ---
    CHECK(FloorGate::passes(0.0, 0) == true,
          "zero floor + zero bid -> passes");
    CHECK(FloorGate::passes(10.0, 0) == true,
          "zero floor + any positive bid -> passes");

    // --- Zero bid against a real floor: should reject ---
    CHECK(FloorGate::passes(0.0, 50) == false,
          "zero bid against nonzero floor -> rejected");

    // --- Large floor_price near uint32_t max (precision check) ---
    // Verifies the double-promotion of floor_price doesn't lose precision
    // for realistic (or even extreme) uint32_t values.
    uint32_t large_floor = 4000000000u; // well within uint32_t range
    CHECK(FloorGate::passes(4000000001.0, large_floor) == true,
          "large floor_price: bid just above -> passes");
    CHECK(FloorGate::passes(3999999999.0, large_floor) == false,
          "large floor_price: bid just below -> rejected");

    // --- Fractional bid just below an integer floor ---
    CHECK(FloorGate::passes(49.999999, 50) == false,
          "fractional bid just under integer floor -> rejected");

    if(g_fail == 0) {
        std::printf("\nAll FloorGate tests passed (%d checks).\n", g_pass);
    } else {
        std::printf("\n%d FloorGate tests passed, %d failed.\n", g_pass, g_fail);
    }
    return g_fail == 0 ? 0 : 1;
}