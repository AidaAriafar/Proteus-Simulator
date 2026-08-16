#ifndef KIARASH_COMMON_RESULT_H
#define KIARASH_COMMON_RESULT_H

#include <string>

namespace kiarash
{

enum class ErrorCode
{
    None,
    InvalidArgument,
    FileNotFound,
    FileUnreadable,
    FileWriteFailed,
    InvalidFormat,
    UnsupportedVersion,
    DuplicateID,
    MissingReference,
    OperationCancelled,
    RendererFailure
};

template <typename T>
class Result
{
private:
    bool ok_;
    T value_;
    ErrorCode code_;
    std::string message_;

public:
    static Result success(const T& value)
    {
        return Result(true, value, ErrorCode::None, "");
    }

    static Result failure(ErrorCode code, const std::string& message)
    {
        return Result(false, T{}, code, message);
    }

    bool ok() const { return ok_; }
    const T& value() const { return value_; }
    T& value() { return value_; }
    ErrorCode code() const { return code_; }
    const std::string& message() const { return message_; }

private:
    Result(bool ok, const T& value, ErrorCode code, const std::string& message)
    :
    ok_(ok),
    value_(value),
    code_(code),
    message_(message)
    {
    }
};

template <>
class Result<void>
{
private:
    bool ok_;
    ErrorCode code_;
    std::string message_;

public:
    static Result success()
    {
        return Result(true, ErrorCode::None, "");
    }

    static Result failure(ErrorCode code, const std::string& message)
    {
        return Result(false, code, message);
    }

    bool ok() const { return ok_; }
    ErrorCode code() const { return code_; }
    const std::string& message() const { return message_; }

private:
    Result(bool ok, ErrorCode code, const std::string& message)
    :
    ok_(ok),
    code_(code),
    message_(message)
    {
    }
};

}

#endif
