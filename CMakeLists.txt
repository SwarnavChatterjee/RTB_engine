cmake_minimum_required(VERSION 3.20)
project(rtb_engine CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile_commands.json — needed for clangd / VSCode IntelliSense
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Warnings on by default in debug builds — catch bugs early rather than
# discover them at runtime (this caught a real bug in BidRequest.cpp earlier).
add_compile_options(-Wall -Wextra)

# --- Core library ---
# Add new .cpp files here as we build out more of the engine
# (FeatureExtractor.cpp, FTRLModel.cpp, etc.)
add_library(rtb_engine STATIC
    src/BidRequest.cpp
)
target_include_directories(rtb_engine PUBLIC include)

# --- Tests ---
# Each test is its own executable for now (no test framework yet —
# plain assert-style checks with a return code). We can introduce
# GoogleTest/Catch2 later if the test suite grows large enough to need it.
enable_testing()

add_executable(test_bid_request tests/test_bid_request.cpp)
target_link_libraries(test_bid_request PRIVATE rtb_engine)
add_test(NAME test_bid_request COMMAND test_bid_request)

# --- Benchmarks ---
# These targets intentionally live outside the production library and existing
# test targets. Runtime outputs are placed in CMake's binary directory.
add_executable(benchmark_feature_extractor
    benchmarks/FeatureExtractorBenchmark.cpp
    src/FeatureExtractor.cpp
    src/BidRequest.cpp
)
target_link_libraries(benchmark_feature_extractor PRIVATE rtb_engine)

add_executable(benchmark_bid_request
    benchmarks/BidRequestBenchmark.cpp
    src/BidRequest.cpp
)

add_executable(benchmark_bid_calculator
    benchmarks/BidCalculatorBenchmark.cpp
    src/BidCalculator.cpp
)

add_executable(benchmark_floor_gate
    benchmarks/FloorGateBenchmark.cpp
    src/FloorGate.cpp
)

add_executable(benchmark_ftrl_model
    benchmarks/FTRLModelBenchmark.cpp
)

foreach(benchmark_target
        benchmark_feature_extractor
        benchmark_bid_request
        benchmark_bid_calculator
        benchmark_floor_gate
        benchmark_ftrl_model)
    target_include_directories(${benchmark_target} PRIVATE include benchmarks)
endforeach()
