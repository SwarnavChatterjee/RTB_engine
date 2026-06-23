#include "BidRequest.hpp"

#include <cstdio>
#include <string>

namespace {
int g_failures = 0;

void check(bool cond, const char* what) {
    if (!cond) {
        std::printf("CHECK FAILED: %s\n", what);
        ++g_failures;
    }
}
} // namespace

int main() {
    using namespace rtb;

    // --- Real sample line from bid.06.txt (21 fields, verified by hand) ---
    std::string line1 =
        "b382c1c156dcbbd5b9317cb50f6a747b\t20130606000104008\tVh16OwT6OQNUXbj\t"
        "mozilla/4.0 (compatible; msie 6.0; windows nt 5.1; sv1; qqdownload 718)\t"
        "180.127.189.*\t80\t87\t1\ttFKETuqyMo1mjMp45SqfNX\t"
        "249b2c34247d400ef1cd3c6bfda4f12a\t\tmm_11402872_1272384_3182279\t"
        "300\t250\t1\t1\t0\t00fccc64a1ee2809348509b7ac2a97a5\t227\t3427\tnull";

    auto r1 = parse_bid_request(line1);
    check(r1.has_value(), "line1 should parse successfully");
    if (r1) {
        const auto& r = *r1;
        check(r.bid_id == "b382c1c156dcbbd5b9317cb50f6a747b", "line1 bid_id");
        check(r.timestamp == 20130606000104008ULL, "line1 timestamp");
        check(r.visitor_id == "Vh16OwT6OQNUXbj", "line1 visitor_id");
        check(r.region == 80, "line1 region");
        check(r.city == 87, "line1 city");
        check(r.adexchange == 1, "line1 adexchange");
        check(r.domain == "tFKETuqyMo1mjMp45SqfNX", "line1 domain");
        check(r.has_anonymous_url_id() == false, "line1 anonymous_url_id absent (empty string form)");
        check(r.adslot_width == 300, "line1 adslot_width");
        check(r.adslot_height == 250, "line1 adslot_height");
        check(r.adslot_visibility_code == 1, "line1 adslot_visibility_code");
        check(r.adslot_format_code == 1, "line1 adslot_format_code");
        check(r.adslot_floor_price == 0, "line1 adslot_floor_price");
        check(r.bidding_price == 227, "line1 bidding_price");
        check(r.advertiser_id == 3427, "line1 advertiser_id");
        check(r.has_user_tag() == false, "line1 user_tag absent (literal 'null' form)");
    }

    // --- Second real sample line, different advertiser/values ---
    std::string line2 =
        "7b6195de0d14203f92001da653bf1de\t20130606000104009\tVhkr1vpROHuhQWB\t"
        "Mozilla/4.0 (compatible; MSIE 8.0; Windows NT 5.1; Trident/4.0),gzip(gfe),gzip(gfe)\t"
        "113.119.105.*\t216\t217\t2\ttrqRTuToMTNUjM9r5rMi\t74419a072f8927222a1fd8aaa18cce56\t"
        "\t433287550\t468\t60\t1\t0\t5\t2f88fc9cf0141b5bbaf251cab07f4ce7\t300\t3386\tnull";

    auto r2 = parse_bid_request(line2);
    check(r2.has_value(), "line2 should parse successfully");
    if (r2) {
        const auto& r = *r2;
        check(r.advertiser_id == 3386, "line2 advertiser_id");
        check(r.bidding_price == 300, "line2 bidding_price");
        check(r.adslot_visibility_code == 1, "line2 adslot_visibility_code");
        check(r.adslot_format_code == 0, "line2 adslot_format_code");
        check(r.adslot_floor_price == 5, "line2 adslot_floor_price");
    }

    // --- Malformed: too few fields ---
    auto too_few = parse_bid_request("a\tb\tc");
    check(!too_few.has_value(), "too few fields must fail to parse");

    // --- Malformed: too many fields (22 instead of 21) ---
    std::string too_many = line1 + "\textra_unexpected_field";
    auto too_many_result = parse_bid_request(too_many);
    check(!too_many_result.has_value(), "too many fields (22) must fail to parse");

    // --- Malformed: non-numeric timestamp ---
    std::string bad_timestamp =
        "b382c1c156dcbbd5b9317cb50f6a747b\tNOTANUMBER\tVh16OwT6OQNUXbj\t"
        "ua\tip\t80\t87\t1\tdom\turl\t\tslot\t300\t250\t1\t1\t0\tcreative\t227\t3427\tnull";
    auto bad_timestamp_result = parse_bid_request(bad_timestamp);
    check(!bad_timestamp_result.has_value(), "non-numeric timestamp must fail to parse");

    // --- Malformed: empty bid_id ---
    std::string empty_bid_id =
        "\t20130606000104008\tVh16OwT6OQNUXbj\tua\tip\t80\t87\t1\tdom\turl\t\tslot\t"
        "300\t250\t1\t1\t0\tcreative\t227\t3427\tnull";
    auto empty_bid_id_result = parse_bid_request(empty_bid_id);
    check(!empty_bid_id_result.has_value(), "empty bid_id must fail to parse");

    // --- Malformed: trailing tab with nothing after (would create empty 22nd field) ---
    std::string trailing_tab = line1 + "\t";
    auto trailing_tab_result = parse_bid_request(trailing_tab);
    check(!trailing_tab_result.has_value(), "trailing tab creating an extra empty field must fail");

    if (g_failures == 0) {
        std::printf("ALL CHECKS PASSED\n");
        return 0;
    }
    std::printf("\n%d CHECK(S) FAILED\n", g_failures);
    return 1;
}
