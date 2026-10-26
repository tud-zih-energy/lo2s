// SPDX-FileCopyrightText: 2016 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <lo2s/trace/trace.hpp>

#include <lo2s/address.hpp>
#include <lo2s/calling_context.hpp>
#include <lo2s/config.hpp>
#include <lo2s/execution_scope_group.hpp>
#include <lo2s/line_info.hpp>
#include <lo2s/local_cctx_tree.hpp>
#include <lo2s/log.hpp>
#include <lo2s/ompt/events.hpp>
#include <lo2s/perf/bio/block_device.hpp>
#include <lo2s/perf/counter/counter_collection.hpp>
#include <lo2s/perf/tracepoint/event_attr.hpp>
#include <lo2s/perf/tracepoint/format.hpp>
#include <lo2s/resolvers.hpp>
#include <lo2s/summary.hpp>
#include <lo2s/syscalls.hpp>
#include <lo2s/thread_fd_instance.hpp>
#include <lo2s/time/time.hpp>
#include <lo2s/trace/handle.hpp>
#include <lo2s/trace/reg_keys.hpp>
#include <lo2s/trace/thread_names.hpp>
#include <lo2s/types/core.hpp>
#include <lo2s/types/cpu.hpp>
#include <lo2s/types/package.hpp>
#include <lo2s/types/process.hpp>
#include <lo2s/types/thread.hpp>
#include <lo2s/util.hpp>
#include <lo2s/version.hpp>

#include <nitro/env/get.hpp>
#include <nitro/env/hostname.hpp>
#include <nitro/lang/string.hpp>
#include <otf2xx/attribute_value.hpp>
#include <otf2xx/common.hpp>
#include <otf2xx/definition/calling_context.hpp>
#include <otf2xx/definition/comm.hpp>
#include <otf2xx/definition/interrupt_generator.hpp>
#include <otf2xx/definition/io_handle.hpp>
#include <otf2xx/definition/io_paradigm.hpp>
#include <otf2xx/definition/io_pre_created_handle_state.hpp>
#include <otf2xx/definition/io_regular_file.hpp>
#include <otf2xx/definition/location.hpp>
#include <otf2xx/definition/location_group.hpp>
#include <otf2xx/definition/mapping_table.hpp>
#include <otf2xx/definition/metric_class.hpp>
#include <otf2xx/definition/metric_instance.hpp>
#include <otf2xx/definition/metric_member.hpp>
#include <otf2xx/definition/region.hpp>
#include <otf2xx/definition/string.hpp>
#include <otf2xx/definition/system_tree_node.hpp>
#include <otf2xx/definition/system_tree_node_domain.hpp>
#include <otf2xx/writer/local.hpp>

#include <chrono>
#include <filesystem>
#include <map>
#include <mutex>
#include <optional>
#include <queue>
#include <regex>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

#include <cassert>
#include <cstdint>

#include <fmt/chrono.h> //NOLINT
#include <fmt/core.h>
#include <fmt/format.h>

extern "C"
{
#include <otf2/OTF2_GeneralDefinitions.h>
}

