#include "BenchmarkCommon.hpp"
#include "BidRequest.hpp"

#include <cstdint>
#include <string_view>

namespace {
constexpr std::string_view kLine =
    "b382c1c156dcbbd5b9317cb50f6a747b\t20130606000104008\tVh16OwT6OQNUXbj\t"
    "mozilla/4.0 (compatible; msie 6.0; windows nt 5.1; sv1; qqdownload 718)\t"
    "180.127.189.*\t80\t87\t1\ttFKETuqyMo1mjMp45SqfNX\t"
    "249b2c34247d400ef1cd3c6bfda4f12a\t\tmm_11402872_1272384_3182279\t"
    "300\t250\t1\t1\t0\t00fccc64a1ee2809348509b7ac2a97a5\t227\t3427\tnull";

volatile std::uint64_t sink = 0;
} // namespace

int main() {
    for (std::size_t i = 0; i < benchmark::kWarmupIterations; ++i) {
        const auto request = rtb::parse_bid_request(kLine);
        if (request) sink += request->timestamp + request->advertiser_id;
    }

    const auto start = benchmark::Clock::now();
    for (std::size_t i = 0; i < benchmark::kIterations; ++i) {
        const auto request = rtb::parse_bid_request(kLine);
        if (request) sink += request->timestamp + request->advertiser_id;
    }
    const auto elapsed = benchmark::Clock::now() - start;

    benchmark::print_result("BidRequest", benchmark::kIterations, elapsed);
    return sink == 0 ? 1 : 0;
}
