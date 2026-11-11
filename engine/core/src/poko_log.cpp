#include "poko_log.h"

#include <chrono>
#include <iostream>
#include <utility>

namespace poko {
namespace {

std::int64_t now_us() noexcept {
    return static_cast<std::int64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count());
}

}  // namespace

Logger& Logger::instance() {
    static Logger logger;
    return logger;
}

void Logger::set_level(LogLevel level) noexcept {
    level_.store(level, std::memory_order_relaxed);
}

LogLevel Logger::level() const noexcept {
    return level_.load(std::memory_order_relaxed);
}

bool Logger::is_enabled(LogLevel level) const noexcept {
    return static_cast<std::uint8_t>(level) >=
           static_cast<std::uint8_t>(level_.load(std::memory_order_relaxed));
}

void Logger::set_sink(LogSink sink) {
    std::lock_guard<std::mutex> guard(mutex_);
    sink_ = std::move(sink);
}

void Logger::clear_sink() {
    std::lock_guard<std::mutex> guard(mutex_);
    sink_ = {};
}

void Logger::log(LogLevel level, std::string_view category, std::string message) {
    LogRecord record{level, std::string(category), std::move(message), now_us()};

    LogSink sink;
    {
        std::lock_guard<std::mutex> guard(mutex_);
        sink = sink_;
    }

    if (sink) {
        sink(record);
    } else {
        std::clog << '[' << to_string(record.level) << "] [" << record.category
                  << "] " << record.message << '\n';
    }
}

const char* to_string(LogLevel level) noexcept {
    switch (level) {
        case LogLevel::Trace:
            return "trace";
        case LogLevel::Debug:
            return "debug";
        case LogLevel::Info:
            return "info";
        case LogLevel::Warn:
            return "warn";
        case LogLevel::Error:
            return "error";
        case LogLevel::Fatal:
            return "fatal";
    }
    return "unknown";
}

void log(LogLevel level, std::string_view category, std::string message) {
    Logger::instance().log(level, category, std::move(message));
}

}  // namespace poko
