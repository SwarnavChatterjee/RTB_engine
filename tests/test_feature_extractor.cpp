#include "FeatureExtractor.hpp"
#include "BidRequest.hpp"

#include <cassert>
#include <cstdio>
#include <cmath>

using namespace rtb;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Returns a fully populated BidRequest with sensible defaults so individual
// tests only need to override the fields they care about.
static BidRequest make_request() {
    BidRequest r{};
    r.bid_id                 = "test-bid-001";
    r.timestamp              = 20130606143000000ULL; // 2013-06-06 14:30:00.000 (Thursday)
    r.visitor_id             = "abc123";
    r.user_agent             = "Mozilla/5.0 (Windows NT 6.1)";
    r.ip                     = "180.127.189.*";
    r.region                 = 1;
    r.city                   = 2;
    r.adexchange             = 3;
    r.domain                 = "example.com";
    r.url                    = "http://example.com/page";
    r.anonymous_url_id       = "null";
    r.adslot_id              = "slot-1";
    r.adslot_width           = 300;
    r.adslot_height          = 250;
    r.adslot_visibility_code = 1;
    r.adslot_format_code     = 0;
    r.adslot_floor_price     = 50;
    r.creative_id            = "cr-001";
    r.bidding_price          = 100;
    r.advertiser_id          = 1458;
    r.user_tag               = "10006,10063";
    return r;
}

static bool near(double a, double b, double eps = 1e-9) {
    return std::fabs(a - b) < eps;
}

// ---------------------------------------------------------------------------
// kHourOfDay (index 0)
// ---------------------------------------------------------------------------

static void test_hour_of_day_midday() {
    BidRequest r = make_request();
    r.timestamp = 20130606143000000ULL; // HH = 14
    FeatureVector v = extract_features(r);
    assert(near(v[kHourOfDay], 14.0) && "hour should be 14 for timestamp 14:30");
    printf("PASS test_hour_of_day_midday\n");
}

static void test_hour_of_day_midnight() {
    BidRequest r = make_request();
    r.timestamp = 20130606000004490ULL; // HH = 00
    FeatureVector v = extract_features(r);
    assert(near(v[kHourOfDay], 0.0) && "hour should be 0 at midnight");
    printf("PASS test_hour_of_day_midnight\n");
}

static void test_hour_of_day_last_hour() {
    BidRequest r = make_request();
    r.timestamp = 20130606235959999ULL; // HH = 23
    FeatureVector v = extract_features(r);
    assert(near(v[kHourOfDay], 23.0) && "hour should be 23 at 23:59");
    printf("PASS test_hour_of_day_last_hour\n");
}

// ---------------------------------------------------------------------------
// kDayOfWeek (index 1)
// Python weekday(): Mon=0, Tue=1, Wed=2, Thu=3, Fri=4, Sat=5, Sun=6
// ---------------------------------------------------------------------------

static void test_day_of_week_thursday() {
    BidRequest r = make_request();
    r.timestamp = 20130606143000000ULL; // 2013-06-06 is a Thursday → Python weekday 3
    FeatureVector v = extract_features(r);
    assert(near(v[kDayOfWeek], 3.0) && "2013-06-06 is Thursday, Python weekday=3");
    printf("PASS test_day_of_week_thursday\n");
}

static void test_day_of_week_monday() {
    BidRequest r = make_request();
    r.timestamp = 20130610143000000ULL; // 2013-06-10 is a Monday → Python weekday 0
    FeatureVector v = extract_features(r);
    assert(near(v[kDayOfWeek], 0.0) && "2013-06-10 is Monday, Python weekday=0");
    printf("PASS test_day_of_week_monday\n");
}

static void test_day_of_week_sunday() {
    BidRequest r = make_request();
    r.timestamp = 20130609143000000ULL; // 2013-06-09 is a Sunday → Python weekday 6
    FeatureVector v = extract_features(r);
    assert(near(v[kDayOfWeek], 6.0) && "2013-06-09 is Sunday, Python weekday=6");
    printf("PASS test_day_of_week_sunday\n");
}

// ---------------------------------------------------------------------------
// kSlotArea (index 2)
// ---------------------------------------------------------------------------

static void test_slot_area() {
    BidRequest r = make_request();
    r.adslot_width  = 300;
    r.adslot_height = 250;
    FeatureVector v = extract_features(r);
    assert(near(v[kSlotArea], 75000.0) && "300x250 slot area should be 75000");
    printf("PASS test_slot_area\n");
}

static void test_slot_area_zero_dimension() {
    BidRequest r = make_request();
    r.adslot_width  = 0;
    r.adslot_height = 250;
    FeatureVector v = extract_features(r);
    assert(near(v[kSlotArea], 0.0) && "slot area with zero width should be 0");
    printf("PASS test_slot_area_zero_dimension\n");
}

