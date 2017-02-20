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

#include <map>
#include <mutex>

using namespace std::literals::string_literals;

namespace lo2s
{
namespace trace
{

constexpr pid_t trace::METRIC_PID;

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
    log::info() << "Using trace directory: " << prefix;
    return prefix;
}

trace::trace(uint64_t sample_period, const std::string& trace_path)
: archive_(get_trace_name(trace_path), "traces"),
  system_tree_root_node_(0, intern(get_hostname()), intern("machine")),
  interrupt_generator_(0u, intern("perf HW_INSTRUCTIONS"),
                       otf2::common::interrupt_generator_mode_type::count,
                       otf2::common::base_type::decimal, 0, sample_period),
  // Yep, the locations group must have id below the self group.
  self_group_(1, intern("self thread"), otf2::definition::comm_self_group::paradigm_type::pthread,
              otf2::definition::comm_self_group::group_flag_type::none),
  self_comm_(0u, intern("self thread"), self_group_)
{
    process(METRIC_PID, "Metric Location Group");

    int otf2_id = 1;
    const auto& sys = topology::instance();
    for (auto& package : sys.packages())
    {
        log::debug() << "Registering package " << package.id;
        const auto& parent = system_tree_root_node_;
        system_tree_package_nodes_.emplace(
            std::piecewise_construct, std::forward_as_tuple(package.id),
            std::forward_as_tuple(otf2_id++, intern(std::to_string(package.id)), intern("package"),
                                  parent));
    }
    for (auto& core : sys.cores())
    {
        log::debug() << "Registering core " << core.id << "@" << core.package_id;
        const auto& parent = system_tree_package_nodes_.at(core.package_id);
        system_tree_core_nodes_.emplace(
            std::piecewise_construct, std::forward_as_tuple(core.id, core.package_id),
            std::forward_as_tuple(
                otf2_id++, intern(std::to_string(core.package_id) + ":"s + std::to_string(core.id)),
                intern("core"), parent));
    }
    for (auto& cpu : sys.cpus())
    {
        log::debug() << "Registering cpu " << cpu.id << "@" << cpu.core_id << ":" << cpu.package_id;
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

void trace::begin_record()
{
    log::info() << "Initialization done. Start recording...";
    starting_time_ = time::now();
}

void trace::end_record()
{
    stopping_time_ = time::now();
    log::info() << "Recording done. Start finalization...";
}

otf2::chrono::time_point trace::record_from() const
{
    return starting_time_;
}

otf2::chrono::time_point trace::record_to() const
{
    return stopping_time_;
}

trace::~trace()
{
    auto function_groups_ = function_groups();
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
    archive_ << location_groups_;
    archive_ << locations_;
    archive_ << source_code_locations_;
    archive_ << regions_;

    archive_ << comm_locations_group;
    archive_ << self_group_;
    archive_ << function_groups_;

    archive_ << self_comm_;
    archive_ << interrupt_generator();
    archive_ << calling_contexts_;
    archive_ << calling_context_properties_;
    archive_ << metric_members_;
    archive_ << metric_classes_;
    archive_ << metric_instances_;
}

std::map<std::string, otf2::definition::regions_group> trace::function_groups()
{
    std::map<std::string, otf2::definition::regions_group> groups;
    for (const auto& elem : regions_)
    {
        const auto& info = elem.first;
        const auto& name = info.dso;
        const auto& region = elem.second;
        if (groups.count(name) == 0)
        {
            // + 2 for locations_group_ and self_group_
            groups.emplace(std::piecewise_construct, std::forward_as_tuple(name),
                           std::forward_as_tuple(groups.size() + 2, intern(name),
                                                 otf2::common::paradigm_type::compiler,
                                                 otf2::common::group_flag_type::none));
        }
        groups.at(name).add_member(region);
    }
    return groups;
}

void trace::process(pid_t pid, const std::string& name)
{
    location_groups_.emplace(
        std::piecewise_construct, std::forward_as_tuple(pid),
        std::forward_as_tuple(location_groups_.size(), intern(name),
                              otf2::definition::location_group::location_group_type::process,
                              system_tree_root_node_));
}

otf2::writer::local& trace::sample_writer(pid_t pid, pid_t tid)
{
    auto name = (boost::format("thread %d") % tid).str();
    auto location = locations_.emplace(locations_.size(), intern(name), location_groups_.at(pid),
                                       otf2::definition::location::location_type::cpu_thread);
    return archive()(location);
}

otf2::writer::local& trace::metric_writer(pid_t pid, pid_t tid)
{
    auto name = (boost::format("metrics for thread %d") % tid).str();
    auto location = locations_.emplace(locations_.size(), intern(name), location_groups_.at(pid),
                                       otf2::definition::location::location_type::metric);
    return archive()(location);
}

otf2::writer::local& trace::metric_writer(const std::string& name)
{
    auto location =
        locations_.emplace(locations_.size(), intern(name), location_groups_.at(METRIC_PID),
                           otf2::definition::location::location_type::metric);
    return archive()(location);
}

otf2::definition::metric_member trace::metric_member(const std::string& name,
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
trace::metric_instance(otf2::definition::metric_class metric_class,
                       otf2::definition::location recorder, otf2::definition::location scope)
{
    auto ref = metric_instances_.size() + metric_classes_.size();
    return metric_instances_.emplace(ref, metric_class, recorder, scope);
}

otf2::definition::metric_instance
trace::metric_instance(otf2::definition::metric_class metric_class,
                       otf2::definition::location recorder,
                       otf2::definition::system_tree_node scope)
{
    auto ref = metric_instances_.size() + metric_classes_.size();
    return metric_instances_.emplace(ref, metric_class, recorder, scope);
}

otf2::definition::metric_class trace::metric_class()
{
    auto ref = metric_instances_.size() + metric_classes_.size();
    return metric_classes_.emplace(ref, otf2::common::metric_occurence::async,
                                   otf2::common::recorder_kind::abstract);
}

static line_info region_info(line_info info)
{
    // TODO lookup actual line info
    info.line = 1;
    return info;
}

void trace::merge_ips(ip_ref_map& new_children, ip_cctx_map& children,
                      std::vector<uint32_t>& mapping_table,
                      otf2::definition::calling_context parent, const memory_map& maps)
{

    for (auto& elem : new_children)
    {
        auto& ip = elem.first;
        auto& local_ref = elem.second.ref;
        auto& local_children = elem.second.children;
        auto line_info = maps.lookup_line_info(ip);
        log::debug() << "resolved " << ip << ": " << line_info;
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
                log::debug() << "mapped " << ip << " to " << instruction;
                value.stringRef = intern(instruction).ref();
                calling_context_properties_.emplace(new_cctx, intern("instruction"),
                                                    otf2::common::type::string, value);
            }
            catch (std::exception& ex)
            {
                log::debug() << "could not read instruction from " << ip << ": " << ex.what();
            }
        }
        const auto& cctx = cctx_it->second.cctx;
        mapping_table.at(local_ref) = cctx.ref();

        merge_ips(local_children, cctx_it->second.children, mapping_table, cctx, maps);
    }
}

otf2::definition::mapping_table trace::merge_ips(ip_ref_map& new_ips, uint64_t ip_count,
                                                 const memory_map& maps)
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

otf2::definition::source_code_location trace::intern_scl(const line_info& info)
{
    auto ref = source_code_locations_.size();
    auto ret =
        source_code_locations_.emplace(std::piecewise_construct, std::forward_as_tuple(info),
                                       std::forward_as_tuple(ref, intern(info.file), info.line));
    return ret.first->second;
}

otf2::definition::region trace::intern_region(const line_info& info)
{
    auto ref = regions_.size();
    auto name_str = intern(info.function);
    auto ret = regions_.emplace(
        std::piecewise_construct, std::forward_as_tuple(info),
        std::forward_as_tuple(ref, name_str, name_str, name_str, otf2::common::role_type::function,
                              otf2::common::paradigm_type::sampling, otf2::common::flags_type::none,
                              intern(info.file), info.line, 0));
    return ret.first->second;
}

otf2::definition::string trace::intern(const std::string& name)
{
    auto ref = strings_.size();
    auto ret = strings_.emplace(std::piecewise_construct, std::forward_as_tuple(name),
                                std::forward_as_tuple(ref, name));
    return ret.first->second;
}
}
}
