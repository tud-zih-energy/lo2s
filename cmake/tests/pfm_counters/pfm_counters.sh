#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &>/dev/null && pwd)

if ! bash $SCRIPT_DIR/../paranoid.sh 0; then
	echo "PFM events test needs kernel.perf_event_paranoid=0" >&2
	exit 127
fi

if ! command -v perf; then
	echo "PFM events test requires 'perf' tool!"
	exit 127
fi

if ! command -v jq; then
	echo "PFM events test requires 'jq' tool!"
	exit 127
fi

if ! perf list --json 2>&1 >/dev/null; then
	echo "PFM events test requires 'perf list' with --json support!"
	exit 127
fi

pfm_event="$(perf list --json | jq -r 'last(.[] | select(.EventType == "PFM event")) | .EventName')"

if test  $pfm_event = "null"; then
	echo "perf not build with libpfm support or no PFM event found!"
	exit 127
fi

rm -rf test_trace

./lo2s -a --metric-count 1 --metric-leader cpu-cycles -E $pfm_event --output-trace test_trace -- sleep 1

if otf2-print test_trace/traces.otf2 | grep "METRIC" | grep $pfm_event >/dev/null; then
	exit 0
else
	echo "Trace does not contain counter metrics for ${pfm_event}!"
	exit 1
fi
