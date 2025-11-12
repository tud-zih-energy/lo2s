#pragma once
#include "lo2s/helpers/errno_error.hpp"
#include "lo2s/helpers/maybe_error.hpp"
#include <fcntl.h>
#include <lo2s/helpers/expected.hpp>
#include <lo2s/helpers/fd.hpp>
#include <unistd.h>

extern "C"
{
#include <sys/mman.h>
}
namespace lo2s
{
    class MemFd : public Fd
    {
        public:
        static Expected<MemFd, ErrnoError> create(std::string name, bool allow_sealing)
        {
            MemFd res;

            int arg = 0;
            if(allow_sealing)
            {
                arg = MFD_ALLOW_SEALING;
            }
            
            res.fd_ = memfd_create(name.c_str(), arg);
            if(res.fd_ == -1)
            {
                return Unexpected(ErrnoError::from_errno());
            }
            return res;
        }

        MaybeError<ErrnoError> seal_grow()
        {
            if(fcntl(fd_, F_ADD_SEALS, F_SEAL_GROW) == -1)
            {
                return ErrnoError::from_errno();
            }
            return {};
        }

        MaybeError<ErrnoError> seal_shrink()
        {
            if(fcntl(fd_, F_ADD_SEALS, F_SEAL_SHRINK) == -1)
            {
                return ErrnoError::from_errno();
            }
            return {};
        }
        
        MaybeError<ErrnoError> seal_sealing()
        {
            if(fcntl(fd_, F_ADD_SEALS, F_SEAL_SEAL) == -1)
            {
                return ErrnoError::from_errno();
            }
            return {};
        }

        MaybeError<ErrnoError> set_size(uint64_t size)
        {
        if (ftruncate(fd_, size) == -1)
        {
            return ErrnoError::from_errno();
        }
        return {};

        }
    };
}
