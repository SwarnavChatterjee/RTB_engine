#include "FTRLModel.hpp"
#include "FeatureExtractor.hpp"
#include <string>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <array>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static bool near(double a, double b, double eps = 1e-9) {
    return std::fabs(a - b) < eps;
}

// Write a temporary binary weight file with kHashSize doubles.
// Returns the path as a std::string. Caller must delete the file.
static std::string write_temp_weights(const std::array<double, rtb::kHashSize>& weights) {
    std::string path = "test_ftrl_weights_tmp.bin";
    std::FILE* f = std::fopen(path.c_str(), "wb");
    assert(f && "could not open temp weight file for writing");
    std::fwrite(weights.data(), sizeof(double), rtb::kHashSize, f);
    std::fclose(f);
    return path;
}

static void remove_temp_file(const std::string& path) {
    std::remove(path.c_str());
}

// Build a FeatureVector where every bucket is 0 and every value is 0.0
// — useful for testing the zero-weights baseline.
static rtb::FeatureVector zero_features() {
    rtb::FeatureVector v{};
    return v;
}

// Build a FeatureVector with a single active feature at a given bucket
// with a given value, all others zeroed.
static rtb::FeatureVector single_feature(uint32_t bucket, double value) {
    rtb::FeatureVector v{};
    v[0] = {bucket, value};
    return v;
}

// ---------------------------------------------------------------------------
// sigmoid tests (via predict() — sigmoid is private but observable through
// predict() with controlled weights)
// ---------------------------------------------------------------------------

static void test_sigmoid_zero_input_gives_half() {
    // All weights zero, all feature values zero → score = 0 → sigmoid(0) = 0.5
    rtb::FTRLModel model;
    rtb::FeatureVector v = zero_features();
    double p = model.predict(v);
    assert(near(p, 0.5) && "sigmoid(0) must be 0.5");
    printf("PASS test_sigmoid_zero_input_gives_half\n");
}

static void test_sigmoid_output_in_unit_interval() {
    // predict() must always return a value in (0, 1)
    rtb::FTRLModel model;

    // positive score
    rtb::FeatureVector v1 = single_feature(100, 1.0);
    double p1 = model.predict(v1);  // w_[100]=0 so still 0.5 — fine
    assert(p1 > 0.0 && p1 < 1.0 && "predict() must be in (0,1)");

    printf("PASS test_sigmoid_output_in_unit_interval\n");
}

static void test_sigmoid_large_positive_score_near_one() {
    // Set w_[bucket] = 100.0 → score = 100 → sigmoid clamps to 35 → ~1.0
    std::array<double, rtb::kHashSize> weights{};
    weights[50] = 100.0;
    std::string path = write_temp_weights(weights);

    rtb::FTRLModel model;
    bool ok = model.load_weights(path);
    assert(ok);

    rtb::FeatureVector v = single_feature(50, 1.0);
    double p = model.predict(v);
    assert(p > 0.999 && "large positive score should give probability near 1");

    remove_temp_file(path);
    printf("PASS test_sigmoid_large_positive_score_near_one\n");
}

static void test_sigmoid_large_negative_score_near_zero() {
    std::array<double, rtb::kHashSize> weights{};
    weights[51] = -100.0;
    std::string path = write_temp_weights(weights);

    rtb::FTRLModel model;
    bool ok = model.load_weights(path);
    assert(ok);

    rtb::FeatureVector v = single_feature(51, 1.0);
    double p = model.predict(v);
    assert(p < 0.001 && "large negative score should give probability near 0");

    remove_temp_file(path);
    printf("PASS test_sigmoid_large_negative_score_near_zero\n");
}

// ---------------------------------------------------------------------------
// load_weights tests
// ---------------------------------------------------------------------------

static void test_load_weights_returns_true_on_valid_file() {
    std::array<double, rtb::kHashSize> weights{};
    std::string path = write_temp_weights(weights);

    rtb::FTRLModel model;
    bool ok = model.load_weights(path);
    assert(ok && "load_weights must return true for a valid file");

    remove_temp_file(path);
    printf("PASS test_load_weights_returns_true_on_valid_file\n");
}

static void test_load_weights_returns_false_on_missing_file() {
    rtb::FTRLModel model;
    bool ok = model.load_weights("this_file_does_not_exist_rtb_test.bin");
    assert(!ok && "load_weights must return false for a missing file");
    printf("PASS test_load_weights_returns_false_on_missing_file\n");
}

