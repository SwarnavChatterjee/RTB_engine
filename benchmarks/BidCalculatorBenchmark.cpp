#include "BenchmarkCommon.hpp"
#include "BidCalculator.hpp"

#include <cstdint>

namespace {
volatile double sink = 0.0;
} // namespace

int main() {
    for (std::size_t i = 0; i < benchmark::kWarmupIterations; ++i)
        sink += BidCalculator::compute_bid(0.002, 3.0);

    const auto start = benchmark::Clock::now();
    for (std::size_t i = 0; i < benchmark::kIterations; ++i)
        sink += BidCalculator::compute_bid(0.002, 3.0);
    const auto elapsed = benchmark::Clock::now() - start;

    benchmark::print_result("BidCalculator", benchmark::kIterations, elapsed);
    return sink == 0.0 ? 1 : 0;
}
