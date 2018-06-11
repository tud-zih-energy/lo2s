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
#include <lo2s/config.hpp>
#include <lo2s/line_info.hpp>
#include <lo2s/mmap.hpp>
#include <lo2s/summary.hpp>
#include <lo2s/time/time.hpp>
#include <lo2s/topology.hpp>
#include <lo2s/util.hpp>
#include <lo2s/version.hpp>

#include <nitro/env/get.hpp>
#include <nitro/env/hostname.hpp>

#include <boost/algorithm/string/replace.hpp>
#include <boost/format.hpp>

#include <map>
#include <mutex>
#include <regex>
#include <stdexcept>
#include <tuple>

using namespace std::literals::string_literals;

namespace lo2s
{
namespace trace
{

constexpr pid_t Trace::METRIC_PID;

std::string get_trace_name(std::string prefix = "")
{
    if (prefix.size() == 0)
    {
        prefix = nitro::env::get("LO2S_OUTPUT_TRACE");
    }

    if (prefix.size() == 0)
    {
        prefix = "lo2s_trace_{DATE}";
    }

    boost::replace_all(prefix, "{DATE}", get_datetime());
    boost::replace_all(prefix, "{HOSTNAME}", nitro::env::hostname());

    auto result = prefix;

    std::regex env_re("\\{ENV=([^}]*)\\}");
    auto env_begin = std::sregex_iterator(prefix.begin(), prefix.end(), env_re);
    auto env_end = std::sregex_iterator();

    for (std::sregex_iterator it = env_begin; it != env_end; ++it)
    {
        boost::replace_all(result, it->str(), nitro::env::get((*it)[1]));
    }

    return result;
}

Trace::Trace()
: trace_name_(get_trace_name(config().trace_path)), archive_(trace_name_, "traces"),
  system_tree_root_node_(0, intern(nitro::env::hostname()), intern("machine")),
  interrupt_generator_(0u, intern("perf HW_INSTRUCTIONS"),
                       otf2::common::interrupt_generator_mode_type::count,
                       otf2::common::base_type::decimal, 0, config().sampling_period)
{
    Log::info() << "Using trace directory: " << trace_name_;
    summary().set_trace_dir(trace_name_);

    archive_.set_creator(std::string("lo2s - ") + lo2s::version());
    archive_.set_description(config().command_line);

    auto& uname = lo2s::get_uname();
    add_lo2s_property("UNAME::SYSNAME", std::string{ uname.sysname });
    add_lo2s_property("UNAME::NODENAME", std::string{ uname.nodename });
    add_lo2s_property("UNAME::RELEASE", std::string{ uname.release });
    add_lo2s_property("UNAME::VERSION", std::string{ uname.version });
    add_lo2s_property("UNAME::MACHINE", std::string{ uname.machine });

    // TODO clean this up, avoid side effect comm stuff
    attach_process_location_group(system_tree_root_node_, METRIC_PID,
                                  intern("Metric Location Group"));

    const auto& sys = Topology::instance();
    for (auto& package : sys.packages())
    {
        Log::debug() << "Registering package " << package.id;
        const auto& parent = system_tree_root_node_;
        system_tree_package_nodes_.emplace(
            std::piecewise_construct, std::forward_as_tuple(package.id),
            std::forward_as_tuple(system_tree_ref(), intern(std::to_string(package.id)),
                                  intern("package"), parent));
    }
    for (auto& core : sys.cores())
    {
        Log::debug() << "Registering core " << core.id << "@" << core.package_id;
        const auto& parent = system_tree_package_nodes_.at(core.package_id);
        system_tree_core_nodes_.emplace(
            std::piecewise_construct, std::forward_as_tuple(core.id, core.package_id),
            std::forward_as_tuple(
                system_tree_ref(),
                intern(std::to_string(core.package_id) + ":"s + std::to_string(core.id)),
                intern("core"), parent));
    }
    for (auto& cpu : sys.cpus())
    {
        Log::debug() << "Registering cpu " << cpu.id << "@" << cpu.core_id << ":" << cpu.package_id;
        const auto& parent =
            system_tree_core_nodes_.at(std::make_pair(cpu.core_id, cpu.package_id));
        system_tree_cpu_nodes_.emplace(std::piecewise_construct, std::forward_as_tuple(cpu.id),
                                       std::forward_as_tuple(system_tree_ref(),
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
    for (const auto& location : cpu_locations_)
    {
        comm_locations_group.add_member(location.second);
    }

    for (const auto& location : thread_locations_)
    {
        comm_locations_group.add_member(location.second);
    }
    archive_ << otf2::definition::clock_properties(starting_time_, stopping_time_);
    archive_ << strings_;
    archive_ << system_tree_root_node_;
    archive_ << system_tree_package_nodes_;
    archive_ << system_tree_core_nodes_;
    archive_ << system_tree_cpu_nodes_;
    archive_ << system_tree_process_nodes_;
    archive_ << location_groups_process_;
    archive_ << location_groups_cpu_;

    for (const auto& location : thread_locations_)
    {
        archive_ << location.second;
    }
    for (const auto& location : cpu_locations_)
    {
        archive_ << location.second;
    }
    for (const auto& location : metric_locations_)
    {
        archive_ << location.second;
    }
    archive_ << named_locations_;

    archive_ << source_code_locations_;
    archive_ << regions_line_info_;
    archive_ << regions_thread_;

    archive_ << comm_locations_group;
    archive_ << process_comm_groups_;
    archive_ << sampling_function_groups_;
    archive_ << regions_groups_executable_;
    archive_ << regions_groups_monitoring_;

    archive_ << process_comms_;

    archive_ << interrupt_generator();
    archive_ << calling_contexts_;
    archive_ << calling_context_properties_;
    archive_ << metric_members_;
    archive_ << metric_classes_;
    archive_ << metric_instances_;
    archive_ << system_tree_node_properties_;
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

otf2::definition::system_tree_node& Trace::intern_process_node(pid_t pid)
{
    auto node_it = system_tree_process_nodes_.find(pid);

    if (node_it != system_tree_process_nodes_.end())
    {
        return node_it->second;
    }
    else
    {
        return system_tree_root_node_;
    }
}

void Trace::process(pid_t pid, pid_t parent, const std::string& name)
{
    auto iname = intern(name);
    const auto& parent_node =
        (parent == NO_PARENT_PROCESS_PID) ? system_tree_root_node_ : intern_process_node(parent);

    auto emplace_result = system_tree_process_nodes_.emplace(
        std::piecewise_construct, std::forward_as_tuple(pid),
        std::forward_as_tuple(system_tree_ref(), iname, intern("process"), parent_node));

    if (!emplace_result.second) /* emplace failed */
    {
        // process already exists process tree; update its name.
        process_update_executable(pid, iname);
        return;
    }

    const auto& emplaced_node = emplace_result.first->second;
    attach_process_location_group(emplaced_node, pid, iname);
}

void Trace::attach_process_location_group(const otf2::definition::system_tree_node& parent,
                                          pid_t id, const otf2::definition::string& iname)
{
    auto r_lg = location_groups_process_.emplace(
        std::piecewise_construct, std::forward_as_tuple(id),
        std::forward_as_tuple(location_group_ref(), iname,
                              otf2::definition::location_group::location_group_type::process,
                              parent));
    if (!r_lg.second)
    {
        const auto err = boost::format("failed to attach process location group for pid %d (%s)") %
                         id % iname.str();
        throw std::runtime_error(err.str());
    }

    auto r_cg = process_comm_groups_.emplace(
        std::piecewise_construct, std::forward_as_tuple(id),
        std::forward_as_tuple(group_ref(), iname, otf2::common::paradigm_type::pthread,
                              otf2::common::group_flag_type::none));
    assert(r_cg.second);
    const auto& group = r_cg.first->second;
    auto r_c = process_comms_.emplace(
        std::piecewise_construct, std::forward_as_tuple(id),
        std::forward_as_tuple(comm_ref(), iname, group, otf2::definition::comm::undefined()));
    assert(r_c.second);
    (void)(r_c.second);
}

void Trace::process_update_executable(pid_t pid, const otf2::definition::string& exe_name)
{
    system_tree_process_nodes_.at(pid).name(exe_name);
    location_groups_process_.at(pid).name(exe_name);

    process_comm_groups_.at(pid).name(exe_name);
    process_comms_.at(pid).name(exe_name);
}

void Trace::process_update_executable(pid_t pid, const std::string& exe_name)
{
    process_update_executable(pid, intern(exe_name));
}

void Trace::add_lo2s_property(const std::string& name, const std::string& value)
{
    std::string property_name{ "LO2S::" };
    property_name.append(name);

    // Add to trace properties.  This is likely not the place to put this
    // information, but it's easily accessible in trace analysis tools.
    archive_.set_property(property_name, value);

    // Add to machine-specific properties which are stored in the system tree
    // root node.  This is the correct way (TM).
    system_tree_node_properties_.emplace(system_tree_root_node_, intern(property_name),
                                         otf2::attribute_value{ intern(value) });
}

otf2::writer::local& Trace::sample_writer(pid_t pid, pid_t tid)
{
    auto name = (boost::format("thread %d") % tid).str();

    // As the tid is unique in this context, create only one writer/location per tid
    auto location = thread_locations_.emplace(
        std::piecewise_construct, std::forward_as_tuple(tid),
        std::forward_as_tuple(location_ref(), intern(name), location_groups_process_.at(pid),
                              otf2::definition::location::location_type::cpu_thread));
    process_comm_groups_.at(pid).add_member(location.first->second);

    return archive()(location.first->second);
}
otf2::writer::local& Trace::cpu_writer(int cpuid)
{
    auto name = (boost::format("cpu %d") % cpuid).str();

    // As CPU IDs are unique in this context, create only one writer/location per
    // CPU ID
    auto r_group = location_groups_cpu_.emplace(
        std::piecewise_construct, std::forward_as_tuple(cpuid),
        std::forward_as_tuple(location_group_ref(), intern(name),
                              otf2::definition::location_group::location_group_type::unknown,
                              system_tree_cpu_nodes_.at(cpuid)));
    const auto& group = r_group.first->second;

    auto location = cpu_locations_.emplace(
        std::piecewise_construct, std::forward_as_tuple(cpuid),
        std::forward_as_tuple(location_ref(), intern(name), group,
                              otf2::definition::location::location_type::cpu_thread));
    return archive()(location.first->second);
}

otf2::writer::local& Trace::metric_writer(pid_t pid, pid_t tid)
{
    auto name = (boost::format("metrics for thread %d") % tid).str();
    // As the tid is unique in this context, create only one
    // writer/location per tid
    auto location = metric_locations_.emplace(
        std::piecewise_construct, std::forward_as_tuple(tid),
        std::forward_as_tuple(location_ref(), intern(name), location_groups_process_.at(pid),
                              otf2::definition::location::location_type::metric));
    return archive()(location.first->second);
}
otf2::writer::local& Trace::metric_writer(const std::string& name)
{
    // As names may not be unique in this context, always generate a new location/writer pair
    auto location = named_locations_.emplace(location_ref(), intern(name),
                                             location_groups_process_.at(METRIC_PID),
                                             otf2::definition::location::location_type::metric);
    return archive()(location);
}

otf2::definition::metric_member Trace::metric_member(const std::string& name,
                                                     const std::string& description,
                                                     otf2::common::metric_mode mode,
                                                     otf2::common::type value_type,
                                                     const std::string& unit)
{
    return metric_members_.emplace(metric_member_ref(), intern(name), intern(description),
                                   otf2::common::metric_type::other, mode, value_type,
                                   otf2::common::base_type::decimal, 0, intern(unit));
}

otf2::definition::metric_instance
Trace::metric_instance(otf2::definition::metric_class metric_class,
                       otf2::definition::location recorder, otf2::definition::location scope)
{
    return metric_instances_.emplace(metric_instance_ref(), metric_class, recorder, scope);
}

otf2::definition::metric_instance
Trace::metric_instance(otf2::definition::metric_class metric_class,
                       otf2::definition::location recorder,
                       otf2::definition::system_tree_node scope)
{
    return metric_instances_.emplace(metric_instance_ref(), metric_class, recorder, scope);
}

otf2::definition::metric_class Trace::metric_class()
{
    return metric_classes_.emplace(metric_class_ref(), otf2::common::metric_occurence::async,
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

            if (config().disassemble)
            {
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

    auto iname = intern((boost::format("%s (%d)") % exe % tid).str());
    auto ret = regions_thread_.emplace(
        std::piecewise_construct, std::forward_as_tuple(tid),
        std::forward_as_tuple(region_ref(), iname, iname, iname, otf2::common::role_type::function,
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
    auto iname = intern((boost::format("lo2s::%s") % name).str());

    // TODO, should be paradigm_type::measurement_system, but that's a bug in Vampir
    auto ret = regions_thread_.emplace(
        std::piecewise_construct, std::forward_as_tuple(tid),
        std::forward_as_tuple(region_ref(), iname, iname, iname, otf2::common::role_type::function,
                              otf2::common::paradigm_type::user, otf2::common::flags_type::none,
                              iname, 0, 0));
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
        std::forward_as_tuple(group_ref(), intern(name), otf2::common::paradigm_type::user,
                              otf2::common::group_flag_type::none));
    return ret.first->second;
}

otf2::definition::comm Trace::process_comm(pid_t pid)
{
    return process_comms_.at(pid);
}

otf2::definition::source_code_location Trace::intern_scl(const LineInfo& info)
{
    auto ret = source_code_locations_.emplace(
        std::piecewise_construct, std::forward_as_tuple(info),
        std::forward_as_tuple(scl_ref(), intern(info.file), info.line));
    return ret.first->second;
}

otf2::definition::region Trace::intern_region(const LineInfo& info)
{
    auto name_str = intern(info.function);
    auto ret = regions_line_info_.emplace(
        std::piecewise_construct, std::forward_as_tuple(info),
        std::forward_as_tuple(region_ref(), name_str, name_str, name_str,
                              otf2::common::role_type::function,
                              otf2::common::paradigm_type::sampling, otf2::common::flags_type::none,
                              intern(info.file), info.line, 0));
    return ret.first->second;
}

otf2::definition::string Trace::intern(const std::string& name)
{
    auto ret = strings_.emplace(std::piecewise_construct, std::forward_as_tuple(name),
                                std::forward_as_tuple(string_ref(), name));
    return ret.first->second;
}
} // namespace trace
} // namespace lo2s
