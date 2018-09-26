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
#include <lo2s/line_info.hpp>
#include <lo2s/mmap.hpp>
#include <lo2s/process_info.hpp>

#include <otf2xx/otf2.hpp>

#include <boost/format.hpp>

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
    IpRefEntry(pid_t pid, otf2::definition::calling_context::reference_type r) : ref(r), pid(pid)
    {
    }

    otf2::definition::calling_context::reference_type ref;
    pid_t pid;
    IpMap<IpRefEntry> children;
};

struct IpCctxEntry
{
    IpCctxEntry(otf2::definition::calling_context c) : cctx(c)
    {
    }

    otf2::definition::calling_context cctx;
    IpMap<IpCctxEntry> children;
};

using IpRefMap = IpMap<IpRefEntry>;
using IpCctxMap = IpMap<IpCctxEntry>;

// TODO Group and sort the mess in this class definition and trace.cpp
class Trace
{
public:
    static constexpr pid_t METRIC_PID = 0;
    static constexpr pid_t NO_PARENT_PROCESS_PID =
        0; //<! sentinel value for an inserted process that has no known parent

    Trace();
    ~Trace();

    void begin_record();
    void end_record();

    otf2::chrono::time_point record_from() const;
    otf2::chrono::time_point record_to() const;

    otf2::writer::archive& archive()
    {
        return archive_;
    }

private:
    /** Add a thread with the required lock (#mutex_) held.
     *
     *  This is a helper needed to avoid constant re-locking when adding
     *  multiple threads via #add_threads.
     **/
    void add_thread_exclusive(pid_t tid, const std::string& name,
                              const std::lock_guard<std::mutex>&);

    void update_process_name(pid_t pid, const otf2::definition::string& name);
    void update_thread_name(pid_t tid, const otf2::definition::string& name);

public:
    void add_cpu(int cpuid);

    void add_process(pid_t pid, pid_t parent, const std::string& name = "");

    void add_thread(pid_t tid, const std::string& name);
    void add_threads(const std::unordered_map<pid_t, std::string>& tid_map);

    void add_monitoring_thread(pid_t tid, const std::string& name, const std::string& group);

    void update_process_name(pid_t pid, const std::string& name);
    void update_thread_name(pid_t tid, const std::string& name);

    otf2::writer::local& thread_sample_writer(pid_t pid, pid_t tid);
    otf2::writer::local& cpu_sample_writer(int cpuid);
    otf2::writer::local& thread_metric_writer(pid_t pid, pid_t tid);
    otf2::writer::local& named_metric_writer(const std::string& name);
    otf2::writer::local& cpu_metric_writer(int cpuid);
    otf2::writer::local& cpu_switch_writer(int cpuid);

    otf2::definition::metric_member
    metric_member(const std::string& name, const std::string& description,
                  otf2::common::metric_mode mode, otf2::common::type value_type,
                  const std::string& unit, std::int64_t exponent = 0,
                  otf2::common::base_type base = otf2::common::base_type::decimal);

    otf2::definition::metric_class metric_class();

    otf2::definition::metric_instance metric_instance(otf2::definition::metric_class metric_class,
                                                      otf2::definition::location recorder,
                                                      otf2::definition::location scope);

    otf2::definition::metric_instance metric_instance(otf2::definition::metric_class metric_class,
                                                      otf2::definition::location recorder,
                                                      otf2::definition::system_tree_node scope);

    otf2::definition::metric_class cpuid_metric_class();
    otf2::definition::metric_class perf_metric_class();

    otf2::definition::mapping_table merge_ips(IpRefMap& new_ips,
                                              std::map<pid_t, ProcessInfo>& infos);

    void merge_ips(IpRefMap& new_children, IpCctxMap& children,
                   std::vector<uint32_t>& mapping_table, otf2::definition::calling_context parent,
                   std::map<pid_t, ProcessInfo>& infos);

    otf2::definition::mapping_table merge_thread_calling_contexts(
        const std::unordered_map<pid_t, otf2::definition::calling_context::reference_type>&
            local_refs);

    const otf2::definition::interrupt_generator& interrupt_generator() const
    {
        return interrupt_generator_;
    }

    const otf2::definition::system_tree_node& system_tree_cpu_node(int cpuid) const
    {
        return system_tree_cpu_nodes_.at(cpuid);
    }

    const otf2::definition::system_tree_node& system_tree_core_node(int coreid, int packageid) const
    {
        return system_tree_core_nodes_.at(std::make_pair(coreid, packageid));
    }

    const otf2::definition::system_tree_node& system_tree_package_node(int packageid) const
    {
        return system_tree_package_nodes_.at(packageid);
    }

    const otf2::definition::system_tree_node& system_tree_root_node() const
    {
        return system_tree_root_node_;
    }

    otf2::definition::regions_group regions_group_executable(const std::string& name);
    otf2::definition::regions_group regions_group_monitoring(const std::string& name);
    otf2::definition::comm process_comm(pid_t pid);

private:
    std::map<std::string, otf2::definition::regions_group> regions_groups_sampling_dso();

    otf2::definition::source_code_location intern_scl(const LineInfo&);

    otf2::definition::region intern_region(const LineInfo&);

    otf2::definition::string intern(const std::string&);

