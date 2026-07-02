#pragma once

#include <array>
#include <cstdint>

#include "BidRequest.hpp"

namespace rtb {

// ---------------------------------------------------------------------------
// Feature vector layout (canonical order — see CLAUDE.md Section 7).
// Every index below must mean the SAME thing every time, in both the
// C++ engine and the Python offline trainer. Changing this order without
// updating both sides silently breaks the model.
// ---------------------------------------------------------------------------
constexpr size_t kFeatureCount = 14;
constexpr uint32_t kHashSize = 1u << 18;  // 2^18 = 262144

enum FeatureIndex : size_t {
    kHourOfDay      = 0,
    kDayOfWeek      = 1,
    kSlotArea       = 2,
    kVisibilityHash = 3,  // hashed categorical, NOT raw code (see CLAUDE.md note)
    kFormatHash     = 4,  // hashed categorical, NOT raw code
    kDomainHash     = 5,
    kUrlHash        = 6,
    kVisitorHash    = 7,
    kFloorRatio     = 8,  // uses GLOBAL_MEAN_FLOOR_PLACEHOLDER, see .cpp
    kIsMobile       = 9,  // case-insensitive check
    kExchangeId     = 10,
    kAdvertiserId   = 11,
    kRegion         = 12,
    kCity           = 13,
};

using FeatureVector = std::array<double, kFeatureCount>;

// ---------------------------------------------------------------------------
// extract_features
//
// Pure transformation: takes one BidRequest, returns its 14-element
// feature vector. Does not look at anything outside the given request
// except the hardcoded placeholder constant for floor_ratio (see .cpp).
//
// No failure mode — every BidRequest that successfully parsed can always
// produce a feature vector (unlike parsing, there's no "malformed" case
// here, since all inputs are already validated typed fields).
// ---------------------------------------------------------------------------
FeatureVector extract_features(const BidRequest& req);

} // namespace rtb
