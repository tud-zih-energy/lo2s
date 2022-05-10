
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

struct ByCore
{
    using key_type = typename std::pair<int, int>;
    key_type key;

    ByCore(int core, int package) : key(std::make_pair(core, package))
    {
    }
};

// Can i haz strong typedef
struct ByPackageTag
{
};
using ByPackage = SimpleKeyType<int, ByPackageTag>;

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

struct ByDevTag
{
};
using ByDev = SimpleKeyType<dev_t, ByDevTag>;

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

template <typename Definition>
struct Holder
{
    using type = typename otf2::get_default_holder<Definition>::type;
};
template <>
struct Holder<otf2::definition::system_tree_node>
{
    using type = otf2::lookup_definition_holder<otf2::definition::system_tree_node, ByCore,
                                                ByProcess, ByDev, ByCpu, ByPackage>;
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
    using type = otf2::lookup_definition_holder<otf2::definition::metric_class, ByString>;
};
template <>
struct Holder<otf2::definition::io_handle>
{
    using type = otf2::lookup_definition_holder<otf2::definition::io_handle, ByDev>;
};
template <>
struct Holder<otf2::definition::io_regular_file>
{
    using type = otf2::lookup_definition_holder<otf2::definition::io_regular_file, ByDev>;
};
template <>
struct Holder<otf2::definition::string>
{
    using type = otf2::lookup_definition_holder<otf2::definition::string, ByString>;
};
template <>
struct Holder<otf2::definition::location_group>
{
    using type = otf2::lookup_definition_holder<otf2::definition::location_group,
                                                ByMeasurementScope, ByExecutionScope, ByDev>;
};
template <>
struct Holder<otf2::definition::location>
{
    using type = otf2::lookup_definition_holder<otf2::definition::location, ByExecutionScope,
                                                ByMeasurementScope, ByDev>;
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
    using type = otf2::lookup_definition_holder<otf2::definition::comm, ByProcess, ByDev>;
};
template <>
struct Holder<otf2::definition::comm_group>
{
    using type = otf2::lookup_definition_holder<otf2::definition::comm_group, ByProcess>;
};
} // namespace trace
} // namespace lo2s
