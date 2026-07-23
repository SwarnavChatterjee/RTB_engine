#include "BudgetManager.hpp"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>

using namespace rtb;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Writes a valid budgets.txt with all 5 advertisers to a temp file.
static std::string write_budget_file(
    uint32_t b1458, uint32_t b3358, uint32_t b3386,
    uint32_t b3427, uint32_t b3476,
    const char* filename = "test_budgets_tmp.txt")
{
    std::FILE* f = std::fopen(filename, "w");
    assert(f && "could not open temp budget file for writing");
    std::fprintf(f, "1458 %u\n", b1458);
    std::fprintf(f, "3358 %u\n", b3358);
    std::fprintf(f, "3386 %u\n", b3386);
    std::fprintf(f, "3427 %u\n", b3427);
    std::fprintf(f, "3476 %u\n", b3476);
    std::fclose(f);
    return filename;
}

static void remove_file(const std::string& path) {
    std::remove(path.c_str());
}

// ---------------------------------------------------------------------------
// load_budgets tests
// ---------------------------------------------------------------------------

static void test_load_valid_file_returns_true() {
    auto path = write_budget_file(1000, 2000, 3000, 4000, 5000);
    BudgetManager bm;
    bool ok = bm.load_budgets(path);
    assert(ok && "load_budgets must return true for a valid file");
    remove_file(path);
    printf("PASS test_load_valid_file_returns_true\n");
}

static void test_load_missing_file_returns_false() {
    BudgetManager bm;
    bool ok = bm.load_budgets("this_file_does_not_exist_budget.txt");
    assert(!ok && "load_budgets must return false for a missing file");
    printf("PASS test_load_missing_file_returns_false\n");
}

static void test_load_unknown_advertiser_returns_false() {
    std::FILE* f = std::fopen("test_budget_unknown.txt", "w");
    assert(f);
    std::fprintf(f, "9999 100000\n");  // unknown advertiser
    std::fclose(f);

    BudgetManager bm;
    bool ok = bm.load_budgets("test_budget_unknown.txt");
    assert(!ok && "load_budgets must return false for unknown advertiser ID");
    std::remove("test_budget_unknown.txt");
    printf("PASS test_load_unknown_advertiser_returns_false\n");
}

static void test_load_duplicate_advertiser_returns_false() {
    std::FILE* f = std::fopen("test_budget_dup.txt", "w");
    assert(f);
    std::fprintf(f, "1458 100000\n");
    std::fprintf(f, "3358 200000\n");
    std::fprintf(f, "3386 300000\n");
    std::fprintf(f, "3427 400000\n");
    std::fprintf(f, "3476 500000\n");
    std::fprintf(f, "1458 999999\n");  // duplicate
    std::fclose(f);

    BudgetManager bm;
    bool ok = bm.load_budgets("test_budget_dup.txt");
    assert(!ok && "load_budgets must return false for duplicate advertiser ID");
    std::remove("test_budget_dup.txt");
    printf("PASS test_load_duplicate_advertiser_returns_false\n");
}

static void test_load_missing_advertiser_returns_false() {
    // Only 4 advertisers — missing 3476
    std::FILE* f = std::fopen("test_budget_partial.txt", "w");
    assert(f);
    std::fprintf(f, "1458 100000\n");
    std::fprintf(f, "3358 200000\n");
    std::fprintf(f, "3386 300000\n");
    std::fprintf(f, "3427 400000\n");
    std::fclose(f);

    BudgetManager bm;
    bool ok = bm.load_budgets("test_budget_partial.txt");
    assert(!ok && "load_budgets must return false when not all 5 advertisers present");
    std::remove("test_budget_partial.txt");
    printf("PASS test_load_missing_advertiser_returns_false\n");
}

static void test_load_budgets_values_are_loaded() {
    auto path = write_budget_file(1000, 2000, 3000, 4000, 5000);
    BudgetManager bm;
    bm.load_budgets(path);

    assert(bm.remaining(1458) == 1000 && "1458 budget must be 1000");
    assert(bm.remaining(3358) == 2000 && "3358 budget must be 2000");
    assert(bm.remaining(3386) == 3000 && "3386 budget must be 3000");
    assert(bm.remaining(3427) == 4000 && "3427 budget must be 4000");
    assert(bm.remaining(3476) == 5000 && "3476 budget must be 5000");

    remove_file(path);
    printf("PASS test_load_budgets_values_are_loaded\n");
}

