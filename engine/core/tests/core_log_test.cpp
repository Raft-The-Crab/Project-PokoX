#include "poko_test_harness.h"

#include <cstddef>
#include <cstring>
#include <string>
#include <vector>

#include "poko/core/log.h"

POKO_TEST(level_filtering) {
    auto& logger = poko::Logger::instance();
    logger.clear_sink();
    logger.set_level(poko::LogLevel::Warn);
    POKO_CHECK(!logger.is_enabled(poko::LogLevel::Trace));
    POKO_CHECK(!logger.is_enabled(poko::LogLevel::Info));
    POKO_CHECK(logger.is_enabled(poko::LogLevel::Warn));
    POKO_CHECK(logger.is_enabled(poko::LogLevel::Fatal));
    logger.set_level(poko::LogLevel::Info);
}

POKO_TEST(sink_receives_formatted_records) {
    auto& logger = poko::Logger::instance();
    std::vector<poko::LogRecord> records;
    logger.set_sink([&records](const poko::LogRecord& record) { records.push_back(record); });
    logger.set_level(poko::LogLevel::Trace);

    POKO_LOG_INFO("unit", "hello " << 42);

    logger.clear_sink();
    logger.set_level(poko::LogLevel::Info);

    POKO_CHECK_EQ(records.size(), std::size_t{1});
    if (!records.empty()) {
        POKO_CHECK_EQ(records[0].message, std::string("hello 42"));
        POKO_CHECK_EQ(records[0].category, std::string("unit"));
        POKO_CHECK(records[0].level == poko::LogLevel::Info);
        POKO_CHECK(records[0].timestamp_us > 0);
    }
}

POKO_TEST(level_names_are_stable) {
    POKO_CHECK(std::strcmp(poko::to_string(poko::LogLevel::Trace), "trace") == 0);
    POKO_CHECK(std::strcmp(poko::to_string(poko::LogLevel::Debug), "debug") == 0);
    POKO_CHECK(std::strcmp(poko::to_string(poko::LogLevel::Info), "info") == 0);
    POKO_CHECK(std::strcmp(poko::to_string(poko::LogLevel::Warn), "warn") == 0);
    POKO_CHECK(std::strcmp(poko::to_string(poko::LogLevel::Error), "error") == 0);
    POKO_CHECK(std::strcmp(poko::to_string(poko::LogLevel::Fatal), "fatal") == 0);
}

POKO_TEST_MAIN()
