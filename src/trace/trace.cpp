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
#include <lo2s/perf/event_collection.hpp>
#include <lo2s/perf/tracepoint/format.hpp>
#include <lo2s/summary.hpp>
#include <lo2s/time/time.hpp>
#include <lo2s/topology.hpp>
#include <lo2s/util.hpp>
#include <lo2s/version.hpp>

#include <nitro/env/get.hpp>
#include <nitro/env/hostname.hpp>

#include <boost/algorithm/string/replace.hpp>
#include <boost/filesystem.hpp>

#include <fmt/core.h>

#include <map>
#include <mutex>
#include <regex>
#include <stdexcept>
#include <tuple>

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
  registry_(archive_.registry()),
  interrupt_generator_(registry_.create<otf2::definition::interrupt_generator>(
      intern("perf HW_INSTRUCTIONS"), otf2::common::interrupt_generator_mode_type::count,
      otf2::common::base_type::decimal, 0, config().sampling_period)),
  comm_locations_group_(registry_.create<otf2::definition::comm_locations_group>(
      intern("All pthread locations"), otf2::common::paradigm_type::pthread,
      otf2::common::group_flag_type::none)),
  lo2s_regions_group_(registry_.create<otf2::definition::regions_group>(
      intern("lo2s"), otf2::common::paradigm_type::user, otf2::common::group_flag_type::none)),

  perf_metric_class_(registry_.create<otf2::definition::metric_class>(
      otf2::common::metric_occurence::async, otf2::common::recorder_kind::abstract)),
  cpuid_metric_class_(registry_.create<otf2::definition::metric_class>(
      otf2::common::metric_occurence::async, otf2::common::recorder_kind::abstract)),
  system_tree_root_node_(registry_.create<otf2::definition::system_tree_node>(
      intern(nitro::env::hostname()), intern("machine")))
{
    cpuid_metric_class_.add_member(metric_member("CPU", "CPU executing the task",
                                                 otf2::common::metric_mode::absolute_point,
                                                 otf2::common::type::int64, "cpuid"));

    const perf::EventCollection& event_collection = perf::requested_events();
    if (!event_collection.events.empty())
    {
        perf_metric_class_.add_member(metric_member(
            event_collection.leader.name, event_collection.leader.name,
            otf2::common::metric_mode::accumulated_start, otf2::common::type::Double, "#"));

        for (const auto& ev : event_collection.events)
        {
            perf_metric_class_.add_member(
                metric_member(ev.name, ev.name, otf2::common::metric_mode::accumulated_start,
                              otf2::common::type::Double, "#"));
        }

        perf_metric_class_.add_member(metric_member("time_enabled", "time event active",
                                                    otf2::common::metric_mode::accumulated_start,
                                                    otf2::common::type::uint64, "ns"));
        perf_metric_class_.add_member(metric_member("time_running", "time event on CPU",
                                                    otf2::common::metric_mode::accumulated_start,
                                                    otf2::common::type::uint64, "ns"));
    }

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

    registry_.create<otf2::definition::location_group>(
        ByProcess(METRIC_PID), intern("Metric Location Group"),
        otf2::definition::location_group::location_group_type::process, system_tree_root_node_);

    registry_.create<otf2::definition::system_tree_node_domain>(
        system_tree_root_node_, otf2::common::system_tree_node_domain::shared_memory);
    const auto& sys = Topology::instance();
    for (auto& package : sys.packages())
    {
        Log::debug() << "Registering package " << package.id;
        const auto& res = registry_.create<otf2::definition::system_tree_node>(
            ByPackage(package.id), intern(std::to_string(package.id)), intern("package"),
            system_tree_root_node_);

        registry_.create<otf2::definition::system_tree_node_domain>(
            res, otf2::common::system_tree_node_domain::socket);
    }
    for (auto& core : sys.cores())
    {
        Log::debug() << "Registering core " << core.id << "@" << core.package_id;

        const auto& res = registry_.create<otf2::definition::system_tree_node>(
            ByCore(core.id, core.package_id),
            intern(std::to_string(core.package_id) + ":" + std::to_string(core.id)), intern("core"),
            registry_.get<otf2::definition::system_tree_node>(ByPackage(core.package_id)));

        registry_.create<otf2::definition::system_tree_node_domain>(
            res, otf2::common::system_tree_node_domain::core);
    }
    for (auto& cpu : sys.cpus())
    {
        const auto& name = intern(fmt::format("cpu {}", cpu.id));

        Log::debug() << "Registering cpu " << cpu.id << "@" << cpu.core_id << ":" << cpu.package_id;
        const auto& res = registry_.create<otf2::definition::system_tree_node>(
            ByCpu(cpu.id), name, intern("cpu"),
            registry_.get<otf2::definition::system_tree_node>(ByCore(cpu.core_id, cpu.package_id)));

        registry_.create<otf2::definition::system_tree_node_domain>(
            res, otf2::common::system_tree_node_domain::pu);

        registry_.create<otf2::definition::location_group>(
            ByCpu(cpu.id), name, otf2::definition::location_group::location_group_type::process,
            registry_.get<otf2::definition::system_tree_node>(ByCpu(cpu.id)));
    }
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

    archive_ << otf2::definition::clock_properties(starting_time_, stopping_time_);

    boost::filesystem::path symlink_path = nitro::env::get("LO2S_OUTPUT_LINK");

    if (symlink_path.empty())
    {
        return;
    }

    if (boost::filesystem::is_symlink(symlink_path))
    {
        boost::filesystem::remove(symlink_path);
    }
    else if (boost::filesystem::exists(symlink_path))
    {
        Log::warn() << "The path " << symlink_path
                    << " exists and isn't a symlink, refusing to create link to latest trace";
        return;
    }
    boost::filesystem::create_symlink(trace_name_, symlink_path);
}