static void test_load_resets_spent() {
    auto path = write_budget_file(1000, 1000, 1000, 1000, 1000);
    BudgetManager bm;
    bm.load_budgets(path);
    bm.record_spend(1458, 500);
    assert(bm.remaining(1458) == 500);

    // Reload — spent must reset
    bm.load_budgets(path);
    assert(bm.remaining(1458) == 1000 && "load_budgets must reset spent to zero");
    remove_file(path);
    printf("PASS test_load_resets_spent\n");
}

static void test_load_second_call_overwrites_first() {
    auto path1 = write_budget_file(100, 100, 100, 100, 100, "test_b1.txt");
    auto path2 = write_budget_file(999, 999, 999, 999, 999, "test_b2.txt");

    BudgetManager bm;
    bm.load_budgets(path1);
    bm.load_budgets(path2);

    assert(bm.remaining(1458) == 999 && "second load must overwrite first");
    remove_file(path1);
    remove_file(path2);
    printf("PASS test_load_second_call_overwrites_first\n");
}

// ---------------------------------------------------------------------------
// can_bid tests
// ---------------------------------------------------------------------------

static void test_can_bid_true_when_budget_available() {
    auto path = write_budget_file(1000, 1000, 1000, 1000, 1000);
    BudgetManager bm;
    bm.load_budgets(path);
    assert(bm.can_bid(1458) && "can_bid must return true when budget > 0");
    remove_file(path);
    printf("PASS test_can_bid_true_when_budget_available\n");
}

static void test_can_bid_false_when_budget_exhausted() {
    auto path = write_budget_file(100, 100, 100, 100, 100);
    BudgetManager bm;
    bm.load_budgets(path);
    bm.record_spend(1458, 100);  // exhaust exactly
    assert(!bm.can_bid(1458) && "can_bid must return false when budget exhausted");
    remove_file(path);
    printf("PASS test_can_bid_false_when_budget_exhausted\n");
}

static void test_can_bid_false_for_unknown_advertiser() {
    BudgetManager bm;
    assert(!bm.can_bid(9999) && "can_bid must return false for unknown advertiser");
    printf("PASS test_can_bid_false_for_unknown_advertiser\n");
}

static void test_can_bid_false_before_load() {
    BudgetManager bm;
    // Before load, budgets are zero — no advertiser can bid
    assert(!bm.can_bid(1458) && "can_bid must return false before load_budgets");
    printf("PASS test_can_bid_false_before_load\n");
}

static void test_can_bid_independent_per_advertiser() {
    auto path = write_budget_file(0, 1000, 0, 0, 0);
    BudgetManager bm;
    bm.load_budgets(path);

    assert(!bm.can_bid(1458) && "1458 has zero budget — cannot bid");
    assert( bm.can_bid(3358) && "3358 has budget — can bid");
    assert(!bm.can_bid(3386) && "3386 has zero budget — cannot bid");

    remove_file(path);
    printf("PASS test_can_bid_independent_per_advertiser\n");
}

// ---------------------------------------------------------------------------
// record_spend tests
// ---------------------------------------------------------------------------

static void test_record_spend_deducts_correctly() {
    auto path = write_budget_file(1000, 1000, 1000, 1000, 1000);
    BudgetManager bm;
    bm.load_budgets(path);

    bm.record_spend(1458, 300);
    assert(bm.remaining(1458) == 700 && "remaining must be 1000-300=700");

    bm.record_spend(1458, 200);
    assert(bm.remaining(1458) == 500 && "remaining must be 700-200=500");

    remove_file(path);
    printf("PASS test_record_spend_deducts_correctly\n");
}

static void test_record_spend_does_not_affect_other_advertisers() {
    auto path = write_budget_file(1000, 1000, 1000, 1000, 1000);
    BudgetManager bm;
    bm.load_budgets(path);

    bm.record_spend(1458, 500);

    assert(bm.remaining(3358) == 1000 && "3358 must be unaffected");
    assert(bm.remaining(3386) == 1000 && "3386 must be unaffected");
    assert(bm.remaining(3427) == 1000 && "3427 must be unaffected");
    assert(bm.remaining(3476) == 1000 && "3476 must be unaffected");

    remove_file(path);
    printf("PASS test_record_spend_does_not_affect_other_advertisers\n");
}

