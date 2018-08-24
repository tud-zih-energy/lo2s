[![Build Status](https://travis-ci.org/tud-zih-energy/lo2s.svg?branch=master)](https://travis-ci.org/tud-zih-energy/lo2s)

# `lo2s` - Linux OTF2 sampling

`lo2s` creates a trace by running an uninstrumented executable. It uses the Linux perf
infrastructure.

The trace contains the following information:

 * Calling context samples based on instruction overflows
 * The calling context samples are annotated with the disassembled assembler instruction string
 * (optional) the framepointer-based call-path for each calling context sample
 * Several performance counter readings
 * The node-level system tree (cpus (HW-threads), cores, packages)

# Build Requirements

 * Linux<sup>1</sup>
 * [OTF2](http://www.vi-hps.org/projects/score-p/index.html) (>= 2.1)
 * libbfd
 * libiberty
 * boost (>= 1.62)
 * CMake (>= 3.5)
 
<sup>1</sup>: Use Linux >= 4.1 for best results. Older versions, even the ancient 2.6.32, will work, but with degraded time synchronization.
 
# Optional Build Dependencies

 * libradare (somewhat recent)
 * [x86_adapt](https://github.com/tud-zih-energy/x86_adapt)
 * [x86_energy](https://github.com/tud-zih-energy/x86_energy)


# Runtime Requirements

 * `kernel.perf_event_paranoid` should be less or equal than `1`. A value of `-1` will give the most features for non-root performance recording, at the cost of some security. Modify as follows:

   `sudo sysctl kernel.perf_event_paranoid=1`
   
 * Tracepoints and global monitoring requires access to debugfs. Grant permissions at your own discretion.
 
   `sudo mount -t debugfs non /sys/kernel/debug`
   

# Installation

 * `cmake /path/to/lo2s`
 * `make`
 * `make install`

# Usage

 * `lo2s ./a.out`
 * `lo2s -- ./a.out --params`
 * `lo2s mpirun ./a.out` Create one trace of mpirun, useful if mpirun is used locally on one node.
 * `mpirun lo2s ./a.out` Creates a separate trace for each process.
 
See `lo2s --help` for a current listing of options.

# Quirks

The `perf_event_open` kernel infrastructure changed significantly over time.
Therefore, it is already hard to just keep track which kernel version introduced which new feature. 
Combine that with the abundance of backports of particular features by different distributors, and you end with a mess of options.

In the effort to keep compatible with older kernels, several quirks have been added to `lo2s`:

1. The initial time synchronization between lo2s and the kernel-space perf is done with a hardware breakpoint. If your kernel doesn't support that, you can disable it using the CMake option `HW_BREAKPOINT_COMPAT`.
2. The used clock source for the kernel-space time measurments can be changed, however if you kernel doesn't support that, you can disable it with the CMake option `USE_PERF_CLOCKID`.
3. If you get the following error message: `event 'ref-cycles' is not available as a metric leader!`, you can fallback to the bus-cycles metric as leader using `--metric-leader bus-cycles`.

# Reference

A description and use cases can be found in the following paper. Please cite this if you use `lo2s` for scientific work.

Thomas Ilsche, Robert Schöne, Mario Bielert, Andreas Gocht and Daniel Hackenberg. ["lo2s – Multi-Core System and Application Performance Analysis for Linux" 📕](https://tu-dresden.de/zih/forschung/ressourcen/dateien/projekte/haec/lo2s.pdf) In: Workshop on Monitoring and Analysis for High Performance Computing Systems Plus Applications (HPCMASPA). 2017. DOI: [10.1109/CLUSTER.2017.116](https://doi.org/10.1109/CLUSTER.2017.116)
