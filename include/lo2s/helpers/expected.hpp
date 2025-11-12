#pragma once

#include <stdexcept>

namespace lo2s
{

template <class E>
class Unexpected
{
public:
    Unexpected(E&& e) : e_(std::move(e)), is_moved_(false)
    {
    }

    E&& unpack()
    {
        if(is_moved_ == true)
        {
            throw std::runtime_error("Unexpected has already been unpacked!");
        }
        is_moved_ = true;
        return std::move(e_);
    }

private:
    E e_;
    bool is_moved_;
};

template <class T, class E>
class Expected
{
public:
    Expected(Unexpected<E>&& e) : ok_(false), e_(std::move(e.unpack())), is_moved_(false)
    {
    }

    Expected(T&& t) : ok_(true), t_(std::move(t)), is_moved_(false)
    {
    }

    bool ok()
    {
        return ok_;
    }

    T&& unpack_ok()
    {
        if (!ok_)
        {
            e_.throw_as_exception();
        }
        
        if(is_moved_)
        {
            throw std::runtime_error("Expected has already been unpacked!");
        }

        is_moved_ = true;
        return std::move(t_);
    }

    E&& unpack_error()
    {
        if (ok_)
        {
            throw std::runtime_error("Can not access error, expected is ok!");
        }
        
        if(is_moved_)
        {
            throw std::runtime_error("Expected has already been unpacked!");
        }

        is_moved_ = true;
        return std::move(e_);
    }

    ~Expected()
    {
        if (!ok_)
        {
            e_.~E();
        }
        else
        {
            t_.~T();
        }
    }

private:
    bool ok_;
    union
    {
        T t_;
        E e_;
    };
    bool is_moved_;
};
} // namespace nlb