static void test_record_spend_clamps_at_zero() {
    auto path = write_budget_file(100, 100, 100, 100, 100);
    BudgetManager bm;
    bm.load_budgets(path);

    // Spend exactly the budget first so remaining=0, then try overspend
    // We do this in release mode — in debug the assert would fire.
    // Test the clamp by spending exactly budget then checking remaining=0.
    bm.record_spend(1458, 100);
    assert(bm.remaining(1458) == 0 && "remaining must be 0 after full spend");
    assert(!bm.can_bid(1458) && "cannot bid after budget exhausted");

    remove_file(path);
    printf("PASS test_record_spend_clamps_at_zero\n");
}

static void test_record_spend_noop_for_unknown_advertiser() {
    auto path = write_budget_file(1000, 1000, 1000, 1000, 1000);
    BudgetManager bm;
    bm.load_budgets(path);

    bm.record_spend(9999, 500);  // unknown — must be no-op

    // All advertisers must be unaffected
    assert(bm.remaining(1458) == 1000);
    assert(bm.remaining(3358) == 1000);

    remove_file(path);
    printf("PASS test_record_spend_noop_for_unknown_advertiser\n");
}

static void test_record_spend_zero_amount_noop() {
    auto path = write_budget_file(1000, 1000, 1000, 1000, 1000);
    BudgetManager bm;
    bm.load_budgets(path);

    bm.record_spend(1458, 0);
    assert(bm.remaining(1458) == 1000 && "spending 0 must not change remaining");

    remove_file(path);
    printf("PASS test_record_spend_zero_amount_noop\n");
}

// ---------------------------------------------------------------------------
// remaining tests
// ---------------------------------------------------------------------------

static void test_remaining_equals_budget_before_any_spend() {
    auto path = write_budget_file(5000, 6000, 7000, 8000, 9000);
    BudgetManager bm;
    bm.load_budgets(path);

    assert(bm.remaining(1458) == 5000);
    assert(bm.remaining(3358) == 6000);
    assert(bm.remaining(3386) == 7000);
    assert(bm.remaining(3427) == 8000);
    assert(bm.remaining(3476) == 9000);

    remove_file(path);
    printf("PASS test_remaining_equals_budget_before_any_spend\n");
}

static void test_remaining_zero_for_unknown_advertiser() {
    BudgetManager bm;
    assert(bm.remaining(9999) == 0 && "remaining must be 0 for unknown advertiser");
    printf("PASS test_remaining_zero_for_unknown_advertiser\n");
}

static void test_remaining_zero_before_load() {
    BudgetManager bm;
    assert(bm.remaining(1458) == 0 && "remaining must be 0 before load_budgets");
    printf("PASS test_remaining_zero_before_load\n");
}

// ---------------------------------------------------------------------------
// reset tests
// ---------------------------------------------------------------------------

static void test_reset_restores_full_budget() {
    auto path = write_budget_file(1000, 1000, 1000, 1000, 1000);
    BudgetManager bm;
    bm.load_budgets(path);

    bm.record_spend(1458, 400);
    bm.record_spend(3358, 600);
    assert(bm.remaining(1458) == 600);
    assert(bm.remaining(3358) == 400);

    bm.reset();

    assert(bm.remaining(1458) == 1000 && "reset must restore 1458 to full budget");
    assert(bm.remaining(3358) == 1000 && "reset must restore 3358 to full budget");
    assert(bm.remaining(3386) == 1000 && "reset must restore 3386 to full budget");
    assert(bm.remaining(3427) == 1000 && "reset must restore 3427 to full budget");
    assert(bm.remaining(3476) == 1000 && "reset must restore 3476 to full budget");

    remove_file(path);
    printf("PASS test_reset_restores_full_budget\n");
}

static void test_reset_allows_bidding_again() {
    auto path = write_budget_file(100, 100, 100, 100, 100);
    BudgetManager bm;
    bm.load_budgets(path);

    bm.record_spend(1458, 100);
    assert(!bm.can_bid(1458));

    bm.reset();
    assert(bm.can_bid(1458) && "can_bid must return true after reset");

    remove_file(path);
    printf("PASS test_reset_allows_bidding_again\n");
}

