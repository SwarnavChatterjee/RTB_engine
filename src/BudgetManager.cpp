#include "BudgetManager.hpp"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <string>

namespace rtb {

// ---------------------------------------------------------------------------
// index_of — const, pure lookup, no state modification.
// Linear scan over 5 elements — faster than hash map at this scale.
// Fits in one cache line; branch predictor learns the pattern quickly.
// ---------------------------------------------------------------------------
size_t BudgetManager::index_of(uint32_t advertiser_id) {
    for (size_t i = 0; i < kNumAdvertisers; ++i) {
        if (kAdvertiserIds[i] == advertiser_id) return i;
    }
    return kNumAdvertisers;  // sentinel: not found
}

// ---------------------------------------------------------------------------
// load_budgets
//
// Strict loading:
//   - Unknown advertiser ID → fail
//   - Duplicate advertiser ID → fail
//   - Missing advertisers (not all 5 present) → fail
//
// Uses std::fill instead of memset — type-safe, identical codegen.
// ---------------------------------------------------------------------------
bool BudgetManager::load_budgets(std::string_view path) {
    std::string path_str(path);
    std::FILE* f = std::fopen(path_str.c_str(), "r");
    if (!f) return false;

    // Zero out before loading.
    std::fill(std::begin(budgets_), std::end(budgets_), 0u);
    std::fill(std::begin(spent_),   std::end(spent_),   0u);

    // Track which advertisers have been loaded to detect duplicates
    // and verify all 5 are present.
    bool seen[kNumAdvertisers]{};

    uint32_t adv_id, budget;
    while (std::fscanf(f, "%u %u", &adv_id, &budget) == 2) {
        size_t idx = index_of(adv_id);

        // Unknown advertiser ID — reject entire file.
        if (idx == kNumAdvertisers) {
            std::fclose(f);
            std::fill(std::begin(budgets_), std::end(budgets_), 0u);
            return false;
        }

        // Duplicate advertiser ID — reject entire file.
        if (seen[idx]) {
            std::fclose(f);
            std::fill(std::begin(budgets_), std::end(budgets_), 0u);
            return false;
        }

        budgets_[idx] = budget;
        seen[idx]     = true;
    }

    std::fclose(f);

    // Strict: all 5 advertisers must be present.
    for (size_t i = 0; i < kNumAdvertisers; ++i) {
        if (!seen[i]) {
            std::fill(std::begin(budgets_), std::end(budgets_), 0u);
            return false;
        }
    }

    return true;
}

// ---------------------------------------------------------------------------
// can_bid
// ---------------------------------------------------------------------------
bool BudgetManager::can_bid(uint32_t advertiser_id) const {
    size_t idx = index_of(advertiser_id);
    if (idx == kNumAdvertisers) return false;
    return remaining(advertiser_id) > 0;
}

// ---------------------------------------------------------------------------
// record_spend
//
// Uses remaining() internally — no duplicated budget-spent calculation.
// Debug assert catches pipeline bugs early.
// Release clamp prevents underflow in production.
// ---------------------------------------------------------------------------
void BudgetManager::record_spend(uint32_t advertiser_id, uint32_t amount_fen) {
    size_t idx = index_of(advertiser_id);
    if (idx == kNumAdvertisers) return;

    uint32_t rem = remaining(advertiser_id);

    // Debug: catch incorrect pipeline usage (spending without checking budget).
    assert(amount_fen <= rem &&
           "record_spend called with amount exceeding remaining budget — "
           "check can_bid() before calling record_spend()");

    // Release: clamp silently — safety net for rounding errors in production.
    spent_[idx] += (amount_fen <= rem) ? amount_fen : rem;
}

// ---------------------------------------------------------------------------
// remaining
//
// Single source of truth for budget-spent arithmetic.
// Used internally by can_bid() and record_spend() to avoid duplication.
// ---------------------------------------------------------------------------
uint32_t BudgetManager::remaining(uint32_t advertiser_id) const {
    size_t idx = index_of(advertiser_id);
    if (idx == kNumAdvertisers) return 0;
    return budgets_[idx] - spent_[idx];
}

// ---------------------------------------------------------------------------
// reset
// ---------------------------------------------------------------------------
void BudgetManager::reset() {
    std::fill(std::begin(spent_), std::end(spent_), 0u);
}

} // namespace rtb