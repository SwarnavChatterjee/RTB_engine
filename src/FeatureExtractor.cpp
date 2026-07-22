#include "FeatureExtractor.hpp"

#include <cctype>
#include <cmath>
#include <cstdint>
#include <ctime>
#include <string>

namespace rtb {

namespace {

constexpr double kGlobalMeanFloorPlaceholder = 50.0;  // TODO: replace once Python pipeline computes real mean from days 06-10

// ---------------------------------------------------------------------------
// FNV-1a 32-bit hash.
// offset_basis=2166136261, prime=16777619.
// Iterates over unsigned char to avoid signed-char overflow bugs.
// ---------------------------------------------------------------------------
uint32_t fnv1a_hash(std::string_view data) {
    uint32_t hash = 2166136261u;
    for (unsigned char c : data) {
        hash ^= c;
        hash *= 16777619u;
    }
    return hash;
}

// ---------------------------------------------------------------------------
// hash_to_bucket
// Maps a string key into [kReservedBuckets, kHashSize).
// The offset ensures hashed categoricals can never land in the reserved
// range [0, kReservedBuckets) which is owned by fixed numeric/binary features.
// ---------------------------------------------------------------------------
uint32_t hash_to_bucket(std::string_view key) {
    return kReservedBuckets + (fnv1a_hash(key) % (kHashSize - kReservedBuckets));
}

// ---------------------------------------------------------------------------
// cat_feature
// Convenience: builds a categorical Feature{bucket, 1.0} from a string key.
// All hashed categoricals have value=1.0 — the weight at that bucket gets
// full credit or none.
// ---------------------------------------------------------------------------
Feature cat_feature(std::string_view key) {
    return {hash_to_bucket(key), 1.0};
}

// ---------------------------------------------------------------------------
// is_mobile_user_agent
// Case-insensitive check for "mobile" anywhere in user_agent.
// ---------------------------------------------------------------------------
bool is_mobile_user_agent(std::string_view user_agent) {
    std::string lower;
    lower.reserve(user_agent.size());
    for (unsigned char c : user_agent)
        lower += static_cast<char>(std::tolower(c));
    return lower.find("mobile") != std::string::npos;
}

} // namespace

FeatureVector extract_features(const BidRequest& req) {
    FeatureVector v{};

    // -----------------------------------------------------------------------
    // kHourOfDay (index 0) — categorical
    // Timestamp format: yyyyMMddHHmmssSSS.
    // (timestamp / 10_000_000) % 100 isolates the HH digits.
    // Bucketed as "hr:N" — hour 14 is not "14x" hour 1, it's a category.
    // -----------------------------------------------------------------------
    {
        uint32_t hour = static_cast<uint32_t>((req.timestamp / 10'000'000ULL) % 100);
        v[kHourOfDay] = cat_feature("hr:" + std::to_string(hour));
    }

    // -----------------------------------------------------------------------
    // kDayOfWeek (index 1) — categorical
    // Sliced from yyyyMMdd prefix via std::tm + mktime.
    // Converted to Python weekday convention: (tm_wday + 6) % 7 → Mon=0, Sun=6.
    // Bucketed as "dow:N" — Monday is not "2x" Sunday, it's a category.
    // NOTE: mktime uses local time, not UTC. See CLAUDE.md for timezone caveat.
    // -----------------------------------------------------------------------
    {
        uint64_t date_part = req.timestamp / 1'000'000'000ULL;
        int year  = static_cast<int>(date_part / 10'000);
        int month = static_cast<int>((date_part / 100) % 100);
        int day   = static_cast<int>(date_part % 100);

        std::tm t{};
        t.tm_year  = year - 1900;
        t.tm_mon   = month - 1;
        t.tm_mday  = day;
        t.tm_isdst = -1;
        std::mktime(&t);

        uint32_t dow = static_cast<uint32_t>((t.tm_wday + 6) % 7);
        v[kDayOfWeek] = cat_feature("dow:" + std::to_string(dow));
    }

    // -----------------------------------------------------------------------
    // kSlotArea (index 2) — numeric
    // log(width x height): log normalises the wide variance in raw pixel area.
    // Uses fixed reserved bucket kSlotAreaBucket — never collides with any
    // hashed categorical.
    // -----------------------------------------------------------------------
    {
        double area = static_cast<double>(req.adslot_width) *
                      static_cast<double>(req.adslot_height);
        v[kSlotArea] = {kSlotAreaBucket, area > 0.0 ? std::log(area) : 0.0};
        //Zero-area slots map to 0.0 instead of log(0) (undefined).
    }

    // -----------------------------------------------------------------------
    // kVisibilityHash (index 3), kFormatHash (index 4) — categorical
    // Numeric codes stringified with prefix to prevent cross-field collisions.
    // -----------------------------------------------------------------------
    v[kVisibilityHash] = cat_feature("vis:" + std::to_string(req.adslot_visibility_code));
    v[kFormatHash]     = cat_feature("fmt:" + std::to_string(req.adslot_format_code));

    // -----------------------------------------------------------------------
    // kDomainHash (index 5), kUrlHash (index 6), kVisitorHash (index 7) — categorical
    // -----------------------------------------------------------------------
    v[kDomainHash]  = cat_feature(req.domain);
    v[kUrlHash]     = cat_feature(req.url);
    v[kVisitorHash] = cat_feature(req.visitor_id);

    // -----------------------------------------------------------------------
    // kFloorRatio (index 8) — numeric
    // floor_price / global_mean. Placeholder mean = 50.0 until Python
    // pipeline computes real mean from training days 06-10.
    // Uses fixed reserved bucket kFloorRatioBucket.
    // -----------------------------------------------------------------------
    v[kFloorRatio] = {
        kFloorRatioBucket,
        static_cast<double>(req.adslot_floor_price) / kGlobalMeanFloorPlaceholder
    };

    // -----------------------------------------------------------------------
    // kIsMobile (index 9) — binary
    // 1.0 if user-agent contains "mobile" (case-insensitive), 0.0 otherwise.
    // Uses fixed reserved bucket kIsMobileBucket.
    // -----------------------------------------------------------------------
    v[kIsMobile] = {kIsMobileBucket, is_mobile_user_agent(req.user_agent) ? 1.0 : 0.0};

    // -----------------------------------------------------------------------
    // kExchangeId (10), kAdvertiserId (11), kRegion (12), kCity (13) — categorical
    // All treated as categories: IDs have no numeric ordinality.
    // exchange 3 is not "3x" exchange 1; advertiser 3476 is not "2.4x" 1458.
    // Prefixed to prevent cross-field collisions.
    // -----------------------------------------------------------------------
    v[kExchangeId]   = cat_feature("exc:" + std::to_string(req.adexchange));
    v[kAdvertiserId] = cat_feature("adv:" + std::to_string(req.advertiser_id));
    v[kRegion]       = cat_feature("reg:" + std::to_string(req.region));
    v[kCity]         = cat_feature("cty:" + std::to_string(req.city));

    return v;
}

} // namespace rtb