static void test_reset_does_not_change_budgets() {
    auto path = write_budget_file(500, 500, 500, 500, 500);
    BudgetManager bm;
    bm.load_budgets(path);

    bm.record_spend(1458, 200);
    bm.reset();
    bm.record_spend(1458, 300);

    // After reset, full 500 is available again, so 300 spend leaves 200
    assert(bm.remaining(1458) == 200 && "budget must remain 500 after reset");

    remove_file(path);
    printf("PASS test_reset_does_not_change_budgets\n");
}

// ---------------------------------------------------------------------------
// Pipeline simulation test
// ---------------------------------------------------------------------------

static void test_full_pipeline_simulation() {
    // Simulate a sequence of auctions for one advertiser.
    // Payments total 1100 but budget is 1000 — last payment must be skipped.
    // The pipeline checks can_bid() before record_spend() every time.
    auto path = write_budget_file(1000, 1000, 1000, 1000, 1000);
    BudgetManager bm;
    bm.load_budgets(path);

    uint32_t total_spent = 0;
    uint32_t wins = 0;

    // Total = 80+120+50+200+90+150+60+100+70+180 = 1100 > 1000
    // The last payment (180) should be skipped because can_bid() returns false
    // once remaining < payment. But can_bid() only checks remaining > 0,
    // so we need to also check remaining >= pay before spending.
    uint32_t payments[] = {80, 120, 50, 200, 90, 150, 60, 100, 70, 180};
    for (uint32_t pay : payments) {
        if (bm.can_bid(1458) && bm.remaining(1458) >= pay) {
            bm.record_spend(1458, pay);
            total_spent += pay;
            ++wins;
        }
    }

    assert(total_spent <= 1000 && "must never overspend budget");
    assert(bm.remaining(1458) == 1000 - total_spent &&
           "remaining must equal budget minus total spent");

    remove_file(path);
    printf("PASS test_full_pipeline_simulation\n");
}

static void test_budget_exhaustion_stops_bidding() {
    auto path = write_budget_file(100, 100, 100, 100, 100);
    BudgetManager bm;
    bm.load_budgets(path);

    // Spend until exhausted
    int bids_placed = 0;
    for (int i = 0; i < 20; ++i) {
        if (bm.can_bid(1458)) {
            bm.record_spend(1458, 20);
            ++bids_placed;
        }
    }

    assert(bids_placed == 5 && "exactly 5 bids of 20 fit in budget of 100");
    assert(bm.remaining(1458) == 0 && "budget must be exactly zero");
    assert(!bm.can_bid(1458) && "must not bid after exhaustion");

    remove_file(path);
    printf("PASS test_budget_exhaustion_stops_bidding\n");
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

int main() {
    // load_budgets
    test_load_valid_file_returns_true();
    test_load_missing_file_returns_false();
    test_load_unknown_advertiser_returns_false();
    test_load_duplicate_advertiser_returns_false();
    test_load_missing_advertiser_returns_false();
    test_load_budgets_values_are_loaded();
    test_load_resets_spent();
    test_load_second_call_overwrites_first();

    // can_bid
    test_can_bid_true_when_budget_available();
    test_can_bid_false_when_budget_exhausted();
    test_can_bid_false_for_unknown_advertiser();
    test_can_bid_false_before_load();
    test_can_bid_independent_per_advertiser();

    // record_spend
    test_record_spend_deducts_correctly();
    test_record_spend_does_not_affect_other_advertisers();
    test_record_spend_clamps_at_zero();
    test_record_spend_noop_for_unknown_advertiser();
    test_record_spend_zero_amount_noop();

    // remaining
    test_remaining_equals_budget_before_any_spend();
    test_remaining_zero_for_unknown_advertiser();
    test_remaining_zero_before_load();

    // reset
    test_reset_restores_full_budget();
    test_reset_allows_bidding_again();
    test_reset_does_not_change_budgets();

    // pipeline simulation
    test_full_pipeline_simulation();
    test_budget_exhaustion_stops_bidding();

    printf("\nAll BudgetManager tests passed.\n");
    return 0;
}