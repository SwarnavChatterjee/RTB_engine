// tests/test_feature_extractor.cpp
#include "FeatureExtractor.hpp"
#include "BidRequest.hpp"

#include <cstdio>
#include <cmath>
#include <string>
#include <ctime>

using namespace rtb;

static int g_pass = 0;
static int g_fail = 0;

#define CHECK(cond, msg) \
    do { \
        if (cond) { g_pass++; } \
        else { g_fail++; std::printf("FAIL: %s (line %d)\n", msg, __LINE__); } \
    } while (0)

static bool nearly_equal(double a, double b, double eps = 1e-9) {
    return std::fabs(a - b) < eps;
}

// ---------------------------------------------------------------------------
// Independent re-implementation of the hashing logic, duplicated deliberately
// from FeatureExtractor.cpp. This is NOT testing "a hash happened" — it
// verifies the EXACT bucket for a given key, so a silent change to the
// FNV-1a constants or the reserved-bucket offset would be caught here even
// if it still "looked like a hash" to a looser test.
// ---------------------------------------------------------------------------
static uint32_t ref_fnv1a_hash(std::string_view data) {
    uint32_t hash = 2166136261u;
    for (unsigned char c : data) {
        hash ^= c;
        hash *= 16777619u;
    }
    return hash;
}

static uint32_t ref_hash_to_bucket(std::string_view key) {
    return kReservedBuckets + (ref_fnv1a_hash(key) % (kHashSize - kReservedBuckets));
}

// Builds a minimal-but-valid BidRequest with sane defaults, so each test
// only needs to override the one or two fields it cares about.
static BidRequest make_request() {
    BidRequest req{};
    req.timestamp              = 20130601143000123ULL; // yyyyMMddHHmmssSSS
    req.adslot_width            = 300;
    req.adslot_height           = 250;
    req.adslot_visibility_code  = 1;
    req.adslot_format_code      = 0;
    req.domain                  = "example-domain-hash";
    req.url                     = "example-url-hash";
    req.visitor_id              = "example-visitor-hash";
    req.adslot_floor_price      = 100;
    req.user_agent              = "Mozilla/5.0 (Windows NT 10.0)";
    req.adexchange              = 2;
    req.advertiser_id           = 3358;
    req.region                  = 79;
    req.city                    = 80;
    return req;
}