namespace lo2s::trace
{

namespace
{
std::string get_trace_name()
{
    std::string path = config().otf2.trace_path;
    nitro::lang::replace_all(path, "{DATE}", get_datetime());
    nitro::lang::replace_all(path, "{HOSTNAME}", nitro::env::hostname());

    auto result = path;

    std::regex const env_re("\\{ENV=([^}]*)\\}");
    const auto env_begin = std::sregex_iterator(path.begin(), path.end(), env_re);
    const auto env_end = std::sregex_iterator();

    for (std::sregex_iterator it = env_begin; it != env_end; ++it)
    {
        nitro::lang::replace_all(result, it->str(), nitro::env::get((*it)[1]));
    }

    return result;
}
} // namespace

Trace::Trace()
: trace_name_(get_trace_name()),
  archive_(trace_name_, "traces", OTF2_FILEMODE_WRITE, config().otf2.event_chunk_size,
           config().otf2.definition_chunk_size),
  registry_(archive_.registry()),
  calling_context_tree_(CallingContext::root(), GlobalCctxNode(nullptr)),
  interrupt_generator_(registry_.create<otf2::definition::interrupt_generator>(
      to_otf2_string("perf HW_INSTRUCTIONS"), otf2::common::interrupt_generator_mode_type::count,
      otf2::common::base_type::decimal, 0, config().perf.sampling.period)),
  comm_locations_group_(registry_.create<otf2::definition::comm_locations_group>(
      to_otf2_string("All pthread locations"), otf2::common::paradigm_type::pthread,
      otf2::common::group_flag_type::none)),
  hardware_comm_locations_group_(registry_.create<otf2::definition::comm_locations_group>(
      to_otf2_string("All hardware locations"), otf2::common::paradigm_type::hardware,
      otf2::common::group_flag_type::none)),
  lo2s_regions_group_(registry_.create<otf2::definition::regions_group>(
      to_otf2_string("lo2s"), otf2::common::paradigm_type::user,
      otf2::common::group_flag_type::none)),
  syscall_regions_group_(registry_.create<otf2::definition::regions_group>(
      to_otf2_string("<syscalls>"), otf2::common::paradigm_type::user,
      otf2::common::group_flag_type::none)),
  kernel_regions_group_(registry_.create<otf2::definition::regions_group>(
      to_otf2_string("<kernel threads>"), otf2::common::paradigm_type::user,
      otf2::common::group_flag_type::none)),
  system_tree_root_node_(registry_.create<otf2::definition::system_tree_node>(
      to_otf2_string(nitro::env::hostname()), to_otf2_string("machine")))
{
    Log::info() << "Using trace directory: " << trace_name_;
    summary().set_trace_dir(trace_name_);

    archive_.set_creator(std::string("lo2s - ") + lo2s::version());
    archive_.set_description(config().general.lo2s_command_line);

    const auto& uname = lo2s::get_uname();
    add_lo2s_property("UNAME::SYSNAME", std::string{ uname.sysname });
    add_lo2s_property("UNAME::NODENAME", std::string{ uname.nodename });
    add_lo2s_property("UNAME::RELEASE", std::string{ uname.release });
    add_lo2s_property("UNAME::VERSION", std::string{ uname.version });
    add_lo2s_property("UNAME::MACHINE", std::string{ uname.machine });

    registry_.create<otf2::definition::location_group>(
        ByThread(Thread(Thread(METRIC_PID))), to_otf2_string("Metric Location Group"),
        otf2::definition::location_group::location_group_type::process, system_tree_root_node_);

    registry_.create<otf2::definition::system_tree_node_domain>(
        system_tree_root_node_, otf2::common::system_tree_node_domain::shared_memory);

    ThreadGroup::instance().add_process(Process::no_parent());

    if (config().perf.block_io.enabled)
    {
        bio_system_tree_node_ = registry_.create<otf2::definition::system_tree_node>(
            to_otf2_string("block devices"), to_otf2_string("hardware"), system_tree_root_node_);

        const std::vector<otf2::common::io_paradigm_property_type> properties;
        const std::vector<otf2::attribute_value> values;
        bio_paradigm_ = registry_.create<otf2::definition::io_paradigm>(
            to_otf2_string("block_io"), to_otf2_string("block layer I/O"),
            otf2::common::io_paradigm_class_type::parallel, otf2::common::io_paradigm_flag_type::os,
            properties, values);

        bio_comm_group_ = registry_.create<otf2::definition::comm_group>(
            to_otf2_string("block devices"), otf2::common::paradigm_type::hardware,
            otf2::common::group_flag_type::none);

        for (const auto& device : get_block_devices())
        {
            if (device.second.type == BlockDeviceType::DISK)
            {
                block_io_handle(device.second);
                bio_writer(device.second);
            }
        }
    }

    if (config().perf.posix_io.enabled)
    {

        const std::vector<otf2::common::io_paradigm_property_type> properties;
        const std::vector<otf2::attribute_value> values;
        posix_paradigm_ = registry_.create<otf2::definition::io_paradigm>(
            to_otf2_string("POSIX"), to_otf2_string("POSIX I/O"),
            otf2::common::io_paradigm_class_type::parallel, otf2::common::io_paradigm_flag_type::os,
            properties, values);

        posix_comm_group_ = registry_.create<otf2::definition::comm_group>(
            to_otf2_string("POSIX I/O files"), otf2::common::paradigm_type::hardware,
            otf2::common::group_flag_type::none);
    }
}

void Trace::begin_record()
{
    starting_time_ = time::now();
    starting_system_time_ = std::chrono::system_clock::now();
    Log::info() << "Initialization done. Start recording...";
    add_lo2s_property("STARTING_TIME", fmt::format("{:%FT%T%z}", starting_system_time_));
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
        system_tree_root_node_, to_otf2_string(property_name),
        otf2::attribute_value{ to_otf2_string(value) });
}

