#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &>/dev/null && pwd)

if ! bash $SCRIPT_DIR/../paranoid.sh 2; then
	echo "userspace metrics test needs kernel.perf_event_paranoid=2" >&2
	exit 127
fi

if ! bash $SCRIPT_DIR/../has_req_perf_events.sh; then
	echo "userspace metrics test needs access to the 'cpu-cycles' perf event!" >&2
	exit 127
fi

rm -rf test_trace

./lo2s --userspace-metric-event cpu-cycles --output-trace test_trace -- sleep 1

if otf2-print test_trace/traces.otf2 | grep "METRIC" | grep "cpu-cycles" >/dev/null; then
	exit 0
else
	echo "Trace does not contain counter metrics read as userspace-metric-event!"
	exit 1
fi
