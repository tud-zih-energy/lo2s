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
#include <lo2s/trace/reg_keys.hpp>

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

struct ThreadCctxRefs
{
    ThreadCctxRefs(pid_t p, otf2::definition::calling_context::reference_type r) : pid(p), entry(r)
    {
    }
    pid_t pid;
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

using ThreadCctxRefMap = std::map<pid_t, ThreadCctxRefs>;
using IpRefMap = IpMap<IpRefEntry>;
using IpCctxMap = IpMap<IpCctxEntry>;

class Trace
{
public:
    static constexpr pid_t NO_PARENT_PROCESS_PID =
        0; //<! sentinel value for an inserted process that has no known parent

    Trace();
    ~Trace();

    void begin_record();
    void end_record();

    otf2::chrono::time_point record_from() const;
    otf2::chrono::time_point record_to() const;

    void add_process(pid_t pid, pid_t parent, const std::string& name = "");

    void add_thread(pid_t tid, const std::string& name);
    void add_threads(const std::unordered_map<pid_t, std::string>& tid_map);

    void add_monitoring_thread(pid_t tid, const std::string& name, const std::string& group);

    void update_process_name(pid_t pid, const std::string& name);
    void update_thread_name(pid_t tid, const std::string& name);

    otf2::definition::mapping_table merge_calling_contexts(ThreadCctxRefMap& new_ips,
                                                           size_t num_ip_refs,
                                                           std::map<pid_t, ProcessInfo>& infos);

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

    otf2::definition::metric_class& metric_class();

    otf2::definition::metric_instance
    metric_instance(const otf2::definition::metric_class& metric_class,
                    const otf2::definition::location& recorder,
                    const otf2::definition::location& scope);

    otf2::definition::metric_instance
    metric_instance(const otf2::definition::metric_class& metric_class,
                    const otf2::definition::location& recorder,
                    const otf2::definition::system_tree_node& scope);

    otf2::definition::metric_class& cpuid_metric_class()
    {
        return cpuid_metric_class_;
    }
    otf2::definition::metric_class& perf_metric_class()
    {
        return perf_metric_class_;
    }

    otf2::definition::metric_class& tracepoint_metric_class(const std::string& event_name);

    const otf2::definition::interrupt_generator& interrupt_generator() const
    {
        return interrupt_generator_;
    }
    const otf2::definition::system_tree_node& system_tree_cpu_node(int cpuid) const
    {
        return registry_.get<otf2::definition::system_tree_node>(ByCpu(cpuid));
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
    otf2::definition::comm& process_comm(pid_t pid)
    {
        return registry_.get<otf2::definition::comm>(ByProcess(pid));
    }

private:
    /** Add a thread with the required lock (#mutex_) held.
     *
     *  This is a helper needed to avoid constant re-locking when adding
     *  multiple threads via #add_threads.
     **/
    void add_thread_exclusive(pid_t tid, const std::string& name,
                              const std::lock_guard<std::recursive_mutex>&);

    void merge_ips(IpRefMap& new_children, IpCctxMap& children,
                   std::vector<uint32_t>& mapping_table, otf2::definition::calling_context& parent,
                   std::map<pid_t, ProcessInfo>& infos, pid_t pid);

    const otf2::definition::source_code_location& intern_scl(const LineInfo&);

    const otf2::definition::region& intern_region(const LineInfo&);

    const otf2::definition::system_tree_node& intern_process_node(pid_t pid);

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

    std::map<pid_t, std::string> process_names_;
    std::map<pid_t, IpCctxEntry> calling_context_tree_;

    otf2::definition::comm_locations_group& comm_locations_group_;
    otf2::definition::regions_group& lo2s_regions_group_;

    otf2::definition::metric_class& perf_metric_class_;
    otf2::definition::metric_class& cpuid_metric_class_;

    const otf2::definition::system_tree_node& system_tree_root_node_;
};
} // namespace trace
} // namespace lo2s
