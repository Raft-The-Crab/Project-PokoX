#include "poko_test_harness.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>

#include "poko/core/diagnostics.h"
#include "poko/core/ownership.h"
#include "poko/core/settings.h"

POKO_TEST(diagnostics_counters_gauges_and_timers) {
    poko::Diagnostics diagnostics;
    diagnostics.increment("frames");
    diagnostics.increment("frames", 4);
    POKO_CHECK_EQ(diagnostics.counter("frames"), std::int64_t{5});
    POKO_CHECK_EQ(diagnostics.counter("missing"), std::int64_t{0});
    POKO_CHECK_EQ(diagnostics.counter_count(), std::size_t{1});

    diagnostics.set_gauge("fps", 60.5);
    POKO_CHECK_EQ(diagnostics.gauge("fps"), 60.5);
    POKO_CHECK_EQ(diagnostics.gauge("missing"), 0.0);

    {
        poko::Diagnostics::ScopedTimer timer(diagnostics, "tick");
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    POKO_CHECK(diagnostics.last_time("tick") > 0.0);

    diagnostics.reset();
    POKO_CHECK_EQ(diagnostics.counter("frames"), std::int64_t{0});
    POKO_CHECK_EQ(diagnostics.last_time("tick"), 0.0);
}

POKO_TEST(settings_parse_and_serialize_roundtrip) {
    auto settings = poko::Settings::parse("# a comment\n"
                                          "graphics.quality = high\n"
                                          "\n"
                                          "frame.cap=60\n"
                                          "invalid_line\n"
                                          "vsync = on\n");
    POKO_CHECK_EQ(settings.size(), std::size_t{3});
    POKO_CHECK_EQ(settings.get("graphics.quality").value_or(""), std::string("high"));
    POKO_CHECK_EQ(settings.get_int("frame.cap", 0), std::int64_t{60});
    POKO_CHECK_EQ(settings.get_int("graphics.quality", -1), std::int64_t{-1});
    POKO_CHECK(settings.get_bool("vsync", false));
    POKO_CHECK(!settings.get_bool("missing", false));
    POKO_CHECK(settings.has("frame.cap"));
    POKO_CHECK(!settings.has("nope"));

    const auto again = poko::Settings::parse(settings.serialize());
    POKO_CHECK_EQ(again.size(), std::size_t{3});
    POKO_CHECK_EQ(again.get("graphics.quality").value_or(""), std::string("high"));
}

POKO_TEST(settings_typed_fallbacks) {
    poko::Settings settings;
    settings.set("count", "not-a-number");
    settings.set("ratio", "1.5");
    settings.set("enabled", "maybe");
    POKO_CHECK_EQ(settings.get_int("count", 7), std::int64_t{7});
    POKO_CHECK_EQ(settings.get_int("missing", 9), std::int64_t{9});
    POKO_CHECK_EQ(settings.get_double("ratio", 0.0), 1.5);
    POKO_CHECK_EQ(settings.get_double("missing", 2.5), 2.5);
    POKO_CHECK(settings.get_bool("enabled", true));
    settings.clear();
    POKO_CHECK_EQ(settings.size(), std::size_t{0});
}

POKO_TEST(observer_ptr_and_scope_exit) {
    int value = 7;
    poko::ObserverPtr<int> observer(&value);
    POKO_CHECK(static_cast<bool>(observer));
    POKO_CHECK_EQ(*observer, 7);
    observer.reset();
    POKO_CHECK(!static_cast<bool>(observer));

    bool ran = false;
    {
        poko::ScopeExit guard([&ran] { ran = true; });
    }
    POKO_CHECK(ran);

    bool cancelled = false;
    {
        poko::ScopeExit guard([&cancelled] { cancelled = true; });
        guard.release();
    }
    POKO_CHECK(!cancelled);
}

POKO_TEST(resource_registry_invalidates_stale_handles) {
    poko::ResourceRegistry<int> registry;
    const auto handle = registry.add(std::make_unique<int>(42));
    POKO_CHECK(poko::is_valid(handle));
    POKO_CHECK(registry.get(handle) != nullptr);
    POKO_CHECK_EQ(*registry.get(handle), 42);
    POKO_CHECK_EQ(registry.size(), std::size_t{1});

    POKO_CHECK(registry.release(handle));
    POKO_CHECK(!registry.release(handle));
    POKO_CHECK(registry.get(handle) == nullptr);
    POKO_CHECK_EQ(registry.size(), std::size_t{0});

    const auto reused = registry.add(std::make_unique<int>(99));
    POKO_CHECK(poko::is_valid(reused));
    POKO_CHECK(!poko::same_handle(handle, reused));
    POKO_CHECK(registry.get(handle) == nullptr);
    POKO_CHECK_EQ(*registry.get(reused), 99);
    POKO_CHECK(!poko::is_valid(registry.add(nullptr)));
}

POKO_TEST_MAIN()
