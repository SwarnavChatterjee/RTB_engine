// include/BidCalculator.hpp
#pragma once

// Single responsibility:
// Convert a predicted click-through rate (pCTR) into a candidate bid price.
//
// BidCalculator is the single source of truth for the bidding formula.
// It does not perform floor validation, budget checks, or pacing decisions.
class BidCalculator {
public:
    // Converts the model's expected value into the bid-price units used
    // by the IPinYou dataset (CPM-style pricing).
    static constexpr double kBidScaleFactor = 1000.0;

    // Computes the candidate bid for an impression.
    //
    // predicted_ctr:
    //     Output of FTRLModel::predict(), already passed through sigmoid,
    //     expected to be in the range [0, 1].
    //
    // advertiser_multiplier:
    //     Advertiser-specific bidding coefficient supplied by the
    //     BiddingEngine. This is independent of the evaluation metric N.
    //
    // Returns:
    //     Candidate bid in the same units as exchange floor prices.
    [[nodiscard]]
    static double compute_bid(double predicted_ctr,
                              double advertiser_multiplier);
};