otf2::writer::local& Trace::sample_writer(SampleKind kind, std::variant<Cpu, Thread> scope)
{
    // TODO we call this function in a hot-loop, locking doesn't sound like a good idea
    std::lock_guard<std::recursive_mutex> const guard(mutex_);

    if (std::holds_alternative<Thread>(scope))
    {
        const auto thread = std::get<Thread>(scope);
        auto process_hd = process_handle(Process::no_parent(),
                                         ThreadGroup::instance().get_process(thread), std::nullopt);

        const auto& intern_location = registry_.emplace<otf2::definition::location>(
            BySampleWriter(std::make_pair(kind, scope)), to_otf2_string(scope_get_name(scope)),
            process_hd.location_group(), otf2::definition::location::location_type::cpu_thread);
        comm_locations_group_.add_member(intern_location);

        process_hd.comm_group().add_member(intern_location);
        return archive_(intern_location);
    }
    const auto cpu = std::get<Cpu>(scope);

    auto cpu_hd = cpu_handle(cpu);

    const auto& intern_location = registry_.emplace<otf2::definition::location>(
        BySampleWriter(std::make_pair(kind, scope)), to_otf2_string(scope_get_name(scope)),
        cpu_hd.location_group(), otf2::definition::location::location_type::cpu_thread);
    comm_locations_group_.add_member(intern_location);
    return archive_(intern_location);
}

otf2::writer::local& Trace::syscall_writer(std::variant<Cpu, Thread> scope)
{
    otf2::definition::location_group* gr = nullptr;

    if (std::holds_alternative<Cpu>(scope))
    {
        gr = &cpu_handle(std::get<Cpu>(scope)).location_group();
    }
    else
    {
        gr = &process_handle(Process::no_parent(),
                             ThreadGroup::instance().get_process(std::get<Thread>(scope)),
                             std::nullopt)
                  .location_group();
    }
    const auto& intern_location = registry_.emplace<otf2::definition::location>(
        BySyscallWriter(scope),
        to_otf2_string(fmt::format("syscall events for {}", scope_get_name(scope))), *gr,
        otf2::definition::location::location_type::cpu_thread);
    return archive_(intern_location);
}

otf2::writer::local& Trace::metric_writer(std::variant<Cpu, Thread> scope)
{
    otf2::definition::location_group* gr = nullptr;

    if (std::holds_alternative<Cpu>(scope))
    {
        gr = &cpu_handle(std::get<Cpu>(scope)).location_group();
    }
    else
    {
        gr = &process_handle(Process::no_parent(),
                             ThreadGroup::instance().get_process(std::get<Thread>(scope)),
                             std::nullopt)
                  .location_group();
    }
    const auto& intern_location = registry_.emplace<otf2::definition::location>(
        ByMetricWriter(scope), to_otf2_string(scope_get_name(scope)), *gr,
        otf2::definition::location::location_type::metric);
    return archive_(intern_location);
}

