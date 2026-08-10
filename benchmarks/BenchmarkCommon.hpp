#pragma once

#include <chrono>
#include <cstddef>
#include <cstdio>

namespace benchmark {

constexpr std::size_t kIterations = 1'000'000;
constexpr std::size_t kWarmupIterations = 10'000;

using Clock = std::chrono::high_resolution_clock;

inline void print_result(const char* name,
                         std::size_t iterations,
                         Clock::duration elapsed) {
    const auto total_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count();
    const double average_ns = static_cast<double>(total_ns) / static_cast<double>(iterations);
    const double average_us = average_ns / 1'000.0;
    const double throughput = static_cast<double>(iterations) /
                              std::chrono::duration<double>(elapsed).count();

    std::printf("==================================\n");
    std::printf("%s Benchmark\n", name);
    std::printf("==================================\n");
    std::printf("Iterations : %zu\n", iterations);
    std::printf("Total time : %lld ns (%.3f ms)\n",
                static_cast<long long>(total_ns),
                static_cast<double>(total_ns) / 1'000'000.0);
    std::printf("Average    : %.3f ns/op (%.6f us/op)\n", average_ns, average_us);
    std::printf("Throughput : %.3f ops/sec\n\n", throughput);
}

} // namespace benchmark
