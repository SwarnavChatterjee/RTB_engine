#include "BiddingEngine.hpp"

#include <cstdio>
#include <cstring>
#include <string>

#include "BidCalculator.hpp"
#include "BidRequest.hpp"
#include "FeatureExtractor.hpp"
#include "FloorGate.hpp"
#include "FTRLModel.hpp"

BiddingEngine::BiddingEngine() = default;

bool BiddingEngine::load_model(std::string_view weights_path) {
    return model_.load_weights(weights_path);
}

bool BiddingEngine::load_budgets(std::string_view budgets_path) {
    return budget_.load_budgets(budgets_path);
}

// ---------------------------------------------------------------------------
// multiplier_for
//
// Linear scan over 5 known advertisers — same pattern as BudgetManager.
// Returns a higher multiplier for advertisers whose scoring N is large,
// reflecting that winning impressions for them is worth more.
// These values are v1 starting points — tune on validation set.
// ---------------------------------------------------------------------------
double BiddingEngine::multiplier_for(uint32_t advertiser_id) {
    for (size_t i = 0; i < 5; ++i) {
        if (kAdvertiserIds[i] == advertiser_id)
            return kMultipliers[i];
    }
    return kDefaultMultiplier;
}

// ---------------------------------------------------------------------------
// get_bid_price
//
// Full per-request pipeline:
//   1. Parse the bid request line
//   2. Extract features
//   3. Predict CTR
//   4. Compute candidate bid
//   5. Check floor price
//   6. Check budget
//   7. Return bid price or -1
//
// Returns -1 on any rejection or parse failure.
// Does NOT call record_spend() — the evaluator only tells us if we won
// after the fact. In a real system, the win notification triggers
// record_spend(). For offline evaluation, we simulate this separately.
// ---------------------------------------------------------------------------
int32_t BiddingEngine::get_bid_price(std::string_view line) {
    // Step 1: Parse
    auto req_opt = rtb::parse_bid_request(line);
    if (!req_opt) return -1;
    const rtb::BidRequest& req = *req_opt;

    // Step 2: Extract features
    rtb::FeatureVector features = rtb::extract_features(req);

    // Step 3: Predict CTR
    double predicted_ctr = model_.predict(features);

    // Step 4: Compute candidate bid
    double multiplier    = multiplier_for(req.advertiser_id);
    double candidate_bid = BidCalculator::compute_bid(predicted_ctr, multiplier);

    // Step 5: Floor gate — does our bid clear the exchange minimum?
    if (!FloorGate::passes(candidate_bid, req.adslot_floor_price))
        return -1;

    // Step 6: Budget gate — does this advertiser still have budget?
    if (!budget_.can_bid(req.advertiser_id))
        return -1;

    // Step 7: Return bid price as integer in fen
    // Cast to uint32_t first to match the dataset's integer price units,
    // then return as int32_t so -1 fits in the same return type.
    return static_cast<int32_t>(static_cast<uint32_t>(candidate_bid));
}

// ---------------------------------------------------------------------------
// run
//
// Reads one bid request per line from stdin, writes one integer per line
// to stdout. Continues until EOF.
//
// Format contract:
//   Input:  tab-separated bid.txt line
//   Output: integer bid price, or -1 (no bid)
// ---------------------------------------------------------------------------
void BiddingEngine::run() {
    std::string line;
    line.reserve(1024);  // typical bid line is 400-600 bytes

    while (std::getline(std::cin, line)) {
        if (line.empty()) continue;
        int32_t bid = get_bid_price(line);
        std::printf("%d\n", bid);
    }
}