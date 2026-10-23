// SPDX-FileCopyrightText: 2026 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <lo2s/trace/thread_names.hpp>
#include <lo2s/types/core.hpp>
#include <lo2s/types/cpu.hpp>
#include <lo2s/types/package.hpp>
#include <lo2s/types/process.hpp>
#include <lo2s/types/thread.hpp>

#include <map>
#include <optional>
#include <shared_mutex>

#include "otf2xx/definition/system_tree_node.hpp"

namespace lo2s::trace
{
class Trace;

template <typename Key>
class LockMap
{
public:
    static LockMap<Key>& instance()
    {
        static LockMap<Key> lm;
        return lm;
    }

    std::shared_mutex& get(Key key)
    {
        return mtxs_[key];
    }

private:
    std::map<Key, std::shared_mutex> mtxs_;
};

class ProcessHandle
{
public:
    ProcessHandle(Process parent, Process process, std::optional<std::string> name,
                  trace::Trace& t);

    otf2::definition::system_tree_node& system_tree_node();
    otf2::definition::regions_group& regions_group();
    otf2::definition::location_group& location_group();
    otf2::definition::comm_group& comm_group();
    otf2::definition::comm& comm();

private:
    Process process_, parent_;
    trace::Trace& t_;
    std::shared_mutex& m_;
};

class CpuHandle
{
public:
    CpuHandle(Cpu cpu, Trace& t);

    otf2::definition::location_group& location_group();
    otf2::definition::system_tree_node& system_tree_node();

private:
    Cpu cpu_;
    Core core_;
    Package package_;
    Trace& t_;
    std::shared_mutex& m_;
};

class ThreadHandle
{
public:
    ThreadHandle(Process process, Thread thread, std::optional<std::string> name, trace::Trace& t);

    otf2::definition::region& region();
    otf2::definition::calling_context& calling_context();

private:
    Thread thread_;
    Process parent_;
    Trace& t_;
    std::shared_mutex& m_;
};

class CoreHandle
{
public:
    CoreHandle(Core core, Trace& t);

    otf2::definition::system_tree_node& system_tree_node();

private:
    Core core_;
    Package package_;
    Trace& t_;
    std::shared_mutex& m_;
};

class PackageHandle
{
public:
    PackageHandle(Package pkg, Trace& t);

    otf2::definition::system_tree_node& system_tree_node();

private:
    Package pkg_;
    Trace& t_;
    std::shared_mutex& m_;
};

} // namespace lo2s::trace