otf2::writer::local& Trace::bio_writer(BlockDevice dev)
{
    std::lock_guard<std::recursive_mutex> const guard(mutex_);

    if (registry_.has<otf2::definition::location>(ByBlockDevice(dev)))
    {
        return archive_(registry_.get<otf2::definition::location>(ByBlockDevice(dev)));
    }

    const auto& name = to_otf2_string(fmt::format("block I/O events for {}", dev.name));

    const auto& node = registry_.emplace<otf2::definition::system_tree_node>(
        ByBlockDevice(dev), to_otf2_string(dev.name), to_otf2_string("block dev"),
        bio_system_tree_node_);

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
    auto& loc = process_handle(Process::no_parent(), ThreadGroup::instance().get_process(thread),
                               std::nullopt)
                    .location_group();

    const auto& intern_location = registry_.emplace<otf2::definition::location>(
        ByPosixIoWriter(thread), to_otf2_string(fmt::format("thread {}", thread.as_int())), loc,
        otf2::definition::location::location_type::cpu_thread);

    return archive_(intern_location);
}

otf2::writer::local& Trace::create_metric_writer(const std::string& name)
{
    const auto& location = registry_.create<otf2::definition::location>(
        to_otf2_string(name),
        registry_.get<otf2::definition::location_group>(ByThread(Thread(METRIC_PID))),
        otf2::definition::location::location_type::metric);

    return archive_(location);
}

otf2::definition::io_handle& Trace::block_io_handle(BlockDevice dev)
{
    std::lock_guard<std::recursive_mutex> const guard(mutex_);

    // io_pre_created_handle can not be emplaced because it has no ref.
    // So we have to check if we already created everything
    if (registry_.has<otf2::definition::io_handle>(ByBlockDevice(dev)))
    {
        return registry_.get<otf2::definition::io_handle>(ByBlockDevice(dev));
    }

    const auto& device_name = to_otf2_string(dev.name);

    const otf2::definition::system_tree_node& parent = bio_parent_node(dev);

    std::string const device_class =
        (dev.type == BlockDeviceType::PARTITION) ? "partition" : "disk";

    const auto& node = registry_.emplace<otf2::definition::system_tree_node>(
        ByBlockDevice(dev), device_name, to_otf2_string(device_class), parent);

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
    ThreadFdInstance const id(thread, fd, instance);

    if (registry_.has<otf2::definition::io_handle>(ByThreadFdInstance(id)))
    {
        return registry_.get<otf2::definition::io_handle>(ByThreadFdInstance(id));
    }

    const auto& filename = to_otf2_string(name);

    const auto& file = registry_.emplace<otf2::definition::io_regular_file>(
        ByString(name), filename, system_tree_root_node_);

    auto& handle = registry_.emplace<otf2::definition::io_handle>(
        ByThreadFdInstance(id), filename, file, posix_paradigm_,
        otf2::common::io_handle_flag_type::none,
        process_handle(Process::no_parent(), ThreadGroup::instance().get_process(thread),
                       std::nullopt)
            .comm());

    // Mark stdin, stdout and stderr as pre-created, because they are
    if (fd < 3)
    {
        otf2::common::io_access_mode_type mode = otf2::common::io_access_mode_type::read_only;

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
        to_otf2_string(name), to_otf2_string(description), otf2::common::metric_type::other, mode,
        value_type, base, exponent, to_otf2_string(unit));
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
    return registry_.get<otf2::definition::metric_class>(ByString(event.name()));
}

otf2::definition::metric_class& Trace::metric_class()
{
    return registry_.create<otf2::definition::metric_class>(otf2::common::metric_occurence::async,
                                                            otf2::common::recorder_kind::abstract);
}

