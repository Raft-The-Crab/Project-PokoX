#include <iostream>

#include "poko_assert.h"
#include "poko_build_config.h"
#include "poko_log.h"
#include "poko_version.h"

// Minimal playable/headless harness required by the Phase 0 exit gate
// (systemplan/02): a headless demo that runs without any graphics device.
int main() {
    poko::Logger::instance().set_level(poko::LogLevel::Debug);

    POKO_LOG_INFO("runtime", "headless runtime " << poko::version_string()
                                                 << " config="
                                                 << poko::build_config_name()
                                                 << " platform="
                                                 << poko::platform_name());

    POKO_ASSERT(poko::version_string() != nullptr, "version string must exist");
    POKO_ASSERT(!poko::config::kDevTools || poko::config::kDevelopmentLike,
                "dev tools are restricted to development configurations");

    if constexpr (poko::config::kHeadless) {
        POKO_LOG_INFO("runtime", "headless mode: no graphics, audio or player UI");
    }

    POKO_LOG_DEBUG("runtime", "self-check complete");
    std::cout << "headless runtime self-check: OK\n";
    return 0;
}
