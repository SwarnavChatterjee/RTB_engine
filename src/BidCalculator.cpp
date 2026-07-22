#include "BidCalculator.hpp"

double BidCalculator::compute_bid(double predicted_ctr,
                                  double advertiser_multiplier) {
    return predicted_ctr * advertiser_multiplier * kBidScaleFactor;
}
// Converts the model's predicted click-through rate (pCTR) into a
// candidate bid price using the project's bidding formula.
//
// This is the single implementation of the pricing equation used by the
// engine. Any future changes to bidding strategy (e.g. calibration,
// pacing multipliers, quality adjustments) should be made here so that
// all downstream components observe the same bid value.