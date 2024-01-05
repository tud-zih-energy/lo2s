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
  syscall_regions_group_(registry_.create<otf2::definition::regions_group>(
      intern("<syscalls>"), otf2::common::paradigm_type::user,
      otf2::common::group_flag_type::none)),
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
            otf2::common::base_type::decimal, 0, config().sampling_period);
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
    starting_time_ = time::now();
    starting_system_time_ = std::chrono::system_clock::now();

    const std::time_t t_c = std::chrono::system_clock::to_time_t(starting_system_time_);
    add_lo2s_property("STARTING_TIME", fmt::format("{:%FT%T%z}", fmt::localtime(t_c)));
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
    if (!cctx_refs_finalized_)
    {
        Log::error()
            << "cctx refs have not been finalized, please report this bug to the developers";
    }
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

otf2::writer::local& Trace::sample_writer(const ExecutionScope& scope)
{
    // TODO we call this function in a hot-loop, locking doesn't sound like a good idea
    std::lock_guard<std::recursive_mutex> guard(mutex_);

    MeasurementScope sample_scope = MeasurementScope::sample(scope);

    const auto& intern_location = registry_.emplace<otf2::definition::location>(
        ByMeasurementScope(sample_scope), intern(sample_scope.name()),
        registry_.get<otf2::definition::location_group>(
            ByExecutionScope(groups_.get_parent(scope))),
        otf2::definition::location::location_type::cpu_thread);

    comm_locations_group_.add_member(intern_location);

    if (groups_.get_parent(scope).is_process())
    {
        registry_
            .get<otf2::definition::comm_group>(ByProcess(groups_.get_process(scope.as_thread())))
            .add_member(intern_location);
    }

    return archive_(intern_location);
}

otf2::writer::local& Trace::cuda_writer(const Thread& thread)
{
    MeasurementScope scope = MeasurementScope::cuda(thread.as_scope());

    const auto& cuda_location_group = registry_.emplace<otf2::definition::location_group>(
        ByMeasurementScope(scope), intern(scope.name()),
        otf2::common::location_group_type::accelerator, system_tree_root_node_);

    const auto& intern_location = registry_.emplace<otf2::definition::location>(
        ByMeasurementScope(scope), intern(scope.name()), cuda_location_group,
        otf2::definition::location::location_type::accelerator_stream);

    return archive_(intern_location);
}

otf2::writer::local& Trace::nec_writer(NecDevice device, const Thread& nec_thread)
{

    auto& intern_name = intern(fmt::format("{} {}", device, get_nec_thread_comm(nec_thread)));

    const auto& node = registry_.emplace<otf2::definition::system_tree_node>(
        ByNecDevice(device), intern(fmt::format("{}", device)), intern("NEC vector accelerator"),
        system_tree_root_node_);

    const auto& nec_location_group = registry_.emplace<otf2::definition::location_group>(
        ByNecThread(nec_thread), intern_name, otf2::common::location_group_type::process, node);

    const auto& intern_location = registry_.emplace<otf2::definition::location>(
        ByNecThread(nec_thread), intern_name, nec_location_group,
        otf2::definition::location::location_type::cpu_thread);

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

otf2::definition::calling_context& Trace::cuda_calling_context(std::string& file,
                                                               std::string& function)
{
    LineInfo info = LineInfo::for_function(file.c_str(), function.c_str(), 0, "");

    return registry_.emplace<otf2::definition::calling_context>(
        ByLineInfo(info), intern_region(info), intern_scl(info));
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

void Trace::merge_ips(const IpRefMap& new_children, IpCctxMap& children,
                      std::vector<uint32_t>& mapping_table,
                      otf2::definition::calling_context& parent,
                      const std::map<Process, ProcessInfo>& infos, Process process)
{
    for (const auto& elem : new_children)
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

otf2::definition::mapping_table
Trace::merge_calling_contexts(const std::map<Thread, ThreadCctxRefs>& new_ips, size_t num_ip_refs,
                              const std::map<Process, ProcessInfo>& infos)
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

                if (auto thread_name = thread_names_.find(thread);
                    thread_name != thread_names_.end())
                {
                    add_thread(thread, thread_name->second);
                }
                else
                {
                    if (auto process_name = thread_names_.find(process.as_thread());
                        process_name != thread_names_.end())
                    {
                        add_thread(thread, process_name->second);
                    }
                    else
                    {
                        add_thread(thread, "<unknown thread>");
                    }
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

otf2::definition::mapping_table
Trace::merge_syscall_contexts(const std::set<int64_t>& used_syscalls)
{
    std::vector<uint32_t> mappings;
    if (!config().syscall_filter.empty())
    {
        mappings.resize(
            *std::max_element(config().syscall_filter.begin(), config().syscall_filter.end()) + 1);
    }
    else
    {
        mappings.resize(__NR_syscalls);
    }

    for (const auto& syscall_nr : used_syscalls)
    {
        const auto& syscall_name = intern_syscall_str(syscall_nr);

        const auto& intern_region = registry_.emplace<otf2::definition::region>(
            BySyscall(syscall_nr), syscall_name, syscall_name, syscall_name,
            otf2::common::role_type::function, otf2::common::paradigm_type::user,
            otf2::common::flags_type::none, syscall_name, 0, 0);

        const auto& intern_scl = registry_.emplace<otf2::definition::source_code_location>(
            BySyscall(syscall_nr), syscall_name, 0);

        const auto& ctx = registry_.emplace<otf2::definition::calling_context>(
            BySyscall(syscall_nr), intern_region, intern_scl);

        syscall_regions_group_.add_member(intern_region);

        mappings.at(syscall_nr) = ctx.ref();
    }

    return otf2::definition::mapping_table(otf2::common::mapping_type_type::calling_context,
                                           mappings);
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

ThreadCctxRefMap& Trace::create_cctx_refs()
{
    std::lock_guard<std::mutex> guard(cctx_refs_mutex_);

    assert(!cctx_refs_finalized_);

    return cctx_refs_.emplace_back();
}

void Trace::merge_calling_contexts(const std::map<Process, ProcessInfo>& process_infos)
{
    for (auto& cctx : cctx_refs_)
    {
        assert(cctx.writer != nullptr);
        if (cctx.ref_count > 0)
        {
            const auto& mapping = merge_calling_contexts(cctx.map, cctx.ref_count, process_infos);
            (*cctx.writer) << mapping;
        }
    }
    cctx_refs_.clear();
    auto finalized_twice = cctx_refs_finalized_.exchange(true);
    if (finalized_twice)
    {
        Log::error() << "Trace::merge_calling_contexts was called twice."
                        "This is a bug, please report it to the developers.";
    }
}
} // namespace trace
} // namespace lo2s