// ---------------------------------------------------------------------------
// kVisibilityHash / kFormatHash (index 3, 4)
// ---------------------------------------------------------------------------

static void test_visibility_hash_in_range() {
    BidRequest r = make_request();
    r.adslot_visibility_code = 1;
    FeatureVector v = extract_features(r);
    assert(v[kVisibilityHash] >= 0.0 && v[kVisibilityHash] < kHashSize &&
           "visibility hash must be in [0, kHashSize)");
    printf("PASS test_visibility_hash_in_range\n");
}

static void test_format_hash_in_range() {
    BidRequest r = make_request();
    r.adslot_format_code = 5;
    FeatureVector v = extract_features(r);
    assert(v[kFormatHash] >= 0.0 && v[kFormatHash] < kHashSize &&
           "format hash must be in [0, kHashSize)");
    printf("PASS test_format_hash_in_range\n");
}

static void test_visibility_and_format_same_code_differ() {
    // visibility=1 and format=1 must hash to DIFFERENT buckets due to "vis:"/"fmt:" prefix
    BidRequest r = make_request();
    r.adslot_visibility_code = 1;
    r.adslot_format_code     = 1;
    FeatureVector v = extract_features(r);
    assert(v[kVisibilityHash] != v[kFormatHash] &&
           "visibility and format with same code must not collide — prefix missing?");
    printf("PASS test_visibility_and_format_same_code_differ\n");
}

static void test_visibility_different_codes_differ() {
    BidRequest r1 = make_request(); r1.adslot_visibility_code = 0;
    BidRequest r2 = make_request(); r2.adslot_visibility_code = 1;
    FeatureVector v1 = extract_features(r1);
    FeatureVector v2 = extract_features(r2);
    assert(v1[kVisibilityHash] != v2[kVisibilityHash] &&
           "different visibility codes must produce different hash buckets");
    printf("PASS test_visibility_different_codes_differ\n");
}

// ---------------------------------------------------------------------------
// kDomainHash / kUrlHash / kVisitorHash (index 5, 6, 7)
// ---------------------------------------------------------------------------

static void test_domain_hash_in_range() {
    BidRequest r = make_request();
    FeatureVector v = extract_features(r);
    assert(v[kDomainHash] >= 0.0 && v[kDomainHash] < kHashSize &&
           "domain hash must be in [0, kHashSize)");
    printf("PASS test_domain_hash_in_range\n");
}

static void test_url_hash_in_range() {
    BidRequest r = make_request();
    FeatureVector v = extract_features(r);
    assert(v[kUrlHash] >= 0.0 && v[kUrlHash] < kHashSize &&
           "url hash must be in [0, kHashSize)");
    printf("PASS test_url_hash_in_range\n");
}

static void test_visitor_hash_in_range() {
    BidRequest r = make_request();
    FeatureVector v = extract_features(r);
    assert(v[kVisitorHash] >= 0.0 && v[kVisitorHash] < kHashSize &&
           "visitor hash must be in [0, kHashSize)");
    printf("PASS test_visitor_hash_in_range\n");
}

static void test_different_domains_differ() {
    BidRequest r1 = make_request(); r1.domain = "example.com";
    BidRequest r2 = make_request(); r2.domain = "other.com";
    FeatureVector v1 = extract_features(r1);
    FeatureVector v2 = extract_features(r2);
    assert(v1[kDomainHash] != v2[kDomainHash] &&
           "different domains must produce different hash buckets");
    printf("PASS test_different_domains_differ\n");
}

static void test_same_domain_same_hash() {
    BidRequest r1 = make_request();
    BidRequest r2 = make_request();
    FeatureVector v1 = extract_features(r1);
    FeatureVector v2 = extract_features(r2);
    assert(near(v1[kDomainHash], v2[kDomainHash]) &&
           "same domain must always produce same hash");
    printf("PASS test_same_domain_same_hash\n");
}

// ---------------------------------------------------------------------------
// kFloorRatio (index 8)
// ---------------------------------------------------------------------------

static void test_floor_ratio_at_mean() {
    BidRequest r = make_request();
    r.adslot_floor_price = 50; // equal to placeholder mean of 50.0
    FeatureVector v = extract_features(r);
    assert(near(v[kFloorRatio], 1.0) && "floor price == mean should give ratio 1.0");
    printf("PASS test_floor_ratio_at_mean\n");
}

static void test_floor_ratio_zero() {
    BidRequest r = make_request();
    r.adslot_floor_price = 0;
    FeatureVector v = extract_features(r);
    assert(near(v[kFloorRatio], 0.0) && "zero floor price should give ratio 0.0");
    printf("PASS test_floor_ratio_zero\n");
}

static void test_floor_ratio_double_mean() {
    BidRequest r = make_request();
    r.adslot_floor_price = 100; // 2x the placeholder mean
    FeatureVector v = extract_features(r);
    assert(near(v[kFloorRatio], 2.0) && "floor price == 2x mean should give ratio 2.0");
    printf("PASS test_floor_ratio_double_mean\n");
}

