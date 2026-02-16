#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &>/dev/null && pwd)

if ! bash $SCRIPT_DIR/../paranoid.sh 0; then
	echo "sampling in system mode test needs kernel.perf_event_paranoid=0" >&2
	exit 127
fi

if ! bash $SCRIPT_DIR/../has_req_perf_events.sh; then
	echo "sampling in system mode test needs access to the 'instructions' perf event!" >&2
	exit 127
fi
rm -rf test_trace

./lo2s -c 1 -A --output-trace test_trace -- sleep 1

if otf2-print test_trace/traces.otf2 | grep "SAMPLE" >/dev/null; then
	exit 0
else
	echo "Trace did not contain calling context samples in system mode!"
	exit 1
fi
