#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &>/dev/null && pwd)

if ! bash $SCRIPT_DIR/../paranoid.sh 0; then
	echo "metrics in system mode test needs kernel.perf_event_paranoid=0" >&2
	exit 127
fi

if ! bash $SCRIPT_DIR/../has_req_perf_events.sh; then
	echo "metrics in system mode test needs access to the 'instructions' perf event!" >&2
	exit 127
fi
rm -rf test_trace
./lo2s -a --metric-count 1 --metric-leader instructions --standard-metrics --output-trace test_trace -- sleep 1

if otf2-print test_trace/traces.otf2 | grep "METRIC" | grep "cpu-cycles" >/dev/null; then
	exit 0
else
	echo "Trace does not contain counter metrics in process mode!"
	exit 1
fi
