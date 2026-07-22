// include/FloorGate.hpp
#pragma once
#include <cstdint>

// Single responsibility: does this ALREADY-COMPUTED candidate bid clear
// the exchange floor? FloorGate knows nothing about pCTR, multipliers, or
// the bid formula — BidCalculator is the sole source of truth for that
// (see Architecture Decision Record: FloorGate was deliberately moved
// after BidCalculator so the pricing formula exists in exactly one place,
// even though this means the bid is computed for requests that are
// ultimately rejected here).
class FloorGate {
public:
    // candidate_bid: output of BidCalculator::compute_bid(), same units as floor_price
    // floor_price:   BidRequest field 18 (Adslotfloorprice), uint32
    //
    // Returns true if the bid clears the floor (continue to BudgetManager),
    // false if it should be rejected (no bid, -1).
    [[nodiscard]] static bool passes(double candidate_bid, uint32_t floor_price);
};