static void test_load_weights_returns_false_on_wrong_size() {
    // Write a file with the wrong number of bytes (too small)
    std::string path = "test_ftrl_wrong_size.bin";
    std::FILE* f = std::fopen(path.c_str(), "wb");
    assert(f);
    double dummy = 1.0;
    std::fwrite(&dummy, sizeof(double), 1, f);  // only 1 double, not kHashSize
    std::fclose(f);

    rtb::FTRLModel model;
    bool ok = model.load_weights(path);
    assert(!ok && "load_weights must return false when file size is wrong");

    std::remove(path.c_str());
    printf("PASS test_load_weights_returns_false_on_wrong_size\n");
}

static void test_load_weights_values_are_actually_loaded() {
    // Write specific values at known buckets, verify predict() uses them
    std::array<double, rtb::kHashSize> weights{};
    weights[200] = 2.0;
    std::string path = write_temp_weights(weights);

    rtb::FTRLModel model;
    model.load_weights(path);

    // score = w_[200] * 1.0 = 2.0 → sigmoid(2.0) ≈ 0.8808
    rtb::FeatureVector v = single_feature(200, 1.0);
    double p = model.predict(v);
    double expected = 1.0 / (1.0 + std::exp(-2.0));
    assert(near(p, expected, 1e-9) && "loaded weights must actually influence predict()");

    remove_temp_file(path);
    printf("PASS test_load_weights_values_are_actually_loaded\n");
}

static void test_load_weights_can_be_called_twice() {
    // Second load should overwrite the first
    std::array<double, rtb::kHashSize> weights1{};
    weights1[300] = 5.0;
    std::string path1 = "test_ftrl_weights1.bin";
    {
        std::FILE* f = std::fopen(path1.c_str(), "wb");
        std::fwrite(weights1.data(), sizeof(double), rtb::kHashSize, f);
        std::fclose(f);
    }

    std::array<double, rtb::kHashSize> weights2{};
    weights2[300] = -5.0;
    std::string path2 = "test_ftrl_weights2.bin";
    {
        std::FILE* f = std::fopen(path2.c_str(), "wb");
        std::fwrite(weights2.data(), sizeof(double), rtb::kHashSize, f);
        std::fclose(f);
    }

    rtb::FTRLModel model;
    model.load_weights(path1);
    model.load_weights(path2);

    // After second load, w_[300] should be -5.0
    rtb::FeatureVector v = single_feature(300, 1.0);
    double p = model.predict(v);
    assert(p < 0.5 && "second load_weights must overwrite first — w_[300] should be negative");

    std::remove(path1.c_str());
    std::remove(path2.c_str());
    printf("PASS test_load_weights_can_be_called_twice\n");
}

// ---------------------------------------------------------------------------
// predict() tests
// ---------------------------------------------------------------------------

static void test_predict_before_load_returns_half() {
    // Before load_weights(), w_ is zero-initialized → score=0 → 0.5
    rtb::FTRLModel model;
    rtb::FeatureVector v = single_feature(42, 1.0);
    double p = model.predict(v);
    assert(near(p, 0.5) && "predict() before load_weights() must return 0.5");
    printf("PASS test_predict_before_load_returns_half\n");
}

static void test_predict_dot_product_correctness() {
    // Set w_[10]=1.0, w_[20]=2.0, w_[30]=3.0
    // Feature: bucket=10 value=1.0, bucket=20 value=0.5, bucket=30 value=2.0
    // Expected score = 1*1 + 2*0.5 + 3*2 = 1 + 1 + 6 = 8
    std::array<double, rtb::kHashSize> weights{};
    weights[10] = 1.0;
    weights[20] = 2.0;
    weights[30] = 3.0;
    std::string path = write_temp_weights(weights);

    rtb::FTRLModel model;
    model.load_weights(path);

    rtb::FeatureVector v{};
    v[0] = {10, 1.0};
    v[1] = {20, 0.5};
    v[2] = {30, 2.0};
    // remaining features are {0, 0.0} — contribute 0 to score

    double p = model.predict(v);
    double expected = 1.0 / (1.0 + std::exp(-8.0));
    assert(near(p, expected, 1e-9) && "dot product must be computed correctly");

    remove_temp_file(path);
    printf("PASS test_predict_dot_product_correctness\n");
}

