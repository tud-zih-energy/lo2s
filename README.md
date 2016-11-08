# lo2s - Linux OTF2 sampling
 
lo2s creates a trace by running an uninstrumented executable. It uses the Linux perf
infrastructure.
 
The trace contains the following information:

 * Calling context samples based on instruction overflows
 * The calling context samples are annotated with the disassembled assembler instruction string
 * (optional) the framepointer-based call-path for each calling context sample
 * Several performance counter readings
 * The node-level system tree (cpus (HW-threads), cores, packages)

# Requirements

 * Linux (>= 4.1)
 * libradare (somewhat recent)
 * OTF2 (>= 2.0)
 * libbfd

# Installation

 * cmake /path/to/lo2s
 * make
 * make install
