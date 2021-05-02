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
#include <lo2s/bfd_resolve.hpp>
#include <lo2s/config.hpp>
#include <lo2s/execution_scope.hpp>
#include <lo2s/line_info.hpp>
#include <lo2s/mmap.hpp>
#include <lo2s/perf/counter/counter_collection.hpp>
#include <lo2s/process_info.hpp>
#include <lo2s/trace/reg_keys.hpp>

#include <otf2xx/otf2.hpp>

#include <map>
#include <mutex>
#include <unordered_map>

namespace lo2s
{
namespace trace
{

template <typename RefMap>
using IpMap = std::map<Address, RefMap>;

struct IpRefEntry
{
    IpRefEntry(otf2::definition::calling_context::reference_type r) : ref(r)
    {
    }

    otf2::definition::calling_context::reference_type ref;
    IpMap<IpRefEntry> children;
};

struct ThreadCctxRefs
{
    ThreadCctxRefs(Process p, otf2::definition::calling_context::reference_type r)
    : process(p), entry(r)
    {
    }
    Process process;
    IpRefEntry entry;
};

struct IpCctxEntry
{
    IpCctxEntry(otf2::definition::calling_context& c) : cctx(c)
    {
    }

    otf2::definition::calling_context& cctx;
    IpMap<IpCctxEntry> children;
};

using ThreadCctxRefMap = std::map<Thread, ThreadCctxRefs>;
using IpRefMap = IpMap<IpRefEntry>;
using IpCctxMap = IpMap<IpCctxEntry>;

class Trace
{
public:
    static Process NO_PARENT_PROCESS; //<! sentinel value for an inserted process that has no known
                                      // parent
    Trace();
    ~Trace();
    void begin_record();
    void end_record();

    otf2::chrono::time_point record_from() const;
    otf2::chrono::time_point record_to() const;

    void add_process(Process parent, Process process, const std::string& name = "");

    void add_thread(Thread t, const std::string& name);
    void add_threads(const std::unordered_map<Thread, std::string>& thread_map);

    void add_monitoring_thread(Thread t, const std::string& name, const std::string& group);

    void update_process_name(Process p, const std::string& name);
    void update_thread_name(Thread t, const std::string& name);

    otf2::definition::mapping_table merge_calling_contexts(ThreadCctxRefMap& new_ips,
                                                           size_t num_ip_refs,
                                                           std::map<Process, ProcessInfo>& infos);

    otf2::writer::local& sample_writer(const ExecutionScope& scope);
    otf2::writer::local& switch_writer(const ExecutionScope& scope);
    otf2::writer::local& metric_writer(const MeasurementScope& scope);
    otf2::writer::local& create_metric_writer(const std::string& name);

    otf2::definition::metric_member
    metric_member(const std::string& name, const std::string& description,
                  otf2::common::metric_mode mode, otf2::common::type value_type,
                  const std::string& unit, std::int64_t exponent = 0,
                  otf2::common::base_type base = otf2::common::base_type::decimal);

    otf2::definition::metric_class& metric_class();

    otf2::definition::metric_instance
    metric_instance(const otf2::definition::metric_class& metric_class,
                    const otf2::definition::location& recorder,
                    const otf2::definition::location& location);

    otf2::definition::metric_instance
    metric_instance(const otf2::definition::metric_class& metric_class,
                    const otf2::definition::location& recorder,
                    const otf2::definition::system_tree_node& location);

    otf2::definition::metric_class cpuid_metric_class()
    {
        if (!cpuid_metric_class_)
        {
            cpuid_metric_class_ = registry_.create<otf2::definition::metric_class>(
                otf2::common::metric_occurence::async, otf2::common::recorder_kind::abstract);
            cpuid_metric_class_->add_member(metric_member("CPU", "CPU executing the task",
                                                          otf2::common::metric_mode::absolute_point,
                                                          otf2::common::type::int64, "cpuid"));
        }
        return cpuid_metric_class_;
    }

    otf2::definition::metric_class perf_metric_class(MeasurementScope scope)
    {
        assert(scope.type == MeasuerementScopeType::GROUP_METRIC ||
               scope.type == MeasurementScopeType::USERSPACE_METRIC);

        if (scope.type == MeasurementScopeType::GROUP_METRIC)
        {
            if (!perf_group_metric_class_)
            {
                create_group_metric_class();
            }
            return perf_group_metric_class_;
        }

        if (!perf_userspace_metric_class_)
        {
            create_userspace_metric_class();
        }
        return perf_userspace_metric_class_;
    }

    otf2::definition::metric_class& tracepoint_metric_class(const std::string& event_name);

    const otf2::definition::interrupt_generator& interrupt_generator() const
    {
        return interrupt_generator_;
    }
    const otf2::definition::system_tree_node& system_tree_cpu_node(Cpu cpu) const
    {
        return registry_.get<otf2::definition::system_tree_node>(ByCpu(cpu));
    }