const otf2::definition::system_tree_node& Trace::intern_process_node(pid_t pid)
{
    if (registry_.has<otf2::definition::system_tree_node>(ByProcess(pid)))
    {
        return registry_.get<otf2::definition::system_tree_node>(ByProcess(pid));
    }
    else
    {
        Log::warn() << "Could not find system tree node for pid " << pid;
        return system_tree_root_node_;
    }
}

void Trace::add_process(pid_t pid, pid_t parent, const std::string& name)
{

    if (registry_.has<otf2::definition::system_tree_node>(ByProcess(pid)))
    {
        update_process_name(pid, name);
        return;
    }
    else
    {
        const auto& iname = intern(name);
        const auto& parent_node = (parent == NO_PARENT_PROCESS_PID) ? system_tree_root_node_ :
                                                                      intern_process_node(parent);

        const auto& ret = registry_.create<otf2::definition::system_tree_node>(
            ByProcess(pid), iname, intern("process"), parent_node);

        registry_.emplace<otf2::definition::location_group>(
            ByProcess(pid), iname, otf2::definition::location_group::location_group_type::process,
            ret);

        const auto& comm_group = registry_.emplace<otf2::definition::comm_group>(
            ByProcess(pid), iname, otf2::common::paradigm_type::pthread,
            otf2::common::group_flag_type::none);

        registry_.emplace<otf2::definition::comm>(ByProcess(pid), iname, comm_group);
    }
}

void Trace::update_process_name(pid_t pid, const std::string& name)
{
    std::lock_guard<std::recursive_mutex> guard(mutex_);
    const auto& iname = intern(name);
    try
    {
        registry_.get<otf2::definition::system_tree_node>(ByProcess(pid)).name(iname);
        registry_.get<otf2::definition::location_group>(ByProcess(pid)).name(iname);
        registry_.get<otf2::definition::comm_group>(ByProcess(pid)).name(iname);
        registry_.get<otf2::definition::comm>(ByProcess(pid)).name(iname);

        auto& region = registry_.get<otf2::definition::region>(ByThread(pid));
        region.name(iname);

        registry_.get<otf2::definition::regions_group>(ByString(name)).add_member(region);
        registry_.get<otf2::definition::regions_group>(ByString(process_names_.at(pid)))
            .remove_member(region);

        process_names_[pid] = name;
    }
    catch (const std::out_of_range&)
    {
        Log::warn() << "Attempting to update name of unknown process " << pid << " (" << name
                    << ")";
    }
}

void Trace::update_thread_name(pid_t tid, const std::string& name)
{
    // TODO we call this function in a hot-loop, locking doesn't sound like a good idea
    std::lock_guard<std::recursive_mutex> guard(mutex_);

    auto& iname = intern(fmt::format("{} (tid {})", name, tid));
    if (registry_.has<otf2::definition::location>(ByThreadSampleWriter(tid)))
    {
        registry_.get<otf2::definition::location>(ByThreadSampleWriter(tid)).name(iname);
    }
    else
    {
        Log::warn() << "Attempting to update name of unknown thread " << tid << " (" << name << ")";
    }
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
    registry_.create<otf2::definition::system_tree_node_property>(
        system_tree_root_node_, intern(property_name), otf2::attribute_value{ intern(value) });
}