otf2::definition::calling_context&
Trace::cctx_for_gpu_kernel(uint64_t kernel_id, Resolvers& r, Process p, GlobalCctxNode& global_node)
{
    LineInfo line_info = LineInfo::for_unknown_function();

    const auto fr = r.gpu_function_resolvers.find(p);

    if (fr != r.gpu_function_resolvers.end())
    {
        line_info = fr->second->lookup_line_info(Address{ kernel_id });
    }

    auto& new_cctx = registry_.create<otf2::definition::calling_context>(
        intern_region(line_info), intern_scl(line_info), *global_node.cctx);

    return new_cctx;
}

otf2::definition::calling_context& Trace::cctx_for_openmp(const CallingContext& context,
                                                          Resolvers& r, Process p,
                                                          GlobalCctxNode& global_node)
{
    LineInfo line_info = LineInfo::for_unknown_function();

    ompt::OMPTCctx cctx = context.to_omp_cctx();
    const auto addr = Address(cctx.addr);

    const auto fr = r.function_resolvers.find(p);
    if (fr != r.function_resolvers.end())
    {
        const auto it = fr->second.find(addr);

        if (it != fr->second.end())
        {
            line_info = it->second->lookup_line_info(addr, it->first.range.start.value());
        }
    }

    if (line_info == LineInfo::for_unknown_function() && ThreadNames::instance().has(p.as_thread()))
    {
        line_info.dso = ThreadNames::instance().get(p.as_thread());
    }

    std::string omp_name;
    switch (cctx.type)
    {
    case ompt::OMPType::PARALLEL:
        omp_name = fmt::format("ompt::parallel({})", cctx.num_threads);
        break;
    case ompt::OMPType::MASTER:
        omp_name = "ompt::master";
        break;
    case ompt::OMPType::SYNC:
        omp_name = "ompt::sync";
        break;
    case ompt::OMPType::LOOP:
        omp_name = "ompt::loop";
        break;
    case ompt::OMPType::WORKSHARE:
        omp_name = "ompt::workshare";
        break;
    case ompt::OMPType::OTHER:
        omp_name = "ompt::other";
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
        intern_region(line_info), intern_scl(line_info), *global_node.cctx);

    return new_cctx;
}

otf2::definition::calling_context& Trace::cctx_for_address(Address addr, Resolvers& r, Process p,
                                                           GlobalCctxNode& global_node)
{
    Log::trace() << "Resolving: " << addr << " in " << p.as_int();

    LineInfo line_info = LineInfo::for_unknown_function();

    auto& fr = r.function_resolvers.emplace(p, p).first->second;
    const auto it = fr.find(addr);
    if (it != fr.end())
    {
        Log::trace() << "Resolving " << addr << " in object: ";
        Log::trace() << it->first.to_string() << " " << it->second->name() << "PID: " << p.as_int()
                     << " (" << ThreadNames::instance().get(p.as_thread()) << ")";
        line_info = it->second->lookup_line_info(
            addr, (it->first.range.start.value() - it->first.pgoff.value()));
    }
    else
    {
        Log::trace() << "No object found for: " << addr << " in " << p.as_int() << " ("
                     << ThreadNames::instance().get(p.as_thread()) << ")";
    }

    auto& new_cctx = registry_.create<otf2::definition::calling_context>(
        intern_region(line_info), intern_scl(line_info), *global_node.cctx);

    registry_.create<otf2::definition::calling_context_property>(
        new_cctx, to_otf2_string("ip"),
        otf2::attribute_value(to_otf2_string(std::to_string(addr.value()))));

    return new_cctx;
}

otf2::definition::calling_context& Trace::cctx_for_thread(Thread thread, Process p)
{

    return thread_handle(p, thread, std::nullopt).calling_context();
}

