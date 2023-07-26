
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
#include <lo2s/address.hpp>
#include <lo2s/execution_scope.hpp>
#include <lo2s/line_info.hpp>
#include <lo2s/measurement_scope.hpp>
#include <lo2s/perf/bio/block_device.hpp>
#include <lo2s/perf/counter/counter_collection.hpp>
#include <lo2s/perf/event_description.hpp>
#include <lo2s/util.hpp>
#include <otf2xx/otf2.hpp>

#include <string>

extern "C"
{
#include <sys/types.h>
}
namespace lo2s
{
namespace trace
{

template <typename KeyType, typename tag>
struct SimpleKeyType
{
    using key_type = KeyType;
    key_type key;

    explicit SimpleKeyType(key_type key) : key(key)
    {
    }
};

// Can i haz strong typedef
struct ByCoreTag
{
};
using ByCore = SimpleKeyType<Core, ByCoreTag>;

struct ByPackageTag
{
};
using ByPackage = SimpleKeyType<Package, ByPackageTag>;

struct ByCpuTag
{
};
using ByCpu = SimpleKeyType<Cpu, ByCpuTag>;

struct ByThreadTag
{
};
using ByThread = SimpleKeyType<Thread, ByThreadTag>;

struct ByProcessTag
{
};
using ByProcess = SimpleKeyType<Process, ByProcessTag>;

struct ByBlockDeviceTag
{
};
using ByBlockDevice = SimpleKeyType<BlockDevice, ByBlockDeviceTag>;

struct ByStringTag
{
};
using ByString = SimpleKeyType<std::string, ByStringTag>;

struct BySyscallTag
{
};
using BySyscall = SimpleKeyType<int64_t, BySyscallTag>;
struct ByAddressTag
{
};
using ByAddress = SimpleKeyType<Address, ByAddressTag>;

struct ByLineInfoTag
{
};
using ByLineInfo = SimpleKeyType<LineInfo, ByLineInfoTag>;

struct ByExecutionScopeTag
{
};
using ByExecutionScope = SimpleKeyType<ExecutionScope, ByExecutionScopeTag>;

struct ByMeasurementScopeTag
{
};
using ByMeasurementScope = SimpleKeyType<MeasurementScope, ByMeasurementScopeTag>;

struct ByEventDescriptionTag
{
};
using ByEventDescription = SimpleKeyType<perf::EventDescription, ByEventDescriptionTag>;

struct ByCounterCollectionTag
{
};

using ByCounterCollection = SimpleKeyType<perf::counter::CounterCollection, ByCounterCollectionTag>;

struct ThreadFdInstance
{
public:
    ThreadFdInstance() : thread(Thread::invalid()), fd(-1), instance(0)
    {
    }
    ThreadFdInstance(Thread thread, int fd, int instance)
    : thread(thread), fd(fd), instance(instance)
    {
    }
    friend bool operator==(const ThreadFdInstance& lhs, const ThreadFdInstance& rhs)
    {
        return lhs.thread == rhs.thread && lhs.fd == rhs.fd && lhs.instance == rhs.instance;
    }
    friend bool operator<(const ThreadFdInstance& lhs, const ThreadFdInstance& rhs)
    {
        if (lhs.thread == rhs.thread)
        {
            if (lhs.fd == rhs.fd)
            {
                return lhs.instance < rhs.instance;
            }
            return lhs.fd < rhs.fd;
        }
        return lhs.thread < rhs.thread;
    }

private:
    Thread thread;
    int fd;
    int instance;
};

struct ByThreadFdInstanceTag
{
};

using ByThreadFdInstance = SimpleKeyType<ThreadFdInstance, ByThreadFdInstanceTag>;

template <typename Definition>
struct Holder
{
    using type = typename otf2::get_default_holder<Definition>::type;
};
template <>
struct Holder<otf2::definition::system_tree_node>
{
    using type = otf2::lookup_definition_holder<otf2::definition::system_tree_node, ByCore,
                                                ByProcess, ByBlockDevice, ByCpu, ByPackage>;
};
template <>
struct Holder<otf2::definition::regions_group>
{
    using type = otf2::lookup_definition_holder<otf2::definition::regions_group, ByString,
                                                ByProcess, ByThread>;
};
template <>
struct Holder<otf2::definition::metric_class>
{
    using type = otf2::lookup_definition_holder<otf2::definition::metric_class, ByString,
                                                ByCounterCollection>;
};

template <>
struct Holder<otf2::definition::metric_member>
{
    using type = otf2::lookup_definition_holder<otf2::definition::metric_member, ByString,
                                                ByEventDescription>;
};
template <>
struct Holder<otf2::definition::io_handle>
{
    using type = otf2::lookup_definition_holder<otf2::definition::io_handle, ByThreadFdInstance,
                                                ByBlockDevice>;
};
template <>
struct Holder<otf2::definition::io_regular_file>
{
    using type =
        otf2::lookup_definition_holder<otf2::definition::io_regular_file, ByString, ByBlockDevice>;
};
template <>
struct Holder<otf2::definition::string>
{
    using type = otf2::lookup_definition_holder<otf2::definition::string, ByString>;
};
template <>
struct Holder<otf2::definition::location_group>
{
    using type =
        otf2::lookup_definition_holder<otf2::definition::location_group, ByMeasurementScope,
                                       ByExecutionScope, ByBlockDevice>;
};
template <>
struct Holder<otf2::definition::location>
{
    using type = otf2::lookup_definition_holder<otf2::definition::location, ByExecutionScope,
                                                ByMeasurementScope, ByBlockDevice>;
};
template <>
struct Holder<otf2::definition::region>
{
    using type =
        otf2::lookup_definition_holder<otf2::definition::region, ByThread, ByLineInfo, BySyscall>;
};
template <>
struct Holder<otf2::definition::calling_context>
{
    using type =
        otf2::lookup_definition_holder<otf2::definition::calling_context, ByThread, BySyscall>;
};
template <>
struct Holder<otf2::definition::source_code_location>
{
    using type = otf2::lookup_definition_holder<otf2::definition::source_code_location, ByLineInfo,
                                                BySyscall>;
};
template <>
struct Holder<otf2::definition::comm>
{
    using type = otf2::lookup_definition_holder<otf2::definition::comm, ByProcess,
                                                ByThreadFdInstance, ByBlockDevice>;
};
template <>
struct Holder<otf2::definition::comm_group>
{
    using type = otf2::lookup_definition_holder<otf2::definition::comm_group, ByProcess>;
};
} // namespace trace
} // namespace lo2s