    const otf2::definition::system_tree_node& system_tree_core_node(int coreid, int packageid) const
    {
        return registry_.get<otf2::definition::system_tree_node>(ByCore(coreid, packageid));
    }

    const otf2::definition::system_tree_node& system_tree_package_node(int packageid) const
    {
        return registry_.get<otf2::definition::system_tree_node>(ByPackage(packageid));
    }

    const otf2::definition::system_tree_node& system_tree_root_node() const
    {
        return system_tree_root_node_;
    }

    otf2::definition::comm& process_comm(Thread thread)
    {
        std::lock_guard<std::recursive_mutex> guard(mutex_);
        return registry_.get<otf2::definition::comm>(ByProcess(groups_.get_process(thread)));
    }

    const otf2::definition::location& location(const ExecutionScope& scope)
    {
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
                .get<otf2::definition::comm_group>(
                    ByProcess(groups_.get_process(scope.as_thread())))
                .add_member(intern_location);
        }
        return intern_location;
    }

private:
    /** Add a thread with the required lock (#mutex_) held.
     *
     *  This is a helper needed to avoid constant re-locking when adding
     *  multiple threads via #add_threads.
     **/
    void add_thread_exclusive(Thread thread, const std::string& name,
                              const std::lock_guard<std::recursive_mutex>&);

    void merge_ips(IpRefMap& new_children, IpCctxMap& children,
                   std::vector<uint32_t>& mapping_table, otf2::definition::calling_context& parent,
                   std::map<Process, ProcessInfo>& infos, Process p);

    void create_group_metric_class()
    {
        auto counter_collection_ = perf::counter::requested_group_counters();

        perf_group_metric_class_ = registry_.create<otf2::definition::metric_class>(
            otf2::common::metric_occurence::async, otf2::common::recorder_kind::abstract);
        if (!counter_collection_.counters.empty())
        {
            perf_group_metric_class_->add_member(metric_member(
                counter_collection_.leader.name, counter_collection_.leader.name,
                otf2::common::metric_mode::accumulated_start, otf2::common::type::Double, "#"));

            for (const auto& counter : counter_collection_.counters)
            {
                perf_group_metric_class_->add_member(metric_member(
                    counter.name, counter.name, otf2::common::metric_mode::accumulated_start,
                    otf2::common::type::Double, "#"));
            }

            perf_group_metric_class_->add_member(metric_member(
                "time_enabled", "time event active", otf2::common::metric_mode::accumulated_start,
                otf2::common::type::uint64, "ns"));
            perf_group_metric_class_->add_member(metric_member(
                "time_running", "time event on CPU", otf2::common::metric_mode::accumulated_start,
                otf2::common::type::uint64, "ns"));
        }
    }

    void create_userspace_metric_class()
    {
        const auto& counter_collection_ = perf::counter::requested_userspace_counters();

        perf_userspace_metric_class_ = registry_.create<otf2::definition::metric_class>(
            otf2::common::metric_occurence::async, otf2::common::recorder_kind::abstract);
        if (!counter_collection_.counters.empty())
        {
            for (const auto& counter : counter_collection_.counters)
            {
                perf_userspace_metric_class_->add_member(metric_member(
                    counter.name, counter.name, otf2::common::metric_mode::accumulated_start,
                    otf2::common::type::Double, "#"));
            }
        }
    }

    const otf2::definition::source_code_location& intern_scl(const LineInfo&);

    const otf2::definition::region& intern_region(const LineInfo&);

    const otf2::definition::system_tree_node& intern_process_node(Process process);

    const otf2::definition::string& intern(const std::string&);

    void add_lo2s_property(const std::string& name, const std::string& value);

private:
    static constexpr pid_t METRIC_PID = 0;

    std::string trace_name_;
    otf2::writer::Archive<otf2::lookup_registry<Holder>> archive_;
    otf2::lookup_registry<Holder>& registry_;

    std::recursive_mutex mutex_;

    otf2::chrono::time_point starting_time_;
    otf2::chrono::time_point stopping_time_;

    otf2::definition::interrupt_generator& interrupt_generator_;

    // TODO add location groups (processes), read path from /proc/self/exe symlink

    std::map<Thread, std::string> thread_names_;
    std::map<Thread, IpCctxEntry> calling_context_tree_;

    otf2::definition::comm_locations_group& comm_locations_group_;
    otf2::definition::regions_group& lo2s_regions_group_;

    otf2::definition::detail::weak_ref<otf2::definition::metric_class> cpuid_metric_class_;
    otf2::definition::detail::weak_ref<otf2::definition::metric_class> perf_group_metric_class_;
    otf2::definition::detail::weak_ref<otf2::definition::metric_class> perf_userspace_metric_class_;

    const otf2::definition::system_tree_node& system_tree_root_node_;

    ExecutionScopeGroup& groups_;
};
} // namespace trace
} // namespace lo2s
