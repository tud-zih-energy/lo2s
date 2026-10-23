// SPDX-FileCopyrightText: 2016 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <lo2s/types/thread.hpp>

#include <map>
#include <mutex>

namespace lo2s
{
class ThreadNames
{
public:
    static ThreadNames& instance()
    {
        static ThreadNames tn;
        return tn;
    }

    void set(Thread t, const std::string& name)
    {
        const std::lock_guard<std::recursive_mutex> g(m_);
        thread_names_[t] = name;
    }

    bool has(Thread t)
    {
        const std::lock_guard<std::recursive_mutex> g(m_);
        return thread_names_.count(t) != 0;
    }

    std::string get(Thread t)
    {
        const std::lock_guard<std::recursive_mutex> g(m_);
        if (!has(t))
        {
            return "<unknown thread>";
        }
        return thread_names_.at(t);
    }

    const std::map<Thread, std::string>& names()
    {
        return thread_names_;
    }

private:
    std::recursive_mutex m_;
    std::map<Thread, std::string> thread_names_;
};
} // namespace lo2s