otf2::writer::local& Trace::thread_sample_writer(pid_t pid, pid_t tid)
{
    // TODO we call this function in a hot-loop, locking doesn't sound like a good idea
    std::lock_guard<std::recursive_mutex> guard(mutex_);

    auto name = (fmt::format("thread {}", tid));

    // As the tid is unique in this context, create only one writer/location per tid
    const auto& location = registry_.emplace<otf2::definition::location>(
        ByThreadSampleWriter(tid), intern(name),
        registry_.get<otf2::definition::location_group>(ByProcess(pid)),
        otf2::definition::location::location_type::cpu_thread);

    registry_.get<otf2::definition::comm_group>(ByProcess(pid)).add_member(location);

    comm_locations_group_.add_member(location);

    return archive_(location);
}

otf2::writer::local& Trace::cpu_sample_writer(int cpuid)
{
#ifdef USE_PERF_RECORD_SWITCH
    // if we use switch records, we write sample events in the same location as switch events,
    // thus we make sure to return the appropriate location here.
    return cpu_switch_writer(cpuid);
#else
    auto name = fmt::format("sample cpu {}", cpuid);

    // As the cpuid is unique in this context, create only one writer/location per cpuid
    const auto& location = registry_.emplace<otf2::definition::location>(
        ByCpuSampleWriter(cpuid), intern(name),
        registry_.get<otf2::definition::location_group>(ByCpu(cpuid)),
        otf2::definition::location::location_type::cpu_thread);

    comm_locations_group_.add_member(location);
    return archive_(location);
#endif
}

otf2::writer::local& Trace::cpu_switch_writer(int cpuid)
{
    // As the cpuid is unique in this context, create only one writer/location per
    // cpuid
    auto name = intern(fmt::format("cpu {}", cpuid));

    const auto& location = registry_.emplace<otf2::definition::location>(
        ByCpuSwitchWriter(cpuid), name,
        registry_.get<otf2::definition::location_group>(ByCpu(cpuid)),
        otf2::definition::location::location_type::cpu_thread);

    comm_locations_group_.add_member(location);
    return archive_(location);
}

otf2::writer::local& Trace::thread_metric_writer(pid_t pid, pid_t tid)
{
    auto name = fmt::format("metrics for thread {}", tid);

    // As the tid is unique in this context, create only one
    // writer/location per tid
    const auto& location = registry_.emplace<otf2::definition::location>(
        ByThreadMetricWriter(tid), intern(name),
        registry_.get<otf2::definition::location_group>(ByProcess(pid)),
        otf2::definition::location::location_type::metric);

    return archive_(location);
}

otf2::writer::local& Trace::cpu_metric_writer(int cpuid)
{
    auto name = intern(fmt::format("metrics for cpu {}", cpuid));

    const auto& location = registry_.emplace<otf2::definition::location>(
        ByCpuMetricWriter(cpuid), name,
        registry_.get<otf2::definition::location_group>(ByCpu(cpuid)),
        otf2::definition::location::location_type::metric);
    return archive_(location);
}

otf2::writer::local& Trace::named_metric_writer(const std::string& name)
{
    // As names may not be unique in this context, always generate a new location/writer pair
    const auto& location = registry_.create<otf2::definition::location>(
        intern(name), registry_.get<otf2::definition::location_group>(ByProcess(METRIC_PID)),
        otf2::definition::location::location_type::metric);
    return archive_(location);
}

otf2::definition::metric_member
Trace::metric_member(const std::string& name, const std::string& description,
                     otf2::common::metric_mode mode, otf2::common::type value_type,
                     const std::string& unit, std::int64_t exponent, otf2::common::base_type base)
{
    return registry_.create<otf2::definition::metric_member>(
        intern(name), intern(description), otf2::common::metric_type::other, mode, value_type, base,
        exponent, intern(unit));
}

otf2::definition::metric_instance
Trace::metric_instance(const otf2::definition::metric_class& metric_class,
                       const otf2::definition::location& recorder,
                       const otf2::definition::location& scope)
{
    return registry_.create<otf2::definition::metric_instance>(metric_class, recorder, scope);
}

otf2::definition::metric_instance
Trace::metric_instance(const otf2::definition::metric_class& metric_class,
                       const otf2::definition::location& recorder,
                       const otf2::definition::system_tree_node& scope)
{
    return registry_.create<otf2::definition::metric_instance>(metric_class, recorder, scope);
}

