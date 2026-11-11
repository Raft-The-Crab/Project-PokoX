#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <variant>

namespace poko {

// Standardized error vocabulary (systemplan/03: "standardize errors and module
// boundaries"). Use Status for void operations and Result<T> for values.
enum class ErrorCode : std::uint8_t {
    None = 0,
    InvalidArgument,
    NotFound,
    OutOfRange,
    AlreadyExists,
    PermissionDenied,
    ResourceExhausted,
    Unsupported,
    FailedPrecondition,
    Internal,
    Unimplemented,
};

inline constexpr const char* to_string(ErrorCode code) noexcept {
    switch (code) {
        case ErrorCode::None:
            return "none";
        case ErrorCode::InvalidArgument:
            return "invalid_argument";
        case ErrorCode::NotFound:
            return "not_found";
        case ErrorCode::OutOfRange:
            return "out_of_range";
        case ErrorCode::AlreadyExists:
            return "already_exists";
        case ErrorCode::PermissionDenied:
            return "permission_denied";
        case ErrorCode::ResourceExhausted:
            return "resource_exhausted";
        case ErrorCode::Unsupported:
            return "unsupported";
        case ErrorCode::FailedPrecondition:
            return "failed_precondition";
        case ErrorCode::Internal:
            return "internal";
        case ErrorCode::Unimplemented:
            return "unimplemented";
    }
    return "unknown";
}

struct Error {
    ErrorCode code = ErrorCode::Internal;
    std::string message;
};

class Status {
public:
    static Status success() { return Status{}; }

    static Status fail(ErrorCode code, std::string message) {
        return Status{Error{code, std::move(message)}};
    }

    bool ok() const noexcept { return !error_.has_value(); }
    explicit operator bool() const noexcept { return ok(); }
    ErrorCode code() const noexcept {
        return error_ ? error_->code : ErrorCode::None;
    }
    const Error& error() const { return *error_; }

private:
    Status() = default;
    explicit Status(Error error) : error_(std::move(error)) {}

    std::optional<Error> error_;
};

template <typename T>
class Result {
public:
    static Result success(T value) { return Result{std::move(value)}; }

    static Result fail(ErrorCode code, std::string message) {
        return Result{Error{code, std::move(message)}};
    }

    bool has_value() const noexcept { return std::holds_alternative<T>(value_); }
    bool ok() const noexcept { return has_value(); }
    explicit operator bool() const noexcept { return has_value(); }
    const T& value() const { return std::get<T>(value_); }
    const Error& error() const { return std::get<Error>(value_); }

private:
    explicit Result(T value) : value_(std::move(value)) {}
    explicit Result(Error error) : value_(std::move(error)) {}

    std::variant<T, Error> value_;
};

}  // namespace poko