otf2::definition::calling_context& Trace::cctx_for_process(Process process)
{
    return thread_handle(process, process.as_thread(), std::nullopt).calling_context();
}

namespace
{
struct MergeElememt
{
    const CallingContext& local_cctx;
    const LocalCctxNode& local_node;
    const CallingContext& global_cctx;
    GlobalCctxNode& global_node;
    Process p;
};
}

otf2::definition::mapping_table Trace::merge_calling_contexts(const LocalCctxTree& local_cctxs,
                                                              Resolvers& r)
{
#ifndef NDEBUG
    std::vector<uint32_t> mappings(local_cctxs.num_cctx(), -1u);
#else
    std::vector<uint32_t> mappings(local_cctxs.num_cctx());
#endif

    std::queue<struct MergeElememt> to_merge;

    to_merge.push(MergeElememt{
        local_cctxs.get_tree().first,
        local_cctxs.get_tree().second,
        calling_context_tree_.first,
        calling_context_tree_.second,
        Process::invalid(),
    });

    while (!to_merge.empty())
    {
        auto cur_merge = to_merge.front();
        to_merge.pop();
        for (const auto& local_child : cur_merge.local_node.children)
        {
            // If there is already a node on the global tree matching the node on the local tree,
            // use it, Otherwise emplace a new global node
            auto global_child = cur_merge.global_node.children.find(local_child.first);
            if (global_child == cur_merge.global_node.children.end())
            {
                otf2::definition::calling_context const* new_cctx = nullptr;

                switch (local_child.first.type)
                {
                case lo2s::CallingContextType::SAMPLE_ADDR:
                    new_cctx = &cctx_for_address(local_child.first.to_addr(), r, cur_merge.p,
                                                 cur_merge.global_node);
                    break;
                case lo2s::CallingContextType::GPU_KERNEL:
                    new_cctx = &cctx_for_gpu_kernel(local_child.first.to_kernel_id(), r,
                                                    cur_merge.p, cur_merge.global_node);
                    break;
                case lo2s::CallingContextType::OPENMP:
                    new_cctx =
                        &cctx_for_openmp(local_child.first, r, cur_merge.p, cur_merge.global_node);
                    break;
                case lo2s::CallingContextType::THREAD:
                    new_cctx = &cctx_for_thread(local_child.first.to_thread(), cur_merge.p);
                    break;
                case lo2s::CallingContextType::PROCESS:
                    new_cctx = &cctx_for_process(local_child.first.to_process());
                    break;
                case lo2s::CallingContextType::SYSCALL:
                    new_cctx = &cctx_for_syscall(local_child.first.to_syscall_id());
                    break;
                case lo2s::CallingContextType::ROOT:
                    throw std::runtime_error(
                        "The cctx tree ROOT should not appear in merge_nodes!");
                }

                const auto r = cur_merge.global_node.children.emplace(
                    std::piecewise_construct, std::forward_as_tuple(local_child.first),
                    std::forward_as_tuple(new_cctx));
                global_child = r.first;
            }

            // Write a mapping local cctx reference number -> global reference number
            mappings.at(local_child.second.ref) = global_child->second.cctx->ref();

            if (!local_child.second.children.empty())
            {
                Process ctx_p = cur_merge.p;
                if (global_child->first.type == CallingContextType::PROCESS)
                {
                    ctx_p = global_child->first.to_process();
                }
                to_merge.push(MergeElememt{
                    local_child.first,
                    local_child.second,
                    global_child->first,
                    global_child->second,
                    ctx_p,
                });
            }
        }
    }

#ifndef NDEBUG
    for (auto id : mappings)
    {
        assert(id != -1u);
    }
#endif

    return { otf2::definition::mapping_table::mapping_type_type::calling_context, mappings };
}