otf2::definition::metric_class& Trace::tracepoint_metric_class(const std::string& event_name)
{
    auto& mc = registry_.emplace<otf2::definition::metric_class>(
        ByString(event_name), otf2::common::metric_occurence::async,
        otf2::common::recorder_kind::abstract);
    perf::tracepoint::EventFormat event(event_name);
    for (const auto& field : event.fields())
    {
        if (field.is_integer())
        {
            mc.add_member(metric_member(event_name + "::" + field.name(), "?",
                                        otf2::common::metric_mode::absolute_next,
                                        otf2::common::type::int64, "#"));
        }
    }
    return mc;
}

otf2::definition::metric_class& Trace::metric_class()
{
    return registry_.create<otf2::definition::metric_class>(otf2::common::metric_occurence::async,
                                                            otf2::common::recorder_kind::abstract);
}

void Trace::merge_ips(IpRefMap& new_children, IpCctxMap& children,
                      std::vector<uint32_t>& mapping_table,
                      otf2::definition::calling_context& parent,
                      std::map<pid_t, ProcessInfo>& infos, pid_t pid)
{
    for (auto& elem : new_children)
    {
        auto& ip = elem.first;
        auto& local_ref = elem.second.ref;
        auto& local_children = elem.second.children;
        LineInfo line_info = LineInfo::for_unknown_function();

        auto info_it = infos.find(pid);
        if (info_it != infos.end())
        {
            MemoryMap maps = info_it->second.maps();
            line_info = maps.lookup_line_info(ip);
        }

        Log::trace() << "resolved " << ip << ": " << line_info;
        auto cctx_it = children.find(ip);
        if (cctx_it == children.end())
        {
            auto& new_cctx = registry_.create<otf2::definition::calling_context>(
                intern_region(line_info), intern_scl(line_info), parent);
            auto r = children.emplace(ip, new_cctx);
            cctx_it = r.first;

            if (config().disassemble && infos.count(pid) == 1)
            {
                try
                {
                    auto instruction = infos.at(pid).maps().lookup_instruction(ip);
                    Log::trace() << "mapped " << ip << " to " << instruction;

                    registry_.create<otf2::definition::calling_context_property>(
                        new_cctx, intern("instruction"),
                        otf2::attribute_value(intern(instruction)));
                }
                catch (std::exception& ex)
                {
                    Log::trace() << "could not read instruction from " << ip << ": " << ex.what();
                }
            }
        }
        auto& cctx = cctx_it->second.cctx;
        mapping_table.at(local_ref) = cctx.ref();

        merge_ips(local_children, cctx_it->second.children, mapping_table, cctx, infos, pid);
    }
}

otf2::definition::mapping_table Trace::merge_calling_contexts(ThreadCctxRefMap& new_ips,
                                                              size_t num_ip_refs,
                                                              std::map<pid_t, ProcessInfo>& infos)
{
#ifndef NDEBUG
    std::vector<uint32_t> mappings(num_ip_refs, -1u);
#else
    std::vector<uint32_t> mappings(num_ip_refs);
#endif

    // Merge local thread tree into global thread tree
    for (auto& local_thread_cctx : new_ips)
    {
        auto tid = local_thread_cctx.first;
        auto local_ref = local_thread_cctx.second.entry.ref;

        auto global_thread_cctx = calling_context_tree_.find(tid);

        if (global_thread_cctx == calling_context_tree_.end())
        {
            if (tid != 0)
            {
                // Threads rarely have their own name, in that case, use the process' name
                auto process_name = process_names_.find(local_thread_cctx.second.pid);
                if (process_name == process_names_.end())
                {
                    add_thread(tid, "<unknown thread>");
                }
                else
                {
                    add_thread(tid, process_name->second);
                }
            }
            else
            {
                add_thread(tid, "<idle>");
            }
            global_thread_cctx = calling_context_tree_.find(tid);
        }

        assert(global_thread_cctx != calling_context_tree_.end());
        mappings.at(local_ref) = global_thread_cctx->second.cctx.ref();

        merge_ips(local_thread_cctx.second.entry.children, global_thread_cctx->second.children,
                  mappings, global_thread_cctx->second.cctx, infos, local_thread_cctx.second.pid);
    }

#ifndef NDEBUG
    for (auto id : mappings)
    {
        assert(id != -1u);
    }
#endif

    return otf2::definition::mapping_table(
        otf2::definition::mapping_table::mapping_type_type::calling_context, mappings);
}

