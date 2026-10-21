#include <lo2s/trace/handle.hpp>

#include <lo2s/topology.hpp>
#include <lo2s/trace/reg_keys.hpp>
#include <lo2s/trace/trace.hpp>

#include <otf2xx/definition/system_tree_node.hpp>

#include <optional>
#include <shared_mutex>

#include "lo2s/types/process.hpp"

namespace lo2s::trace
{
ProcessHandle::ProcessHandle(Process parent, Process process, std::optional<std::string> name,
                             trace::Trace& t)
: process_(process), parent_(parent), t_(t), m_(LockMap<Process>::instance().get(process))
{
    std::string proc_name;
    if (name.has_value())
    {
        ThreadNames::instance().set(process.as_thread(), name.value());
        proc_name = name.value();
    }
    else
    {
        proc_name = ThreadNames::instance().get(process.as_thread());
    }

    if (t_.registry().has<otf2::definition::system_tree_node>(ByProcess(process_)))
    {
        if (name.has_value())
        {
            std::unique_lock lock(m_);
            const auto& iname = t_.to_otf2_string(name.value());
            try
            {
                t_.registry()
                    .get<otf2::definition::system_tree_node>(ByProcess(process_))
                    .name(iname);
                t_.registry()
                    .get<otf2::definition::location_group>(ByProcess(process_))
                    .name(iname);
                t_.registry().get<otf2::definition::comm_group>(ByProcess(process_)).name(iname);
                t_.registry().get<otf2::definition::comm>(ByProcess(process_)).name(iname);

                t_.registry().get<otf2::definition::regions_group>(ByProcess(process_)).name(iname);
            }
            catch (const std::out_of_range&)
            {
                Log::warn() << "Attempting to update name of unknown " << process << " ("
                            << name.value() << ")";
            }
        }

        if (parent != Process::no_parent())
        {
            auto hd = t_.process_handle(Process::no_parent(), parent, std::nullopt);
            std::unique_lock lock(m_);
            t_.registry()
                .get<otf2::definition::system_tree_node>(ByProcess(process_))
                .parent(hd.system_tree_node());
        }
        return;
    }
    else
    {
        const auto& par_node =
            parent == Process::no_parent() ?
                t_.system_tree_root_node() :
                t_.process_handle(Process::no_parent(), parent, std::nullopt).system_tree_node();

        std::unique_lock lock(m_);
        const auto& iname = t_.to_otf2_string(proc_name);
        t_.registry().emplace<otf2::definition::regions_group>(ByProcess(process_), iname,
                                                               otf2::common::paradigm_type::user,
                                                               otf2::common::group_flag_type::none);

        const auto& ret = t_.registry().create<otf2::definition::system_tree_node>(
            ByProcess(process_), iname, t_.to_otf2_string("process"), par_node);

        t_.registry().emplace<otf2::definition::location_group>(
            ByProcess(process_), iname,
            otf2::definition::location_group::location_group_type::process, ret);

        const auto& comm_group = t_.registry().emplace<otf2::definition::comm_group>(
            ByProcess(process_), iname, otf2::common::paradigm_type::pthread,
            otf2::common::group_flag_type::none);

        t_.registry().emplace<otf2::definition::comm>(ByProcess(process_), iname, comm_group,
                                                      otf2::definition::comm::comm_flag_type::none);
    }
}

otf2::definition::system_tree_node& ProcessHandle::system_tree_node()
{
    std::shared_lock lock(m_);
    return t_.registry().get<otf2::definition::system_tree_node>(ByProcess(process_));
}

CpuHandle::CpuHandle(Cpu cpu, trace::Trace& t)
: cpu_(cpu), core_(Core::invalid()), package_(Package::invalid()), t_(t),
  m_(LockMap<Cpu>::instance().get(cpu))
{
    if (t_.registry().has<otf2::definition::system_tree_node>(ByCpu(cpu_)))
    {
        return;
    }

    auto& topo = Topology::instance();
    core_ = topo.core_of(cpu);
    package_ = topo.package_of(cpu);

    auto core_hd = t_.core_handle(core_);

    std::unique_lock lock(m_);
    Log::debug() << "Registering cpu " << cpu.as_int() << "@" << package_.as_int() << ":"
                 << core_.core_as_int();

    const auto& name = t_.to_otf2_string(fmt::format("{}", cpu.as_int()));
    const auto& cpu_node = t_.registry().create<otf2::definition::system_tree_node>(
        ByCpu(cpu), name, t_.to_otf2_string("cpu"), core_hd.system_tree_node());
    t_.registry().create<otf2::definition::system_tree_node_domain>(
        cpu_node, otf2::common::system_tree_node_domain::pu);

    // We need to a different name for the location_group than the system tree node,
    // because a system_tree_node gets attached its class_name in Vampir.
    const auto& lg_name = t.to_otf2_string(fmt::format("{}", cpu));
    t_.registry().create<otf2::definition::location_group>(
        ByCpu(cpu_), lg_name, otf2::definition::location_group::location_group_type::process,
        cpu_node);
}

ThreadHandle::ThreadHandle(Process process, Thread thread, std::optional<std::string> name,
                           trace::Trace& t)
: thread_(thread), parent_(process), t_(t), m_(LockMap<Thread>::instance().get(thread))
{
    if (t_.registry().has<otf2::definition::region>(ByThread(thread)) && !name.has_value())
    {
        return;
    }

    ThreadGroup::instance().add_thread(thread, process);
    t_.process_handle(Process::no_parent(), process, std::nullopt);

    std::string thread_name;

    if (thread == Thread(0))
    {
        thread_name = "<idle>";
    }
    else
    {
        if (!name.has_value())
        {
            thread_name = fmt::format("({})", thread.as_int());
        }
        else
        {
            thread_name = fmt::format("{} ({})", name.value(), thread.as_int());
            ThreadNames::instance().set(thread, name.value());
        }
    }

    if (t_.registry().has<otf2::definition::region>(ByThread(thread)))
    {
        std::unique_lock lock(m_);
        const auto& iname = t_.to_otf2_string(thread_name);
        auto& thread_region = t_.registry().get<otf2::definition::region>(ByThread(thread_));
        thread_region.name(iname);
        thread_region.canonical_name(iname);
        thread_region.source_file(iname);
        thread_region.description(iname);

        if (t_.registry().has<otf2::definition::location>(
                BySampleWriter(std::make_pair<trace::SampleKind, std::variant<Cpu, Thread>>(
                    SampleKind::SAMPLE, thread))))
        {
            t_.registry()
                .get<otf2::definition::location>(
                    BySampleWriter(std::make_pair<trace::SampleKind, std::variant<Cpu, Thread>>(
                        SampleKind::SAMPLE, thread)))
                .name(iname);
        }
        return;
    }
    else
    {
        std::unique_lock lock(m_);

        const auto& iname = t_.to_otf2_string(thread_name);

        auto& thread_region = t_.registry().emplace<otf2::definition::region>(
            ByThread(thread_), iname, iname, iname, otf2::common::role_type::function,
            otf2::common::paradigm_type::user, otf2::common::flags_type::none, iname, 0, 0);

        if (is_kernel_thread(thread))
        {
            t_.kernel_regions_group().add_member(thread_region);
        }

        // create calling context
        t_.registry().create<otf2::definition::calling_context>(
            ByThread(thread_), thread_region, otf2::definition::source_code_location());
    }
}

PackageHandle::PackageHandle(Package pkg, Trace& t)
: pkg_(pkg), t_(t), m_(LockMap<Package>::instance().get(pkg))
{
    if (t_.registry().has<otf2::definition::system_tree_node>(ByPackage(pkg_)))
    {
        return;
    }

    std::unique_lock lock(m_);

    otf2::definition::system_tree_node& package_node =
        t_.registry().emplace<otf2::definition::system_tree_node>(
            ByPackage(pkg_), t_.to_otf2_string(std::to_string(pkg_.as_int())),
            t_.to_otf2_string("package"), t_.system_tree_root_node());

    t_.registry().create<otf2::definition::system_tree_node_domain>(
        package_node, otf2::common::system_tree_node_domain::socket);
}

CoreHandle::CoreHandle(Core core, Trace& t)
: core_(core), package_(Package(core_.package_as_int())), t_(t),
  m_(LockMap<Core>::instance().get(core))
{
    if (t_.registry().has<otf2::definition::system_tree_node>(ByCore(core)))
    {
        return;
    }

    auto package = Package(core_.package_as_int());
    auto package_hd = t_.package_handle(package);

    std::unique_lock lock(LockMap<Core>::instance().get(core));

    otf2::definition::system_tree_node& core_node =
        t_.registry().emplace<otf2::definition::system_tree_node>(
            ByCore(core),
            t_.to_otf2_string(fmt::format("{}:{}", package.as_int(), core.core_as_int())),
            t_.to_otf2_string("core"), package_hd.system_tree_node());

    t_.registry().create<otf2::definition::system_tree_node_domain>(
        core_node, otf2::common::system_tree_node_domain::core);
}

otf2::definition::system_tree_node& CoreHandle::system_tree_node()
{
    std::shared_lock lock(m_);
    return t_.registry().get<otf2::definition::system_tree_node>(ByCore(core_));
}

otf2::definition::regions_group& ProcessHandle::regions_group()
{
    std::shared_lock lock(m_);
    return t_.registry().get<otf2::definition::regions_group>(ByProcess(process_));
}

otf2::definition::comm_group& ProcessHandle::comm_group()
{
    std::shared_lock lock(m_);
    return t_.registry().get<otf2::definition::comm_group>(ByProcess(process_));
}

otf2::definition::calling_context& ThreadHandle::calling_context()
{
    std::shared_lock lock(m_);
    return t_.registry().get<otf2::definition::calling_context>(ByThread(thread_));
}

otf2::definition::region& ThreadHandle::region()
{
    std::shared_lock lock(m_);
    return t_.registry().get<otf2::definition::region>(ByThread(thread_));
}

otf2::definition::comm& ProcessHandle::comm()
{
    std::shared_lock lock(m_);
    return t_.registry().get<otf2::definition::comm>(ByProcess(process_));
}

otf2::definition::location_group& ProcessHandle::location_group()
{
    std::shared_lock lock(m_);
    return t_.registry().get<otf2::definition::location_group>(ByProcess(process_));
}

otf2::definition::system_tree_node& CpuHandle::system_tree_node()
{
    std::shared_lock lock(m_);
    return t_.registry().get<otf2::definition::system_tree_node>(ByCpu(cpu_));
}

otf2::definition::location_group& CpuHandle::location_group()
{
    std::shared_lock lock(m_);
    return t_.registry().get<otf2::definition::location_group>(ByCpu(cpu_));
}

otf2::definition::system_tree_node& PackageHandle::system_tree_node()
{
    std::shared_lock lock(m_);
    return t_.registry().get<otf2::definition::system_tree_node>(ByPackage(pkg_));
}
} // namespace lo2s::trace
