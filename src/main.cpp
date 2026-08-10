#include <cstdio>
#include <cstdlib>
#include <string_view>

#include "BiddingEngine.hpp"

// ---------------------------------------------------------------------------
// main
//
// Usage:
//   ./rtb_engine <weights_path> <budgets_path>
//
// Example:
//   ./rtb_engine models/weights.bin models/budgets.txt < dataset/bid.11.txt
//
// Reads bid requests from stdin, writes bid prices to stdout.
// Exits with non-zero status if model or budgets fail to load.
// ---------------------------------------------------------------------------
int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::fprintf(stderr,
            "Usage: %s <weights_path> <budgets_path>\n"
            "  weights_path: binary weight file from Python FTRL trainer\n"
            "  budgets_path: per-advertiser budget file (text, fen units)\n",
            argv[0]);
        return EXIT_FAILURE;
    }

    BiddingEngine engine;

    if (!engine.load_model(argv[1])) {
        std::fprintf(stderr, "ERROR: failed to load model weights from '%s'\n", argv[1]);
        std::fprintf(stderr, "       Check file exists and has exactly %zu bytes\n",
                     rtb::kHashSize * sizeof(double));
        return EXIT_FAILURE;
    }

    if (!engine.load_budgets(argv[2])) {
        std::fprintf(stderr, "ERROR: failed to load budgets from '%s'\n", argv[2]);
        std::fprintf(stderr, "       File must contain all 5 advertiser IDs "
                             "(1458 3358 3386 3427 3476) with no duplicates\n");
        return EXIT_FAILURE;
    }

    engine.run();
    return EXIT_SUCCESS;
}