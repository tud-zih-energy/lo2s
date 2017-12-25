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

# Requirements for installation

 * Linux<sup>1</sup>
 * libradare (somewhat recent)
 * OTF2 (>= 2.0)
 * libbfd
 * libiberty
 * boost (1.62, older versions might also work)
 * CMake (>= 3.5)

<sup>1</sup>: Use Linux >= 4.1 for best results. Older versions, even the ancient 2.6.32, will work, but with degraded time synchronization.

# Requirements for running

 * kernel.perf_event_paranoid should be less or equal than 1. If you have any trouble, use:

   `sudo sysctl kernel.perf_event_paranoid=1`

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

# Reference

A description and use cases can be found in the following paper. Please cite this if you use `lo2s` for scientific work.

Thomas Ilsche, Robert Schöne, Mario Bielert, Andreas Gocht and Daniel Hackenberg. ["lo2s – Multi-Core System and Application Performance Analysis for Linux" 📕](https://tu-dresden.de/zih/forschung/ressourcen/dateien/projekte/haec/lo2s.pdf) In: Workshop on Monitoring and Analysis for High Performance Computing Systems Plus Applications (HPCMASPA). 2017. DOI: [10.1109/CLUSTER.2017.116](https://doi.org/10.1109/CLUSTER.2017.116)
