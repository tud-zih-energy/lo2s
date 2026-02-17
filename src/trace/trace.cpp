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
#include <lo2s/config.hpp>
#include <lo2s/line_info.hpp>
#include <lo2s/monitor/main_monitor.hpp>
#include <lo2s/perf/bio/block_device.hpp>
#include <lo2s/perf/tracepoint/format.hpp>
#include <lo2s/summary.hpp>
#include <lo2s/syscalls.hpp>
#include <lo2s/time/time.hpp>
#include <lo2s/topology.hpp>
#include <lo2s/util.hpp>
#include <lo2s/version.hpp>

#include <nitro/env/get.hpp>
#include <nitro/env/hostname.hpp>
#include <nitro/lang/string.hpp>

#include <fmt/chrono.h>
#include <fmt/core.h>

#include <filesystem>
#include <map>
#include <mutex>
#include <regex>
#include <stdexcept>
#include <tuple>

extern "C"
{
#include <asm-generic/unistd.h>
}

namespace lo2s
{
namespace trace
{

constexpr pid_t Trace::METRIC_PID;

std::string get_trace_name()
{
    std::string path = config().trace_path;
    nitro::lang::replace_all(path, "{DATE}", get_datetime());
    nitro::lang::replace_all(path, "{HOSTNAME}", nitro::env::hostname());

    auto result = path;

    std::regex env_re("\\{ENV=([^}]*)\\}");
    auto env_begin = std::sregex_iterator(path.begin(), path.end(), env_re);
    auto env_end = std::sregex_iterator();

    for (std::sregex_iterator it = env_begin; it != env_end; ++it)
    {
        nitro::lang::replace_all(result, it->str(), nitro::env::get((*it)[1]));
    }

    return result;
}

Trace::Trace()
: trace_name_(get_trace_name()), archive_(trace_name_, "traces"), registry_(archive_.registry()),
  calling_context_tree_(CallingContext::root(), GlobalCctxNode(nullptr)),
  interrupt_generator_(registry_.create<otf2::definition::interrupt_generator>(
      intern("perf HW_INSTRUCTIONS"), otf2::common::interrupt_generator_mode_type::count,
      otf2::common::base_type::decimal, 0, config().perf_sampling_period)),
  comm_locations_group_(registry_.create<otf2::definition::comm_locations_group>(
      intern("All pthread locations"), otf2::common::paradigm_type::pthread,
      otf2::common::group_flag_type::none)),
  hardware_comm_locations_group_(registry_.create<otf2::definition::comm_locations_group>(
      intern("All hardware locations"), otf2::common::paradigm_type::hardware,
      otf2::common::group_flag_type::none)),
  lo2s_regions_group_(registry_.create<otf2::definition::regions_group>(
      intern("lo2s"), otf2::common::paradigm_type::user, otf2::common::group_flag_type::none)),
  syscall_regions_group_(registry_.create<otf2::definition::regions_group>(
      intern("<syscalls>"), otf2::common::paradigm_type::user,
      otf2::common::group_flag_type::none)),
  kernel_regions_group_(registry_.create<otf2::definition::regions_group>(
      intern("<kernel threads>"), otf2::common::paradigm_type::user,
      otf2::common::group_flag_type::none)),
  system_tree_root_node_(registry_.create<otf2::definition::system_tree_node>(
      intern(nitro::env::hostname()), intern("machine"))),
  groups_(ExecutionScopeGroup::instance())
{
    Log::info() << "Using trace directory: " << trace_name_;
    summary().set_trace_dir(trace_name_);

    archive_.set_creator(std::string("lo2s - ") + lo2s::version());
    archive_.set_description(config().lo2s_command_line);

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
    for (auto& cpu : sys.cpus())
    {
        Core core = sys.core_of(cpu);
        Package package = sys.package_of(cpu);

        Log::debug() << "Registering cpu " << cpu.as_int() << "@" << package.as_int() << ":"
                     << core.core_as_int();

        auto package_node = registry_.find<otf2::definition::system_tree_node>(ByPackage(package));
        if (!package_node)
        {
            package_node = registry_.emplace<otf2::definition::system_tree_node>(
                ByPackage(package), intern(std::to_string(package.as_int())), intern("package"),
                system_tree_root_node_);

            registry_.create<otf2::definition::system_tree_node_domain>(
                package_node, otf2::common::system_tree_node_domain::socket);
        }

        auto core_node = registry_.find<otf2::definition::system_tree_node>(ByCore(core));
        if (!core_node)
        {
            core_node = registry_.emplace<otf2::definition::system_tree_node>(
                ByCore(core), intern(fmt::format("{}:{}", package.as_int(), core.core_as_int())),
                intern("core"), package_node);

            registry_.create<otf2::definition::system_tree_node_domain>(
                core_node, otf2::common::system_tree_node_domain::core);
        }

        const auto& name = intern(fmt::format("{}", cpu.as_int()));
        const auto& cpu_node = registry_.create<otf2::definition::system_tree_node>(
            ByCpu(cpu), name, intern("cpu"), core_node);
        registry_.create<otf2::definition::system_tree_node_domain>(
            cpu_node, otf2::common::system_tree_node_domain::pu);

        // We need to a different name for the location_group than the system tree node,
        // because a system_tree_node gets attached its class_name in Vampir.
        const auto& lg_name = intern(fmt::format("{}", cpu));
        registry_.create<otf2::definition::location_group>(
            ByExecutionScope(cpu.as_scope()), lg_name,
            otf2::definition::location_group::location_group_type::process, cpu_node);

        groups_.add_cpu(cpu);
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
            if (device.second.type == BlockDeviceType::DISK)
            {
                block_io_handle(device.second);
                bio_writer(device.second);
            }
        }
    }