    // This generates a contiguous set of IDs for all locations
    otf2::definition::location::reference_type location_ref() const
    {
        return thread_sample_locations_.size() + cpu_sample_locations_.size() +
               cpu_switch_locations_.size() + thread_metric_locations_.size() +
               named_metric_locations_.size() + cpu_metric_locations_.size();
    }

    otf2::definition::location_group::reference_type location_group_ref() const
    {
        return location_groups_process_.size() + location_groups_cpu_.size();
    }

    otf2::definition::system_tree_node::reference_type system_tree_ref() const
    {
        // + for system_tree_root_node_
        return 1 + system_tree_package_nodes_.size() + system_tree_core_nodes_.size() +
               system_tree_cpu_nodes_.size() + system_tree_process_nodes_.size();
    }

    otf2::definition::region::reference_type region_ref() const
    {
        return regions_line_info_.size() + regions_thread_.size();
    }

    otf2::definition::regions_group::reference_type group_ref() const
    {
        // + 1 for locations_group_
        // use static_cast because we are narrowing from std::size_t
        return static_cast<otf2::definition::regions_group::reference_type>(
            1 + regions_groups_executable_.size() + regions_groups_monitoring_.size() +
            process_comm_groups_.size());
    }

    otf2::definition::comm::reference_type comm_ref() const
    {
        return process_comms_.size();
    }

    otf2::definition::metric_member::reference_type metric_member_ref() const
    {
        return metric_members_.size();
    }

    // metric classes and metric instances share a reference space
    otf2::definition::metric_class::reference_type metric_class_ref() const
    {
        return static_cast<otf2::definition::metric_class::reference_type>(
            metric_instances_.size() + metric_classes_.size());
    }

    otf2::definition::metric_instance::reference_type metric_instance_ref() const
    {
        return static_cast<otf2::definition::metric_instance::reference_type>(
            metric_instances_.size() + metric_classes_.size());
    }

    otf2::definition::source_code_location::reference_type scl_ref() const
    {
        return source_code_locations_.size();
    }

    otf2::definition::string::reference_type string_ref() const
    {
        return strings_.size();
    }

    otf2::definition::calling_context::reference_type calling_context_ref() const
    {
        return calling_contexts_.size() + calling_contexts_thread_.size();
    }

    otf2::definition::system_tree_node& intern_process_node(pid_t pid);

    void attach_process_location_group(const otf2::definition::system_tree_node& parent, pid_t id,
                                       const otf2::definition::string& iname);

    void add_lo2s_property(const std::string& name, const std::string& value);

private:
    std::mutex mutex_;

    otf2::chrono::time_point starting_time_;
    otf2::chrono::time_point stopping_time_;

    std::string trace_name_;

    otf2::writer::archive archive_;

    std::map<std::string, otf2::definition::string> strings_;

    otf2::definition::system_tree_node system_tree_root_node_;

    std::map<int, otf2::definition::system_tree_node> system_tree_package_nodes_;
    std::map<std::pair<int, int>, otf2::definition::system_tree_node> system_tree_core_nodes_;
    std::map<int, otf2::definition::system_tree_node> system_tree_cpu_nodes_;
    std::map<pid_t, otf2::definition::system_tree_node> system_tree_process_nodes_;

    otf2::definition::interrupt_generator interrupt_generator_;

    std::map<pid_t, otf2::definition::location_group> location_groups_process_;
    std::map<int, otf2::definition::location_group> location_groups_cpu_;

    // TODO add location groups (processes), read path from /proc/self/exe symlink
    std::map<pid_t, otf2::definition::location> thread_sample_locations_;
    std::map<int, otf2::definition::location> cpu_sample_locations_;
    std::map<pid_t, otf2::definition::location> thread_metric_locations_;
    std::map<int, otf2::definition::location> cpu_metric_locations_;
    std::map<int, otf2::definition::location> cpu_switch_locations_;
    otf2::definition::container<otf2::definition::location> named_metric_locations_;

    std::map<LineInfo, otf2::definition::source_code_location> source_code_locations_;
    std::map<LineInfo, otf2::definition::region> regions_line_info_;
    std::map<pid_t, otf2::definition::region> regions_thread_;

    std::map<std::string, otf2::definition::regions_group> regions_groups_executable_;
    std::map<std::string, otf2::definition::regions_group> regions_groups_monitoring_;
    std::map<pid_t, otf2::definition::comm_group> process_comm_groups_;

    std::map<pid_t, otf2::definition::comm> process_comms_;

    IpCctxMap calling_context_tree_;
    otf2::definition::container<otf2::definition::calling_context> calling_contexts_;
    std::map<int, otf2::definition::calling_context> calling_contexts_thread_;
    otf2::definition::container<otf2::definition::calling_context_property>
        calling_context_properties_;
    otf2::definition::container<otf2::definition::metric_member> metric_members_;
    otf2::definition::container<otf2::definition::metric_class> metric_classes_;
    otf2::definition::detail::weak_ref<otf2::definition::metric_class> cpuid_metric_class_;
    otf2::definition::detail::weak_ref<otf2::definition::metric_class> perf_metric_class_;
    otf2::definition::container<otf2::definition::metric_instance> metric_instances_;
    otf2::definition::container<otf2::definition::system_tree_node_property>
        system_tree_node_properties_;
};
} // namespace trace
} // namespace lo2s
