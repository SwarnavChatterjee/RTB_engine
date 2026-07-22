// src/FloorGate.cpp
#include "FloorGate.hpp"

bool FloorGate::passes(double candidate_bid, uint32_t floor_price) {
    return candidate_bid >= static_cast<double>(floor_price);
    //static_cast to double to ensure proper comparison between double and uint32_t and convert floor_price to double for comparison
}
/*  The simple file just tells if a candidates bid satisfies the floor price. It does not know anything about how the bid was computed, it just compares the two values.
    FloorGate assumes that the candidate bid has already been computed by BidCalculator and is in the same units as the floor price. It returns true if the candidate bid 
    is greater than or equal to the floor price, indicating that the bid clears the floor and can proceed to further evaluation (e.g., budget checks). If the candidate bid 
    is less than the floor price, it returns false, indicating that the bid should be rejected.
*/   