otf2::definition::metric_class& Trace::perf_group_metric_class(std::variant<Cpu, Thread> scope)
{
    const perf::counter::CounterCollection& counter_collection =
        perf::EventComposer::instance().group_counters_for(scope);

    if (registry_.has<otf2::definition::metric_class>(ByCounterCollection(counter_collection)))
    {
        return registry_.get<otf2::definition::metric_class>(
            ByCounterCollection(counter_collection));
    }

    auto& metric_class = registry_.emplace<otf2::definition::metric_class>(
        ByCounterCollection(counter_collection), otf2::common::metric_occurence::async,
        otf2::common::recorder_kind::abstract);

    if (!counter_collection.leader)
    {
        throw std::runtime_error("No suitable grouped metrics leader event!");
    }
    metric_class.add_member(get_event_metric_member(counter_collection.leader.value()));

    for (const auto& counter : counter_collection.counters)
    {
        metric_class.add_member(get_event_metric_member(counter));
    }

    const auto& enabled_metric_member = registry_.emplace<otf2::definition::metric_member>(
        ByString("time_enabled"), to_otf2_string("time_enabled"),
        to_otf2_string("time event active"), otf2::common::metric_type::other,
        otf2::common::metric_mode::accumulated_start, otf2::common::type::uint64,
        otf2::common::base_type::decimal, 0, to_otf2_string("ns"));

    metric_class.add_member(enabled_metric_member);

    const auto& running_metric_member = registry_.emplace<otf2::definition::metric_member>(
        ByString("time_running"), to_otf2_string("time_running"),
        to_otf2_string("time event on CPU"), otf2::common::metric_type::other,
        otf2::common::metric_mode::accumulated_start, otf2::common::type::uint64,
        otf2::common::base_type::decimal, 0, to_otf2_string("ns"));

    metric_class.add_member(running_metric_member);
    return metric_class;
}

otf2::definition::metric_class& Trace::perf_userspace_metric_class(std::variant<Cpu, Thread> scope)
{
    const perf::counter::CounterCollection& counter_collection =
        perf::EventComposer::instance().userspace_counters_for(scope);

    if (registry_.has<otf2::definition::metric_class>(ByCounterCollection(counter_collection)))
    {
        return registry_.get<otf2::definition::metric_class>(
            ByCounterCollection(counter_collection));
    }

    auto& metric_class = registry_.emplace<otf2::definition::metric_class>(
        ByCounterCollection(counter_collection), otf2::common::metric_occurence::async,
        otf2::common::recorder_kind::abstract);

    for (const auto& counter : counter_collection.counters)
    {
        metric_class.add_member(get_event_metric_member(counter));
    }

    return metric_class;
}

