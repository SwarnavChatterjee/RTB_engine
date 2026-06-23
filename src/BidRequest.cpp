#include "BidRequest.hpp"

#include <array>
#include <charconv>

namespace rtb {

namespace {

constexpr int kExpectedFields = 21;

bool split_fields(std::string_view line,
                   std::array<std::string_view, kExpectedFields>& out) {
    size_t start = 0;

    for (int field_idx = 0; field_idx + 1 < kExpectedFields; ++field_idx) {
        size_t tab_pos = line.find('\t', start);
        if (tab_pos == std::string_view::npos) {  // ran out of tabs before filling 20 fields => too few
            return false;
        }
        //each string_view, which itself contains a pointer+length, gets stored into the out array
        out[field_idx] = line.substr(start, tab_pos - start);
        start = tab_pos + 1;
    }

    std::string_view last_field = line.substr(start);
    if (last_field.find('\t') != std::string_view::npos) {  // tab still present in remainder => too many fields
        return false;
    }
    out[kExpectedFields - 1] = last_field;
    return true;
}

template <typename T>
bool parse_uint(std::string_view field, T& out) {
    if (field.empty()) {
        return false;
    }
    auto result = 
    std::from_chars(
        field.data(),
        field.data() + field.size(),
        out);
    return result.ec == std::errc() && result.ptr == field.data() + field.size();  //check for valid conversions 
}

} // namespace

std::optional<BidRequest> parse_bid_request(std::string_view line) {
    std::array<std::string_view, kExpectedFields> f;
    if (!split_fields(line, f)) {
        return std::nullopt;
    }

    BidRequest req{};

    req.bid_id = f[0];
    if (req.bid_id.empty()) return std::nullopt;

    if (!parse_uint(f[1], req.timestamp)) return std::nullopt;

    req.visitor_id = f[2];
    req.user_agent = f[3];
    req.ip         = f[4];

    if (!parse_uint(f[5], req.region))     return std::nullopt;
    if (!parse_uint(f[6], req.city))       return std::nullopt;
    if (!parse_uint(f[7], req.adexchange)) return std::nullopt;

    req.domain           = f[8];
    req.url              = f[9];
    req.anonymous_url_id = f[10];
    req.adslot_id        = f[11];

    if (!parse_uint(f[12], req.adslot_width))           return std::nullopt;
    if (!parse_uint(f[13], req.adslot_height))          return std::nullopt;
    if (!parse_uint(f[14], req.adslot_visibility_code)) return std::nullopt;
    if (!parse_uint(f[15], req.adslot_format_code))     return std::nullopt;
    if (!parse_uint(f[16], req.adslot_floor_price))     return std::nullopt;

    req.creative_id = f[17];

    if (!parse_uint(f[18], req.bidding_price)) return std::nullopt;
    if (!parse_uint(f[19], req.advertiser_id)) return std::nullopt;

    req.user_tag = f[20];

    return req;
}

} // namespace rtb