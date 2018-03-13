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
    IpRefEntry(otf2::definition::calling_context::reference_type r) : ref(r)
    {
    }

    otf2::definition::calling_context::reference_type ref;
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

    void process(pid_t pid, const std::string& name = "");

    otf2::writer::local& sample_writer(pid_t pid, pid_t tid);
    otf2::writer::local& cpu_writer(int cpuid);
    otf2::writer::local& metric_writer(pid_t pid, pid_t tid);
    otf2::writer::local& metric_writer(const std::string& name);

    otf2::definition::metric_member metric_member(const std::string& name,
                                                  const std::string& description,
                                                  otf2::common::metric_mode mode,
                                                  otf2::common::type value_type,
                                                  const std::string& unit);

    otf2::definition::metric_class metric_class();

    otf2::definition::metric_instance metric_instance(otf2::definition::metric_class metric_class,
                                                      otf2::definition::location recorder,
                                                      otf2::definition::location scope);

    otf2::definition::metric_instance metric_instance(otf2::definition::metric_class metric_class,
                                                      otf2::definition::location recorder,
                                                      otf2::definition::system_tree_node scope);

    otf2::definition::mapping_table merge_ips(IpRefMap& new_ips, uint64_t ip_count,
                                              const MemoryMap& maps);

    void merge_ips(IpRefMap& new_children, IpCctxMap& children,
                   std::vector<uint32_t>& mapping_table, otf2::definition::calling_context parent,
                   const MemoryMap& maps);

    void register_tid(pid_t tid, const std::string& exe);
    void register_tids(const std::unordered_map<pid_t, std::string>& tid_map);

    otf2::definition::mapping_table merge_tids(
        const std::unordered_map<pid_t, otf2::definition::region::reference_type>& local_refs);

    void register_monitoring_tid(pid_t tid, const std::string& name, const std::string& grou);

    const otf2::definition::interrupt_generator& interrupt_generator() const
    {
        return interrupt_generator_;
    }

    const otf2::definition::system_tree_node& system_tree_cpu_node(int cpuid) const
    {
        return system_tree_cpu_nodes_.at(cpuid);
    }

    otf2::definition::regions_group regions_group_executable(const std::string& name);
    otf2::definition::regions_group regions_group_monitoring(const std::string& name);
    otf2::definition::comm process_comm(pid_t pid);

private:
    std::map<std::string, otf2::definition::regions_group> regions_groups_sampling_dso();

    otf2::definition::source_code_location intern_scl(const LineInfo&);

    otf2::definition::region intern_region(const LineInfo&);

    otf2::definition::string intern(const std::string&);

    otf2::definition::location_group::reference_type location_group_ref() const
    {
        return location_groups_process_.size() + location_groups_cpu_.size();
    }

    otf2::definition::region::reference_type region_ref() const
    {
        return regions_line_info_.size() + regions_thread_.size();
    }

    otf2::definition::regions_group::reference_type group_ref() const
    {
        // + 1 for locations_group_
        return 1 + regions_groups_executable_.size() + regions_groups_monitoring_.size() +
               process_comm_groups_.size();
    }

    otf2::definition::comm::reference_type comm_ref() const
    {
        return process_comms_.size();
    }

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

    otf2::definition::interrupt_generator interrupt_generator_;

    std::map<pid_t, otf2::definition::location_group> location_groups_process_;
    std::map<int, otf2::definition::location_group> location_groups_cpu_;

    // TODO add location groups (processes), read path from /proc/self/exe symlink
    otf2::definition::container<otf2::definition::location> locations_;

    std::map<LineInfo, otf2::definition::source_code_location> source_code_locations_;
    std::map<LineInfo, otf2::definition::region> regions_line_info_;
    std::map<pid_t, otf2::definition::region> regions_thread_;

    std::map<std::string, otf2::definition::regions_group> regions_groups_executable_;
    std::map<std::string, otf2::definition::regions_group> regions_groups_monitoring_;
    std::map<pid_t, otf2::definition::comm_group> process_comm_groups_;

    std::map<pid_t, otf2::definition::comm> process_comms_;

    IpCctxMap calling_context_tree_;
    otf2::definition::container<otf2::definition::calling_context> calling_contexts_;
    otf2::definition::container<otf2::definition::calling_context_property>
        calling_context_properties_;
    otf2::definition::container<otf2::definition::metric_member> metric_members_;
    otf2::definition::container<otf2::definition::metric_class> metric_classes_;
    otf2::definition::container<otf2::definition::metric_instance> metric_instances_;
};
} // namespace trace
} // namespace lo2s
