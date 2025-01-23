#!/usr/bin/env bash
set -euo pipefail

rm -rf test_trace

./lo2s -t syscalls/sys_enter_write -o test_trace -- echo "FOOBAR"

if otf2-print test_trace/traces.otf2 | grep "METRIC" | grep "sys_enter_write::__syscall_nr" > /dev/null
then
    exit 0
else
    echo "Trace did not contain write() syscall events!"
    exit 1
fi
