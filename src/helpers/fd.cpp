#include <lo2s/helpers/fd.hpp>

namespace lo2s
{
    bool operator==(const WeakFd&lhs, const Fd&rhs)
    {
        return lhs.fd_ == rhs.fd_;
    }
    bool operator!=(const WeakFd&lhs, const Fd&rhs)
    {
        return lhs.fd_ == rhs.fd_;
    }
}
