#pragma once

#include <cstdint>
#include <string_view>

namespace rtb {

// ---------------------------------------------------------------------------
// BudgetManager
//
// Tracks per-advertiser spend against a daily budget loaded from a
// Python-generated config file (models/budgets.txt).
//
// All amounts are in fen (1/100 CNY) — integer arithmetic throughout,
// no floating point error accumulation across millions of auctions.
//
// Internal storage: flat arrays of 5 slots (one per advertiser), mapped
// by static index. O(1) for every operation. Linear scan over 5 elements
// beats a hash map at this scale — fits in one cache line.
//
// Thread safety: NONE. Single-threaded use only for v1.
// If v2 adds concurrent request handling, add mutex around record_spend()
// and can_bid(), or switch spent_ to std::atomic<uint32_t>.
//
// Daily reset: call reset() between evaluation days. Resets spent amounts
// to zero without reloading budgets.
// ---------------------------------------------------------------------------
class BudgetManager {
public:
    BudgetManager() = default;

    // Non-copyable — budget state should have one owner.
    BudgetManager(const BudgetManager&)            = delete;
    BudgetManager& operator=(const BudgetManager&) = delete;

    // Movable.
    BudgetManager(BudgetManager&&)            = default;
    BudgetManager& operator=(BudgetManager&&) = default;

    // -----------------------------------------------------------------------
    // load_budgets
    //
    // Reads a text file produced by the Python training pipeline.
    // Format: one "advertiser_id budget_fen\n" pair per line.
    // Units: fen (1/100 CNY) — must match the dataset's price unit.
    //
    // Strict loading: fails if:
    //   - File not found or unreadable
    //   - Any unknown advertiser ID
    //   - Any advertiser ID appears more than once (duplicate = bug)
    //   - Not all 5 known advertisers are present
    //
    // Resets all spent amounts to zero on successful load.
    // Safe to call multiple times — second call overwrites first.
    // -----------------------------------------------------------------------
    [[nodiscard]] bool load_budgets(std::string_view path);

    // -----------------------------------------------------------------------
    // can_bid
    //
    // Returns true if the advertiser has remaining budget > 0.
    // Called in the pipeline BEFORE BidCalculator — reject-early principle.
    // Note: called before bid amount is known, so cannot check
    // remaining >= bid_amount here. That check is handled by record_spend's
    // clamp after BidCalculator runs.
    // Returns false for unknown advertiser IDs (safe default: don't bid).
    // -----------------------------------------------------------------------
    [[nodiscard]] bool can_bid(uint32_t advertiser_id) const;

    // -----------------------------------------------------------------------
    // record_spend
    //
    // Deducts amount_fen from the advertiser's remaining budget.
    // Called when we WIN an auction and pay the market price.
    //
    // In debug builds: asserts that amount_fen <= remaining budget.
    // In release builds: clamps silently — safety net for rounding errors.
    // Correct pipeline usage (can_bid checked first) should never trigger
    // the clamp, but it protects against bugs silently in production.
    //
    // No-op for unknown advertiser IDs.
    // Units: fen.
    // -----------------------------------------------------------------------
    void record_spend(uint32_t advertiser_id, uint32_t amount_fen);

    // -----------------------------------------------------------------------
    // remaining
    //
    // Returns remaining budget in fen for the given advertiser.
    // Returns 0 for unknown advertiser IDs.
    // Used for logging, diagnostics, and v2 pacing.
    // Units: fen.
    // -----------------------------------------------------------------------
    [[nodiscard]] uint32_t remaining(uint32_t advertiser_id) const;

    // -----------------------------------------------------------------------
    // reset
    //
    // Resets all spent amounts to zero without reloading budgets.
    // Use between evaluation days in multi-day runs.
    // -----------------------------------------------------------------------
    void reset();

private:
    // The 5 known advertiser IDs in canonical order.
    // Index in this array = index into budgets_/spent_ arrays.
    static constexpr uint32_t kAdvertiserIds[5] = {
        1458, 3358, 3386, 3427, 3476
    };
    static constexpr size_t kNumAdvertisers = 5;

    // Returns index [0,4] for a given advertiser_id.
    // Returns kNumAdvertisers (5) if not found — callers treat as invalid.
    [[nodiscard]] static size_t index_of(uint32_t advertiser_id);

    // Initial daily budget per advertiser (fen). Set by load_budgets().
    uint32_t budgets_[kNumAdvertisers]{};

    // Total spent today per advertiser (fen). Reset by reset().
    uint32_t spent_[kNumAdvertisers]{};
};

} // namespace rtb