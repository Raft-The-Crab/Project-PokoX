#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace poko {

// Instance-based configuration store (systemplan/03: "configuration").
// No global mutable state, no implicit file or environment reads: the caller
// supplies entries explicitly through set() or parse().
//
// Thread-safety contract: all methods are safe to call concurrently.
class Settings {
public:
    Settings() = default;

    // std::mutex is not copyable, so copying is explicit: lock the source and
    // copy only the values into a fresh mutex. Needed for parse()/return-by-value.
    Settings(const Settings& other) {
        std::lock_guard<std::mutex> guard(other.mutex_);
        values_ = other.values_;
    }

    Settings& operator=(const Settings& other) {
        if (this != &other) {
            std::scoped_lock lock(mutex_, other.mutex_);
            values_ = other.values_;
        }
        return *this;
    }

    void set(std::string_view key, std::string value) {
        std::lock_guard<std::mutex> guard(mutex_);
        values_[std::string(key)] = std::move(value);
    }

    std::optional<std::string> get(std::string_view key) const {
        std::lock_guard<std::mutex> guard(mutex_);
        const auto it = values_.find(std::string(key));
        if (it == values_.end()) {
            return std::nullopt;
        }
        return it->second;
    }

    bool has(std::string_view key) const { return get(key).has_value(); }

    bool get_bool(std::string_view key, bool fallback) const {
        const auto raw = get(key);
        if (!raw) {
            return fallback;
        }
        if (*raw == "true" || *raw == "1" || *raw == "yes" || *raw == "on") {
            return true;
        }
        if (*raw == "false" || *raw == "0" || *raw == "no" || *raw == "off") {
            return false;
        }
        return fallback;
    }

    std::int64_t get_int(std::string_view key, std::int64_t fallback) const {
        const auto raw = get(key);
        if (!raw) {
            return fallback;
        }
        try {
            return static_cast<std::int64_t>(std::stoll(*raw));
        } catch (...) {
            return fallback;
        }
    }

    double get_double(std::string_view key, double fallback) const {
        const auto raw = get(key);
        if (!raw) {
            return fallback;
        }
        try {
            return std::stod(*raw);
        } catch (...) {
            return fallback;
        }
    }

    std::size_t size() const {
        std::lock_guard<std::mutex> guard(mutex_);
        return values_.size();
    }

    void clear() {
        std::lock_guard<std::mutex> guard(mutex_);
        values_.clear();
    }

    // Deterministic, sorted "key=value" lines for reproducible builds.
    std::string serialize() const {
        std::lock_guard<std::mutex> guard(mutex_);
        std::string out;
        for (const auto& entry : values_) {
            out += entry.first;
            out += '=';
            out += entry.second;
            out += '\n';
        }
        return out;
    }

    // Parses simple key=value lines. '#' and ';' comments and blank lines are
    // ignored. Malformed lines are skipped rather than failing the whole load.
    static Settings parse(std::string_view text) {
        Settings settings;
        std::size_t begin = 0;
        while (begin <= text.size()) {
            const std::size_t end = text.find('\n', begin);
            const std::size_t length =
                end == std::string_view::npos ? text.size() - begin : end - begin;
            std::string_view line = trim(text.substr(begin, length));
            begin = end == std::string_view::npos ? text.size() + 1 : end + 1;

            if (line.empty() || line.front() == '#' || line.front() == ';') {
                continue;
            }
            const std::size_t equals = line.find('=');
            if (equals == std::string_view::npos) {
                continue;
            }
            const std::string_view key = trim(line.substr(0, equals));
            const std::string_view value = trim(line.substr(equals + 1));
            if (key.empty()) {
                continue;
            }
            settings.set(key, std::string(value));
        }
        return settings;
    }

private:
    static std::string_view trim(std::string_view value) noexcept {
        constexpr std::string_view whitespace = " \t\r\n";
        const std::size_t first = value.find_first_not_of(whitespace);
        if (first == std::string_view::npos) {
            return {};
        }
        const std::size_t last = value.find_last_not_of(whitespace);
        return value.substr(first, last - first + 1);
    }

    mutable std::mutex mutex_;
    std::map<std::string, std::string> values_;
};

}  // namespace poko
