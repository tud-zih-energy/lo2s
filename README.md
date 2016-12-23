# lo2s - Linux OTF2 sampling
 
lo2s creates a trace by running an uninstrumented executable. It uses the Linux perf
infrastructure.
 
The trace contains the following information:

 * Calling context samples based on instruction overflows
 * The calling context samples are annotated with the disassembled assembler instruction string
 * (optional) the framepointer-based call-path for each calling context sample
 * Several performance counter readings
 * The node-level system tree (cpus (HW-threads), cores, packages)

# Requirements for installation

 * Linux (>= 4.1)
 * libradare (somewhat recent)
 * OTF2 (>= 2.0)
 * libbfd
 * libiberty
 * boost (1.62, older versions might also work)

# Requirements for running

 * kernel.perf_event_paranoid should be less or equal than 1
   sudo sysctl kernel.perf_event_paranoid=1 if you have any trouble

# Installation

 * cmake /path/to/lo2s
 * make
 * make install
