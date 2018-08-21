/*
 * This file is part of the lo2s software.
 * Linux OTF2 sampling
 *
 * Copyright (c) 2016,
 *    Technische Universitaet Dresden, Germany
 *
 * lo2s is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * lo2s is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with lo2s.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <cstdint>
#include <cstdlib>
#include <otf2xx/common.hpp>
#include <stdexcept>

namespace lo2s
{
namespace metric
{
namespace plugin
{
namespace wrapper
{

template <class T>
struct MallocDelete
{
    void operator()(T* ptr) const
    {
        free(ptr);
    }
};

enum class Mode
{
    ACCUMULATED_START = 0,
    ACCUMULATED_POINT = 1,
    ACCUMULATED_LAST = 2,
    ACCUMULATED_NEXT = 3,
    ABSOLUTE_POINT = 4,
    ABSOLUTE_LAST = 5,
    ABSOLUTE_NEXT = 6,
    RELATIVE_POINT = 7,
    RELATIVE_LAST = 8,
    RELATIVE_NEXT = 9,
};

inline otf2::common::metric_mode convert_mode(Mode scorep_mode)
{
    using mm = otf2::common::metric_mode;
    switch (scorep_mode)
    {
    case Mode::ACCUMULATED_START:
        return mm::accumulated_start;
    case Mode::ACCUMULATED_POINT:
        return mm::accumulated_point;
    case Mode::ACCUMULATED_LAST:
        return mm::accumulated_last;
    case Mode::ACCUMULATED_NEXT:
        return mm::accumulated_next;
    case Mode::ABSOLUTE_POINT:
        return mm::absolute_point;
    case Mode::ABSOLUTE_LAST:
        return mm::absolute_last;
    case Mode::ABSOLUTE_NEXT:
        return mm::absolute_next;
    case Mode::RELATIVE_POINT:
        return mm::relative_point;
    case Mode::RELATIVE_LAST:
        return mm::relative_last;
    case Mode::RELATIVE_NEXT:
        return mm::relative_next;
    default:
        // I don't want to live on this planet anymore
        throw std::runtime_error("Unexpected Mode.");
    }
}

enum class ValueType
{
    INT64,
    UINT64,
    DOUBLE
};

inline otf2::common::type convert_type(wrapper::ValueType value_type)
{
    switch (value_type)
    {
    case wrapper::ValueType::INT64:
        return otf2::common::type::int64;
    case wrapper::ValueType::UINT64:
        return otf2::common::type::uint64;
    case wrapper::ValueType::DOUBLE:
        return otf2::common::type::Double;
    default:
        throw std::runtime_error("Unexpected value type given");
    }
}

enum class ValueBase
{
    BINARY = 0,
    DECIMAL = 1
};

inline otf2::common::base_type convert_base(wrapper::ValueBase value_base)
{
    switch (value_base)
    {
    case wrapper::ValueBase::BINARY:
        return otf2::common::base_type::binary;
    case wrapper::ValueBase::DECIMAL:
        return otf2::common::base_type::decimal;
    default:
        throw std::runtime_error("Unexpected value base given");
    }
}

enum class Per
{
    THREAD = 0,
    PROCESS,
    HOST,
    ONCE
};

enum class Synchronicity
{
    STRICTLY_SYNC = 0,
    SYNC,
    ASYNC_EVENT,
    ASYNC
};

enum SynchronizationMode
{
    BEGIN,
    BEGIN_MPP,
    END
};

struct Properties
{
    /** Plugin name */
    char* name;
    /** Additional information about the metric */
    char* description;
    /** Metric mode: valid combination of ACCUMULATED|ABSOLUTE|RELATIVE + POINT|START|LAST|NEXT
     *  @see SCOREP_MetricMode
     * */
    Mode mode;
    /** Value type: signed 64 bit integer, unsigned 64 bit integer, double */
    ValueType value_type;
    /** Base of metric: decimal, binary */
    ValueBase base;
    /** Exponent to scale metric: e.g., 3 for kilo */
    std::int64_t exponent;
    /** Unit string of recorded metric */
    char* unit;
};

struct TimeValuePair
{
    /** Timestamp in Score-P time! */
    std::uint64_t timestamp;
    /** Current metric value */
    std::uint64_t value;
};

struct PluginInfo
{

    uint32_t plugin_version;

    Per run_per;

    Synchronicity sync;

    std::uint64_t delta_t;

    int32_t (*initialize)(void);

    void (*finalize)(void);

    Properties* (*get_event_info)(const char* token);

    int32_t (*add_counter)(const char* metric_name);

    uint64_t (*get_current_value)(std::int32_t id);

    bool (*get_optional_value)(std::int32_t id, std::uint64_t* value);

    void (*set_clock_function)(uint64_t (*clock_time)(void));

    uint64_t (*get_all_values)(int32_t id, TimeValuePair** time_value_list);

    void (*synchronize)(bool is_responsible, SynchronizationMode sync_mode);

    /** Reserved space for future features, should be zeroed */
    // NOTE: The size is WAY bigger than the value in Score-P, but it is necessary to fix
    //       problems with different sizes in different versions.
    std::uint64_t reserved[200];
};
} // namespace wrapper
} // namespace plugin
} // namespace metric
} // namespace lo2s