int main() {
    // =========================================================================
    // Structural invariants — these should hold for ANY valid request, not
    // just the specific values below. Catches design regressions, not just
    // formula bugs.
    // =========================================================================
    {
        BidRequest req = make_request();
        FeatureVector v = extract_features(req);

        CHECK(v.size() == kFeatureCount, "feature vector has exactly 14 entries");

        // Reserved-bucket features must use their fixed IDs, never a hashed one.
        CHECK(v[kSlotArea].bucket   == kSlotAreaBucket,   "slot area uses reserved bucket");
        CHECK(v[kFloorRatio].bucket == kFloorRatioBucket, "floor ratio uses reserved bucket");
        CHECK(v[kIsMobile].bucket   == kIsMobileBucket,   "is_mobile uses reserved bucket");

        // Every hashed categorical must land OUTSIDE the reserved range —
        // this is the actual enforcement of the "no collision with numeric
        // features" guarantee described in the header comment, not just a
        // claim in a comment.
        size_t hashed_indices[] = {
            kHourOfDay, kDayOfWeek, kVisibilityHash, kFormatHash,
            kDomainHash, kUrlHash, kVisitorHash,
            kExchangeId, kAdvertiserId, kRegion, kCity
        };
        bool all_in_range = true;
        for (size_t idx : hashed_indices) {
            if (v[idx].bucket < kReservedBuckets || v[idx].bucket >= kHashSize) {
                all_in_range = false;
            }
        }
        CHECK(all_in_range, "all hashed categorical buckets fall within [kReservedBuckets, kHashSize)");

        // All categorical features must carry value == 1.0 exactly (per the
        // documented design: "the weight at that bucket gets full credit or none").
        bool all_cat_values_one = true;
        for (size_t idx : hashed_indices) {
            if (!nearly_equal(v[idx].value, 1.0)) all_cat_values_one = false;
        }
        CHECK(all_cat_values_one, "all hashed categorical features have value == 1.0");
    }

    // =========================================================================
    // kHourOfDay — exact value + exact bucket
    // =========================================================================
    {
        BidRequest req = make_request();
        req.timestamp = 20130601143000123ULL; // HH = 14
        FeatureVector v = extract_features(req);
        uint32_t expected_bucket = ref_hash_to_bucket("hr:14");
        CHECK(v[kHourOfDay].bucket == expected_bucket, "hour_of_day: HH=14 hashes to expected bucket");
        CHECK(nearly_equal(v[kHourOfDay].value, 1.0), "hour_of_day: value is 1.0");
    }
    {
        BidRequest req = make_request();
        req.timestamp = 20130601003000123ULL; // HH = 00, midnight edge case
        FeatureVector v = extract_features(req);
        uint32_t expected_bucket = ref_hash_to_bucket("hr:0");
        CHECK(v[kHourOfDay].bucket == expected_bucket, "hour_of_day: midnight (HH=00) hashes correctly");
    }

    // =========================================================================
    // kDayOfWeek — NOTE: mktime uses LOCAL time, a documented, accepted
    // limitation (see CLAUDE.md). These three dates are the canary: if all
    // three fail together while everything else passes, the test machine's
    // timezone is not UTC+8 and the mktime caveat is a live issue, not just
    // theoretical. 2013-06-03 was a Monday (UTC+8 reference).
    // =========================================================================
    {
        BidRequest req = make_request();
        req.timestamp = 20130603120000000ULL; // 2013-06-03 = Monday
        FeatureVector v = extract_features(req);
        uint32_t expected_bucket = ref_hash_to_bucket("dow:0"); // Mon=0
        CHECK(v[kDayOfWeek].bucket == expected_bucket,
              "day_of_week: Monday maps to dow:0 (timezone canary #1)");
    }
    {
        BidRequest req = make_request();
        req.timestamp = 20130606120000000ULL; // 2013-06-06 = Thursday
        FeatureVector v = extract_features(req);
        uint32_t expected_bucket = ref_hash_to_bucket("dow:3"); // Thu=3
        CHECK(v[kDayOfWeek].bucket == expected_bucket,
              "day_of_week: Thursday maps to dow:3 (timezone canary #2)");
    }
    {
        BidRequest req = make_request();
        req.timestamp = 20130609120000000ULL; // 2013-06-09 = Sunday
        FeatureVector v = extract_features(req);
        uint32_t expected_bucket = ref_hash_to_bucket("dow:6"); // Sun=6
        CHECK(v[kDayOfWeek].bucket == expected_bucket,
              "day_of_week: Sunday maps to dow:6 (timezone canary #3)");
    }

    // =========================================================================
    // kSlotArea — numeric, log(width*height), reserved bucket
    // =========================================================================
    {
        BidRequest req = make_request();
        req.adslot_width  = 300;
        req.adslot_height = 250;
        FeatureVector v = extract_features(req);
        double expected = std::log(300.0 * 250.0);
        CHECK(nearly_equal(v[kSlotArea].value, expected), "slot_area: log(300*250) computed correctly");
    }
    {
        // Zero-area edge case: log(0) is undefined, must map to 0.0, not NaN/-inf.
        BidRequest req = make_request();
        req.adslot_width  = 0;
        req.adslot_height = 250;
        FeatureVector v = extract_features(req);
        CHECK(nearly_equal(v[kSlotArea].value, 0.0), "slot_area: zero width maps to 0.0, not log(0)");
        CHECK(!std::isnan(v[kSlotArea].value) && !std::isinf(v[kSlotArea].value),
              "slot_area: zero-area case is never NaN or Inf");
    }

    // =========================================================================
    // kVisibilityHash / kFormatHash — prefixed to avoid cross-field collision
    // =========================================================================
    {
        BidRequest req = make_request();
        req.adslot_visibility_code = 1;
        req.adslot_format_code     = 1; // SAME numeric value as visibility on purpose
        FeatureVector v = extract_features(req);

        uint32_t expected_vis = ref_hash_to_bucket("vis:1");
        uint32_t expected_fmt = ref_hash_to_bucket("fmt:1");

        CHECK(v[kVisibilityHash].bucket == expected_vis, "visibility_hash: correct bucket for code=1");
        CHECK(v[kFormatHash].bucket == expected_fmt, "format_hash: correct bucket for code=1");
        // The actual point of this test: same raw numeric code (1) for both
        // fields must NOT collide, because of the "vis:"/"fmt:" prefixing.
        CHECK(v[kVisibilityHash].bucket != v[kFormatHash].bucket,
              "visibility code=1 and format code=1 do NOT collide (prefix works)");
    }

    // =========================================================================
    // kDomainHash / kUrlHash / kVisitorHash — plain string hashing
    // =========================================================================
    {
        BidRequest req = make_request();
        req.domain     = "domainA";
        req.url        = "urlA";
        req.visitor_id = "visitorA";
        FeatureVector v = extract_features(req);

        CHECK(v[kDomainHash].bucket  == ref_hash_to_bucket("domainA"),  "domain_hash: correct bucket");
        CHECK(v[kUrlHash].bucket     == ref_hash_to_bucket("urlA"),     "url_hash: correct bucket");
        CHECK(v[kVisitorHash].bucket == ref_hash_to_bucket("visitorA"), "visitor_hash: correct bucket");
    }
    {
        // Two different requests with the same domain string must hash to
        // the SAME bucket — determinism check, not just "does it not crash."
        BidRequest req1 = make_request();
        BidRequest req2 = make_request();
        req1.domain = "shared-domain";
        req2.domain = "shared-domain";
        FeatureVector v1 = extract_features(req1);
        FeatureVector v2 = extract_features(req2);
        CHECK(v1[kDomainHash].bucket == v2[kDomainHash].bucket,
              "domain_hash: deterministic across separate calls with same input");
    }

    // =========================================================================
    // kFloorRatio — floor_price / 50.0 placeholder
    // =========================================================================
    {
        BidRequest req = make_request();
        req.adslot_floor_price = 100;
        FeatureVector v = extract_features(req);
        CHECK(nearly_equal(v[kFloorRatio].value, 100.0 / 50.0), "floor_ratio: 100/50.0 = 2.0");
    }
    {
        BidRequest req = make_request();
        req.adslot_floor_price = 0;
        FeatureVector v = extract_features(req);
        CHECK(nearly_equal(v[kFloorRatio].value, 0.0), "floor_ratio: zero floor price -> 0.0");
    }

    // =========================================================================
    // kIsMobile — case-insensitive substring check
    // =========================================================================
    {
        BidRequest req = make_request();
        req.user_agent = "Mozilla/5.0 mobile Safari";
        FeatureVector v = extract_features(req);
        CHECK(nearly_equal(v[kIsMobile].value, 1.0), "is_mobile: lowercase 'mobile' detected");
    }
    {
        BidRequest req = make_request();
        req.user_agent = "Mozilla/5.0 Mobile Safari"; // capitalized, as seen in real data
        FeatureVector v = extract_features(req);
        CHECK(nearly_equal(v[kIsMobile].value, 1.0), "is_mobile: capitalized 'Mobile' detected");
    }
    {
        BidRequest req = make_request();
        req.user_agent = "MoBiLe"; // mixed case
        FeatureVector v = extract_features(req);
        CHECK(nearly_equal(v[kIsMobile].value, 1.0), "is_mobile: mixed-case 'MoBiLe' detected");
    }
    {
        BidRequest req = make_request();
        req.user_agent = "Mozilla/5.0 Windows NT Desktop";
        FeatureVector v = extract_features(req);
        CHECK(nearly_equal(v[kIsMobile].value, 0.0), "is_mobile: desktop UA correctly not flagged");
    }
    {
        BidRequest req = make_request();
        req.user_agent = ""; // empty user agent, edge case
        FeatureVector v = extract_features(req);
        CHECK(nearly_equal(v[kIsMobile].value, 0.0), "is_mobile: empty user_agent -> 0.0, no crash");
    }

    // =========================================================================
    // kExchangeId / kAdvertiserId / kRegion / kCity — prefixed hashes
    // =========================================================================
    {
        BidRequest req = make_request();
        req.adexchange    = 2;
        req.advertiser_id = 3358;
        req.region        = 79;
        req.city           = 80;
        FeatureVector v = extract_features(req);

        CHECK(v[kExchangeId].bucket   == ref_hash_to_bucket("exc:2"),    "exchange_id: correct bucket");
        CHECK(v[kAdvertiserId].bucket == ref_hash_to_bucket("adv:3358"), "advertiser_id: correct bucket");
        CHECK(v[kRegion].bucket       == ref_hash_to_bucket("reg:79"),   "region: correct bucket");
        CHECK(v[kCity].bucket         == ref_hash_to_bucket("cty:80"),   "city: correct bucket");
    }
    {
        // Same numeric value across different ID fields must not collide,
        // same reasoning as the visibility/format test above.
        BidRequest req = make_request();
        req.adexchange    = 5;
        req.region        = 5;
        FeatureVector v = extract_features(req);
        CHECK(v[kExchangeId].bucket != v[kRegion].bucket,
              "exchange=5 and region=5 do NOT collide (prefix works across ID fields too)");
    }

    std::printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}