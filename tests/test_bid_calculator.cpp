// tests/test_bid_calculator.cpp
#include "BidCalculator.hpp"
#include <cstdio>
#include <cmath>

static int g_pass = 0;
static int g_fail = 0;

#define CHECK(cond, msg) \
    do { \
        if (cond) { g_pass++; } \
        else { g_fail++; std::printf("FAIL: %s (line %d)\n", msg, __LINE__); } \
    } while (0)

static bool nearly_equal(double a, double b, double eps = 1e-9) {
    return std::fabs(a - b) < eps;
}

int main() {
    // --- Basic formula correctness ---
    // ctr=0.5, multiplier=2.0 -> 0.5 * 2.0 * 1000 = 1000.0
    CHECK(nearly_equal(BidCalculator::compute_bid(0.5, 2.0), 1000.0),
          "basic formula: ctr=0.5, mult=2.0");

    // ctr=0.0 -> bid should be exactly 0, regardless of multiplier
    CHECK(nearly_equal(BidCalculator::compute_bid(0.0, 5.0), 0.0),
          "zero ctr produces zero bid");

    // multiplier=0.0 -> bid should be exactly 0, regardless of ctr
    CHECK(nearly_equal(BidCalculator::compute_bid(0.9, 0.0), 0.0),
          "zero multiplier produces zero bid");

    // ctr=1.0 (max valid pCTR), multiplier=1.0 -> bid = kBidScaleFactor exactly
    CHECK(nearly_equal(BidCalculator::compute_bid(1.0, 1.0),
                        BidCalculator::kBidScaleFactor),
          "ctr=1.0, mult=1.0 equals scale factor exactly");

    // --- Scale factor sanity ---
    CHECK(nearly_equal(BidCalculator::kBidScaleFactor, 1000.0),
          "kBidScaleFactor is 1000.0 as documented");

    // --- Small/realistic pCTR values (this is what real FTRL output looks like —
    // most predicted CTRs in RTB are small, e.g. 0.001-0.05, not 0.5) ---
    // ctr=0.002, mult=3.0 -> 0.002 * 3.0 * 1000 = 6.0
    CHECK(nearly_equal(BidCalculator::compute_bid(0.002, 3.0), 6.0),
          "realistic small ctr: 0.002, mult=3.0");

    // --- Monotonicity: higher ctr should never produce a lower bid
    //     (same multiplier) ---
    double bid_low_ctr  = BidCalculator::compute_bid(0.01, 2.0);
    double bid_high_ctr = BidCalculator::compute_bid(0.05, 2.0);
    CHECK(bid_high_ctr > bid_low_ctr,
          "monotonic in ctr: higher ctr -> higher bid");

    // --- Monotonicity: higher multiplier should never produce a lower bid
    //     (same ctr) ---
    double bid_low_mult  = BidCalculator::compute_bid(0.03, 1.0);
    double bid_high_mult = BidCalculator::compute_bid(0.03, 4.0);
    CHECK(bid_high_mult > bid_low_mult,
          "monotonic in multiplier: higher multiplier -> higher bid");

    if(g_fail == 0) {
        std::printf("\nAll %d tests passed .\n", g_pass);
    } else {
        std::printf("\n%d passed, %d failed\n", g_pass, g_fail);
    }
    return g_fail == 0 ? 0 : 1;
}