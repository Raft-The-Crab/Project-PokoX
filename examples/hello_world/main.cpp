#include <iostream>

#include "poko_version.h"

#ifndef POKO_IMPL_LABEL
#define POKO_IMPL_LABEL "STUB"
#endif

// Windowed demo required by the Phase 0 exit gate (systemplan/02). A real
// window needs SDL2 and a Vulkan/OpenGL device, which are not vendored until
// Phase 1 (systemplan/04). This target is deliberately a labelled STUB and is
// excluded from the headless Server configuration.
int main() {
    std::cout << "poko_windowed_demo " << poko::version_string() << " [" << POKO_IMPL_LABEL << "]\n"
              << "STUB: requires SDL2 windowing and a render device.\n"
              << "Tracked by systemplan/04 (renderer build sequence).\n";
    return 0;
}
