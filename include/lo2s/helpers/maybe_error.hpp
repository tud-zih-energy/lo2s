#pragma once

#include <stdexcept>

namespace lo2s
{
template <class E>
class MaybeError
{
public:
    MaybeError(E&& e) : ok_(false), is_moved_(false), e_(std::move(e))
    {
    }

    MaybeError() : ok_(true), is_moved_(true)
    {
    }

    bool ok()
    {
        return ok_;
    }

    E&& unpack_error()
    {
        if (ok_)
        {
            throw std::runtime_error("Can not get error from result if state is ok!");
        }

        if(is_moved_)
        {
            throw std::runtime_error("Error has already been unpacked!");
        }
        return std::move(e_);
    }

     void throw_if_error()
    {
        if(!ok_)
        {
            e_.throw_as_exception();
        }
    }

    ~MaybeError()
    {
        if (!ok_)
        {
            e_.~E();
        }
    }

private:
    bool ok_;

    bool is_moved_;
    union
    {
        E e_;
        char placeholder_;
    };
};
} // namespace nlb
