#!/usr/bin/env bash
set -euo pipefail

if ! command -v perf; then
	echo "predefined events list test requires 'perf' tool!"
	exit 127
fi

if ! command -v jq; then
	echo "predefined events list test requires 'jq' tool!"
	exit 127
fi

if ! perf list --json 2>&1 >/dev/null; then
	echo "predefined events list test requires 'perf list' with --json support!"
	exit 127
fi

perf list --json | jq \
	'.[] | select(.EventType=="Hardware event"
    or .EventType=="Software event"
    or .EventType=="Hardware cache event")
    | .EventName' | tr -d \" | while read ev; do
	rm -rf test_trace

	./lo2s -a --userspace-metric-event $ev --output-trace test_trace -- sleep 1

	if otf2-print test_trace/traces.otf2 | grep "METRIC" | grep $ev; then
		echo "Succesfully recorded $ev!"
	else
		echo "Trace does not contain \"$ev\" metrics in process mode!"
		exit 1
	fi
done
