#include "ITCHProcessor.hpp"
#include "Logger.hpp"
#include "MatchingEngine.hpp"

#include <cerrno>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <print>
#include <string>

namespace {

void print_usage(const char* argv0) {
    std::println(stderr,
        "Usage: {} [options] <path-to-itch-file>\n"
        "\n"
        "  <path-to-itch-file> must be a raw decompressed ITCH file\n"
        "  (e.g. 12302019.NASDAQ_ITCH50). Gzip (.gz) is NOT supported —\n"
        "  decompress first with: gunzip -k <file>.gz\n"
        "\n"
        "Options:\n"
        "  --order-pool-size N     Order pool capacity (default: 16777216). Power of two.\n"
        "  --level-pool-size N     PriceLevel pool capacity (default: 4194304). Power of two.\n"
        "  --log-path PATH         Where to write the diagnostic log (default: itch.log).\n"
        "  --help, -h              Show this message.\n"
        "\n"
        "Example:\n"
        "  {} 12302019.NASDAQ_ITCH50\n"
        "  {} --order-pool-size 8388608 12302019.NASDAQ_ITCH50",
        argv0, argv0, argv0);
}

bool parse_size(const char* s, std::size_t& out) {
    char* end = nullptr;
    errno = 0;
    unsigned long long v = std::strtoull(s, &end, 10);
    if (errno != 0 || end == s || *end != '\0' || v == 0) return false;
    out = static_cast<std::size_t>(v);
    return true;
}

bool is_pow2(std::size_t n) { return n > 0 && (n & (n - 1)) == 0; }

} // namespace

int main(int argc, char** argv) {
    std::string path;
    std::string log_path = "itch.log";
    std::size_t order_pool_size = itch::kOrderPoolSize;
    std::size_t level_pool_size = itch::kPriceLevelPoolSize;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return EXIT_SUCCESS;
        } else if (arg == "--order-pool-size" && i + 1 < argc) {
            if (!parse_size(argv[++i], order_pool_size) || !is_pow2(order_pool_size)) {
                std::println(stderr, "Error: --order-pool-size must be a power-of-two > 0");
                return EXIT_FAILURE;
            }
        } else if (arg == "--level-pool-size" && i + 1 < argc) {
            if (!parse_size(argv[++i], level_pool_size) || !is_pow2(level_pool_size)) {
                std::println(stderr, "Error: --level-pool-size must be a power-of-two > 0");
                return EXIT_FAILURE;
            }
        } else if (arg == "--log-path" && i + 1 < argc) {
            log_path = argv[++i];
        } else if (arg.size() >= 2 && arg[0] == '-') {
            std::println(stderr, "Error: unknown option '{}'", arg);
            print_usage(argv[0]);
            return EXIT_FAILURE;
        } else {
            path = arg;
        }
    }

    if (path.empty()) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    if (path.ends_with(".gz")) {
        std::println(stderr,
            "[ITCHProcessor] error: gzip input is not supported "
            "(gzip support removed).\n"
            "  Decompress first: gunzip -k {}",
            path);
        return EXIT_FAILURE;
    }

    std::println(stderr, "[ITCHProcessor] config:");
    std::println(stderr, "  input             : {}", path);
    std::println(stderr, "  log path          : {}", log_path);
    std::println(stderr, "  order pool size   : {} slots ({:.1f} MB)",
                 order_pool_size,
                 static_cast<double>(order_pool_size * sizeof(itch::Order)) / (1024.0 * 1024.0));
    std::println(stderr, "  level pool size   : {} slots ({:.1f} MB)",
                 level_pool_size,
                 static_cast<double>(level_pool_size * sizeof(itch::PriceLevel)) / (1024.0 * 1024.0));
    std::println(stderr, "");

    itch::ITCHProcessor processor(log_path, order_pool_size, level_pool_size);
    return processor.run(path) ? EXIT_SUCCESS : EXIT_FAILURE;
}
