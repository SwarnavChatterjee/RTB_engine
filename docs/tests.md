g++ -std=c++17 -I include src/BidRequest.cpp tests/test_bid_request.cpp -o test_bid_request && ./test_bid_request

g++ -std=c++17 -I include src/ArenaAllocator.cpp tests/test_arena_allocator.cpp -o test_arena && ./test_arena

g++ -std=c++17 -I include src/BidRequest.cpp src/FeatureExtractor.cpp tests/test_feature_extractor.cpp -o test_features && ./test_features