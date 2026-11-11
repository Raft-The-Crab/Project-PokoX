#include <iostream>
#include <string_view>

#include "poko_version.h"

#ifndef POKO_IMPL_LABEL
#define POKO_IMPL_LABEL "STUB"
#endif

// Shared headless dedicated server runtime (systemplan/18). Phase 6 adds the
// doctor/status/logs/restart/update/verify-package subcommands and .pkxx
// loading. Phase 0 only proves the target builds and runs headlessly.
int main(int argc, char** argv) {
    if (argc > 1) {
        const std::string_view arg(argv[1]);
        if (arg == "--version") {
            std::cout << "poko-server " << poko::version_string() << " ["
                      << POKO_IMPL_LABEL << "]\n";
            return 0;
        }
    }

    std::cout << "poko-server " << poko::version_string() << " ["
              << POKO_IMPL_LABEL << "]\n"
              << "STUB: shared headless dedicated server runtime "
                 "(systemplan/18).\n"
              << "Phase 6 adds .pkxx loading, registration, tickets and "
                 "heartbeat.\n";
    return 0;
}
