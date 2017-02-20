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
#include <lo2s/line_info.hpp>
#include <lo2s/mmap.hpp>

#include <otf2xx/otf2.hpp>

#include <boost/format.hpp>

#include <map>
#include <mutex>

namespace lo2s
{
namespace trace
{

template <typename RefMap>
using ip_map = std::map<address, RefMap>;

struct ip_ref_entry
{
    ip_ref_entry(otf2::definition::calling_context::reference_type r) : ref(r)
    {
    }

    otf2::definition::calling_context::reference_type ref;
    ip_map<ip_ref_entry> children;
};

struct ip_cctx_entry
{
    ip_cctx_entry(otf2::definition::calling_context c) : cctx(c)
    {
    }

    otf2::definition::calling_context cctx;
    ip_map<ip_cctx_entry> children;
};

using ip_ref_map = ip_map<ip_ref_entry>;
using ip_cctx_map = ip_map<ip_cctx_entry>;

class trace
{
public:
    static constexpr pid_t METRIC_PID = 0;

    trace(uint64_t sample_period, const std::string& trace_path);

    ~trace();

    void begin_record();

    void end_record();

    otf2::chrono::time_point record_from() const;

    otf2::chrono::time_point record_to() const;

    otf2::writer::archive& archive()
    {
        return archive_;
    }

    void process(pid_t pid, const std::string& name);

    otf2::writer::local& sample_writer(pid_t pid, pid_t tid);

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

    otf2::definition::mapping_table merge_ips(ip_ref_map& new_ips, uint64_t ip_count,
                                              const memory_map& maps);

    void merge_ips(ip_ref_map& new_children, ip_cctx_map& children,
                   std::vector<uint32_t>& mapping_table, otf2::definition::calling_context parent,
                   const memory_map& maps);

    const otf2::definition::interrupt_generator& interrupt_generator() const
    {
        return interrupt_generator_;
    }

    const otf2::definition::comm self_comm() const
    {
        return self_comm_;
    }

    const otf2::definition::system_tree_node& system_tree_cpu_node(int cpuid) const
    {
        return system_tree_cpu_nodes_.at(cpuid);
    }

private:
    std::map<std::string, otf2::definition::regions_group> function_groups();

    otf2::definition::source_code_location intern_scl(const line_info&);

    otf2::definition::region intern_region(const line_info&);

    otf2::definition::string intern(const std::string&);

private:
    std::mutex mutex_;

    otf2::chrono::time_point starting_time_;
    otf2::chrono::time_point stopping_time_;

    otf2::writer::archive archive_;

    std::map<std::string, otf2::definition::string> strings_;

    otf2::definition::system_tree_node system_tree_root_node_;
    std::map<int, otf2::definition::system_tree_node> system_tree_package_nodes_;
    std::map<std::pair<int, int>, otf2::definition::system_tree_node> system_tree_core_nodes_;
    std::map<int, otf2::definition::system_tree_node> system_tree_cpu_nodes_;

    otf2::definition::interrupt_generator interrupt_generator_;

    std::map<pid_t, otf2::definition::location_group> location_groups_;
    // TODO add location groups (processes), read path from /proc/self/exe symlink
    otf2::definition::container<otf2::definition::location> locations_;

    std::map<line_info, otf2::definition::source_code_location> source_code_locations_;
    std::map<line_info, otf2::definition::region> regions_;
    ip_cctx_map calling_context_tree_;
    otf2::definition::container<otf2::definition::calling_context> calling_contexts_;
    otf2::definition::container<otf2::definition::calling_context_property>
        calling_context_properties_;
    otf2::definition::container<otf2::definition::metric_member> metric_members_;
    otf2::definition::container<otf2::definition::metric_class> metric_classes_;
    otf2::definition::container<otf2::definition::metric_instance> metric_instances_;

    otf2::definition::comm_self_group self_group_;
    otf2::definition::comm self_comm_;
};
}
}
