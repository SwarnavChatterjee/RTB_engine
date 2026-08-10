#pragma once

#include <array>
#include <cstdint>
#include <string_view>

#include "BudgetManager.hpp"
#include "FeatureExtractor.hpp"
#include "FTRLModel.hpp"

// ---------------------------------------------------------------------------
// BiddingEngine
//
// Top-level orchestrator. Owns all per-server state (model, budgets,
// multipliers) and wires the full per-request pipeline:
//
//   parse_bid_request()
//       ↓
//   FeatureExtractor::extract_features()
//       ↓
//   FTRLModel::predict()
//       ↓
//   BidCalculator::compute_bid()
//       ↓
//   FloorGate::passes()
//       ↓
//   BudgetManager::can_bid()
//       ↓
//   BudgetManager::record_spend()  [on win — not here, called externally]
//       ↓
//   output: bid price in fen, or -1 (no bid)
//
// Input:  one tab-separated line from bid.txt (via stdin or file)
// Output: one integer per line — bid price or -1
//
// Startup sequence:
//   1. load_model("models/weights.bin")
//   2. load_budgets("models/budgets.txt")
//   3. run()  — reads stdin line by line until EOF
// ---------------------------------------------------------------------------
class BiddingEngine {
public:
    BiddingEngine();

    // Non-copyable, non-movable — owns large model state.
    BiddingEngine(const BiddingEngine&)            = delete;
    BiddingEngine& operator=(const BiddingEngine&) = delete;
    BiddingEngine(BiddingEngine&&)                 = delete;
    BiddingEngine& operator=(BiddingEngine&&)      = delete;

    // -----------------------------------------------------------------------
    // load_model
    // Loads trained FTRL weights from binary file.
    // Must be called before run().
    // -----------------------------------------------------------------------
    [[nodiscard]] bool load_model(std::string_view weights_path);

    // -----------------------------------------------------------------------
    // load_budgets
    // Loads per-advertiser daily budgets from Python-generated text file.
    // Must be called before run().
    // -----------------------------------------------------------------------
    [[nodiscard]] bool load_budgets(std::string_view budgets_path);

    // -----------------------------------------------------------------------
    // get_bid_price
    //
    // Core per-request method. Takes one raw bid request line, runs the
    // full pipeline, returns:
    //   >= 0  : bid price in fen
    //   -1    : no bid (floor rejected, budget exhausted, or parse failure)
    //
    // This is the method the evaluator calls for each request.
    // Must complete within 5ms per the contest spec.
    // -----------------------------------------------------------------------
    [[nodiscard]] int32_t get_bid_price(std::string_view line);

    // -----------------------------------------------------------------------
    // run
    //
    // Reads bid requests line by line from stdin, calls get_bid_price()
    // for each, writes result to stdout. Runs until EOF.
    // -----------------------------------------------------------------------
    void run();

private:
    // Returns the advertiser multiplier for a given advertiser_id.
    // Informed by N values from the scoring function but tuned separately.
    // Returns a default multiplier for unknown IDs.
    [[nodiscard]] static double multiplier_for(uint32_t advertiser_id);

    // Per-server state — allocated once at startup.
    rtb::FTRLModel    model_;
    rtb::BudgetManager budget_;

    // Per-advertiser multipliers — informed by N scoring weights.
    // Stored as parallel arrays matching kAdvertiserIds order.
    // {1458, 3358, 3386, 3427, 3476}
    static constexpr uint32_t kAdvertiserIds[5] = {1458, 3358, 3386, 3427, 3476};
    static constexpr double   kMultipliers[5]   = {
        1.0,   // 1458: N=0, clicks only
        1.2,   // 3358: N=2, conversions worth 2x
        1.0,   // 3386: N=0, clicks only
        1.0,   // 3427: N=0, clicks only
        1.5,   // 3476: N=10, conversions worth 10x — bid more aggressively
    };
    static constexpr double kDefaultMultiplier = 1.0;
};