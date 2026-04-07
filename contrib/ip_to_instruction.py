#!/usr/bin/env python3

# SPDX-FileCopyrightText: 2026 (c) Technische Universität Dresden
# SPDX-License-Identifier: GPL-3.0-or-later

# Example for parsing the instruction pointers in lo2s traces to instructions
import sys
import otf2
import re
from capstone import *


# Get the (grand-)parent calling context that has a MAPS cctx property
def get_maps_for(cctx, trace):
    for prop in trace.definitions.calling_context_properties:
        if prop.calling_context == cctx:
            if prop.name == "MAPS":
                return prop.value
    if cctx.parent != None:
        return get_maps_for(cctx.parent, trace)
    return ""


md = Cs(CS_ARCH_X86, CS_MODE_64)

with otf2.reader.open(sys.argv[1]) as trace:
    for loc, ev in trace.events:
        if isinstance(ev, otf2.events.CallingContextSample):
            ip = 0
            for prop in trace.definitions.calling_context_properties:
                if prop.calling_context == ev.calling_context:
                    if prop.name == "ip":
                        ip = int(prop.value)
            for line in get_maps_for(ev.calling_context, trace).splitlines():
                # Long and ugly regex to parse entries from /proc/{pid}/maps,
                # which we save into the "MAPS" cctx_property
                # For more info on the format, see man 5 proc_pid_maps
                x = re.match(
                    "([0-9a-f]+)\\-([0-9a-f]+)\\s+[r-][w-][x-].?\\s+([0-9a-z]+)\\s+\\S+\\s+\\d+\\s+(.*)",
                    line,
                )
                if x:
                    start = int(x.group(1), 16)
                    end = int(x.group(2), 16)
                    pgoff = int(x.group(3), 16)
                    filename = x.group(4)

                    # This python code does not have the beans to decode into the kernel currently, so ignore it.
                    if filename == "[kernel]":
                        continue
                    if ip >= start and ip < end:
                        f = open(filename, "rb")
                        f.seek(ip - start + pgoff)
                        for address, size, mnemonic, op_str in md.disasm_lite(
                            f.read(1024), 0x0
                        ):
                            print(hex(ip), "is", mnemonic, op_str)
                            break
