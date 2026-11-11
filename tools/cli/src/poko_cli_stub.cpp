#include "poko_cli_stub.h"

#include <iostream>
#include <string_view>

#include "poko_version.h"

#ifndef POKO_IMPL_LABEL
#define POKO_IMPL_LABEL "STUB"
#endif

namespace poko::cli {
namespace {

void print_usage(std::string_view tool, std::string_view purpose) {
    std::cout << tool << " - PokoX Phase 0 tool [" << POKO_IMPL_LABEL << "]\n"
              << "Version: " << version_string() << " (" << build_config_name() << ")\n"
              << "Purpose: " << purpose << "\n\n"
              << "STUB: no functionality is implemented yet.\n"
              << "Usage: " << tool << " [--version | --help]\n";
}

} // namespace

int run_stub(std::string_view tool, std::string_view purpose, int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg(argv[i]);
        if (arg == "--version") {
            std::cout << tool << " " << version_string() << " [" << POKO_IMPL_LABEL << "]\n";
            return 0;
        }
        if (arg == "--help" || arg == "-h") {
            print_usage(tool, purpose);
            return 0;
        }
        std::cerr << tool << ": unknown argument '" << arg << "'\n";
        print_usage(tool, purpose);
        return 1;
    }
    print_usage(tool, purpose);
    return 0;
}

} // namespace poko::cli
