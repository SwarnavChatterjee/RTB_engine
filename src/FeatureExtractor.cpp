#include "FeatureExtractor.hpp"

#include <cctype>
#include <cstdint>
#include <ctime>
#include <string>

namespace rtb {

namespace {

// Placeholder for the dataset-wide average floor price. v1 uses a
// hardcoded stand-in (see CLAUDE.md Section 7) instead of a value computed
// from real training data. Replace this once the offline Python pipeline
// exists and can compute the real mean from days 06-10.
constexpr double kGlobalMeanFloorPlaceholder = 50.0;  // TODO: not yet verified

// FNV-1a 32-bit hash.
// Standard recipe: offset_basis=2166136261, prime=16777619.
// Each byte of input is XOR'd into the accumulator, then multiplied by the prime.
uint32_t fnv1a_hash(std::string_view data) {
    uint32_t hash = 2166136261u;
    for (unsigned char c : data) {
        hash ^= c;
        hash *= 16777619u;
    }
    return hash;
}

// Hashes any string_view into a bucket in [0, kHashSize).
// Used for domain, url, visitor_id, AND the visibility/format categoricals.
uint32_t hash_to_bucket(std::string_view data) {
    return fnv1a_hash(data) % kHashSize;
}

// Case-insensitive check for "mobile" anywhere in user_agent.
// Lowercases both strings before searching so "Mobile", "MOBILE", "mobile" all match.
bool is_mobile_user_agent(std::string_view user_agent) {
    // Build a lowercase copy of the haystack.
    std::string lower;
    lower.reserve(user_agent.size());
    for (unsigned char c : user_agent) {
        lower += static_cast<char>(std::tolower(c));
    }
    return lower.find("mobile") != std::string::npos;
}

} // namespace

FeatureVector extract_features(const BidRequest& req) {
    FeatureVector v{};

    // -----------------------------------------------------------------------
    // kHourOfDay (index 0)
    // Timestamp format: yyyyMMddHHmmssSSS (17 digits, stored as uint64_t).
    // The HH digits sit at position 8-9 from the left.
    // Dividing by 10_000_000 strips the mmssSSS portion; modulo 100 isolates HH.
    // Example: 20130606000004490 / 10000000 = 2013060600 → % 100 = 0  (midnight)
    // -----------------------------------------------------------------------
    v[kHourOfDay] = static_cast<double>((req.timestamp / 10'000'000ULL) % 100);

    // -----------------------------------------------------------------------
    // kDayOfWeek (index 1)
    // We need the calendar weekday from the yyyyMMdd prefix.
    // Strategy: slice year/month/day out of the timestamp, fill a std::tm,
    // call mktime() to let the C library normalise and fill tm_wday (0=Sun…6=Sat).
    // -----------------------------------------------------------------------
    {
        // Strip the HHmmssSSS suffix (9 digits) to get the yyyyMMdd integer.
        uint64_t date_part = req.timestamp / 1'000'000'000ULL;
        int year  = static_cast<int>(date_part / 10'000);       // leading 4 digits
        int month = static_cast<int>((date_part / 100) % 100);  // digits 5-6
        int day   = static_cast<int>(date_part % 100);          // digits 7-8

        std::tm t{};
        t.tm_year = year - 1900;  // tm_year is years since 1900
        t.tm_mon  = month - 1;    // tm_mon is 0-based
        t.tm_mday = day;
        t.tm_hour = 0;
        t.tm_min  = 0;
        t.tm_sec  = 0;
        t.tm_isdst = -1;          // let mktime figure out DST

        std::mktime(&t);          // fills t.tm_wday as a side effect
        v[kDayOfWeek] = static_cast<double>((t.tm_wday + 6) % 7);  // 0=Sun, 6=Sat
    }

    // -----------------------------------------------------------------------
    // kSlotArea (index 2)
    // Raw pixel area of the ad slot.
    // -----------------------------------------------------------------------
    v[kSlotArea] = static_cast<double>(req.adslot_width) *
                   static_cast<double>(req.adslot_height);

    // -----------------------------------------------------------------------
    // kVisibilityHash (index 3), kFormatHash (index 4)
    // These are numeric codes (uint32). We can't pass the raw number straight
    // to hash_to_bucket (which takes string_view), so we stringify it first.
    // Using a "vis:" / "fmt:" prefix avoids collisions between the two spaces
    // should visibility and format ever share the same code value.
    // -----------------------------------------------------------------------
    {
        std::string key = "vis:" + std::to_string(req.adslot_visibility_code);
        v[kVisibilityHash] = static_cast<double>(hash_to_bucket(key));
    }
    {
        std::string key = "fmt:" + std::to_string(req.adslot_format_code);
        v[kFormatHash] = static_cast<double>(hash_to_bucket(key));
    }

    // -----------------------------------------------------------------------
    // kDomainHash (index 5), kUrlHash (index 6), kVisitorHash (index 7)
    // Straightforward: hash the raw string_view field into [0, kHashSize).
    // -----------------------------------------------------------------------
    v[kDomainHash]  = static_cast<double>(hash_to_bucket(req.domain));
    v[kUrlHash]     = static_cast<double>(hash_to_bucket(req.url));
    v[kVisitorHash] = static_cast<double>(hash_to_bucket(req.visitor_id));

    // -----------------------------------------------------------------------
    // kFloorRatio (index 8)
    // Normalises the slot's floor price against the dataset-wide mean.
    // kGlobalMeanFloorPlaceholder is 50.0 for now (see CLAUDE.md Section 7).
    // -----------------------------------------------------------------------
    v[kFloorRatio] = static_cast<double>(req.adslot_floor_price) /
                     kGlobalMeanFloorPlaceholder;

    // -----------------------------------------------------------------------
    // kIsMobile (index 9)
    // 1.0 if user-agent contains "mobile" (case-insensitive), 0.0 otherwise.
    // -----------------------------------------------------------------------
    v[kIsMobile] = is_mobile_user_agent(req.user_agent) ? 1.0 : 0.0;

    // -----------------------------------------------------------------------
    // kExchangeId (10), kAdvertiserId (11), kRegion (12), kCity (13)
    // Raw numeric passthrough for now (see CLAUDE.md open question — may need
    // hashing like visibility/format once that's investigated).
    // -----------------------------------------------------------------------
    v[kExchangeId]    = static_cast<double>(req.adexchange);
    v[kAdvertiserId]  = static_cast<double>(req.advertiser_id);
    v[kRegion]        = static_cast<double>(req.region);
    v[kCity]          = static_cast<double>(req.city);

    return v;
}

} // namespace rtb