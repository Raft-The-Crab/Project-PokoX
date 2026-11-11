#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>

namespace poko {

enum class LogLevel : std::uint8_t {
    Trace = 0,
    Debug,
    Info,
    Warn,
    Error,
    Fatal,
};

struct LogRecord {
    LogLevel level;
    std::string category;
    std::string message;
    std::int64_t timestamp_us;
};

using LogSink = std::function<void(const LogRecord&)>;

// Minimal structured logger. A sink can be installed for tests or crash
// reporting; otherwise records go to std::clog. Never log credentials,
// personal data or moderation evidence (masterplan/17, systemplan/23).
class Logger {
public:
    static Logger& instance();

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    void set_level(LogLevel level) noexcept;
    LogLevel level() const noexcept;
    bool is_enabled(LogLevel level) const noexcept;

    void set_sink(LogSink sink);
    void clear_sink();

    void log(LogLevel level, std::string_view category, std::string message);

private:
    Logger() = default;

    mutable std::mutex mutex_;
    std::atomic<LogLevel> level_{LogLevel::Info};
    LogSink sink_;
};

const char* to_string(LogLevel level) noexcept;
void log(LogLevel level, std::string_view category, std::string message);

}  // namespace poko

#define POKO_LOG_STREAM(level_value, category_value, expression)               \
    do {                                                                       \
        if (::poko::Logger::instance().is_enabled(level_value)) {              \
            std::ostringstream _poko_stream;                                    \
            _poko_stream << expression;                                         \
            ::poko::Logger::instance().log(level_value, category_value,         \
                                           _poko_stream.str());                 \
        }                                                                      \
    } while (false)

#define POKO_LOG_TRACE(category_value, expression) \
    POKO_LOG_STREAM(::poko::LogLevel::Trace, category_value, expression)
#define POKO_LOG_DEBUG(category_value, expression) \
    POKO_LOG_STREAM(::poko::LogLevel::Debug, category_value, expression)
#define POKO_LOG_INFO(category_value, expression) \
    POKO_LOG_STREAM(::poko::LogLevel::Info, category_value, expression)
#define POKO_LOG_WARN(category_value, expression) \
    POKO_LOG_STREAM(::poko::LogLevel::Warn, category_value, expression)
#define POKO_LOG_ERROR(category_value, expression) \
    POKO_LOG_STREAM(::poko::LogLevel::Error, category_value, expression)
