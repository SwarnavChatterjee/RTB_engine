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