otf2::definition::calling_context& Trace::cctx_for_syscall(int64_t syscall_id)
{
    const auto& syscall_name = to_otf2_string(syscall_name_for_nr(syscall_id));

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

void Trace::emplace_monitoring_thread(Thread thread, const std::string& name,
                                      const std::string& group)
{
    // We must guard this here because this is called by monitoring threads itself rather than
    // the usual call from the single monitoring process
    std::lock_guard<std::recursive_mutex> const guard(mutex_);

    Log::debug() << "Adding monitoring " << thread << " (" << name << "): group " << group;
    const auto& iname = to_otf2_string(fmt::format("lo2s::{}", name));

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

const otf2::definition::source_code_location& Trace::intern_scl(const LineInfo& info)
{
    return registry_.emplace<otf2::definition::source_code_location>(
        ByLineInfo(info), to_otf2_string(info.file), info.line);
}

const otf2::definition::region& Trace::intern_region(const LineInfo& info)
{
    const auto& name_str = to_otf2_string(info.function);
    const auto& region = registry_.emplace<otf2::definition::region>(
        ByLineInfo(info), name_str, name_str, name_str, otf2::common::role_type::function,
        otf2::common::paradigm_type::sampling, otf2::common::flags_type::none,
        to_otf2_string(info.file), info.line, 0);

    if (registry_.has<otf2::definition::regions_group>(ByString(info.dso)))
    {
        registry_.get<otf2::definition::regions_group>(ByString(info.dso)).add_member(region);
    }
    else
    {
        registry_.emplace<otf2::definition::regions_group>(
            ByString(info.dso), to_otf2_string(info.dso), otf2::common::paradigm_type::compiler,
            otf2::common::group_flag_type::none);
    }
    return region;
}

const otf2::definition::string& Trace::to_otf2_string(const std::string& name)
{
    std::lock_guard<std::recursive_mutex> const guard(mutex_);

    return registry_.emplace<otf2::definition::string>(ByString(name), name);
}

LocalCctxTree& Trace::create_local_cctx_tree(SampleKind kind, std::variant<Cpu, Thread> scope)
{
    std::lock_guard<std::mutex> const guard(local_cctx_trees_mutex_);

    assert(!local_cctx_trees_finalized_);

    return local_cctx_trees_.emplace_back(*this, kind, scope);
}

void Trace::finalize(Resolvers& resolvers)
{
    Log::trace() << "Registered resolvers: ";
    for (auto& resolver : resolvers.function_resolvers)
    {
        Log::trace() << resolver.first;
        resolver.second.print();
    }
    for (auto& local_cctx : local_cctx_trees_)
    {
        if (local_cctx.num_cctx() > 0)
        {
            const auto& mapping = merge_calling_contexts(local_cctx, resolvers);
            local_cctx.writer() << mapping;
        }
    }
    for (const auto& thread : ThreadNames::instance().names())
    {
        const auto& cctx =
            thread_handle(Process::no_parent(), thread.first, std::nullopt).calling_context();
        try
        {
            Process const process = ThreadGroup::instance().get_process(thread.first);
            if (resolvers.function_resolvers.count(process) == 1)
            {
                registry_.create<otf2::definition::calling_context_property>(
                    cctx, to_otf2_string("MAPS"),
                    otf2::attribute_value(
                        to_otf2_string(resolvers.function_resolvers.at(process).to_string())));
            }
        }
        catch (const std::out_of_range&)
        {
            Log::warn() << "Could not insert \'MAPS\' cctx property for" << thread.first;
        }
    }
    local_cctx_trees_.clear();
    const auto finalized_twice = local_cctx_trees_finalized_.exchange(true);
    if (finalized_twice)
    {
        Log::error() << "Trace::merge_calling_contexts was called twice."
                        "This is a bug, please report it to the developers.";
    }

    for (const auto& thread : ThreadNames::instance().names())
    {

        if (ThreadGroup::instance().is_process(thread.first))
        {
            process_handle(Process::no_parent(), thread.first.as_process(), thread.second);
        }
        auto thread_hd = thread_handle(Process::no_parent(), thread.first, thread.second);

        process_handle(Process::no_parent(), ThreadGroup::instance().get_process(thread.first),
                       std::nullopt)
            .regions_group()
            .add_member(thread_hd.region());
    }

    if (starting_time_ > stopping_time_)
    {
        stopping_time_ = starting_time_;
    }

    archive_ << otf2::definition::clock_properties(starting_time_, stopping_time_,
                                                   starting_system_time_);

    std::filesystem::path const symlink_path = nitro::env::get("LO2S_OUTPUT_LINK");

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

ProcessHandle Trace::process_handle(Process parent, Process process,
                                    std::optional<std::string> name)
{
    return { parent, process, std::move(name), *this };
}

ThreadHandle Trace::thread_handle(Process process, Thread thread, std::optional<std::string> name)
{
    return { process, thread, std::move(name), *this };
}

CpuHandle Trace::cpu_handle(Cpu cpu)
{
    return { cpu, *this };
}

CoreHandle Trace::core_handle(Core core)
{
    return { core, *this };
}

PackageHandle Trace::package_handle(Package pkg)
{
    return { pkg, *this };
}
} // namespace lo2s::trace