static void test_predict_categorical_value_is_one() {
    // For categorical features, value=1.0, so score = w_[bucket]
    std::array<double, rtb::kHashSize> weights{};
    weights[999] = 0.7;
    std::string path = write_temp_weights(weights);

    rtb::FTRLModel model;
    model.load_weights(path);

    // categorical: value=1.0
    rtb::FeatureVector v = single_feature(999, 1.0);
    double p = model.predict(v);
    double expected = 1.0 / (1.0 + std::exp(-0.7));
    assert(near(p, expected, 1e-9) && "categorical feature (value=1.0) must work correctly");

    remove_temp_file(path);
    printf("PASS test_predict_categorical_value_is_one\n");
}

static void test_predict_numeric_feature_scales_weight() {
    // For numeric features, score = w_[bucket] * value
    std::array<double, rtb::kHashSize> weights{};
    weights[rtb::kSlotAreaBucket] = 0.5;
    std::string path = write_temp_weights(weights);

    rtb::FTRLModel model;
    model.load_weights(path);

    // numeric: value = log(300*250) = log(75000) ≈ 11.225
    double area = std::log(300.0 * 250.0);
    rtb::FeatureVector v = single_feature(rtb::kSlotAreaBucket, area);
    double p = model.predict(v);
    double expected = 1.0 / (1.0 + std::exp(-0.5 * area));
    assert(near(p, expected, 1e-9) && "numeric feature must scale the weight by value");

    remove_temp_file(path);
    printf("PASS test_predict_numeric_feature_scales_weight\n");
}

static void test_predict_zero_value_feature_contributes_nothing() {
    // A feature with value=0.0 must contribute 0 to the score
    // regardless of what w_[bucket] is
    std::array<double, rtb::kHashSize> weights{};
    weights[500] = 999.0;  // large weight
    std::string path = write_temp_weights(weights);

    rtb::FTRLModel model;
    model.load_weights(path);

    // value=0.0 → contribution = 999.0 * 0.0 = 0.0
    rtb::FeatureVector v = single_feature(500, 0.0);
    double p = model.predict(v);
    assert(near(p, 0.5) && "feature with value=0 must contribute nothing to score");

    remove_temp_file(path);
    printf("PASS test_predict_zero_value_feature_contributes_nothing\n");
}

static void test_predict_monotone_in_weight() {
    // Increasing a weight for an active feature must increase predict()
    auto make_model_with_weight = [](double w) -> double {
        std::array<double, rtb::kHashSize> weights{};
        weights[42] = w;
        std::string path = "test_ftrl_mono.bin";
        std::FILE* f = std::fopen(path.c_str(), "wb");
        std::fwrite(weights.data(), sizeof(double), rtb::kHashSize, f);
        std::fclose(f);

        rtb::FTRLModel model;
        model.load_weights(path);
        std::remove(path.c_str());

        rtb::FeatureVector v = single_feature(42, 1.0);
        return model.predict(v);
    };

    double p1 = make_model_with_weight(-1.0);
    double p2 = make_model_with_weight(0.0);
    double p3 = make_model_with_weight(1.0);

    assert(p1 < p2 && p2 < p3 && "predict() must be monotonically increasing in weight");
    printf("PASS test_predict_monotone_in_weight\n");
}

static void test_predict_deterministic() {
    // Same model + same features must produce the same result every time
    std::array<double, rtb::kHashSize> weights{};
    weights[100] = 0.3;
    weights[200] = -0.2;
    std::string path = write_temp_weights(weights);

    rtb::FTRLModel model;
    model.load_weights(path);

    rtb::FeatureVector v{};
    v[0] = {100, 1.0};
    v[1] = {200, 1.0};

    double p1 = model.predict(v);
    double p2 = model.predict(v);
    double p3 = model.predict(v);

    assert(near(p1, p2) && near(p2, p3) && "predict() must be deterministic");

    remove_temp_file(path);
    printf("PASS test_predict_deterministic\n");
}

