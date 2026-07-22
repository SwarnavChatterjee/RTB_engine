# ------------------------------
# BidRequest Tests
# ------------------------------
g++ -std=c++17 -I include \
    src/BidRequest.cpp \
    tests/test_bid_request.cpp \
    -o test_bid_request && ./test_bid_request


# ------------------------------
# ArenaAllocator Tests
# ------------------------------
g++ -std=c++17 -I include \
    src/ArenaAllocator.cpp \
    tests/test_arena_allocator.cpp \
    -o test_arena && ./test_arena


# ------------------------------
# FeatureExtractor Tests
# ------------------------------
g++ -std=c++17 -I include \
    src/BidRequest.cpp \
    src/FeatureExtractor.cpp \
    tests/test_feature_extractor.cpp \
    -o test_features && ./test_features


# ------------------------------
# BidCalculator Tests
# ------------------------------
g++ -std=c++17 -I include \
    src/BidCalculator.cpp \
    tests/test_bid_calculator.cpp \
    -o test_bid_calculator && ./test_bid_calculator


# ------------------------------
# FloorGate Tests
# ------------------------------
g++ -std=c++17 -I include \
    src/FloorGate.cpp \
    tests/test_floorgate.cpp \
    -o test_floorgate && ./test_floorgate