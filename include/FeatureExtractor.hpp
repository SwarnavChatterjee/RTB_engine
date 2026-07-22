#pragma once

#include <array>
#include <cstdint>

#include "BidRequest.hpp"

namespace rtb {

// ---------------------------------------------------------------------------
// Hash space layout
//
// kHashSize: total number of buckets available.
// kReservedBuckets: the first 32 buckets are reserved exclusively for fixed
//   numeric and binary features. Hashed categorical features are offset to
//   start at kReservedBuckets, guaranteeing they can never collide with
//   fixed numeric ones.
//
// Categoricals can still collide with each other — that is expected and
// acceptable in feature hashing.
// ---------------------------------------------------------------------------
constexpr uint32_t kHashSize        = 1u << 18;  // 262144 total buckets
constexpr uint32_t kReservedBuckets = 32;         // buckets 0–31 reserved

// Reserved bucket IDs — grouped as an enum because they are one conceptual
// unit. Add future numeric/binary features here (e.g. kBidPriceBucket).
// Never assign a value >= kReservedBuckets.
enum ReservedBucket : uint32_t {
    kSlotAreaBucket   = 0,
    kFloorRatioBucket = 1,
    kIsMobileBucket   = 2,
    // 3–31 available for future numeric/binary features
};


// ---------------------------------------------------------------------------
// Feature
//
// A single (bucket, value) pair. Every extracted feature — whether numeric,
// binary, or hashed categorical — is represented this way.
//
// predict() uses one uniform rule for all features:
//   score += w[feature.bucket] * feature.value
//
// Numeric/binary features:
//   bucket = fixed reserved ID from ReservedBucket enum
//   value  = the actual numeric value (log area, ratio, 0.0/1.0)
//
// Hashed categorical features:
//   bucket = kReservedBuckets + (fnv1a_hash(key) % (kHashSize - kReservedBuckets))
//   value  = 1.0
// ---------------------------------------------------------------------------
struct Feature {
    uint32_t bucket{};
    double   value{};
};

// ---------------------------------------------------------------------------
// FeatureIndex
//
// Names the 14 positions in a FeatureVector for human readability only.
// These are array indices (extraction order), NOT bucket IDs.
// The model uses Feature::bucket to index into the weight table during
// inference; the array index has no semantic meaning during inference.
//
// Example:
//   v[kDomainHash].bucket  → the actual weight-table bucket for this domain
//   v[kDomainHash].value   → 1.0 (categorical)
//   v[kSlotArea].bucket    → kSlotAreaBucket (reserved)
//   v[kSlotArea].value     → log(width × height)
// ---------------------------------------------------------------------------
enum FeatureIndex : size_t {
    kHourOfDay      = 0,
    kDayOfWeek      = 1,
    kSlotArea       = 2,
    kVisibilityHash = 3,
    kFormatHash     = 4,
    kDomainHash     = 5,
    kUrlHash        = 6,
    kVisitorHash    = 7,
    kFloorRatio     = 8,
    kIsMobile       = 9,
    kExchangeId     = 10,
    kAdvertiserId   = 11,
    kRegion         = 12,
    kCity           = 13,
    
};

constexpr size_t kFeatureCount = 14;

using FeatureVector = std::array<Feature, kFeatureCount>;

// ---------------------------------------------------------------------------
// extract_features
//
// Pure transformation: takes one BidRequest, returns its 14-element
// FeatureVector. Does not look at anything outside the given request
// except the hardcoded placeholder constant for floor_ratio (see .cpp).
//
// No failure mode — every BidRequest that successfully parsed can always
// produce a feature vector.
// ---------------------------------------------------------------------------
FeatureVector extract_features(const BidRequest& req);

} // namespace rtb