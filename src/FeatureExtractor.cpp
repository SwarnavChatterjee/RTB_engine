#include "FeatureExtractor.hpp"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <ctime>

namespace rtb {

namespace {

constexpr double kGlobalMeanFloorPlaceholder = 50.0;

uint32_t fnv1a_hash(std::string_view data) {
    uint32_t hash = 2166136261u;
    for (unsigned char c : data)
        hash = (hash ^ c) * 16777619u;
    return hash;
}

uint32_t hash_to_bucket(std::string_view key) {
    return kReservedBuckets + (fnv1a_hash(key) % (kHashSize - kReservedBuckets));
}

Feature cat_feature(std::string_view key) {
    return {hash_to_bucket(key), 1.0};
}

// Stack-allocated key helper — no heap allocation.
// snprintf writes "prefix:value" into a 16-byte stack buffer.
// 16 bytes is sufficient: longest prefix is 4 chars ("adv:"),
// longest uint32 is 10 digits → 4+10+null = 15 bytes max.
Feature cat_feature_int(const char* prefix, uint32_t value) {
    char key[16];
    std::snprintf(key, sizeof(key), "%s%u", prefix, value);
    return cat_feature(std::string_view(key, std::strlen(key)));
}

// Case-insensitive "mobile" check using a fixed stack buffer.
// User-agents longer than 512 bytes are treated as non-mobile —
// real UAs are never that long and this avoids any heap allocation.
bool is_mobile_user_agent(std::string_view user_agent) {
    constexpr size_t kMaxUA = 512;
    char lower[kMaxUA];
    size_t len = user_agent.size() < kMaxUA ? user_agent.size() : kMaxUA - 1;
    for (size_t i = 0; i < len; ++i)
        lower[i] = static_cast<char>(std::tolower(
                       static_cast<unsigned char>(user_agent[i])));
    lower[len] = '\0';
    return std::strstr(lower, "mobile") != nullptr;
}

} // namespace

FeatureVector extract_features(const BidRequest& req) {
    FeatureVector v{};

    // kHourOfDay — categorical, stack key
    {
        uint32_t hour = static_cast<uint32_t>((req.timestamp / 10'000'000ULL) % 100);
        v[kHourOfDay] = cat_feature_int("hr:", hour);
    }

    // kDayOfWeek — categorical, stack key
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
        v[kDayOfWeek] = cat_feature_int("dow:", dow);
    }

    // kSlotArea — numeric, log transform
    {
        double area = static_cast<double>(req.adslot_width) *
                      static_cast<double>(req.adslot_height);
        v[kSlotArea] = {kSlotAreaBucket, area > 0.0 ? std::log(area) : 0.0};
    }

    // kVisibilityHash, kFormatHash — categorical, stack keys
    v[kVisibilityHash] = cat_feature_int("vis:", req.adslot_visibility_code);
    v[kFormatHash]     = cat_feature_int("fmt:", req.adslot_format_code);

    // kDomainHash, kUrlHash, kVisitorHash — categorical, raw string_view (no alloc)
    v[kDomainHash]  = cat_feature(req.domain);
    v[kUrlHash]     = cat_feature(req.url);
    v[kVisitorHash] = cat_feature(req.visitor_id);

    // kFloorRatio — numeric
    v[kFloorRatio] = {
        kFloorRatioBucket,
        static_cast<double>(req.adslot_floor_price) / kGlobalMeanFloorPlaceholder
    };

    // kIsMobile — binary, stack buffer
    v[kIsMobile] = {kIsMobileBucket, is_mobile_user_agent(req.user_agent) ? 1.0 : 0.0};

    // kExchangeId, kAdvertiserId, kRegion, kCity — categorical, stack keys
    v[kExchangeId]   = cat_feature_int("exc:", req.adexchange);
    v[kAdvertiserId] = cat_feature_int("adv:", req.advertiser_id);
    v[kRegion]       = cat_feature_int("reg:", req.region);
    v[kCity]         = cat_feature_int("cty:", req.city);

    return v;
}

} // namespace rtb