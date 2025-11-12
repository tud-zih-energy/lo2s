#pragma once

#include "lo2s/helpers/errno_error.hpp"
#include "lo2s/helpers/expected.hpp"
#include "lo2s/helpers/maybe_error.hpp"

#include <fmt/format.h>
extern "C"
{
#include <unistd.h>
#include <fcntl.h>
}
namespace lo2s
{
    class Fd;
class WeakFd
{
    public:
        explicit WeakFd(int fd ) : fd_(fd)
        {
        }
    int as_int() const
    {
        return fd_;
    }
    static WeakFd make_invalid()
    {
        return WeakFd(-1);
    }

    bool invalid()
    {
        return fd_ == -1;
    }
    friend bool operator==(const WeakFd&lhs, const Fd& rhs);
    friend bool operator!=(const WeakFd& lhs, const Fd& rhs);

    friend bool operator==(const WeakFd& lhs, const WeakFd& rhs)
    {
        return lhs.fd_ == rhs.fd_;
    }

    friend bool operator<(const WeakFd& lhs, const WeakFd& rhs)
    {
        return lhs.fd_ < rhs.fd_;
    }

    friend inline std::ostream& operator<<(std::ostream& os, const WeakFd& fd)
    {
        return os << std::string("fd") << std::to_string(fd.fd_);
    }

    private:
        int fd_;
};
class Fd
{
    public:
    Fd(Fd&) = delete;
    Fd &operator=(Fd&) = delete;

    Fd(Fd&& other)
    {
        this->fd_ = other.fd_;
        other.fd_ = -1;
    }

    static Expected<Fd, ErrnoError> open(std::string path, int flags)
    {
        Fd res;
        res.fd_ = ::open(path.c_str(), flags);

        if (res.fd_ == -1)
        {
            return Unexpected(ErrnoError::from_errno());
        }

        return res;
    }

    Fd &operator=(Fd&& other)
    {
        this->fd_ = other.fd_;
        other.fd_ = -1;
        return *this;
    }
    WeakFd to_weak() const
    {
        return WeakFd(fd_);
    }

    ~Fd()
    {
        close(fd_);
        fd_ = -1;
    }

    MaybeError<ErrnoError> make_nonblock()
    {
        if(fcntl(fd_, F_SETFL, O_NONBLOCK) == -1)
        {
            return ErrnoError::from_errno();
        }
        return {};
    }

    
    static Fd from_int(int fd) 
    {
        Fd res;
        res.fd_ = fd;
        return res;
    }

    friend bool operator==(const WeakFd&lhs, const Fd& rhs);
    friend bool operator!=(const WeakFd&lhs, const Fd& rhs);
    bool invalid()
    {
        return fd_ == -1;
    }

    friend inline std::ostream& operator<<(std::ostream& os, const Fd& fd)
    {
        return os << std::string("fd") << std::to_string(fd.fd_);
    }

    protected:
    Fd() : fd_(-1)
    {
    }
    int fd_ = -1;
};
} // namespace lo2s

namespace fmt
{
template <>
struct formatter<lo2s::WeakFd>
{
    constexpr auto parse(format_parse_context& ctx)
    {
        auto it = ctx.begin(), end = ctx.end();
        if (it != end && *it != '}')
        {
            throw format_error("invalid format");
        }

        return it;
    }

    template <typename FormatContext>
    auto format(const lo2s::WeakFd& addr, FormatContext& ctx) const
    {
        return fmt::format_to(ctx.out(), "{}", addr.as_int());
    }
};

} // namespace fmt