    if (config().use_nec)
    {
        nec_interrupt_generator_ = registry_.create<otf2::definition::interrupt_generator>(
            intern("NEC sampling timer"), otf2::common::interrupt_generator_mode_type::count,
            otf2::common::base_type::decimal, 0, config().perf_sampling_period);
    }

    if (config().use_posix_io)
    {

        const std::vector<otf2::common::io_paradigm_property_type> properties;
        const std::vector<otf2::attribute_value> values;
        posix_paradigm_ = registry_.create<otf2::definition::io_paradigm>(
            intern("POSIX"), intern("POSIX I/O"), otf2::common::io_paradigm_class_type::parallel,
            otf2::common::io_paradigm_flag_type::os, properties, values);

        posix_comm_group_ = registry_.create<otf2::definition::comm_group>(
            intern("POSIX I/O files"), otf2::common::paradigm_type::hardware,
            otf2::common::group_flag_type::none);
    }
}

void Trace::begin_record()
{
    Log::info() << "Initialization done. Start recording...";
    add_lo2s_property("STARTING_TIME", fmt::format("{:%FT%T%z}", std::chrono::system_clock::now()));
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
    if (!local_cctx_trees_finalized_)
    {
        Log::error()
            << "cctx refs have not been finalized, please report this bug to the developers";
    }
    for (auto& thread : thread_names_)
    {
        if (!registry_.has<otf2::definition::region>(ByThread(thread.first)))
        {
            continue;
        }

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

    archive_ << otf2::definition::clock_properties(starting_time_, stopping_time_,
                                                   starting_system_time_);

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

void Trace::emplace_process(Process parent, Process process, const std::string& name)
{
    std::lock_guard<std::recursive_mutex> guard(mutex_);

    if (registry_.has<otf2::definition::system_tree_node>(ByProcess(process)))
    {
        update_process(parent, process, name);
        return;
    }
    else
    {
        groups_.add_process(process);

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

void Trace::update_process(Process parent, Process process, const std::string& name)
{
    std::lock_guard<std::recursive_mutex> guard(mutex_);

    if (name != "")
    {
        const auto& iname = intern(name);
        try
        {
            registry_.get<otf2::definition::system_tree_node>(ByProcess(process)).name(iname);
            registry_.get<otf2::definition::location_group>(ByExecutionScope(process.as_scope()))
                .name(iname);
            registry_.get<otf2::definition::comm_group>(ByProcess(process)).name(iname);
            registry_.get<otf2::definition::comm>(ByProcess(process)).name(iname);

            update_thread(process.as_thread(), name);
        }
        catch (const std::out_of_range&)
        {
            Log::warn() << "Attempting to update name of unknown " << process << " (" << name
                        << ")";
        }
    }

    if (parent != NO_PARENT_PROCESS)
    {
        emplace_process(NO_PARENT_PROCESS, parent, "");
        registry_.get<otf2::definition::system_tree_node>(ByProcess(process))
            .parent(registry_.get<otf2::definition::system_tree_node>(ByProcess(parent)));
    }
}

void Trace::update_thread(Thread thread, const std::string& name)
{
    // TODO we call this function in a hot-loop, locking doesn't sound like a good idea
    std::lock_guard<std::recursive_mutex> guard(mutex_);

    if (name == "")
    {
        return;
    }
    try
    {
        auto& iname = intern(fmt::format("{} ({})", name, thread.as_int()));
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

otf2::writer::local& Trace::sample_writer(const MeasurementScope& scope)
{
    assert(scope.scope.is_thread() || scope.scope.is_cpu());
    if (scope.scope.is_thread())
    {
        emplace_thread(trace::NO_PARENT_PROCESS, scope.scope.as_thread(), "");
    }

    // TODO we call this function in a hot-loop, locking doesn't sound like a good idea
    std::lock_guard<std::recursive_mutex> guard(mutex_);

    // If no process can be found for the thread, emplace it as its own process, this probably
    // breaks some other lo2s assumptions, but at least it results in lo2s not crashing
    if (!registry_.has<otf2::definition::location_group>(
            ByExecutionScope(groups_.get_parent(scope.scope))))
    {
        emplace_process(Process(scope.scope.as_thread().as_process()), NO_PARENT_PROCESS,
                        thread_names_[scope.scope.as_thread()]);
    }

    auto& intern_location = registry_.emplace<otf2::definition::location>(
        ByMeasurementScope(scope), intern(scope.name()),
        registry_.get<otf2::definition::location_group>(
            ByExecutionScope(groups_.get_parent(scope.scope))),
        otf2::definition::location::location_type::cpu_thread);

    comm_locations_group_.add_member(intern_location);

    if (groups_.get_parent(scope.scope).is_process())
    {
        registry_
            .get<otf2::definition::comm_group>(
                ByProcess(groups_.get_process(scope.scope.as_thread())))
            .add_member(intern_location);
    }

    return archive_(intern_location);
}

otf2::writer::local& Trace::syscall_writer(const ExecutionScope& scope)
{
    MeasurementScope meas_scope = MeasurementScope::syscall(scope);

    const auto& intern_location = registry_.emplace<otf2::definition::location>(
        ByMeasurementScope(meas_scope), intern(meas_scope.name()),
        registry_.get<otf2::definition::location_group>(
            ByExecutionScope(groups_.get_parent(scope))),
        otf2::definition::location::location_type::cpu_thread);

    return archive_(intern_location);
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

otf2::writer::local& Trace::bio_writer(BlockDevice dev)
{
    std::lock_guard<std::recursive_mutex> guard(mutex_);

    if (registry_.has<otf2::definition::location>(ByBlockDevice(dev)))
    {
        return archive_(registry_.get<otf2::definition::location>(ByBlockDevice(dev)));
    }

    const auto& name = intern(fmt::format("block I/O events for {}", dev.name));

    const auto& node = registry_.emplace<otf2::definition::system_tree_node>(
        ByBlockDevice(dev), intern(dev.name), intern("block dev"), bio_system_tree_node_);

    const auto& bio_location_group = registry_.emplace<otf2::definition::location_group>(
        ByBlockDevice(dev), name, otf2::common::location_group_type::process, node);

    const auto& intern_location = registry_.emplace<otf2::definition::location>(
        ByBlockDevice(dev), name, bio_location_group,
        otf2::definition::location::location_type::cpu_thread);

    hardware_comm_locations_group_.add_member(intern_location);
    return archive_(intern_location);
}

otf2::writer::local& Trace::posix_io_writer(Thread thread)
{
    MeasurementScope scope = MeasurementScope::posix_io(thread.as_scope());

    const auto& intern_location = registry_.emplace<otf2::definition::location>(
        ByMeasurementScope(scope), intern(scope.name()),
        registry_.get<otf2::definition::location_group>(
            ByExecutionScope(groups_.get_parent(thread.as_scope()))),
        otf2::definition::location::location_type::cpu_thread);

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

otf2::definition::io_handle& Trace::block_io_handle(BlockDevice dev)
{

    std::lock_guard<std::recursive_mutex> guard(mutex_);

    // io_pre_created_handle can not be emplaced because it has no ref.
    // So we have to check if we already created everything
    if (registry_.has<otf2::definition::io_handle>(ByBlockDevice(dev)))
    {
        return registry_.get<otf2::definition::io_handle>(ByBlockDevice(dev));
    }

    const auto& device_name = intern(dev.name);

    const otf2::definition::system_tree_node& parent = bio_parent_node(dev);

    std::string device_class = (dev.type == BlockDeviceType::PARTITION) ? "partition" : "disk";

    const auto& node = registry_.emplace<otf2::definition::system_tree_node>(
        ByBlockDevice(dev), device_name, intern(device_class), parent);

    const auto& file =
        registry_.emplace<otf2::definition::io_regular_file>(ByBlockDevice(dev), device_name, node);

    const auto& block_comm =
        registry_.emplace<otf2::definition::comm>(ByBlockDevice(dev), device_name, bio_comm_group_,
                                                  otf2::definition::comm::comm_flag_type::none);

    // we could have io handle parents and childs here (block dev being the parent (sda),
    // partition being the child (sda1)) but that seems like it would be overkill.
    auto& handle = registry_.emplace<otf2::definition::io_handle>(
        ByBlockDevice(dev), device_name, file, bio_paradigm_,
        otf2::common::io_handle_flag_type::pre_created, block_comm);

    // todo: set status flags accordingly
    registry_.create<otf2::definition::io_pre_created_handle_state>(
        handle, otf2::common::io_access_mode_type::read_write,
        otf2::common::io_status_flag_type::none);
    return handle;
}

otf2::definition::io_handle& Trace::posix_io_handle(Thread thread, int fd, int instance,
                                                    std::string& name)
{
    ThreadFdInstance id(thread, fd, instance);

    if (registry_.has<otf2::definition::io_handle>(ByThreadFdInstance(id)))
    {
        return registry_.get<otf2::definition::io_handle>(ByThreadFdInstance(id));
    }

    const auto& filename = intern(name);

    const auto& file = registry_.emplace<otf2::definition::io_regular_file>(
        ByString(name), filename, system_tree_root_node_);

    auto& handle = registry_.emplace<otf2::definition::io_handle>(
        ByThreadFdInstance(id), filename, file, posix_paradigm_,
        otf2::common::io_handle_flag_type::none,
        registry_.get<otf2::definition::comm>(ByProcess(groups_.get_process(thread))));

    // Mark stdin, stdout and stderr as pre-created, because they are
    if (fd < 3)
    {
        otf2::common::io_access_mode_type mode;

        if (fd == 0)
        {
            mode = otf2::common::io_access_mode_type::read_only;
        }
        else
        {
            mode = otf2::common::io_access_mode_type::write_only;
        }

        registry_.create<otf2::definition::io_pre_created_handle_state>(
            handle, mode, otf2::common::io_status_flag_type::none);
    }
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

otf2::definition::metric_class&
Trace::tracepoint_metric_class(const perf::tracepoint::TracepointEventAttr& event)
{
    if (!registry_.has<otf2::definition::metric_class>(ByString(event.name())))
    {
        auto& mc = registry_.create<otf2::definition::metric_class>(
            ByString(event.name()), otf2::common::metric_occurence::async,
            otf2::common::recorder_kind::abstract);

        for (const auto& field : event.fields())
        {
            if (field.is_integer())
            {
                mc.add_member(metric_member(event.name() + "::" + field.name(), "?",
                                            otf2::common::metric_mode::absolute_next,
                                            otf2::common::type::int64, "#"));
            }
        }
        return mc;
    }
    else
    {
        return registry_.get<otf2::definition::metric_class>(ByString(event.name()));
    }
}

otf2::definition::metric_class& Trace::metric_class()
{
    return registry_.create<otf2::definition::metric_class>(otf2::common::metric_occurence::async,
                                                            otf2::common::recorder_kind::abstract);
}

otf2::definition::calling_context&
Trace::cctx_for_gpu_kernel(uint64_t kernel_id, Resolvers& r, struct MergeContext& ctx,
                           GlobalCctxMap::value_type* global_node)
{
    LineInfo line_info = LineInfo::for_unknown_function();

    auto fr = r.gpu_function_resolvers.find(ctx.p);

    if (fr != r.gpu_function_resolvers.end())
    {
        auto it = fr->second.find(kernel_id);
        if (it != fr->second.end())
        {
            line_info = it->second->lookup_line_info(kernel_id);
        }
    }
    auto& new_cctx = registry_.create<otf2::definition::calling_context>(
        intern_region(line_info), intern_scl(line_info), *global_node->second.cctx);

    return new_cctx;
}

otf2::definition::calling_context& Trace::cctx_for_openmp(const CallingContext& context,
                                                          Resolvers& r, struct MergeContext& ctx,
                                                          GlobalCctxMap::value_type* global_node)
{
    LineInfo line_info = LineInfo::for_unknown_function();

    omp::OMPTCctx cctx = context.to_omp_cctx();
    auto addr = Address(cctx.addr);

    auto fr = r.function_resolvers.find(ctx.p);
    if (fr != r.function_resolvers.end())
    {
        auto it = fr->second.find(addr);

        if (it != fr->second.end())
        {
            line_info =
                it->second->lookup_line_info(addr - it->first.range.start + it->first.pgoff);
        }
    }

    if (line_info == LineInfo::for_unknown_function())
    {
        if (thread_names_.count(ctx.p.as_thread()))
        {
            line_info.dso = thread_names_.at(ctx.p.as_thread());
        }
    }

    std::string omp_name;
    switch (cctx.type)
    {
    case omp::OMPType::PARALLEL:
        omp_name = fmt::format("omp::parallel({})", cctx.num_threads);
        break;
    case omp::OMPType::MASTER:
        omp_name = "omp::master";
        break;
    case omp::OMPType::SYNC:
        omp_name = "omp::sync";
        break;
    case omp::OMPType::LOOP:
        omp_name = "omp::loop";
        break;
    case omp::OMPType::WORKSHARE:
        omp_name = "omp::workshare";
        break;
    case omp::OMPType::OTHER:
        omp_name = "omp::other";
        break;
    }

    if (line_info == LineInfo::for_unknown_function())
    {
        line_info.function = omp_name;
    }
    else
    {
        line_info.function = fmt::format("{} {}", omp_name, line_info.function);
    }

    auto& new_cctx = registry_.create<otf2::definition::calling_context>(
        intern_region(line_info), intern_scl(line_info), *global_node->second.cctx);

    return new_cctx;
}

otf2::definition::calling_context& Trace::cctx_for_address(Address addr, Resolvers& r,
                                                           struct MergeContext& ctx,
                                                           GlobalCctxMap::value_type* global_node)
{
    LineInfo line_info = LineInfo::for_unknown_function();

    auto& fr = r.function_resolvers.emplace(ctx.p, ctx.p).first->second;
    auto it = fr.find(addr);
    if (it != fr.end())
    {
        line_info = it->second->lookup_line_info(addr - it->first.range.start + it->first.pgoff);
    }

    auto& new_cctx = registry_.create<otf2::definition::calling_context>(
        intern_region(line_info), intern_scl(line_info), *global_node->second.cctx);

    if (config().disassemble)
    {
        auto it = r.instruction_resolvers[ctx.p].find(addr);

        if (it != r.instruction_resolvers[ctx.p].end())
        {
            try
            {
                std::string instruction =
                    it->second->lookup_instruction(addr - it->first.range.start + it->first.pgoff);
                Log::trace() << "mapped " << addr << " to " << instruction;

                registry_.create<otf2::definition::calling_context_property>(
                    new_cctx, intern("instruction"), otf2::attribute_value(intern(instruction)));
            }
            catch (std::exception& ex)
            {
                Log::trace() << "could not read instruction from " << addr << ": " << ex.what();
            }
        }
    }
    return new_cctx;
}

otf2::definition::calling_context& Trace::cctx_for_thread(Thread thread, MergeContext& ctx)
{

    emplace_thread(ctx.p, thread, "");

    if (registry_.has<otf2::definition::calling_context>(ByThread(thread)))
    {
        return registry_.get<otf2::definition::calling_context>(ByThread(thread));
    }

    auto name = thread_names_.at(thread);
    auto& iname = intern(fmt::format("{} ({})", name, thread.as_int()));

    auto& thread_region = registry_.emplace<otf2::definition::region>(
        ByThread(thread), iname, iname, iname, otf2::common::role_type::function,
        otf2::common::paradigm_type::user, otf2::common::flags_type::none, iname, 0, 0);

    try
    {
        Process parent = groups_.get_process(thread);

        auto proc_name = thread_names_[parent.as_thread()];
        auto& regions_group = registry_.emplace<otf2::definition::regions_group>(
            ByProcess(parent), intern(proc_name), otf2::common::paradigm_type::user,
            otf2::common::group_flag_type::none);

        regions_group.add_member(thread_region);
    }
    // If no parent can be found emplace a thread as it's own regions_group
    catch (const std::out_of_range&)
    {
        auto& regions_group = registry_.emplace<otf2::definition::regions_group>(
            ByThread(thread), intern(name), otf2::common::paradigm_type::user,
            otf2::common::group_flag_type::none);

        regions_group.add_member(thread_region);
    }

    // create calling context
    return registry_.create<otf2::definition::calling_context>(
        ByThread(thread), thread_region, otf2::definition::source_code_location());
}

otf2::definition::calling_context& Trace::cctx_for_process(Process process)
{
    emplace_process(NO_PARENT_PROCESS, process, "");

    if (registry_.has<otf2::definition::calling_context>(ByProcess(process)))
    {
        return registry_.get<otf2::definition::calling_context>(ByProcess(process));
    }

    auto name = thread_names_[process.as_thread()];
    auto& iname = intern(name);

    auto& thread_region = registry_.emplace<otf2::definition::region>(
        ByProcess(process), iname, iname, iname, otf2::common::role_type::function,
        otf2::common::paradigm_type::user, otf2::common::flags_type::none, iname, 0, 0);

    // create calling context
    return registry_.create<otf2::definition::calling_context>(
        ByProcess(process), thread_region, otf2::definition::source_code_location());
}

/*
 * This function merges the nodes of the local calling context tree into the global calling context
 * tree.
 *
 * This is accomplished using a recursive depth first walk of the local calling context tree
 */
void Trace::merge_nodes(const std::map<CallingContext, LocalCctxNode>::value_type& local_node,
                        std::map<CallingContext, GlobalCctxNode>::value_type* global_node,
                        std::vector<uint32_t>& mapping_table, Resolvers& r,
                        struct MergeContext& ctx)
{
    for (const auto& local_child : local_node.second.children)
    {
        // If there is already a node on the global tree matching the node on the local tree, use
        // it, Otherwise emplace a new global node
        auto global_child = global_node->second.children.find(local_child.first);
        if (global_child == global_node->second.children.end())
        {
            otf2::definition::calling_context* new_cctx = nullptr;

            switch (local_child.first.type)
            {
            case lo2s::CallingContextType::SAMPLE_ADDR:
                new_cctx = &cctx_for_address(local_child.first.to_addr(), r, ctx, global_node);
                break;
            case lo2s::CallingContextType::GPU_KERNEL:
                new_cctx =
                    &cctx_for_gpu_kernel(local_child.first.to_kernel_id(), r, ctx, global_node);
                break;
            case lo2s::CallingContextType::OPENMP:
                new_cctx = &cctx_for_openmp(local_child.first, r, ctx, global_node);
                break;
            case lo2s::CallingContextType::THREAD:
                new_cctx = &cctx_for_thread(local_child.first.to_thread(), ctx);
                break;
            case lo2s::CallingContextType::PROCESS:
                new_cctx = &cctx_for_process(local_child.first.to_process());
                break;
            case lo2s::CallingContextType::SYSCALL:
                new_cctx = &cctx_for_syscall(local_child.first.to_syscall_id());
                break;
            case lo2s::CallingContextType::ROOT:
                throw std::runtime_error("The cctx tree ROOT should not appear in merge_nodes!");
            }

            auto r = global_node->second.children.emplace(std::piecewise_construct,
                                                          std::forward_as_tuple(local_child.first),
                                                          std::forward_as_tuple(new_cctx));
            global_child = r.first;
        }

        // Write a mapping local cctx reference number -> global reference number
        mapping_table.at(local_child.second.ref) = global_child->second.cctx->ref();

        // If we later want to resolve addresses, we need to know in which process we are,
        // so if we are currently in a Process node, save the Process for later use.
        if (global_child->first.type == CallingContextType::PROCESS)
        {
            ctx.p = global_child->first.to_process();
        }

        // Depth first recursive descent
        merge_nodes(local_child, &(*global_child), mapping_table, r, ctx);
    }
}

otf2::definition::mapping_table Trace::merge_calling_contexts(const LocalCctxTree& local_cctxs,
                                                              Resolvers& r)
{
    std::lock_guard<std::recursive_mutex> guard(mutex_);
#ifndef NDEBUG
    std::vector<uint32_t> mappings(local_cctxs.num_cctx(), -1u);
#else
    std::vector<uint32_t> mappings(local_cctxs.num_cctx());
#endif

    struct MergeContext ctx;

    merge_nodes(local_cctxs.get_tree(), &calling_context_tree_, mappings, r, ctx);

#ifndef NDEBUG
    for (auto id : mappings)
    {
        assert(id != -1u);
    }
#endif

    return otf2::definition::mapping_table(
        otf2::definition::mapping_table::mapping_type_type::calling_context, mappings);
}

otf2::definition::calling_context& Trace::cctx_for_syscall(uint64_t syscall_id)
{
    const auto& syscall_name = intern_syscall_str(syscall_id);

    const auto& intern_region = registry_.emplace<otf2::definition::region>(
        BySyscall(syscall_id), syscall_name, syscall_name, syscall_name,
        otf2::common::role_type::function, otf2::common::paradigm_type::user,
        otf2::common::flags_type::none, syscall_name, 0, 0);

    const auto& intern_scl = registry_.emplace<otf2::definition::source_code_location>(
        BySyscall(syscall_id), syscall_name, 0);

    auto& ctx = registry_.emplace<otf2::definition::calling_context>(BySyscall(syscall_id),
                                                                     intern_region, intern_scl);

    syscall_regions_group_.add_member(intern_region);

    return ctx;
}

void Trace::emplace_thread_exclusive(Process process, Thread thread, const std::string& name,
                                     const std::lock_guard<std::recursive_mutex>&)
{

    emplace_process(NO_PARENT_PROCESS, process, "");
    groups_.add_thread(thread, process);

    std::string thread_name = name;
    if (thread == Thread(0))
    {
        thread_name = "<idle>";
    }

    if (registry_.has<otf2::definition::calling_context>(ByThread(thread)))
    {
        update_thread(thread, name);
        return;
    }

    thread_names_.emplace(std::piecewise_construct, std::forward_as_tuple(thread),
                          std::forward_as_tuple(thread_name));

    if (thread != Thread(0))
    {
        thread_name = fmt::format("{} ({})", name, thread.as_int());
    }

    auto& iname = intern(thread_name);

    auto& thread_region = registry_.emplace<otf2::definition::region>(
        ByThread(thread), iname, iname, iname, otf2::common::role_type::function,
        otf2::common::paradigm_type::user, otf2::common::flags_type::none, iname, 0, 0);

    if (is_kernel_thread(thread))
    {
        kernel_regions_group_.add_member(thread_region);
    }

    // create calling context
    registry_.create<otf2::definition::calling_context>(ByThread(thread), thread_region,
                                                        otf2::definition::source_code_location());
}

void Trace::emplace_thread(Process process, Thread thread, const std::string& name)
{
    // Lock this to avoid conflict on regions_thread_ with emplace_monitoring_thread
    std::lock_guard<std::recursive_mutex> guard(mutex_);
    emplace_thread_exclusive(process, thread, name, guard);
}

void Trace::emplace_monitoring_thread(Thread thread, const std::string& name,
                                      const std::string& group)
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

        registry_.emplace<otf2::definition::calling_context>(
            ByThread(thread), ret, otf2::definition::source_code_location());
    }
}

void Trace::emplace_threads(const std::map<Process, std::map<Thread, std::string>>& thread_map)
{
    // Lock here to avoid conflicts when writing to regions_thread_
    std::lock_guard<std::recursive_mutex> guard(mutex_);
    for (const auto& process : thread_map)
    {

        emplace_process(NO_PARENT_PROCESS, process.first,
                        process.second.at(process.first.as_thread()));
        for (const auto& thread : process.second)
        {
            emplace_thread_exclusive(process.first, thread.first, thread.second, guard);
        }
    }
}

const otf2::definition::string& Trace::intern_syscall_str(int64_t syscall_nr)
{
    return intern(syscall_name_for_nr(syscall_nr));
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

LocalCctxTree& Trace::create_local_cctx_tree(const MeasurementScope& scope)
{
    std::lock_guard<std::mutex> guard(local_cctx_trees_mutex_);

    assert(!local_cctx_trees_finalized_);

    return local_cctx_trees_.emplace_back(*this, scope);
}

void Trace::finalize(Resolvers& resolvers)
{
    for (auto& local_cctx : local_cctx_trees_)
    {
        if (local_cctx.num_cctx() > 0)
        {
            const auto& mapping = merge_calling_contexts(local_cctx, resolvers);
            local_cctx.writer() << mapping;
        }
    }
    local_cctx_trees_.clear();
    auto finalized_twice = local_cctx_trees_finalized_.exchange(true);
    if (finalized_twice)
    {
        Log::error() << "Trace::merge_calling_contexts was called twice."
                        "This is a bug, please report it to the developers.";
    }
}
} // namespace trace
} // namespace lo2s
