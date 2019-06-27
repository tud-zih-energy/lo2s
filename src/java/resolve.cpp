/*
 * This file is part of the lo2s software.
 * Linux OTF2 sampling
 *
 * Copyright (c) 2019,
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

#include <lo2s/java/resolve.hpp>

#include <lo2s/line_info.hpp>
#include <lo2s/log.hpp>

#include <boost/dll/runtime_symbol_info.hpp>

#include <iomanip>
#include <thread>

namespace lo2s
{
namespace java
{
JVMSymbols::JVMSymbols(pid_t jvm_pid) : pid_(jvm_pid)
{
    attach();
    fifo_ = std::make_unique<ipc::Fifo>(pid_, "jvmti");
}

std::unique_ptr<JVMSymbols> JVMSymbols::instance;

LineInfo JVMSymbols::lookup(Address addr) const
{
    // on purpose now fallback to an unknown line info
    return LineInfo::for_java_symbol(symbols_.at(addr));
}

void JVMSymbols::attach()
{
    Log::info() << "Attaching JVM agent to pid: " << pid_;

    ipc::Fifo::create(pid_, "jvmti");

    auto path = boost::dll::program_location().parent_path();
    auto location = path / "lo2s-agent.jar";

    Log::debug() << "lo2s JVM agent jar: " << location;

    std::string command("$JAVA_HOME/bin/java -cp " + location.string() +
                        ":$JAVA_HOME/lib/tools.jar de.tudresden.zih.lo2s.AttachOnce " +
                        std::to_string(pid_) + " " + path.string());
    std::thread attacher_([command]() { system(command.c_str()); });

    attacher_.detach();
}

void JVMSymbols::read_symbols()
{
    std::lock_guard<std::mutex> lock(mutex_);

    try
    {
        while (true)
        {
            if (!fifo_->has_data())
            {
                break;
            }

            std::uint64_t address;

            fifo_->read(address);

            int len;

            fifo_->read(len);

            std::string symbol;

            fifo_->read(symbol);

            Log::info() << "Read java symbol from fifo: 0x" << std::hex << address << " " << symbol;

            auto res =
                symbols_.emplace(std::piecewise_construct, std::make_tuple(address, address + len),
                                 std::make_tuple(symbol));

            if (!res.second)
            {
                Log::warn() << "Didn't inserted 0x" << std::hex << address << "-0x" << address + len
                            << ": " << symbol;
            }
        }
    }
    catch (std::exception& e)
    {
        // eof in the fifo will throw, so this is fine... sorta
        Log::error() << "Fifo read failed: " << e.what();
    }
}
} // namespace java
} // namespace lo2s
