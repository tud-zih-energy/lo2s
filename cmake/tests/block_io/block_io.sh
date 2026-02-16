#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &>/dev/null && pwd)

if ! bash $SCRIPT_DIR/../paranoid.sh -1; then
	echo "block I/O recording test needs kernel.perf_event_paranoid=-1" >&2
	exit 127
fi

if ! bash $SCRIPT_DIR/../test_sys_kernel_tracing.sh; then
	echo "/sys/kernel/tracing needs to be read-accessible for block I/O test" >&2
	exit 127
fi

rm -rf test_trace

./lo2s --no-instruction-sampling --block-io --output-trace test_trace -- bash -c "echo "TEST!" > test_file && sleep 1 && sync"

rm test_file

if otf2-print test_trace/traces.otf2 | grep "IO_OPERATION" >/dev/null; then
	exit 0
else
	echo "Trace did not contain IO operations!"
	exit 1
fi
