// SPDX-FileCopyrightText: 2017 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <lo2s/perf/clock.hpp>
#include <lo2s/perf/time/reader.hpp>

#include <otf2xx/chrono/duration.hpp>
#include <otf2xx/chrono/time_point.hpp>

#include <cstdint>

namespace lo2s::perf::time
{

class Converter
{
private:
    Converter();

public:
    static Converter& instance()
    {
        static Converter c;
        return c;
    }

    Converter(const Converter&) = default;
    Converter(Converter&&) = default;
    Converter& operator=(const Converter&) = default;
    Converter& operator=(Converter&&) = default;
    ~Converter() = default;

    otf2::chrono::time_point operator()(std::uint64_t perf_raw) const
    {
        return operator()(convert_time_point(perf_raw));
    }

    otf2::chrono::time_point operator()(perf::Clock::time_point perf_tp) const
    {
        return otf2::chrono::time_point(perf_tp.time_since_epoch() + offset);
    }

    perf::Clock::time_point operator()(otf2::chrono::time_point local_tp) const
    {
        return perf::Clock::time_point(local_tp.time_since_epoch() - offset);
    }

private:
    otf2::chrono::duration offset;
};
} // namespace lo2s::perf::time
