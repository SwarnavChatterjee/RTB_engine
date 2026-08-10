#include "BenchmarkCommon.hpp"
#include "FloorGate.hpp"

#include <cstdint>

namespace {
volatile std::uint64_t sink = 0;
} // namespace

int main() {
    for (std::size_t i = 0; i < benchmark::kWarmupIterations; ++i)
        sink += FloorGate::passes(100.0, 50) ? 1u : 0u;

    const auto start = benchmark::Clock::now();
    for (std::size_t i = 0; i < benchmark::kIterations; ++i)
        sink += FloorGate::passes(100.0, 50) ? 1u : 0u;
    const auto elapsed = benchmark::Clock::now() - start;

    benchmark::print_result("FloorGate", benchmark::kIterations, elapsed);
    return sink == 0 ? 1 : 0;
}