// ---------------------------------------------------------------------------
// kIsMobile (index 9)
// ---------------------------------------------------------------------------

static void test_is_mobile_lowercase() {
    BidRequest r = make_request();
    r.user_agent = "Mozilla/5.0 (mobile; Android)";
    FeatureVector v = extract_features(r);
    assert(near(v[kIsMobile], 1.0) && "lowercase 'mobile' in UA should give 1.0");
    printf("PASS test_is_mobile_lowercase\n");
}

static void test_is_mobile_capitalized() {
    BidRequest r = make_request();
    r.user_agent = "Mozilla/5.0 (Mobile; iPhone)";
    FeatureVector v = extract_features(r);
    assert(near(v[kIsMobile], 1.0) && "capitalized 'Mobile' in UA should give 1.0");
    printf("PASS test_is_mobile_capitalized\n");
}

static void test_is_mobile_uppercase() {
    BidRequest r = make_request();
    r.user_agent = "MOBILE-BROWSER/1.0";
    FeatureVector v = extract_features(r);
    assert(near(v[kIsMobile], 1.0) && "uppercase 'MOBILE' in UA should give 1.0");
    printf("PASS test_is_mobile_uppercase\n");
}

static void test_is_not_mobile() {
    BidRequest r = make_request();
    r.user_agent = "Mozilla/5.0 (Windows NT 6.1; WOW64)";
    FeatureVector v = extract_features(r);
    assert(near(v[kIsMobile], 0.0) && "desktop UA should give 0.0");
    printf("PASS test_is_not_mobile\n");
}

static void test_is_mobile_empty_ua() {
    BidRequest r = make_request();
    r.user_agent = "";
    FeatureVector v = extract_features(r);
    assert(near(v[kIsMobile], 0.0) && "empty UA should give 0.0");
    printf("PASS test_is_mobile_empty_ua\n");
}

// ---------------------------------------------------------------------------
// kExchangeId / kAdvertiserId / kRegion / kCity (index 10-13)
// ---------------------------------------------------------------------------

static void test_raw_passthrough_fields() {
    BidRequest r = make_request();
    r.adexchange    = 3;
    r.advertiser_id = 1458;
    r.region        = 7;
    r.city          = 42;
    FeatureVector v = extract_features(r);
    assert(near(v[kExchangeId],   3.0)    && "exchange_id passthrough wrong");
    assert(near(v[kAdvertiserId], 1458.0) && "advertiser_id passthrough wrong");
    assert(near(v[kRegion],       7.0)    && "region passthrough wrong");
    assert(near(v[kCity],         42.0)   && "city passthrough wrong");
    printf("PASS test_raw_passthrough_fields\n");
}

// ---------------------------------------------------------------------------
// General sanity
// ---------------------------------------------------------------------------

static void test_feature_vector_size() {
    BidRequest r = make_request();
    FeatureVector v = extract_features(r);
    assert(v.size() == kFeatureCount && "feature vector must have exactly kFeatureCount elements");
    printf("PASS test_feature_vector_size\n");
}

static void test_deterministic() {
    // Same request twice must produce identical feature vectors.
    BidRequest r = make_request();
    FeatureVector v1 = extract_features(r);
    FeatureVector v2 = extract_features(r);
    for (size_t i = 0; i < kFeatureCount; ++i) {
        assert(near(v1[i], v2[i]) && "extract_features must be deterministic");
    }
    printf("PASS test_deterministic\n");
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

int main() {
    // kHourOfDay
    test_hour_of_day_midday();
    test_hour_of_day_midnight();
    test_hour_of_day_last_hour();

    // kDayOfWeek
    test_day_of_week_thursday();
    test_day_of_week_monday();
    test_day_of_week_sunday();

    // kSlotArea
    test_slot_area();
    test_slot_area_zero_dimension();

    // kVisibilityHash / kFormatHash
    test_visibility_hash_in_range();
    test_format_hash_in_range();
    test_visibility_and_format_same_code_differ();
    test_visibility_different_codes_differ();

    // kDomainHash / kUrlHash / kVisitorHash
    test_domain_hash_in_range();
    test_url_hash_in_range();
    test_visitor_hash_in_range();
    test_different_domains_differ();
    test_same_domain_same_hash();

    // kFloorRatio
    test_floor_ratio_at_mean();
    test_floor_ratio_zero();
    test_floor_ratio_double_mean();

    // kIsMobile
    test_is_mobile_lowercase();
    test_is_mobile_capitalized();
    test_is_mobile_uppercase();
    test_is_not_mobile();
    test_is_mobile_empty_ua();

    // raw passthroughs
    test_raw_passthrough_fields();

    // general
    test_feature_vector_size();
    test_deterministic();

    printf("\nAll FeatureExtractor tests passed.\n");
    return 0;
}