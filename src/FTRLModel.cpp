#include "FTRLModel.hpp"
#include <string>
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace rtb {

// ---------------------------------------------------------------------------
// sigmoid
//
// Standard logistic function: 1 / (1 + exp(-x)).
// Clamped to [-35, 35] before computing exp() — beyond this range the
// result is indistinguishable from 0.0 or 1.0 in double precision, and
// unclamped values cause exp() to overflow to infinity.
// ---------------------------------------------------------------------------
double FTRLModel::sigmoid(double x) noexcept {
    x = std::max(-35.0, std::min(35.0, x));
    return 1.0 / (1.0 + std::exp(-x));
}

// ---------------------------------------------------------------------------
// load_weights
//
// Opens the binary file produced by the Python trainer and reads exactly
// kHashSize doubles into w_.
//
// File format (produced by Python):
//   numpy.array(w, dtype=numpy.float64).tofile(path)
// which writes kHashSize * 8 bytes, little-endian on x86, no header.
//
// Returns false (does not throw) on:
//   - File not found / permission denied
//   - File size is not exactly kHashSize * sizeof(double) bytes
//   - Partial read
// ---------------------------------------------------------------------------
bool FTRLModel::load_weights(std::string_view path) {
    // std::string_view may not be null-terminated; copy to string for fopen.
    std::string path_str(path);

    std::FILE* f = std::fopen(path_str.c_str(), "rb");
    if (!f) return false;

    constexpr size_t kExpectedBytes = kHashSize * sizeof(double);

    // Verify file size before reading — wrong size means wrong model or
    // wrong kHashSize, either of which would silently corrupt predictions.
    std::fseek(f, 0, SEEK_END);
    long file_size = std::ftell(f);
    std::rewind(f);

    if (file_size < 0 || static_cast<size_t>(file_size) != kExpectedBytes) {
        std::fclose(f);
        return false;
    }

    size_t items_read = std::fread(w_.data(), sizeof(double), kHashSize, f);
    std::fclose(f);

    return items_read == kHashSize;
}

// ---------------------------------------------------------------------------
// predict
//
// Dot product over all features, then sigmoid.
//
// For hashed categoricals: feature.value = 1.0, so this reduces to
//   score += w_[feature.bucket]
// which is just a weight lookup.
//
// For numeric/binary features: feature.value holds the actual number, so
//   score += w_[feature.bucket] * feature.value
// is a genuine weighted multiply.
//
// One loop, no branching, no special cases — the (bucket, value) design
// pays off here.
// ---------------------------------------------------------------------------
double FTRLModel::predict(const FeatureVector& features) const {
    double score = 0.0;
    for (const auto& f : features)
        score += w_[f.bucket] * f.value;
    return sigmoid(score);
}

} // namespace rtb