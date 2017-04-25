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

#include <lo2s/trace/trace.hpp>

#include <lo2s/address.hpp>
#include <lo2s/bfd_resolve.hpp>
#include <lo2s/line_info.hpp>
#include <lo2s/mmap.hpp>
#include <lo2s/time/time.hpp>
#include <lo2s/topology.hpp>
#include <lo2s/util.hpp>

#include <nitro/env/hostname.hpp>

#include <boost/format.hpp>

#include <map>
#include <mutex>
#include <stdexcept>
#include <tuple>

using namespace std::literals::string_literals;

namespace lo2s
{
namespace trace
{

constexpr pid_t Trace::METRIC_PID;

std::string get_trace_name(std::string prefix = "", bool append_time = false)
{
    if (prefix.length() == 0)
    {
        prefix = "lo2s_trace";
        append_time = true;
    }
    if (append_time)
    {
        prefix += "_" + get_datetime();
    }
    Log::info() << "Using trace directory: " << prefix;
    return prefix;
}

Trace::Trace(uint64_t sample_period, const std::string& trace_path)
: archive_(get_trace_name(trace_path), "traces"),
  system_tree_root_node_(0, intern(nitro::env::hostname()), intern("machine")),
  interrupt_generator_(0u, intern("perf HW_INSTRUCTIONS"),
                       otf2::common::interrupt_generator_mode_type::count,
                       otf2::common::base_type::decimal, 0, sample_period),
  // Yep, the locations group must have id below the self group.
  comm_self_group_(1, intern("self thread"),
                   otf2::definition::comm_self_group::paradigm_type::pthread,
                   otf2::definition::comm_self_group::group_flag_type::none),
  self_comm_(0u, intern("self thread"), comm_self_group_)
{
    process(METRIC_PID, "Metric Location Group");

    int otf2_id = 1;
    const auto& sys = Topology::instance();
    for (auto& package : sys.packages())
    {
        Log::debug() << "Registering package " << package.id;
        const auto& parent = system_tree_root_node_;
        system_tree_package_nodes_.emplace(
            std::piecewise_construct, std::forward_as_tuple(package.id),
            std::forward_as_tuple(otf2_id++, intern(std::to_string(package.id)), intern("package"),
                                  parent));
    }
    for (auto& core : sys.cores())
    {
        Log::debug() << "Registering core " << core.id << "@" << core.package_id;
        const auto& parent = system_tree_package_nodes_.at(core.package_id);
        system_tree_core_nodes_.emplace(
            std::piecewise_construct, std::forward_as_tuple(core.id, core.package_id),
            std::forward_as_tuple(
                otf2_id++, intern(std::to_string(core.package_id) + ":"s + std::to_string(core.id)),
                intern("core"), parent));
    }
    for (auto& cpu : sys.cpus())
    {
        Log::debug() << "Registering cpu " << cpu.id << "@" << cpu.core_id << ":" << cpu.package_id;
        const auto& parent =
            system_tree_core_nodes_.at(std::make_pair(cpu.core_id, cpu.package_id));
        system_tree_cpu_nodes_.emplace(std::piecewise_construct, std::forward_as_tuple(cpu.id),
                                       std::forward_as_tuple(otf2_id++,
                                                             intern(std::to_string(cpu.id)),
                                                             intern("cpu"), parent));
    }
}

template <class IT, class DT>
otf2::writer::archive& operator<<(otf2::writer::archive& ar, const std::map<IT, DT>& map)
{
    static_assert(otf2::traits::is_definition<DT>::value, "not a definition type.");
    for (const auto& elem : map)
    {
        ar << elem.second;
    }
    return ar;
}

void Trace::begin_record()
{
    Log::info() << "Initialization done. Start recording...";
    starting_time_ = time::now();
}

void Trace::end_record()
{
    stopping_time_ = time::now();
    Log::info() << "Recording done. Start finalization...";
}

otf2::chrono::time_point Trace::record_from() const
{
    return starting_time_;
}

otf2::chrono::time_point Trace::record_to() const
{
    return stopping_time_;
}

Trace::~Trace()
{
    auto sampling_function_groups_ = regions_groups_sampling_dso();
    otf2::definition::comm_locations_group comm_locations_group(
        0, intern("All pthread locations"), otf2::common::paradigm_type::pthread,
        otf2::common::group_flag_type::none);
    for (const auto& location : locations_)
    {
        if (location.type() == otf2::common::location_type::cpu_thread)
        {
            comm_locations_group.add_member(location);
        }
    }

    archive_ << otf2::definition::clock_properties(starting_time_, stopping_time_);
    archive_ << strings_;
    archive_ << system_tree_root_node_;
    archive_ << system_tree_package_nodes_;
    archive_ << system_tree_core_nodes_;
    archive_ << system_tree_cpu_nodes_;
    archive_ << location_groups_process_;
    archive_ << location_groups_cpu_;
    archive_ << locations_;
    archive_ << source_code_locations_;
    archive_ << regions_line_info_;
    archive_ << regions_thread_;

    archive_ << comm_locations_group;
    archive_ << comm_self_group_;
    archive_ << sampling_function_groups_;
    archive_ << regions_groups_executable_;
    archive_ << regions_groups_monitoring_;

    archive_ << self_comm_;
    archive_ << interrupt_generator();
    archive_ << calling_contexts_;
    archive_ << calling_context_properties_;
    archive_ << metric_members_;
    archive_ << metric_classes_;
    archive_ << metric_instances_;
}

std::map<std::string, otf2::definition::regions_group> Trace::regions_groups_sampling_dso()
{
    std::map<std::string, otf2::definition::regions_group> groups;
    for (const auto& elem : regions_line_info_)
    {
        const auto& info = elem.first;
        const auto& name = info.dso;
        const auto& region = elem.second;
        if (groups.count(name) == 0)
        {
            groups.emplace(std::piecewise_construct, std::forward_as_tuple(name),
                           std::forward_as_tuple(groups.size() + group_ref(), intern(name),
                                                 otf2::common::paradigm_type::compiler,
                                                 otf2::common::group_flag_type::none));
        }
        groups.at(name).add_member(region);
    }
    return groups;
}

void Trace::process(pid_t pid, const std::string& name)
{
    auto r = location_groups_process_.emplace(
        std::piecewise_construct, std::forward_as_tuple(pid),
        std::forward_as_tuple(location_group_ref(), intern(name),
                              otf2::definition::location_group::location_group_type::process,
                              system_tree_root_node_));
    if (r.second == false)
    {
        r.first->second.name(intern(name));
    }
}

otf2::writer::local& Trace::sample_writer(pid_t pid, pid_t tid)
{
    auto name = (boost::format("thread %d") % tid).str();
    auto location =
        locations_.emplace(locations_.size(), intern(name), location_groups_process_.at(pid),
                           otf2::definition::location::location_type::cpu_thread);
    return archive()(location);
}

otf2::writer::local& Trace::cpu_writer(int cpuid)
{
    auto name = intern((boost::format("cpu %d") % cpuid).str());
    auto r_group = location_groups_cpu_.emplace(
        std::piecewise_construct, std::forward_as_tuple(cpuid),
        std::forward_as_tuple(location_group_ref(), name,
                              otf2::definition::location_group::location_group_type::unknown,
                              system_tree_cpu_nodes_.at(cpuid)));
    const auto& group = r_group.first->second;

    auto location = locations_.emplace(locations_.size(), name, group,
                                       otf2::definition::location::location_type::cpu_thread);
    return archive()(location);
}

otf2::writer::local& Trace::metric_writer(pid_t pid, pid_t tid)
{
    auto name = (boost::format("metrics for thread %d") % tid).str();
    auto location =
        locations_.emplace(locations_.size(), intern(name), location_groups_process_.at(pid),
                           otf2::definition::location::location_type::metric);
    return archive()(location);
}

otf2::writer::local& Trace::metric_writer(const std::string& name)
{
    auto location =
        locations_.emplace(locations_.size(), intern(name), location_groups_process_.at(METRIC_PID),
                           otf2::definition::location::location_type::metric);
    return archive()(location);
}

otf2::definition::metric_member Trace::metric_member(const std::string& name,
                                                     const std::string& description,
                                                     otf2::common::metric_mode mode,
                                                     otf2::common::type value_type,
                                                     const std::string& unit)
{
    auto ref = metric_members_.size();
    return metric_members_.emplace(ref, intern(name), intern(description),
                                   otf2::common::metric_type::other, mode, value_type,
                                   otf2::common::base_type::decimal, 0, intern(unit));
}

otf2::definition::metric_instance
Trace::metric_instance(otf2::definition::metric_class metric_class,
                       otf2::definition::location recorder, otf2::definition::location scope)
{
    auto ref = metric_instances_.size() + metric_classes_.size();
    return metric_instances_.emplace(ref, metric_class, recorder, scope);
}

otf2::definition::metric_instance
Trace::metric_instance(otf2::definition::metric_class metric_class,
                       otf2::definition::location recorder,
                       otf2::definition::system_tree_node scope)
{
    auto ref = metric_instances_.size() + metric_classes_.size();
    return metric_instances_.emplace(ref, metric_class, recorder, scope);
}

otf2::definition::metric_class Trace::metric_class()
{
    auto ref = metric_instances_.size() + metric_classes_.size();
    return metric_classes_.emplace(ref, otf2::common::metric_occurence::async,
                                   otf2::common::recorder_kind::abstract);
}

static LineInfo region_info(LineInfo info)
{
    // TODO lookup actual line info
    info.line = 1;
    return info;
}

void Trace::merge_ips(IpRefMap& new_children, IpCctxMap& children,
                      std::vector<uint32_t>& mapping_table,
                      otf2::definition::calling_context parent, const MemoryMap& maps)
{

    for (auto& elem : new_children)
    {
        auto& ip = elem.first;
        auto& local_ref = elem.second.ref;
        auto& local_children = elem.second.children;
        auto line_info = maps.lookup_line_info(ip);
        Log::trace() << "resolved " << ip << ": " << line_info;
        auto cctx_it = children.find(ip);
        if (cctx_it == children.end())
        {
            auto ref = calling_contexts_.size();
            auto region = intern_region(region_info(line_info));
            auto scl = intern_scl(line_info);
            auto new_cctx = calling_contexts_.emplace(ref, region, scl, parent);
            auto r = children.emplace(ip, new_cctx);
            assert(r.second);
            cctx_it = r.first;

            try
            {
                // TODO do not write properties for useless unknown stuff
                OTF2_AttributeValue_union value;
                auto instruction = maps.lookup_instruction(ip);
                Log::trace() << "mapped " << ip << " to " << instruction;
                value.stringRef = intern(instruction).ref();
                calling_context_properties_.emplace(new_cctx, intern("instruction"),
                                                    otf2::common::type::string, value);
            }
            catch (std::exception& ex)
            {
                Log::trace() << "could not read instruction from " << ip << ": " << ex.what();
            }
        }
        const auto& cctx = cctx_it->second.cctx;
        mapping_table.at(local_ref) = cctx.ref();

        merge_ips(local_children, cctx_it->second.children, mapping_table, cctx, maps);
    }
}

otf2::definition::mapping_table Trace::merge_ips(IpRefMap& new_ips, uint64_t ip_count,
                                                 const MemoryMap& maps)
{
    std::lock_guard<std::mutex> guard(mutex_);

    if (new_ips.empty())
    {
        // Otherwise the mapping table is empty and OTF2 cries like a little girl
        throw std::runtime_error("no ips");
    }

#ifndef NDEBUG
    std::vector<uint32_t> mappings(ip_count, -1u);
#else
    std::vector<uint32_t> mappings(ip_count);
#endif

    merge_ips(new_ips, calling_context_tree_, mappings, otf2::definition::calling_context(), maps);

#ifndef NDEBUG
    for (auto id : mappings)
    {
        assert(id != -1u);
    }
#endif

    return otf2::definition::mapping_table(
        otf2::definition::mapping_table::mapping_type_type::calling_context, mappings);
}

void Trace::register_tid(pid_t tid, const std::string& exe)
{
    // Lock this to avoid conflict on regions_thread_ with register_monitoring_tid
    std::lock_guard<std::mutex> guard(mutex_);

    auto ref = region_ref();
    auto iname = intern((boost::format("%s (%d)") % exe % tid).str());
    auto ret = regions_thread_.emplace(
        std::piecewise_construct, std::forward_as_tuple(tid),
        std::forward_as_tuple(ref, iname, iname, iname, otf2::common::role_type::function,
                              otf2::common::paradigm_type::user, otf2::common::flags_type::none,
                              iname, 0, 0));
    if (ret.second)
    {
        regions_group_executable(exe).add_member(ret.first->second);
    }
    // TODO update iname if not newly inserted
}

void Trace::register_monitoring_tid(pid_t tid, const std::string& name, const std::string& group)
{
    // We must guard this here because this is called by monitoring threads itself rather than
    // the usual call from the single monitoring process
    std::lock_guard<std::mutex> guard(mutex_);

    Log::debug() << "register_monitoring_tid(" << tid << "," << name << "," << group << ");";
    auto ref = region_ref();
    auto iname = intern((boost::format("lo2s::%s") % name).str());

    // TODO, should be paradigm_type::measurement_system, but that's a bug in Vampir
    auto ret = regions_thread_.emplace(
        std::piecewise_construct, std::forward_as_tuple(tid),
        std::forward_as_tuple(ref, iname, iname, iname, otf2::common::role_type::function,
                              otf2::common::paradigm_type::user,
                              otf2::common::flags_type::none, iname, 0, 0));
    if (ret.second)
    {
        (void)group;
        // Put all lo2s stuff in one single group, ignore the group within lo2s
        regions_group_monitoring("lo2s").add_member(ret.first->second);
    }
}

void Trace::register_tids(const std::unordered_map<pid_t, std::string>& tid_map)
{
    Log::debug() << "register_tid size " << tid_map.size();
    for (const auto& elem : tid_map)
    {
        register_tid(elem.first, elem.second);
    }
}

otf2::definition::mapping_table Trace::merge_tids(
    const std::unordered_map<pid_t, otf2::definition::region::reference_type>& local_refs)
{
#ifndef NDEBUG
    std::vector<uint32_t> mappings(local_refs.size(), -1u);
#else
    std::vector<uint32_t> mappings(local_refs.size());
#endif
    for (const auto& elem : local_refs)
    {
        auto pid = elem.first;
        auto local_ref = elem.second;
        if (regions_thread_.count(pid) == 0)
        {
            register_tid(pid, "<unknown>");
        }
        auto global_ref = regions_thread_.at(pid).ref();
        mappings.at(local_ref) = global_ref;
    }

#ifndef NDEBUG
    for (auto id : mappings)
    {
        assert(id != -1u);
    }
#endif

    return otf2::definition::mapping_table(
        otf2::definition::mapping_table::mapping_type_type::region, mappings);
}

otf2::definition::regions_group Trace::regions_group_executable(const std::string& name)
{
    auto ret = regions_groups_executable_.emplace(
        std::piecewise_construct, std::forward_as_tuple(name),
        std::forward_as_tuple(group_ref(), intern(name), otf2::common::paradigm_type::user,
                              otf2::common::group_flag_type::none));
    return ret.first->second;
}

otf2::definition::regions_group Trace::regions_group_monitoring(const std::string& name)
{
    // TODO, should be praradigm_type::measurement_system, but that's a bug in Vampir
    auto ret = regions_groups_monitoring_.emplace(
        std::piecewise_construct, std::forward_as_tuple(name),
        std::forward_as_tuple(group_ref(), intern(name),
                              otf2::common::paradigm_type::user,
                              otf2::common::group_flag_type::none));
    return ret.first->second;
}

otf2::definition::source_code_location Trace::intern_scl(const LineInfo& info)
{
    auto ref = source_code_locations_.size();
    auto ret =
        source_code_locations_.emplace(std::piecewise_construct, std::forward_as_tuple(info),
                                       std::forward_as_tuple(ref, intern(info.file), info.line));
    return ret.first->second;
}

otf2::definition::region Trace::intern_region(const LineInfo& info)
{
    auto ref = region_ref();
    auto name_str = intern(info.function);
    auto ret = regions_line_info_.emplace(
        std::piecewise_construct, std::forward_as_tuple(info),
        std::forward_as_tuple(ref, name_str, name_str, name_str, otf2::common::role_type::function,
                              otf2::common::paradigm_type::sampling, otf2::common::flags_type::none,
                              intern(info.file), info.line, 0));
    return ret.first->second;
}

otf2::definition::string Trace::intern(const std::string& name)
{
    auto ref = strings_.size();
    auto ret = strings_.emplace(std::piecewise_construct, std::forward_as_tuple(name),
                                std::forward_as_tuple(ref, name));
    return ret.first->second;
}
}
}
