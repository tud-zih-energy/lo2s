// Original source:
// https://github.com/jvm-profiling-tools/perf-map-agent/blob/master/src/java/AttachOnce.java

/*
 *   libperfmap: a JVM agent to create perf-<pid>.map files for consumption
 *               with linux perf-tools
 *   Copyright (C) 2013-2015 Johannes Rudolph<johannes.rudolph@gmail.com>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License along
 *   with this program; if not, write to the Free Software Foundation, Inc.,
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

package de.tudresden.zih.lo2s;

import java.io.File;

import com.sun.tools.attach.VirtualMachine;
import java.lang.management.ManagementFactory;
import java.util.Locale;

public class AttachOnce {
    public static void main(String[] args) throws Exception {
        String pid = args[0];
        loadAgent(pid);
    }

    static void loadAgent(String pid) throws Exception {
        VirtualMachine vm = VirtualMachine.attach(pid);
        try {
            final File lib = new File("liblo2s-perfmap.so");
            String fullPath = lib.getAbsolutePath();
            if (!lib.exists()) {
                System.out.printf("Expected %s at '%s' but it didn't exist.\n", lib.getName(), fullPath);
                System.exit(1);
            }
            else vm.loadAgentPath(fullPath, "");
        } catch(com.sun.tools.attach.AgentInitializationException e) {
            // rethrow all but the expected exception
            if (!e.getMessage().equals("Agent_OnAttach failed")) throw e;
        } finally {
            vm.detach();
        }
    }
}