void Trace::add_thread_exclusive(pid_t tid, const std::string& name,
                                 const std::lock_guard<std::recursive_mutex>&)
{
    if (registry_.has<otf2::definition::calling_context>(ByThread(tid)))
    {
        return;
    }

    process_names_.emplace(std::piecewise_construct, std::forward_as_tuple(tid),
                           std::forward_as_tuple(name));

    auto& iname = intern(fmt::format("{} ({})", name, tid));

    if (!registry_.has<otf2::definition::region>(ByThread(tid)))
    {
        auto& thread_region = registry_.create<otf2::definition::region>(
            ByThread(tid), iname, iname, iname, otf2::common::role_type::function,
            otf2::common::paradigm_type::user, otf2::common::flags_type::none, iname, 0, 0);
        auto& regions_group = registry_.create<otf2::definition::regions_group>(
            ByString(name), intern(name), otf2::common::paradigm_type::user,
            otf2::common::group_flag_type::none);
        regions_group.add_member(thread_region);
    }
    auto& thread_region = registry_.get<otf2::definition::region>(ByThread(tid));
    // TODO update iname if not newly inserted

    // create calling context
    auto& thread_cctx = registry_.create<otf2::definition::calling_context>(
        ByThread(tid), thread_region, otf2::definition::source_code_location());

    calling_context_tree_.emplace(std::piecewise_construct, std::forward_as_tuple(tid),
                                  std::forward_as_tuple(thread_cctx));
}

void Trace::add_thread(pid_t tid, const std::string& name)
{
    // Lock this to avoid conflict on regions_thread_ with add_monitoring_thread
    std::lock_guard<std::recursive_mutex> guard(mutex_);
    add_thread_exclusive(tid, name, guard);
}

void Trace::add_monitoring_thread(pid_t tid, const std::string& name, const std::string& group)
{
    // We must guard this here because this is called by monitoring threads itself rather than
    // the usual call from the single monitoring process
    std::lock_guard<std::recursive_mutex> guard(mutex_);

    Log::debug() << "Adding monitoring thread " << tid << " (" << name << "): group " << group;
    auto& iname = intern(fmt::format("lo2s::{}", name));

    // TODO, should be paradigm_type::measurement_system, but that's a bug in Vampir
    if (!registry_.has<otf2::definition::region>(ByThread(tid)))
    {
        const auto& ret = registry_.create<otf2::definition::region>(
            ByThread(tid), iname, iname, iname, otf2::common::role_type::function,
            otf2::common::paradigm_type::user, otf2::common::flags_type::none, iname, 0, 0);

        lo2s_regions_group_.add_member(ret);

        auto& lo2s_cctx = registry_.create<otf2::definition::calling_context>(
            ByThread(tid), ret, otf2::definition::source_code_location());
        calling_context_tree_.emplace(std::piecewise_construct, std::forward_as_tuple(tid),
                                      std::forward_as_tuple(lo2s_cctx));
    }
}

void Trace::add_threads(const std::unordered_map<pid_t, std::string>& tid_map)
{
    Log::debug() << "Adding " << tid_map.size() << " monitored thread(s) to the trace";

    // Lock here to avoid conflicts when writing to regions_thread_
    std::lock_guard<std::recursive_mutex> guard(mutex_);
    for (const auto& elem : tid_map)
    {
        add_thread_exclusive(elem.first, elem.second, guard);
    }
}

const otf2::definition::source_code_location& Trace::intern_scl(const LineInfo& info)
{
    return registry_.emplace<otf2::definition::source_code_location>(ByLineInfo(info),
                                                                     intern(info.file), info.line);
}

const otf2::definition::region& Trace::intern_region(const LineInfo& info)
{
    const auto& name_str = intern(info.function);
    const auto& region = registry_.emplace<otf2::definition::region>(
        ByLineInfo(info), name_str, name_str, name_str, otf2::common::role_type::function,
        otf2::common::paradigm_type::sampling, otf2::common::flags_type::none, intern(info.file),
        info.line, 0);
    registry_.emplace<otf2::definition::regions_group>(ByString(info.dso), intern(info.dso),
                                                       otf2::common::paradigm_type::compiler,
                                                       otf2::common::group_flag_type::none);
    return region;
}
const otf2::definition::string& Trace::intern(const std::string& name)
{
    std::lock_guard<std::recursive_mutex> guard(mutex_);

    return registry_.emplace<otf2::definition::string>(ByString(name), name);
}
} // namespace trace
} // namespace lo2s
