
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
#include <lo2s/perf/event_attr.hpp>
#include <lo2s/thread_fd_instance.hpp>
#include <lo2s/types/core.hpp>
#include <lo2s/types/cpu.hpp>
#include <lo2s/types/nec_device.hpp>
#include <lo2s/types/package.hpp>

#include <otf2xx/definition/calling_context.hpp>
#include <otf2xx/definition/comm.hpp>
#include <otf2xx/definition/io_handle.hpp>
#include <otf2xx/definition/io_regular_file.hpp>
#include <otf2xx/definition/location.hpp>
#include <otf2xx/definition/location_group.hpp>
#include <otf2xx/definition/metric_class.hpp>
#include <otf2xx/definition/metric_member.hpp>
#include <otf2xx/definition/region.hpp>
#include <otf2xx/definition/source_code_location.hpp>
#include <otf2xx/definition/string.hpp>
#include <otf2xx/definition/system_tree_node.hpp>
#include <otf2xx/registry.hpp>

#include <string>

#include <cstdint>

namespace lo2s::trace
{

template <typename KeyType, typename tag>
struct SimpleKeyType
{
    using key_type = KeyType;
    key_type key;

    explicit SimpleKeyType(key_type key) : key(std::move(key))
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

struct ByNecThreadTag
{
};

using ByNecThread = SimpleKeyType<Thread, ByNecThreadTag>;

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

struct BySamplingEventName
{
};

using BySamplingEvent = SimpleKeyType<perf::EventAttr, BySamplingEventName>;

struct ByCounterCollectionTag
{
};

using ByCounterCollection = SimpleKeyType<perf::counter::CounterCollection, ByCounterCollectionTag>;

struct ByNecDeviceTag
{
};

using ByNecDevice = SimpleKeyType<NecDevice, ByNecDeviceTag>;

struct ByMeasurementScopeTypeTag
{
};

using ByMeasurementScopeType = SimpleKeyType<MeasurementScopeType, ByMeasurementScopeTypeTag>;

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
    using type = otf2::lookup_definition_holder<otf2::definition::system_tree_node, ByNecDevice,
                                                ByCore, ByProcess, ByBlockDevice, ByCpu, ByPackage>;
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
                                                ByCounterCollection, ByMeasurementScopeType>;
};

template <>
struct Holder<otf2::definition::metric_member>
{
    using type =
        otf2::lookup_definition_holder<otf2::definition::metric_member, ByString, BySamplingEvent>;
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
                                       ByExecutionScope, ByNecThread, ByBlockDevice>;
};

template <>
struct Holder<otf2::definition::location>
{
    using type = otf2::lookup_definition_holder<otf2::definition::location, ByExecutionScope,
                                                ByMeasurementScope, ByNecThread, ByBlockDevice>;
};

template <>
struct Holder<otf2::definition::region>
{
    using type = otf2::lookup_definition_holder<otf2::definition::region, ByProcess, ByThread,
                                                ByLineInfo, BySyscall>;
};

template <>
struct Holder<otf2::definition::calling_context>
{
    using type = otf2::lookup_definition_holder<otf2::definition::calling_context, ByProcess,
                                                ByThread, ByLineInfo, BySyscall>;
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
} // namespace lo2s::trace
