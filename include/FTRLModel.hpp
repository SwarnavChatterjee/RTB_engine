#pragma once

#include <array>
#include <cstdint>
#include <string_view>

#include "FeatureExtractor.hpp"

namespace rtb {

// ---------------------------------------------------------------------------
// FTRLModel
//
// Inference-only FTRL-Proximal model for click-through rate prediction.
//
// The C++ engine never runs the FTRL update rule — that lives entirely in
// the offline Python trainer. This class does exactly two things:
//   1. Load a trained weight vector from a binary file (once at startup).
//   2. Predict click probability for a single FeatureVector (hot path).
//
// Weight storage:
//   A single std::array<double, kHashSize> w_ stores the trained weights.
//   ~2MB, lives inside FTRLModel. FTRLModel must be heap-allocated via
//   std::make_unique<FTRLModel>() to avoid a stack overflow.
//   z and n accumulators are Python-only — they do not exist here.
//
// predict() uses one uniform rule for all features:
//   score += w_[feature.bucket] * feature.value
// Hashed categoricals: bucket = weight table index, value = 1.0
// Numeric/binary:      bucket = reserved fixed index, value = actual number
// No branching, no special cases.
// ---------------------------------------------------------------------------
class FTRLModel {
public:
    FTRLModel() = default;

    // Non-copyable — 2MB weight array makes accidental copies expensive.
    FTRLModel(const FTRLModel&)            = delete;
    FTRLModel& operator=(const FTRLModel&) = delete;

    // Movable — allows transfer of ownership without copying.
    FTRLModel(FTRLModel&&)            = default;
    FTRLModel& operator=(FTRLModel&&) = default;

    // -----------------------------------------------------------------------
    // load_weights
    //
    // Reads kHashSize doubles from a raw binary file produced by the Python
    // trainer. The file must contain exactly kHashSize * sizeof(double) bytes
    // in the same byte order as the host machine (little-endian on x86).
    //
    // Returns true on success, false on any error (file not found, wrong
    // size, read failure). Does not throw.
    // -----------------------------------------------------------------------
    [[nodiscard]] bool load_weights(std::string_view path);

    // -----------------------------------------------------------------------
    // predict
    //
    // Returns the predicted click probability in [0.0, 1.0] for the given
    // feature vector. Computes a dot product over all features, then applies
    // sigmoid to squash the raw score into a probability.
    //
    // Assumes load_weights() has been called successfully. If called before
    // loading weights, w_ is zero-initialized and predict() returns 0.5
    // (sigmoid(0) = 0.5) for any input.
    // -----------------------------------------------------------------------
    [[nodiscard]] double predict(const FeatureVector& features) const;

private:
    // sigmoid: maps any real number to (0, 1).
    // Clamped to [-35, 35] to prevent exp() overflow on extreme scores.
    [[nodiscard]] static double sigmoid(double x) noexcept;

    // Weight table. Zero-initialized at construction — safe to predict()
    // before loading, just not meaningful.
    std::array<double, kHashSize> w_{};
};

} // namespace rtb