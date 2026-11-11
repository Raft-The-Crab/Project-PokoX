#pragma once

#include <string_view>

namespace poko::cli {

// Shared entry point for Phase 0 CLI stubs. Prints the implementation label
// so a STUB can never be mistaken for a production tool (systemplan/00).
int run_stub(std::string_view tool, std::string_view purpose, int argc, char** argv);

} // namespace poko::cli
