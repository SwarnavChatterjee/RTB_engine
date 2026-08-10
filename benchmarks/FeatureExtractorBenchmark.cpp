#include "BenchmarkCommon.hpp"
#include "FeatureExtractor.hpp"

#include <cstdint>

namespace {
volatile std::uint64_t sink = 0;

rtb::BidRequest make_request() {
    rtb::BidRequest request{};
    request.timestamp = 20130606143000123ULL;
    request.visitor_id = "visitor-hash";
    request.user_agent = "Mozilla/5.0 Mobile Safari";
    request.region = 80;
    request.city = 87;
    request.adexchange = 1;
    request.domain = "domain-hash";
    request.url = "url-hash";
    request.adslot_width = 300;
    request.adslot_height = 250;
    request.adslot_visibility_code = 1;
    request.adslot_format_code = 0;
    request.adslot_floor_price = 5;
    request.advertiser_id = 3427;
    return request;
}
} // namespace

int main() {
    const rtb::BidRequest request = make_request();

    for (std::size_t i = 0; i < benchmark::kWarmupIterations; ++i) {
        const auto features = rtb::extract_features(request);
        sink += features[rtb::kDomainHash].bucket +
                static_cast<std::uint64_t>(features[rtb::kSlotArea].value);
    }

    const auto start = benchmark::Clock::now();
    for (std::size_t i = 0; i < benchmark::kIterations; ++i) {
        const auto features = rtb::extract_features(request);
        sink += features[rtb::kDomainHash].bucket +
                static_cast<std::uint64_t>(features[rtb::kSlotArea].value);
    }
    const auto elapsed = benchmark::Clock::now() - start;

    benchmark::print_result("FeatureExtractor", benchmark::kIterations, elapsed);
    return sink == 0 ? 1 : 0;
}
