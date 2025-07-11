[![Build](https://github.com/tud-zih-energy/lo2s/actions/workflows/cmake.yml/badge.svg)](https://github.com/tud-zih-energy/lo2s/actions/workflows/cmake.yml)
[![Current Release](https://img.shields.io/github/v/release/tud-zih-energy/lo2s?color=violet&label=Current%20release&sort=semver)](https://github.com/tud-zih-energy/lo2s/releases/latest)
[![manpage](https://img.shields.io/badge/manpage-master-blue)](https://github.com/tud-zih-energy/lo2s/blob/master/man/lo2s.1.pod)

<p align="center">
    <img src="share/logo/rendered/full.svg" alt="lo2s logo" width="300"/>
</p>

<p align="center">
    lo2s is a lightweight node-level performance monitoring tool used to analyze applications, the operating system and hardware.
</p>

# Lightweight Node-Level Performance Monitoring

lo2s creates parallel OTF2 traces with a focus on both application and system view.
The traces can contain any of the following information:

 * From running threads
   * Calling context samples based on instruction overflows
   * The calling context samples are annotated with the disassembled assembler instruction string
   * The framepointer-based call-path for each calling context sample
   * Per-thread performance counter readings
   * Which thread was scheduled on which CPU at what time
   * Information about executed OpenMP constructs
   * Accelerator activity events from NVidia and AMD GPUs as well as NEC SX-Aurora Vector Engines
   * Application level I/O activity
 * From the system
   * Metrics from tracepoints (e.g. the selected C-state or P-state)
   * The node-level system tree (cpus (HW-threads), cores, packages)
   * CPU power measurements (x86_energy)
   * Microarchitecture specific metrics (x86_adapt, per package or per core)
   * Hardware sensors using lm_sensors
   * Arbitrary metrics through plugins (Score-P compatible)
   * Syscall activity
   * Block layer I/O activity

In general `lo2s` operates either in **process monitoring** or **system monitoring** mode.

With **process monitoring**, all information is grouped by each thread of a monitored process group - it shows you *on which CPU is each monitored thread running*.
`lo2s` either acts as a prefix command to run the process (and also tracks its children), or `lo2s` attaches to a running process.

In the **system monitoring** mode, information is grouped by logical CPU - it shows you *which thread was running on a given CPU*.
Metrics are also shown per CPU.

In both modes, system-level metrics (e.g. tracepoints), are always grouped by their respective system hardware component.

# Build Requirements

 * Linux (>= 4.3)<sup>1</sup>
 * [OTF2](http://www.vi-hps.org/projects/score-p/index.html) (>= 3.1)
 * elfutils (specifically libelf and libdw)
 * CMake (>= 3.11)
 * A C++ Compiler with C++17 support and the std::filesystem library (GCC > 7, Clang > 5)

<sup>1</sup>: Older kernels can work as the required features are oftentimes backported. Otherwise [lo2s 1.7.0](https://github.com/tud-zih-energy/lo2s/releases/tag/v1.7.0) can be used, which is the newest lo2s version with support for kernels as old as even 2.6.32.

# Optional Build Dependencies

 * [x86_adapt](https://github.com/tud-zih-energy/x86_adapt) for mircorarchitecture specific metrics
 * [x86_energy](https://github.com/tud-zih-energy/x86_energy) for CPU power metrics
 * [radare2](https://rada.re/n/radare2.html) (>= 5.8.0) for disassembled instruction strings
 * [lm-sensors](https://github.com/lm-sensors/lm-sensors) for sensor readings
 * [libaudit](https://github.com/linux-audit/audit-userspace/) to resolve syscall names, otherwise only syscall nrs can be used in syscall tracing
 * [pod2man](https://www.eyrie.org/~eagle/software/podlators/) to generate the man pages (typically distributed as part of `perl`)
 * `gzip` to compress the man pages
 * [libbpf](https://github.com/libbpf/libbpf) and bpftool to enable POSIX I/O recording
 * [libpfm](https://perfmon2.sourceforge.net/manv3/libpfm.html) to support the event name resolution through it
 * CUDA to record NVidia GPU Activity
 * [rocprofiler-sdk](https://github.com/ROCm/rocprofiler-sdk) to record AMD GPU activity
 * libveosinfo to record NEC SX-Aurora activity
 * libdebuginfod to download DWARF debug information for recorded applications on-the-fly
 * OpenMP to record OpenMP activity

# Runtime Requirements

 * `kernel.perf_event_paranoid` should be less than or equal to `1` for process monitoring mode and less than or equal to `0` in system monitoring mode. A value of `-1` will give the most features for non-root performance recording, such as tracepoints and block I/O, at the cost of some security. Modify as follows:

   `sudo sysctl kernel.perf_event_paranoid=1`

 * Tracepoints, block I/O and syscalls require access to debugfs.
 Grant permissions at your own discretion.

   `sudo mount -t debugfs none /sys/kernel/debug`


# Installation

 * It is recommended to create an empty build directory anywhere.
 * `cmake /path/to/lo2s`
 * Configure cmake as usual, e.g. with `ccmake .`
 * `make`
 * `make install`

# Usage

To monitor a given application in process monitoring execute

 * `lo2s -- ./a.out --app-args`

To monitor all activity on a system run

 * `lo2s -a` (stop the recording with ctrl+c)

For a full documentation of options see [the manpage](https://github.com/tud-zih-energy/lo2s/blob/master/man/lo2s.1.pod).

## Usage with MPI

You can record simple traces from MPI programs, but `lo2s` does not record MPI communication.
To create fully-featured MPI-aware traces, use [Score-P](https://score-p.org/).

 * `lo2s mpirun ./a.out` Create one trace of mpirun, useful if mpirun is used locally on one node.
 * `mpirun lo2s ./a.out` Creates a separate trace for each process.

See `man lo2s` or `lo2s --help` for a full listing of options and usage.

# Quirks

The `perf_event_open` kernel infrastructure changed significantly over time.
Therefore, it is already hard to just keep track which kernel version introduced which new feature.
Combine that with the abundance of backports of particular features by different distributors, and you end with a mess of options.

In the effort to keep compatible with older kernels and some architectures that lack hardware breakpoint support, several quirks have been added to `lo2s`:

1. The initial time synchronization between lo2s and the kernel-space perf is done with a hardware breakpoint. If your kernel or processor architecture doesn't support that, you can use a fallback using the CMake option `USE_HW_BREAKPOINT_COMPAT`.
2. If you get the following error message: `event 'ref-cycles' is not available as a metric leader!`, you can fallback to the bus-cycles metric as leader using the lo2s command-lind argument `--metric-leader bus-cycles`.

# Working with traces

Traces can be visualized with [Vampir](https://vampir.eu).
You can use [OTF2](https://score-p.org/) or any of its tools.
Native interfaces are available for C and Python

# Acknowledgements

This work is supported in part by the German Research Foundation (DFG) within the CRC 912 - HAEC and the german National High Performance Computing (NHR@TUD).

# Primary Reference

A description and use cases can be found in the following paper. Please cite this if you use `lo2s` for scientific work.

Thomas Ilsche, Robert Schöne, Mario Bielert, Andreas Gocht and Daniel Hackenberg. [lo2s – Multi-Core System and Application Performance Analysis for Linux 📕](https://tu-dresden.de/zih/forschung/ressourcen/dateien/projekte/haec/lo2s.pdf) In: Workshop on Monitoring and Analysis for High Performance Computing Systems Plus Applications (HPCMASPA). 2017. DOI: [10.1109/CLUSTER.2017.116](https://doi.org/10.1109/CLUSTER.2017.116)

# Additional References

Thomas Ilsche, Marcus Hähnel, Robert Schöne, Mario Bielert and Daniel Hackenberg: [Powernightmares: The Challenge of Efficiently Using Sleep States on Multi-Core Systems 📕](https://tu-dresden.de/zih/forschung/ressourcen/dateien/projekte/haec/powernightmares.pdf) In: 5th Workshop on Runtime and Operating Systems for the Many-core Era (ROME). 2017, DOI: [10.1007/978-3-319-75178-8_50](https://doi.org/10.1007/978-3-319-75178-8_50)

Thomas Ilsche, Robert Schöne, Philipp Joram, Mario Bielert and Andreas Gocht: [System Monitoring with lo2s: Power and Runtime Impact of C-State Transitions 📕](https://tu-dresden.de/zih/forschung/ressourcen/dateien/projekte/haec/lo2s-cstate_authorversion.pdf) In: 2018 IEEE International Parallel and Distributed Processing Symposium Workshops (IPDPSW), DOI: [10.1109/IPDPSW.2018.00114](https://doi.org/10.1109/IPDPSW.2018.00114)

Thomas Ilsche, Mario Bielert, Christian von Elm: [Bridging the Gap between Application Performance Analysis and System Monitoring 📕](https://tu-dresden.de/zih/forschung/ressourcen/dateien/projekte/energyefficiency/2022-lo2s-system-monitoring.pdf) In: 2022 IEEE International Conference on Cluster Computing (CLUSTER), DOI: [10.1109/CLUSTER51413.2022.00080](https://doi.org/10.1109/CLUSTER51413.2022.00080)

# Name

The name `lo2s` is an acronym for **L**inux **O**TF**2** **S**ampling
