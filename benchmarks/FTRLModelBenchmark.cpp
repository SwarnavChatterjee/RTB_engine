#include <string>

#include "BenchmarkCommon.hpp"
#include "FTRLModel.hpp"

// FTRLModel.cpp currently relies on <string> being included transitively.
// Include it above the implementation here so this benchmark can exercise the
// existing production implementation without changing production files.
#include "../src/FTRLModel.cpp"

#include <cstdint>

namespace {
volatile double sink = 0.0;

rtb::FeatureVector make_features() {
    rtb::FeatureVector features{};
    features[rtb::kSlotArea] = {rtb::kSlotAreaBucket, 11.225};
    features[rtb::kFloorRatio] = {rtb::kFloorRatioBucket, 0.1};
    features[rtb::kIsMobile] = {rtb::kIsMobileBucket, 1.0};
    features[rtb::kDomainHash] = {12345, 1.0};
    features[rtb::kUrlHash] = {23456, 1.0};
    features[rtb::kVisitorHash] = {34567, 1.0};
    return features;
}
} // namespace

int main() {
    rtb::FTRLModel model;
    const rtb::FeatureVector features = make_features();

    for (std::size_t i = 0; i < benchmark::kWarmupIterations; ++i)
        sink += model.predict(features);

    const auto start = benchmark::Clock::now();
    for (std::size_t i = 0; i < benchmark::kIterations; ++i)
        sink += model.predict(features);
    const auto elapsed = benchmark::Clock::now() - start;

    benchmark::print_result("FTRLModel", benchmark::kIterations, elapsed);
    return sink == 0.0 ? 1 : 0;
}
