#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &>/dev/null && pwd)

if ! bash $SCRIPT_DIR/../paranoid.sh -1; then
	echo "syscall recording test needs kernel.perf_event_paranoid=-1" >&2
	exit 127
fi

if ! bash $SCRIPT_DIR/../test_sys_kernel_tracing.sh; then
	echo "/sys/kernel/tracing needs to be read-accessible for syscall recording test" >&2
	exit 127
fi

rm -rf test_trace

./lo2s -s write -o test_trace -- echo "FOOBAR"

if otf2-print test_trace/traces.otf2 | grep "ENTER" | grep "write" >/dev/null; then
	exit 0
else
	echo "Trace does not contain write() syscall events!"
	exit 1
fi