static void test_predict_reserved_bucket_collision_impossible() {
    // Verify that reserved buckets (0,1,2) are accessible and distinct
    // from hashed categoricals, which land in [kReservedBuckets, kHashSize)
    std::array<double, rtb::kHashSize> weights{};
    weights[rtb::kSlotAreaBucket]   = 1.0;
    weights[rtb::kFloorRatioBucket] = 2.0;
    weights[rtb::kIsMobileBucket]   = 3.0;
    // All hashed categoricals at 0 — should not interfere
    std::string path = write_temp_weights(weights);

    rtb::FTRLModel model;
    model.load_weights(path);

    // Access each reserved bucket independently
    {
        rtb::FeatureVector v = single_feature(rtb::kSlotAreaBucket, 1.0);
        double p = model.predict(v);
        double expected = 1.0 / (1.0 + std::exp(-1.0));
        assert(near(p, expected, 1e-9) && "kSlotAreaBucket must be independently accessible");
    }
    {
        rtb::FeatureVector v = single_feature(rtb::kFloorRatioBucket, 1.0);
        double p = model.predict(v);
        double expected = 1.0 / (1.0 + std::exp(-2.0));
        assert(near(p, expected, 1e-9) && "kFloorRatioBucket must be independently accessible");
    }
    {
        rtb::FeatureVector v = single_feature(rtb::kIsMobileBucket, 1.0);
        double p = model.predict(v);
        double expected = 1.0 / (1.0 + std::exp(-3.0));
        assert(near(p, expected, 1e-9) && "kIsMobileBucket must be independently accessible");
    }

    remove_temp_file(path);
    printf("PASS test_predict_reserved_bucket_collision_impossible\n");
}

static void test_predict_all_14_features_accumulate() {
    // All 14 features active, each with weight=1.0 and value=1.0
    // → score = 14.0 → sigmoid(14.0) close to 1.0
    std::array<double, rtb::kHashSize> weights{};
    for (size_t i = 0; i < rtb::kFeatureCount; ++i)
        weights[i + rtb::kReservedBuckets] = 1.0;
    // Also set reserved buckets
    weights[rtb::kSlotAreaBucket]   = 1.0;
    weights[rtb::kFloorRatioBucket] = 1.0;
    weights[rtb::kIsMobileBucket]   = 1.0;

    std::string path = write_temp_weights(weights);

    rtb::FTRLModel model;
    model.load_weights(path);

    rtb::FeatureVector v{};
    for (size_t i = 0; i < rtb::kFeatureCount; ++i)
        v[i] = {static_cast<uint32_t>(i + rtb::kReservedBuckets), 1.0};

    double p = model.predict(v);
    double expected = 1.0 / (1.0 + std::exp(-static_cast<double>(rtb::kFeatureCount)));
    assert(near(p, expected, 1e-9) && "all 14 features must accumulate into score");

    remove_temp_file(path);
    printf("PASS test_predict_all_14_features_accumulate\n");
}

// ---------------------------------------------------------------------------
// Move semantics test
// ---------------------------------------------------------------------------

static void test_model_is_movable() {
    std::array<double, rtb::kHashSize> weights{};
    weights[77] = 1.5;
    std::string path = write_temp_weights(weights);

    rtb::FTRLModel model1;
    model1.load_weights(path);

    // Move into model2
    rtb::FTRLModel model2 = std::move(model1);

    rtb::FeatureVector v = single_feature(77, 1.0);
    double p = model2.predict(v);
    double expected = 1.0 / (1.0 + std::exp(-1.5));
    assert(near(p, expected, 1e-9) && "moved model must retain weights");

    remove_temp_file(path);
    printf("PASS test_model_is_movable\n");
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

int main() {
    // sigmoid (via predict)
    test_sigmoid_zero_input_gives_half();
    test_sigmoid_output_in_unit_interval();
    test_sigmoid_large_positive_score_near_one();
    test_sigmoid_large_negative_score_near_zero();

    // load_weights
    test_load_weights_returns_true_on_valid_file();
    test_load_weights_returns_false_on_missing_file();
    test_load_weights_returns_false_on_wrong_size();
    test_load_weights_values_are_actually_loaded();
    test_load_weights_can_be_called_twice();

    // predict
    test_predict_before_load_returns_half();
    test_predict_dot_product_correctness();
    test_predict_categorical_value_is_one();
    test_predict_numeric_feature_scales_weight();
    test_predict_zero_value_feature_contributes_nothing();
    test_predict_monotone_in_weight();
    test_predict_deterministic();
    test_predict_reserved_bucket_collision_impossible();
    test_predict_all_14_features_accumulate();

    // move semantics
    test_model_is_movable();

    printf("\nAll FTRLModel tests passed.\n");
    return 0;
}