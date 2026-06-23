#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

namespace rtb {

// ---------------------------------------------------------------------------
// BidRequest
//
// Represents ONE pre-auction bid opportunity, as it exists at bid time.
//
// Deliberately contains ONLY the 21 fields available in bid.*.txt logs.
// Outcome-only fields (Logtype, Payingprice, KeypageURL) do not exist in
// this struct AT ALL — not as optional, not as a default. This is an
// architectural choice: if the type has no field for it, the online bidding
// path cannot accidentally read it. Labels are joined separately, only in
// the offline Python training pipeline, by matching on bid_id against
// imp/clk/conv logs.
//
// LIFETIME WARNING:
// Every string_view field below points into the caller-owned line buffer
// that was parsed (or, in production, into the per-request arena). This
// struct does NOT own any memory. It becomes invalid the instant the
// underlying buffer is freed, reused, or modified. Never store a BidRequest
// past the lifetime of the buffer it was parsed from.
//
// Column mapping (1-indexed, verified against real bid.06.txt sample data):
//   1  bid_id                  string_view   unique auction ID
//   2  timestamp               uint64_t      yyyyMMddHHmmssSSS — needs 64-bit
//   3  visitor_id              string_view   hashed cookie (user identity)
//   4  user_agent              string_view   browser/OS/device string
//   5  ip                      string_view   hashed IP (e.g. 180.127.189.*)
//   6  region                  uint32_t      numeric region code
//   7  city                    uint32_t      numeric city code
//   8  adexchange              uint32_t      which exchange
//   9  domain                  string_view   hashed domain
//   10 url                     string_view   hashed URL
//   11 anonymous_url_id        string_view   "" or literal "null" when URL known —
//                                            BOTH forms verified in real data,
//                                            use has_anonymous_url_id() below
//   12 adslot_id               string_view   specific ad slot identifier
//   13 adslot_width            uint32_t      pixels
//   14 adslot_height           uint32_t      pixels
//   15 adslot_visibility_code  uint32_t      VERIFIED raw numeric code (0,1,2,...) —
//                                            not yet an engineered feature
//   16 adslot_format_code      uint32_t      VERIFIED raw numeric code — same caveat
//   17 adslot_floor_price      uint32_t      minimum bid — hard gate
//   18 creative_id             string_view   ad creative identifier
//   19 bidding_price           uint32_t      DSP's own bid (training signal only,
//                                            never a model input feature)
//   20 advertiser_id           uint32_t      one of 5 advertisers
//                                            (1458/3358/3386/3427/3476)
//   21 user_tag                string_view   comma-separated segment IDs, or "null" —
//                                            use has_user_tag() below
// ---------------------------------------------------------------------------
struct BidRequest {
    // --- Identity & timing ---
    std::string_view bid_id{};
    uint64_t         timestamp = 0;

    // --- User ---
    std::string_view visitor_id{};
    std::string_view user_agent{};
    std::string_view ip{};

    // --- Geo ---
    uint32_t region = 0;
    uint32_t city   = 0;

    // --- Exchange / placement ---
    uint32_t          adexchange = 0;
    std::string_view  domain{};
    std::string_view  url{};
    std::string_view  anonymous_url_id{};
    std::string_view  adslot_id{};

    uint32_t adslot_width          = 0;
    uint32_t adslot_height         = 0;
    uint32_t adslot_visibility_code = 0;  // raw encoded, not yet a feature
    uint32_t adslot_format_code     = 0;  // raw encoded, not yet a feature
    uint32_t adslot_floor_price     = 0;

    std::string_view creative_id{};

    // --- Bidding (DSP's own bid — training signal only, never a feature) ---
    uint32_t bidding_price = 0;

    uint32_t advertiser_id = 0;

    std::string_view user_tag{};

    // Field is inconsistent in the wild: empty string in some files,
    // literal "null" in others. Centralizing the check here means callers
    // never have to remember both forms.
    [[nodiscard]] bool has_anonymous_url_id() const noexcept {
        return !anonymous_url_id.empty() && anonymous_url_id != "null";
    }

    [[nodiscard]] bool has_user_tag() const noexcept {
        return !user_tag.empty() && user_tag != "null";
    }
};

// ---------------------------------------------------------------------------
// parse_bid_request
//
// Parses one tab-separated line from a bid.*.txt file (exactly 21 columns)
// into a BidRequest.
//
// Returns std::nullopt on any malformed input: wrong field count (too few
// OR too many — both are rejected, not just too few), or a numeric field
// that doesn't parse as a number. No exceptions are thrown — this is the
// hot path, and exception unwinding has unbounded cost, incompatible with
// a 5ms p99 latency budget.
//
// `line` must outlive the returned BidRequest (see LIFETIME WARNING above).
// ---------------------------------------------------------------------------
std::optional<BidRequest> parse_bid_request(std::string_view line);

} // namespace rtb
