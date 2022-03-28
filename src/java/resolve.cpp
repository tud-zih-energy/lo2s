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

#include <iomanip>
#include <thread>

namespace lo2s
{
namespace java
{
JVMSymbols::JVMSymbols(Process jvm_pid) : pid_(jvm_pid)
{
    attach();
    fifo_ = std::make_unique<ipc::Fifo>(pid_, "jvmti");

    std::thread reader{ [this]() { this->read_symbols(); } };
    reader.detach();
}

std::unique_ptr<JVMSymbols> JVMSymbols::instance;

LineInfo JVMSymbols::lookup(Address addr) const
{
    std::lock_guard<std::mutex> lock(mutex_);

    // on purpose now fallback to an unknown line info
    return LineInfo::for_java_symbol(symbols_.at(addr));
}

void JVMSymbols::attach()
{
    Log::info() << "Attaching JVM agent to pid: " << pid_;

    ipc::Fifo::create(pid_, "jvmti");

    auto path = std::filesystem::canonical("/proc/self/exe").parent_path();
    auto location = path / "lo2s-agent.jar";

    Log::debug() << "lo2s JVM agent jar: " << location;

    std::string command("$JAVA_HOME/bin/java -cp " + location.string() +
                        ":$JAVA_HOME/lib/tools.jar de.tudresden.zih.lo2s.AttachOnce " +
                        std::to_string(pid_.as_pid_t()) + " " + path.string());
    std::thread attacher_([command]() { system(command.c_str()); });

    attacher_.detach();
}

void JVMSymbols::read_symbols()
{
    Log::warn() << "read symbols called";

    try
    {
        while (running_)
        {
            fifo_->await_data();

            std::uint64_t address;

            fifo_->read(address);

            int len;

            fifo_->await_data();
            fifo_->read(len);

            std::string symbol;

            fifo_->await_data();
            fifo_->read(symbol);

            Log::debug() << "Read java symbol from fifo: 0x" << std::hex << address << " "
                         << symbol;
            try
            {
                std::lock_guard<std::mutex> lock(mutex_);

                auto res = symbols_.emplace(std::piecewise_construct,
                                            std::make_tuple(address, address + len),
                                            std::make_tuple(symbol));

                if (!res.second)
                {
                    Log::warn() << "Didn't inserted 0x" << std::hex << address << "-0x"
                                << address + len << ": " << symbol;
                }
            }
            catch (std::exception& e)
            {
                // eof in the fifo will throw, so this is fine... sorta
                Log::error() << "Inserting symbol failed: " << e.what();
            }
        }
    }
    catch (std::exception& e)
    {
        // eof in the fifo will throw, so this is fine... sorta
        Log::error() << "Fifo read failed: " << e.what();
    }

    Log::warn() << "read symbols ended";
}
} // namespace java
} // namespace lo2s
