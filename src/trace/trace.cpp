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
#include <lo2s/perf/bio/block_device.hpp>
#include <lo2s/perf/tracepoint/format.hpp>
#include <lo2s/summary.hpp>
#include <lo2s/time/time.hpp>
#include <lo2s/topology.hpp>
#include <lo2s/util.hpp>
#include <lo2s/version.hpp>

#include <nitro/env/get.hpp>
#include <nitro/env/hostname.hpp>
#include <nitro/lang/string.hpp>

#include <filesystem>

#include <fmt/chrono.h>
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

Process Trace::NO_PARENT_PROCESS = Process(0);

std::string get_trace_name(std::string prefix = "")
{
    nitro::lang::replace_all(prefix, "{DATE}", get_datetime());
    nitro::lang::replace_all(prefix, "{HOSTNAME}", nitro::env::hostname());

    auto result = prefix;

    std::regex env_re("\\{ENV=([^}]*)\\}");
    auto env_begin = std::sregex_iterator(prefix.begin(), prefix.end(), env_re);
    auto env_end = std::sregex_iterator();

    for (std::sregex_iterator it = env_begin; it != env_end; ++it)
    {
        nitro::lang::replace_all(result, it->str(), nitro::env::get((*it)[1]));
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
  hardware_comm_locations_group_(registry_.create<otf2::definition::comm_locations_group>(
      intern("All hardware locations"), otf2::common::paradigm_type::hardware,
      otf2::common::group_flag_type::none)),
  lo2s_regions_group_(registry_.create<otf2::definition::regions_group>(
      intern("lo2s"), otf2::common::paradigm_type::user, otf2::common::group_flag_type::none)),
  system_tree_root_node_(registry_.create<otf2::definition::system_tree_node>(
      intern(nitro::env::hostname()), intern("machine"))),
  groups_(ExecutionScopeGroup::instance())
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

    registry_.create<otf2::definition::location_group>(
        ByExecutionScope(ExecutionScope(Thread(METRIC_PID))), intern("Metric Location Group"),
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
        const auto& name = intern(Cpu(cpu.id).name());

        Log::debug() << "Registering cpu " << cpu.id << "@" << cpu.core_id << ":" << cpu.package_id;
        const auto& res = registry_.create<otf2::definition::system_tree_node>(
            ByCpu(Cpu(cpu.id)), name, intern("cpu"),
            registry_.get<otf2::definition::system_tree_node>(ByCore(cpu.core_id, cpu.package_id)));

        registry_.create<otf2::definition::system_tree_node_domain>(
            res, otf2::common::system_tree_node_domain::pu);

        registry_.create<otf2::definition::location_group>(
            ByExecutionScope(Cpu(cpu.id).as_scope()), name,
            otf2::definition::location_group::location_group_type::process,
            registry_.get<otf2::definition::system_tree_node>(ByCpu(Cpu(cpu.id))));

        groups_.add_cpu(Cpu(cpu.id));
    }

    groups_.add_process(NO_PARENT_PROCESS);

    if (config().use_block_io)
    {
        bio_system_tree_node_ = registry_.create<otf2::definition::system_tree_node>(
            intern("block devices"), intern("hardware"), system_tree_root_node_);

        const std::vector<otf2::common::io_paradigm_property_type> properties;
        const std::vector<otf2::attribute_value> values;
        bio_paradigm_ = registry_.create<otf2::definition::io_paradigm>(
            intern("block_io"), intern("block layer I/O"),
            otf2::common::io_paradigm_class_type::parallel, otf2::common::io_paradigm_flag_type::os,
            properties, values);

        bio_comm_group_ = registry_.create<otf2::definition::comm_group>(
            intern("block devices"), otf2::common::paradigm_type::hardware,
            otf2::common::group_flag_type::none);

        for (auto& device : get_block_devices())
        {
            if (device.type == BlockDeviceType::DISK)
            {
                block_io_handle(device);
                bio_writer(device);
            }
        }
    }
}

void Trace::begin_record()
{
    Log::info() << "Initialization done. Start recording...";
    starting_time_ = time::now();

    const std::time_t t_c = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    add_lo2s_property("STARTING_TIME", fmt::format("{:%c}", fmt::localtime(t_c)));
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
    for (auto& thread : thread_names_)
    {
        auto& thread_region = registry_.get<otf2::definition::region>(ByThread(thread.first));

        try
        {
            Process parent = groups_.get_process(thread.first);

            auto& regions_group = registry_.emplace<otf2::definition::regions_group>(
                ByProcess(parent), intern(thread_names_[parent.as_thread()]),
                otf2::common::paradigm_type::user, otf2::common::group_flag_type::none);

            regions_group.add_member(thread_region);
        }
        // If no parent can be found emplace a thread as it's own regions_group
        catch (const std::out_of_range&)
        {
            auto& regions_group = registry_.emplace<otf2::definition::regions_group>(
                ByThread(thread.first), intern(thread.second), otf2::common::paradigm_type::user,
                otf2::common::group_flag_type::none);

            regions_group.add_member(thread_region);
        }
    }

    if (starting_time_ > stopping_time_)
    {
        stopping_time_ = starting_time_;
    }

    archive_ << otf2::definition::clock_properties(starting_time_, stopping_time_);

    std::filesystem::path symlink_path = nitro::env::get("LO2S_OUTPUT_LINK");

    if (symlink_path.empty())
    {
        return;
    }

    if (std::filesystem::is_symlink(symlink_path))
    {
        std::filesystem::remove(symlink_path);
    }
    else if (std::filesystem::exists(symlink_path))
    {
        Log::warn() << "The path " << symlink_path
                    << " exists and isn't a symlink, refusing to create link to latest trace";
        return;
    }
    std::filesystem::create_symlink(trace_name_, symlink_path);
}

const otf2::definition::system_tree_node& Trace::intern_process_node(Process process)
{
    if (registry_.has<otf2::definition::system_tree_node>(ByProcess(process)))
    {
        return registry_.get<otf2::definition::system_tree_node>(ByProcess(process));
    }
    else
    {
        Log::warn() << "Could not find system tree node for  " << process;
        return system_tree_root_node_;
    }
}

void Trace::add_process(Process parent, Process process, const std::string& name)
{
    std::lock_guard<std::recursive_mutex> guard(mutex_);

    if (registry_.has<otf2::definition::system_tree_node>(ByProcess(process)))
    {
        update_process_name(process, name);
        return;
    }
    else
    {
        thread_names_.emplace(std::piecewise_construct, std::forward_as_tuple(process.as_thread()),
                              std::forward_as_tuple(name));

        const auto& iname = intern(name);

        const auto& parent_node =
            (parent == NO_PARENT_PROCESS) ? system_tree_root_node_ : intern_process_node(parent);

        const auto& ret = registry_.create<otf2::definition::system_tree_node>(
            ByProcess(process), iname, intern("process"), parent_node);

        registry_.emplace<otf2::definition::location_group>(
            ByExecutionScope(ExecutionScope(Process(process))), iname,
            otf2::definition::location_group::location_group_type::process, ret);

        const auto& comm_group = registry_.emplace<otf2::definition::comm_group>(
            ByProcess(process), iname, otf2::common::paradigm_type::pthread,
            otf2::common::group_flag_type::none);

        registry_.emplace<otf2::definition::comm>(ByProcess(process), iname, comm_group,
                                                  otf2::definition::comm::comm_flag_type::none);
    }
}

void Trace::update_process_name(Process process, const std::string& name)
{
    std::lock_guard<std::recursive_mutex> guard(mutex_);
    const auto& iname = intern(name);
    try
    {
        registry_.get<otf2::definition::system_tree_node>(ByProcess(process)).name(iname);
        registry_.get<otf2::definition::location_group>(ByExecutionScope(process.as_scope()))
            .name(iname);
        registry_.get<otf2::definition::comm_group>(ByProcess(process)).name(iname);
        registry_.get<otf2::definition::comm>(ByProcess(process)).name(iname);

        update_thread_name(process.as_thread(), name);
    }
    catch (const std::out_of_range&)
    {
        Log::warn() << "Attempting to update name of unknown " << process << " (" << name << ")";
    }
}

void Trace::update_thread_name(Thread thread, const std::string& name)
{
    // TODO we call this function in a hot-loop, locking doesn't sound like a good idea
    std::lock_guard<std::recursive_mutex> guard(mutex_);

    try
    {
        auto& iname = intern(fmt::format("{} ({})", name, thread.as_pid_t()));
        auto& thread_region = registry_.get<otf2::definition::region>(ByThread(thread));
        thread_region.name(iname);
        thread_region.canonical_name(iname);
        thread_region.source_file(iname);
        thread_region.description(iname);

        if (registry_.has<otf2::definition::location>(
                ByMeasurementScope(MeasurementScope::sample(thread.as_scope()))))
        {
            registry_
                .get<otf2::definition::location>(
                    ByMeasurementScope(MeasurementScope::sample(thread.as_scope())))
                .name(iname);
        }

        thread_names_[thread] = name;
    }
    catch (const std::out_of_range&)
    {
        Log::warn() << "Attempting to update name of unknown " << thread << " (" << name << ")";
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

otf2::writer::local& Trace::sample_writer(const ExecutionScope& writer_scope)
{
    // TODO we call this function in a hot-loop, locking doesn't sound like a good idea
    std::lock_guard<std::recursive_mutex> guard(mutex_);

    return archive_(location(writer_scope));
}

otf2::writer::local& Trace::metric_writer(const MeasurementScope& writer_scope)
{
    const auto& intern_location = registry_.emplace<otf2::definition::location>(
        ByMeasurementScope(writer_scope), intern(writer_scope.name()),
        registry_.get<otf2::definition::location_group>(
            ByExecutionScope(groups_.get_parent(writer_scope.scope))),
        otf2::definition::location::location_type::metric);
    return archive_(intern_location);
}

otf2::writer::local& Trace::bio_writer(BlockDevice& device)
{
    std::lock_guard<std::recursive_mutex> guard(mutex_);

    const auto& name = intern(fmt::format("block I/O events for {}", device.name));

    const auto& node = registry_.emplace<otf2::definition::system_tree_node>(
        ByDev(device.id), intern(device.name), intern("block device"), bio_system_tree_node_);

    const auto& bio_location_group = registry_.emplace<otf2::definition::location_group>(
        ByDev(device.id), name, otf2::common::location_group_type::process, node);

    const auto& intern_location = registry_.emplace<otf2::definition::location>(
        ByDev(device.id), name, bio_location_group,
        otf2::definition::location::location_type::cpu_thread);

    hardware_comm_locations_group_.add_member(intern_location);
    return archive_(intern_location);
}

otf2::writer::local& Trace::switch_writer(const ExecutionScope& writer_scope)
{
    MeasurementScope scope = MeasurementScope::context_switch(writer_scope);

    const auto& intern_location = registry_.emplace<otf2::definition::location>(
        ByMeasurementScope(scope), intern(scope.name()),
        registry_.get<otf2::definition::location_group>(
            ByExecutionScope(groups_.get_parent(writer_scope))),
        otf2::definition::location::location_type::cpu_thread);

    comm_locations_group_.add_member(intern_location);

    return archive_(intern_location);
}

otf2::writer::local& Trace::create_metric_writer(const std::string& name)
{
    const auto& location = registry_.create<otf2::definition::location>(
        intern(name),
        registry_.get<otf2::definition::location_group>(
            ByExecutionScope(ExecutionScope(Thread(METRIC_PID)))),
        otf2::definition::location::location_type::metric);
    return archive_(location);
}

otf2::definition::io_handle& Trace::block_io_handle(BlockDevice& device)
{

    std::lock_guard<std::recursive_mutex> guard(mutex_);

    // io_pre_created_handle can not be emplaced because it has no ref.
    // So we have to check if we already created everything
    if (registry_.has<otf2::definition::io_handle>(ByDev(device.id)))
    {
        return registry_.get<otf2::definition::io_handle>(ByDev(device.id));
    }

    const auto& device_name = intern(device.name);

    const otf2::definition::system_tree_node& parent = bio_parent_node(device);

    std::string device_class = (device.type == BlockDeviceType::PARTITION) ? "partition" : "disk";

    const auto& node = registry_.emplace<otf2::definition::system_tree_node>(
        ByDev(device.id), device_name, intern(device_class), parent);

    const auto& file =
        registry_.emplace<otf2::definition::io_regular_file>(ByDev(device.id), device_name, node);

    const auto& block_comm =
        registry_.emplace<otf2::definition::comm>(ByDev(device.id), device_name, bio_comm_group_,
                                                  otf2::definition::comm::comm_flag_type::none);

    // we could have io handle parents and childs here (block device being the parent (sda),
    // partition being the child (sda1)) but that seems like it would be overkill.
    auto& handle = registry_.emplace<otf2::definition::io_handle>(
        ByDev(device.id), device_name, file, bio_paradigm_,
        otf2::common::io_handle_flag_type::pre_created, block_comm);

    // todo: set status flags accordingly
    registry_.create<otf2::definition::io_pre_created_handle_state>(
        handle, otf2::common::io_access_mode_type::read_write,
        otf2::common::io_status_flag_type::none);
    return handle;
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
    if (!registry_.has<otf2::definition::metric_class>(ByString(event_name)))
    {
        auto& mc = registry_.create<otf2::definition::metric_class>(
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
    else
    {
        return registry_.get<otf2::definition::metric_class>(ByString(event_name));
    }
}

otf2::definition::metric_class& Trace::metric_class()
{
    return registry_.create<otf2::definition::metric_class>(otf2::common::metric_occurence::async,
                                                            otf2::common::recorder_kind::abstract);
}

void Trace::merge_ips(IpRefMap& new_children, IpCctxMap& children,
                      std::vector<uint32_t>& mapping_table,
                      otf2::definition::calling_context& parent,
                      std::map<Process, ProcessInfo>& infos, Process process)
{
    for (auto& elem : new_children)
    {
        auto& ip = elem.first;
        auto& local_ref = elem.second.ref;
        auto& local_children = elem.second.children;
        LineInfo line_info = LineInfo::for_unknown_function();

        auto info_it = infos.find(process);
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

            if (config().disassemble && infos.count(process) == 1)
            {
                try
                {
                    auto instruction = infos.at(process).maps().lookup_instruction(ip);
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

        merge_ips(local_children, cctx_it->second.children, mapping_table, cctx, infos, process);
    }
}

otf2::definition::mapping_table Trace::merge_calling_contexts(ThreadCctxRefMap& new_ips,
                                                              size_t num_ip_refs,
                                                              std::map<Process, ProcessInfo>& infos)
{
    std::lock_guard<std::recursive_mutex> guard(mutex_);
#ifndef NDEBUG
    std::vector<uint32_t> mappings(num_ip_refs, -1u);
#else
    std::vector<uint32_t> mappings(num_ip_refs);
#endif

    // Merge local thread tree into global thread tree
    for (auto& local_thread_cctx : new_ips)
    {

        auto thread = local_thread_cctx.first;
        auto process = local_thread_cctx.second.process;

        groups_.add_thread(thread, process);
        auto local_ref = local_thread_cctx.second.entry.ref;

        auto global_thread_cctx = calling_context_tree_.find(thread);

        if (global_thread_cctx == calling_context_tree_.end())
        {
            if (thread != Thread(0))
            {
                auto thread_name = thread_names_.find(thread);
                if (thread_name == thread_names_.end())
                {
                    add_thread(thread, "<unknown thread>");
                }
                else
                {
                    add_thread(thread, thread_name->second);
                }
            }
            else
            {
                add_thread(thread, "<idle>");
            }
            global_thread_cctx = calling_context_tree_.find(thread);
        }

        assert(global_thread_cctx != calling_context_tree_.end());
        mappings.at(local_ref) = global_thread_cctx->second.cctx.ref();

        merge_ips(local_thread_cctx.second.entry.children, global_thread_cctx->second.children,
                  mappings, global_thread_cctx->second.cctx, infos, process);
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

void Trace::add_thread_exclusive(Thread thread, const std::string& name,
                                 const std::lock_guard<std::recursive_mutex>&)
{
    if (registry_.has<otf2::definition::calling_context>(ByThread(thread)))
    {
        update_thread_name(thread, name);
        return;
    }

    thread_names_.emplace(std::piecewise_construct, std::forward_as_tuple(thread),
                          std::forward_as_tuple(name));

    auto& iname = intern(fmt::format("{} ({})", name, thread.as_pid_t()));

    auto& thread_region = registry_.emplace<otf2::definition::region>(
        ByThread(thread), iname, iname, iname, otf2::common::role_type::function,
        otf2::common::paradigm_type::user, otf2::common::flags_type::none, iname, 0, 0);

    // create calling context
    auto& thread_cctx = registry_.create<otf2::definition::calling_context>(
        ByThread(thread), thread_region, otf2::definition::source_code_location());

    calling_context_tree_.emplace(std::piecewise_construct, std::forward_as_tuple(thread),
                                  std::forward_as_tuple(thread_cctx));
}

void Trace::add_thread(Thread thread, const std::string& name)
{
    // Lock this to avoid conflict on regions_thread_ with add_monitoring_thread
    std::lock_guard<std::recursive_mutex> guard(mutex_);
    add_thread_exclusive(thread, name, guard);
}

void Trace::add_monitoring_thread(Thread thread, const std::string& name, const std::string& group)
{
    // We must guard this here because this is called by monitoring threads itself rather than
    // the usual call from the single monitoring process
    std::lock_guard<std::recursive_mutex> guard(mutex_);

    Log::debug() << "Adding monitoring " << thread << " (" << name << "): group " << group;
    auto& iname = intern(fmt::format("lo2s::{}", name));

    // TODO, should be paradigm_type::measurement_system, but that's a bug in Vampir
    if (!registry_.has<otf2::definition::region>(ByThread(thread)))
    {
        const auto& ret = registry_.create<otf2::definition::region>(
            ByThread(thread), iname, iname, iname, otf2::common::role_type::function,
            otf2::common::paradigm_type::user, otf2::common::flags_type::none, iname, 0, 0);

        lo2s_regions_group_.add_member(ret);

        auto& lo2s_cctx = registry_.create<otf2::definition::calling_context>(
            ByThread(thread), ret, otf2::definition::source_code_location());
        calling_context_tree_.emplace(std::piecewise_construct, std::forward_as_tuple(thread),
                                      std::forward_as_tuple(lo2s_cctx));
    }
}

void Trace::add_threads(const std::unordered_map<Thread, std::string>& thread_map)
{
    Log::debug() << "Adding " << thread_map.size() << " monitored thread(s) to the trace";

    // Lock here to avoid conflicts when writing to regions_thread_
    std::lock_guard<std::recursive_mutex> guard(mutex_);
    for (const auto& elem : thread_map)
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

    if (registry_.has<otf2::definition::regions_group>(ByString(info.dso)))
    {
        registry_.get<otf2::definition::regions_group>(ByString(info.dso)).add_member(region);
    }
    else
    {
        registry_.emplace<otf2::definition::regions_group>(ByString(info.dso), intern(info.dso),
                                                           otf2::common::paradigm_type::compiler,
                                                           otf2::common::group_flag_type::none);
    }
    return region;
}
const otf2::definition::string& Trace::intern(const std::string& name)
{
    std::lock_guard<std::recursive_mutex> guard(mutex_);

    return registry_.emplace<otf2::definition::string>(ByString(name), name);
}
} // namespace trace
} // namespace lo2s
