#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &>/dev/null && pwd)

if ! bash $SCRIPT_DIR/../has_req_perf_events.sh; then
	echo "Required perf events (instructions, cpu-cycles) for metrics in process mode test could not be found!"
	exit 127
fi

rm -rf test_trace

./lo2s --metric-count 1 --metric-leader instructions --standard-metrics --output-trace test_trace -- sleep 1

if otf2-print test_trace/traces.otf2 | grep "METRIC" | grep "cpu-cycles" >/dev/null; then
	exit 0
else
	echo "Trace does not contain counter metrics in process mode!"
	exit 1
fi
