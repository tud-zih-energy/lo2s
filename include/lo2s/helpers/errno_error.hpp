#pragma once

#include <string>
#include <system_error>

#include <cerrno>
#include <cstring>

namespace lo2s
{
class ErrnoError
{
public:
    ErrnoError(int errnum, const char* str) : errnum_(errnum), str_(str)
    {
    }

    ErrnoError(int errnum, std::string str) : errnum_(errnum), str_(str)
    {
    }

    static ErrnoError from_errno()
    {
        ErrnoError err;

        err.errnum_ = errno;
        err.str_ = strerror(errno);

        return err;
    }

    [[noreturn]] void throw_as_exception()
    {
        throw std::system_error(errno, std::system_category());
    }
    int errnum()
    {
        return errnum_;
    }

    std::string str()
    {
        return str_;
    }

private:
    ErrnoError()
    {
    }

    int errnum_;
    std::string str_;
};

} // namespace nlb
