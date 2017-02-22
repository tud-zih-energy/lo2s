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

namespace lo2s
{
namespace metric
{
namespace plugin
{
namespace wrapper
{

template<class T>
struct MallocDelete
{
    void operator()(T *ptr) const
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

enum class ValueType
{
    INT64,
    UINT64,
    DOUBLE
};

enum class ValueBase
{
    BINARY = 0,
    DECIMAL = 1
};

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
    char *name;
    /** Additional information about the metric */
    char *description;
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
    char *unit;
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

    Properties *(*get_event_info)(const char *token);

    int32_t (*add_counter)(const char *metric_name);

    uint64_t (*get_current_value)(std::int32_t id);

    bool (*get_optional_value)(std::int32_t id, std::uint64_t *value);

    void (*set_clock_function)(uint64_t (*clock_time)(void));

    uint64_t (*get_all_values)(int32_t id, TimeValuePair **time_value_list);

    void (*synchronize)(bool is_responsible, SynchronizationMode sync_mode);

    /** Reserved space for future features, should be zeroed */
    // NOTE: The size is WAY bigger than the value in Score-P, but it is necessary to fix
    //       problems with different sizes in different versions.
    std::uint64_t reserved[200];
};
}
}
}
}
